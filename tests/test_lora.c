/* Host-side tests for lora_proto.c and lora_config.c. Run: make -C tests */
#include "lora_config.h"
#include "lora_proto.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;
static int checks = 0;

#define CHECK(cond)                                                \
    do {                                                           \
        checks++;                                                  \
        if(!(cond)) {                                              \
            failures++;                                            \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        }                                                          \
    } while(0)

static void test_classify(void) {
    CHECK(lora_proto_classify("+OK") == LoRaLineOk);
    CHECK(lora_proto_classify("+READY") == LoRaLineReady);
    CHECK(lora_proto_classify("+RESET") == LoRaLineReset);
    CHECK(lora_proto_classify("+ERR=17") == LoRaLineError);
    CHECK(lora_proto_classify("+RCV=1,2,hi,-50,10") == LoRaLineReceive);
    CHECK(lora_proto_classify("+ADDRESS=120") == LoRaLineValue);
    CHECK(lora_proto_classify("+IPR=9600") == LoRaLineValue);
    CHECK(lora_proto_classify("+CPIN=No Password!") == LoRaLineValue);
    CHECK(lora_proto_classify("+FACTORY") == LoRaLineValue);
    CHECK(lora_proto_classify("") == LoRaLineUnknown);
    CHECK(lora_proto_classify("garbage") == LoRaLineUnknown);
    CHECK(lora_proto_classify("+\x93\x12") == LoRaLineUnknown);
    CHECK(lora_proto_classify("+OKAY") == LoRaLineUnknown);
    CHECK(lora_proto_classify(NULL) == LoRaLineUnknown);
}

static void test_error_code(void) {
    CHECK(lora_proto_error_code("+ERR=1") == 1);
    CHECK(lora_proto_error_code("+ERR=20") == 20);
    CHECK(lora_proto_error_code("+ERR=") == -1);
    CHECK(lora_proto_error_code("+OK") == -1);
    CHECK(strcmp(lora_proto_error_str(17), "Radio busy") == 0);
    CHECK(strcmp(lora_proto_error_str(99), "Module error") == 0);
}

static void test_value(void) {
    CHECK(strcmp(lora_proto_value("+ADDRESS=120", "ADDRESS"), "120") == 0);
    CHECK(strcmp(lora_proto_value("+NETWORK=6", "NETWORK"), "6") == 0);
    CHECK(lora_proto_value("+NETWORK=6", "NETWORKID") == NULL);
    CHECK(lora_proto_value("+ADDRESSX=1", "ADDRESS") == NULL);
    CHECK(lora_proto_value("+OK", "ADDRESS") == NULL);
}

static void test_rcv(void) {
    LoRaRxMessage m;

    CHECK(lora_proto_parse_rcv("+RCV=50,5,HELLO,-99,40", &m));
    CHECK(m.address == 50);
    CHECK(m.length == 5);
    CHECK(strcmp(m.data, "HELLO") == 0);
    CHECK(m.rssi == -99);
    CHECK(m.snr == 40);

    // Commas inside the payload
    CHECK(lora_proto_parse_rcv("+RCV=7,12,hi, there,,x,-40,-3", &m));
    CHECK(m.address == 7);
    CHECK(strcmp(m.data, "hi, there,,x") == 0);
    CHECK(m.rssi == -40);
    CHECK(m.snr == -3);

    // Spaces as printed in the AT guide
    CHECK(lora_proto_parse_rcv("+RCV=50,5,HELLO, -99, 40", &m));
    CHECK(strcmp(m.data, "HELLO") == 0);
    CHECK(m.rssi == -99);

    // Declared length wrong: fall back to last two fields
    CHECK(lora_proto_parse_rcv("+RCV=1,3,HELLO,-20,9", &m));
    CHECK(strcmp(m.data, "HELLO") == 0);
    CHECK(m.length == 5);
    CHECK(m.rssi == -20);
    CHECK(m.snr == 9);

    // Empty payload
    CHECK(lora_proto_parse_rcv("+RCV=3,0,,-60,5", &m));
    CHECK(m.length == 0);
    CHECK(m.data[0] == '\0');

    // Control characters sanitised
    CHECK(lora_proto_parse_rcv("+RCV=3,3,a\tb,-60,5", &m));
    CHECK(strcmp(m.data, "a b") == 0);

    // Max payload
    char line[LORA_LINE_MAX];
    char payload[LORA_MAX_PAYLOAD + 1];
    memset(payload, 'x', LORA_MAX_PAYLOAD);
    payload[LORA_MAX_PAYLOAD] = '\0';
    snprintf(line, sizeof(line), "+RCV=65535,240,%s,-120,-20", payload);
    CHECK(lora_proto_parse_rcv(line, &m));
    CHECK(m.address == 65535);
    CHECK(m.length == 240);
    CHECK(strcmp(m.data, payload) == 0);

    // Rejects
    CHECK(!lora_proto_parse_rcv("+RCV=70000,1,a,-1,1", &m));
    CHECK(!lora_proto_parse_rcv("+RCV=1,241,a,-1,1", &m));
    CHECK(!lora_proto_parse_rcv("+RCV=1,1,a", &m));
    CHECK(!lora_proto_parse_rcv("+RCV=", &m));
    CHECK(!lora_proto_parse_rcv("+OK", &m));
    CHECK(!lora_proto_parse_rcv("+RCV=1,1,a,-1,x", &m));
}

