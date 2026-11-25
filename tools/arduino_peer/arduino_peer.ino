/*
 * LoRa Talk test peer: a second RYLR998 on an Arduino, for testing the
 * Flipper app end to end.
 *
 *  - Echo bot: every message it receives is sent straight back to the sender
 *    as "echo: <text>" (toggle with /echo).
 *  - Serial bridge: lines typed in the Serial Monitor (115200 baud, "Both NL &
 *    CR" or "Newline") go to the module, e.g. AT+PARAMETER?
 *  - /send <addr> <text> sends a message, e.g. "/send 0 hello flipper".
 *  - /status prints the module settings.
 *
 * ---------------------------------------------------------------------------
 * WIRING
 *
 *   Arduino          RYLR998
 *   3.3V supply  ->  VDD   (see POWER below)
 *   GND          ->  GND
 *   LORA_TX pin  ->  RXD   through a level shifter or divider on 5V boards
 *                          (e.g. 1k from the Arduino pin, 2k to GND)
 *   LORA_RX pin  <-  TXD   (3.3V output; a 5V AVR reads it as HIGH)
 *   NRST            leave unconnected
 *
 * Arduino Mega 2560 (recommended: has a spare hardware UART, Serial1):
 *   3.3V        -> VDD   (or an external 3.3V regulator, see POWER)
 *   GND         -> GND
 *   18 (TX1)    -> 1k -+-> RXD      voltage divider: the Mega's TX is 5V
 *                      2k
 *                      |
 *                     GND
 *   19 (RX1)    <- TXD
 *
 * The RYLR998 is a 3.3V part: never put 5V on VDD or RXD.
 *
 * POWER: the module draws ~140 mA when transmitting at 22 dBm. An Uno's 3.3V
 * pin is only good for ~150 mA, so either use a separate 3.3V regulator or
 * lower the power. On a desk, keep TX power low anyway (TX_POWER below):
 * two modules at full power a few cm apart can overload each other's receiver.
 *
 * UART: boards with a spare hardware serial port (Mega, Leonardo, Micro,
 * Nano Every, ESP32, ...) use Serial1 at 115200. Boards without one (Uno,
 * Nano) use SoftwareSerial on pins 10 (RX) / 11 (TX), which can't receive
 * reliably at 115200. For those, set the module to 9600 baud first: wire it
 * to the Flipper, set Baud Rate = 9600 in LoRa Talk > Module Settings, apply,
 * then move it to the Arduino.
 * ---------------------------------------------------------------------------
 */

// ---- Settings: must match the Flipper (LoRa Talk > Module Info) ----
#define PEER_ADDRESS 2 // this module's address
#define NETWORK_ID   18
#define BAND_HZ      915000000UL
#define PARAMETER    "9,7,1,12" // SF, BW code, CR code, preamble (module default)
#define TX_POWER     10 // dBm; plenty for bench testing
#define PASSWORD     "" // "" = none, or 8 hex digits e.g. "EEDCAA90"
#define APPLY_SETTINGS_ON_BOOT 1 // 0 = leave the module as it is

#if defined(HAVE_HWSERIAL1) || defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_RP2040) || \
    defined(ARDUINO_ARCH_SAMD) || defined(ARDUINO_ARCH_MEGAAVR)
#define LoRa      Serial1
#define LORA_BAUD 115200
#else
#include <SoftwareSerial.h>
#define LORA_RX_PIN 10
#define LORA_TX_PIN 11
SoftwareSerial LoRa(LORA_RX_PIN, LORA_TX_PIN);
#define LORA_BAUD 9600
#endif

// Sized for an Uno's 2 KB of RAM: "+RCV=65535,240,<240 bytes>,-120,-20" fits
static const size_t LINE_MAX_LEN = 264;
static char lora_line[LINE_MAX_LEN + 1];
static size_t lora_len = 0;
static char usb_line[LINE_MAX_LEN + 1];
static size_t usb_len = 0;
static bool echo_enabled = true;

// Read one line from the module, waiting up to timeout_ms. Returns false on timeout.
static bool lora_read_line(char* out, size_t size, unsigned long timeout_ms) {
    unsigned long start = millis();
    size_t len = 0;
    while(millis() - start < timeout_ms) {
        while(LoRa.available()) {
            char c = (char)LoRa.read();
            if(c == '\r') continue;
            if(c == '\n') {
                if(len == 0) continue;
                out[len] = '\0';
                return true;
            }
            if(len < size - 1) out[len++] = c;
        }
    }
    return false;
}

// Wait for the reply to a command that has been sent. Returns true on +OK / +VALUE.
static bool lora_wait_reply(unsigned long timeout_ms) {
    char reply[64]; // replies are short; a +RCV arriving meanwhile is shown truncated
    while(lora_read_line(reply, sizeof(reply), timeout_ms)) {
        Serial.print(F("<< "));
        Serial.println(reply);
        if(strncmp(reply, "+RCV=", 5) == 0) continue; // not our answer
        return strncmp(reply, "+ERR", 4) != 0;
    }
    Serial.println(F("!! no reply"));
    return false;
}

// Send an AT command and print the reply.
static bool lora_command(const char* cmd, unsigned long timeout_ms = 1000) {
    Serial.print(F(">> "));
    Serial.println(cmd);
    LoRa.print(cmd);
    LoRa.print(F("\r\n"));
    return lora_wait_reply(timeout_ms);
}

