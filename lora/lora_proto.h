/**
 * RYLR998 AT protocol helpers: classifying and parsing the lines the module
 * prints on its UART TX pin.
 *
 * Pure C (no Furi dependencies) so it can be unit-tested on a PC.
 * See tests/ for the host test suite.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum AT+SEND payload, per the REYAX AT command guide */
#define LORA_MAX_PAYLOAD (240)

/** Longest line we expect: "+RCV=65535,240,<240 bytes>,-120,-20" plus slack */
#define LORA_LINE_MAX (300)

typedef enum {
    LoRaLineUnknown, /**< Not recognised (noise, wrong baud rate, ...) */
    LoRaLineOk, /**< +OK */
    LoRaLineError, /**< +ERR=<code> */
    LoRaLineReceive, /**< +RCV=<addr>,<len>,<data>,<rssi>,<snr> */
    LoRaLineReady, /**< +READY (end of reset) */
    LoRaLineReset, /**< +RESET (start of reset) */
    LoRaLineValue, /**< +KEY=value (query responses, +IPR=..., ...) */
} LoRaLineType;

typedef struct {
    uint16_t address; /**< Sender address */
    uint8_t length; /**< Payload length in bytes */
    char data[LORA_MAX_PAYLOAD + 1]; /**< Payload, NUL terminated */
    int16_t rssi; /**< dBm */
    int16_t snr; /**< dB */
} LoRaRxMessage;

/** Classify a single line (without the trailing CR/LF). */
LoRaLineType lora_proto_classify(const char* line);

/** Return the code of a "+ERR=n" line, or -1 if the line isn't one. */
int lora_proto_error_code(const char* line);

/**
 * Parse a "+RCV=..." line. The payload may itself contain commas, so the
 * declared length is used to find where it ends; if that doesn't line up,
 * RSSI/SNR are taken from the last two fields instead.
 *
 * Control characters in the payload are replaced with spaces.
 */
bool lora_proto_parse_rcv(const char* line, LoRaRxMessage* out);

/**
 * If line is "+<key>=<value>", return a pointer to <value>, else NULL.
 * key is given without '+' or '=' (e.g. "ADDRESS").
 */
const char* lora_proto_value(const char* line, const char* key);

/** Human readable description of a module +ERR code. */
const char* lora_proto_error_str(int code);

/** Replace control characters (CR, LF, TAB, ...) with spaces, in place. */
void lora_proto_sanitize(char* text);

#ifdef __cplusplus
}
#endif