static void test_config(void) {
    LoRaConfig c;
    lora_config_default(&c);
    CHECK(c.band_hz == 915000000UL);
    CHECK(c.sf == 9 && c.bw == LORA_BW_125 && c.cr == 1 && c.preamble == 12);
    CHECK(c.network_id == 18 && c.rf_power == 22 && c.baud == 115200);
    CHECK(lora_config_sanitize(&c));

    CHECK(lora_config_sf_max(LORA_BW_125) == 9);
    CHECK(lora_config_sf_max(LORA_BW_250) == 10);
    CHECK(lora_config_sf_max(LORA_BW_500) == 11);

    LoRaConfig bad = c;
    bad.sf = 11; // too high for 125 kHz
    bad.network_id = 16;
    bad.band_hz = 868000000UL;
    bad.rf_power = 30;
    bad.baud = 12345;
    bad.preamble = 30;
    CHECK(!lora_config_sanitize(&bad));
    CHECK(bad.sf == 9);
    CHECK(bad.network_id == 18);
    CHECK(bad.band_hz == 915000000UL);
    CHECK(bad.rf_power == 22);
    CHECK(bad.baud == 115200);
    CHECK(bad.preamble == 12);

    LoRaConfig locked = c;
    locked.network_id = 5;
    locked.preamble = 8;
    lora_config_sanitize(&locked);
    CHECK(locked.preamble == 12);

    LoRaConfig open = c;
    open.preamble = 8;
    CHECK(lora_config_sanitize(&open));
    CHECK(open.preamble == 8);

    CHECK(lora_config_password_valid(""));
    CHECK(lora_config_password_valid("EEDCAA90"));
    CHECK(lora_config_password_valid("eedcaa90"));
    CHECK(!lora_config_password_valid("00000000"));
    CHECK(!lora_config_password_valid("EEDCAA9"));
    CHECK(!lora_config_password_valid("EEDCAA9G"));

    LoRaConfig pw = c;
    strcpy(pw.password, "abcdef12");
    lora_config_sanitize(&pw);
    CHECK(strcmp(pw.password, "ABCDEF12") == 0);

    CHECK(lora_config_baud_valid(300));
    CHECK(!lora_config_baud_valid(230400));

    LoRaConfig other = c;
    other.address = 99;
    other.rf_power = 3;
    other.baud = 9600;
    CHECK(lora_config_radio_equal(&c, &other));
    other.sf = 8;
    CHECK(!lora_config_radio_equal(&c, &other));
}

static void test_airtime(void) {
    LoRaConfig c;
    lora_config_default(&c);
    // SF9/125k/CR4/5/12 preamble, 8 byte payload (+8 framing): 44.25 symbols * 4.096 ms
    CHECK(lora_config_airtime_ms(&c, 8) == 182);
    // Longer payloads take longer; slower settings take longer
    CHECK(lora_config_airtime_ms(&c, 240) > lora_config_airtime_ms(&c, 8));
    LoRaConfig fast = c;
    fast.sf = 5;
    fast.bw = LORA_BW_500;
    CHECK(lora_config_airtime_ms(&fast, 240) < lora_config_airtime_ms(&c, 240));
    LoRaConfig slow = c;
    slow.sf = 11;
    slow.bw = LORA_BW_500;
    slow.cr = 4;
    slow.preamble = 24;
    CHECK(lora_config_airtime_ms(&slow, 240) > 1000);
}

int main(void) {
    test_classify();
    test_error_code();
    test_value();
    test_rcv();
    test_config();
    test_airtime();
    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