// AT+SEND=<addr>,<len>,<prefix><text>, streamed so no extra buffer is needed
static void lora_send(unsigned address, const char* prefix, const char* text) {
    size_t prefix_len = strlen(prefix);
    size_t len = strlen(text);
    if(prefix_len + len > 240) len = 240 - prefix_len;

    Serial.print(F(">> AT+SEND="));
    Serial.print(address);
    Serial.print(',');
    Serial.print(prefix_len + len);
    Serial.print(',');
    Serial.print(prefix);
    Serial.write((const uint8_t*)text, len);
    Serial.println();

    LoRa.print(F("AT+SEND="));
    LoRa.print(address);
    LoRa.print(',');
    LoRa.print(prefix_len + len);
    LoRa.print(',');
    LoRa.print(prefix);
    LoRa.write((const uint8_t*)text, len);
    LoRa.print(F("\r\n"));
    lora_wait_reply(5000); // allow for time on air at slow settings
}

// +RCV=<addr>,<len>,<data>,<rssi>,<snr>; data may contain commas.
// Parsed in place (line is modified).
static void handle_rcv(char* line) {
    char* p = line + 5;
    unsigned address = (unsigned)strtoul(p, &p, 10);
    if(*p++ != ',') return;
    unsigned len = (unsigned)strtoul(p, &p, 10);
    if(*p++ != ',') return;
    if(len > 240 || strlen(p) < len) return;

    char* text = p;
    char* tail = p + len; // ",<rssi>,<snr>"
    char tail_copy[16];
    strncpy(tail_copy, tail, sizeof(tail_copy) - 1);
    tail_copy[sizeof(tail_copy) - 1] = '\0';
    *tail = '\0';

    Serial.print(F("** From "));
    Serial.print(address);
    Serial.print(F(" (rssi,snr"));
    Serial.print(tail_copy);
    Serial.print(F("): "));
    Serial.println(text);

    if(echo_enabled) {
        delay(100); // give the sender time to switch back to receive
        lora_send(address, "echo: ", text);
    }
}

static void handle_lora_line(char* line) {
    if(strncmp(line, "+RCV=", 5) == 0) {
        handle_rcv(line);
    } else {
        Serial.print(F("<< "));
        Serial.println(line);
    }
}

static void print_status() {
    const char* queries[] = {"AT+ADDRESS?", "AT+NETWORKID?", "AT+BAND?", "AT+PARAMETER?",
                             "AT+CRFOP?", "AT+CPIN?", "AT+VER?"};
    for(size_t i = 0; i < sizeof(queries) / sizeof(queries[0]); i++)
        lora_command(queries[i]);
    Serial.print(F("Echo: "));
    Serial.println(echo_enabled ? F("on") : F("off"));
}

static void handle_usb_line(char* line) {
    if(strncmp(line, "/echo", 5) == 0) {
        echo_enabled = !echo_enabled;
        Serial.print(F("Echo "));
        Serial.println(echo_enabled ? F("on") : F("off"));
    } else if(strncmp(line, "/send ", 6) == 0) {
        char* p = line + 6;
        unsigned address = (unsigned)strtoul(p, &p, 10);
        while(*p == ' ')
            p++;
        if(*p) lora_send(address, "", p);
    } else if(strcmp(line, "/status") == 0) {
        print_status();
    } else if(line[0]) {
        lora_command(line);
    }
}

void setup() {
    Serial.begin(115200);
    LoRa.begin(LORA_BAUD);
    delay(500);
    Serial.println(F("LoRa Talk test peer"));

    if(!lora_command("AT")) {
        // First command after power-up can fail on junk in the module's buffer
        if(!lora_command("AT")) {
            Serial.println(F("No module found: check wiring and LORA_BAUD."));
        }
    }

#if APPLY_SETTINGS_ON_BOOT
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "AT+NETWORKID=%d", NETWORK_ID);
    lora_command(cmd);
    snprintf(cmd, sizeof(cmd), "AT+ADDRESS=%d", PEER_ADDRESS);
    lora_command(cmd);
    snprintf(cmd, sizeof(cmd), "AT+BAND=%lu", (unsigned long)BAND_HZ);
    lora_command(cmd);
    lora_command("AT+PARAMETER=" PARAMETER);
    snprintf(cmd, sizeof(cmd), "AT+CRFOP=%d", TX_POWER);
    lora_command(cmd);
    if(strlen(PASSWORD) == 8) lora_command("AT+CPIN=" PASSWORD);
#endif

    print_status();
    Serial.println(F("Ready. Commands: /send <addr> <text>, /echo, /status, or raw AT."));
}

void loop() {
    while(LoRa.available()) {
        char c = (char)LoRa.read();
        if(c == '\r') continue;
        if(c == '\n') {
            lora_line[lora_len] = '\0';
            if(lora_len) handle_lora_line(lora_line);
            lora_len = 0;
        } else if(lora_len < LINE_MAX_LEN) {
            lora_line[lora_len++] = c;
        }
    }

    while(Serial.available()) {
        char c = (char)Serial.read();
        if(c == '\r' || c == '\n') {
            usb_line[usb_len] = '\0';
            if(usb_len) handle_usb_line(usb_line);
            usb_len = 0;
        } else if(usb_len < LINE_MAX_LEN) {
            usb_line[usb_len++] = c;
        }
    }
}
