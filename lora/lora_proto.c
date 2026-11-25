#include "lora_proto.h"

#include <stdlib.h>
#include <string.h>

static bool starts_with(const char* s, const char* prefix) {
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

LoRaLineType lora_proto_classify(const char* line) {
    if(!line || line[0] != '+') return LoRaLineUnknown;
    if(strcmp(line, "+OK") == 0) return LoRaLineOk;
    if(strcmp(line, "+READY") == 0) return LoRaLineReady;
    if(strcmp(line, "+RESET") == 0) return LoRaLineReset;
    if(starts_with(line, "+ERR=")) return LoRaLineError;
    if(starts_with(line, "+RCV=")) return LoRaLineReceive;

    // +KEY=value, KEY made of upper-case letters
    const char* p = line + 1;
    if(*p < 'A' || *p > 'Z') return LoRaLineUnknown;
    while(*p >= 'A' && *p <= 'Z')
        p++;
    if(*p == '=') return LoRaLineValue;

    // The only bare reply besides +OK/+READY/+RESET
    if(strcmp(line, "+FACTORY") == 0) return LoRaLineValue;
    return LoRaLineUnknown;
}

int lora_proto_error_code(const char* line) {
    if(!line || !starts_with(line, "+ERR=")) return -1;
    char* end = NULL;
    long code = strtol(line + 5, &end, 10);
    if(end == line + 5 || code < 0) return -1;
    return (int)code;
}

/* Parse a decimal number, allowing leading spaces (the AT guide prints
 * "+RCV=50, 5, HELLO, -99, 40" with spaces, real modules don't). */
static bool parse_long(const char* s, const char** end, long* out) {
    while(*s == ' ')
        s++;
    char* e = NULL;
    long v = strtol(s, &e, 10);
    if(e == s) return false;
    *out = v;
    *end = e;
    return true;
}

void lora_proto_sanitize(char* text) {
    for(; *text; text++) {
        if((unsigned char)*text < 0x20 || *text == 0x7F) *text = ' ';
    }
}

/* Parse ",<rssi>,<snr>" at p (must be the whole rest of the line). */
static bool parse_tail(const char* p, LoRaRxMessage* out) {
    long rssi, snr;
    if(*p++ != ',') return false;
    if(!parse_long(p, &p, &rssi)) return false;
    if(*p++ != ',') return false;
    if(!parse_long(p, &p, &snr)) return false;
    while(*p == ' ')
        p++;
    if(*p != '\0') return false;
    out->rssi = (int16_t)rssi;
    out->snr = (int16_t)snr;
    return true;
}

bool lora_proto_parse_rcv(const char* line, LoRaRxMessage* out) {
    if(!line || !out || !starts_with(line, "+RCV=")) return false;
    memset(out, 0, sizeof(*out));

    const char* p = line + 5;
    long address, length;
    if(!parse_long(p, &p, &address) || address < 0 || address > 65535) return false;
    if(*p++ != ',') return false;
    if(!parse_long(p, &p, &length) || length < 0 || length > LORA_MAX_PAYLOAD) return false;
    if(*p++ != ',') return false;

    const char* data = p;
    size_t remaining = strlen(data);
    size_t data_len = 0;

    // Preferred: trust the declared length
    if((size_t)length <= remaining && parse_tail(data + length, out)) {
        data_len = (size_t)length;
    } else {
        // Fallback: RSSI and SNR are the last two comma separated fields
        const char* last = strrchr(data, ',');
        if(!last || last == data) return false;
        const char* second_last = last - 1;
        while(second_last > data && *second_last != ',')
            second_last--;
        if(*second_last != ',') return false;
        if(!parse_tail(second_last, out)) return false;
        data_len = (size_t)(second_last - data);
        if(data_len > LORA_MAX_PAYLOAD) data_len = LORA_MAX_PAYLOAD;
    }

    out->address = (uint16_t)address;
    memcpy(out->data, data, data_len);
    out->data[data_len] = '\0';
    out->length = (uint8_t)data_len;
    lora_proto_sanitize(out->data);
    return true;
}

const char* lora_proto_value(const char* line, const char* key) {
    if(!line || !key || line[0] != '+') return NULL;
    size_t key_len = strlen(key);
    if(strncmp(line + 1, key, key_len) != 0) return NULL;
    if(line[1 + key_len] != '=') return NULL;
    return line + 2 + key_len;
}

const char* lora_proto_error_str(int code) {
    switch(code) {
    case 1:
        return "Missing CR/LF";
    case 2:
        return "Bad command";
    case 4:
        return "Unknown command";
    case 5:
        return "Length mismatch";
    case 10:
        return "TX timed out";
    case 12:
        return "CRC error";
    case 13:
        return "Over 240 bytes";
    case 14:
        return "Flash write failed";
    case 15:
        return "Unknown failure";
    case 17:
        return "Radio busy";
    case 18:
        return "Preamble not allowed";
    case 19:
        return "RX header error";
    case 20:
        return "Bad sleep timing";
    default:
        return "Module error";
    }
}
