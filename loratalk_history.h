/**
 * Chat history: the most recent messages in RAM, with every message also
 * appended to a log on the SD card (when one is present).
 *
 * Not thread safe by itself: callers that read messages from another thread
 * (the chat view's draw callback) must hold loratalk_history_lock().
 */
#pragma once

#include "lora/lora_proto.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LORATALK_HISTORY_MAX (50)

typedef enum {
    LoRaTalkMessageIn,
    LoRaTalkMessageOut,
} LoRaTalkMessageDir;

typedef enum {
    LoRaTalkMessageStatusNone, /**< Incoming */
    LoRaTalkMessageStatusSending,
    LoRaTalkMessageStatusSent,
    LoRaTalkMessageStatusFailed,
} LoRaTalkMessageStatus;

typedef struct {
    uint32_t id; /**< Outgoing: send id; incoming: 0 */
    uint32_t timestamp; /**< Unix time */
    uint16_t address; /**< Sender (in) or destination (out) */
    int16_t rssi;
    int16_t snr;
    uint8_t dir; /**< LoRaTalkMessageDir */
    uint8_t status; /**< LoRaTalkMessageStatus */
    char* text;
} LoRaTalkMessage;

typedef struct LoRaTalkHistory LoRaTalkHistory;

LoRaTalkHistory* loratalk_history_alloc(void);
void loratalk_history_free(LoRaTalkHistory* history);

void loratalk_history_lock(LoRaTalkHistory* history);
void loratalk_history_unlock(LoRaTalkHistory* history);

/** Load the latest messages from the SD card log. */
void loratalk_history_load(LoRaTalkHistory* history);

void loratalk_history_add_incoming(LoRaTalkHistory* history, const LoRaRxMessage* rx);

void loratalk_history_add_outgoing(
    LoRaTalkHistory* history,
    uint32_t id,
    uint16_t address,
    const char* text);

/** Set the final status of an outgoing message (also logs it to SD). */
void loratalk_history_set_status(
    LoRaTalkHistory* history,
    uint32_t id,
    LoRaTalkMessageStatus status);

/** Delete all messages, in RAM and on the SD card. */
void loratalk_history_clear(LoRaTalkHistory* history);

/** Number of messages (lock held). */
size_t loratalk_history_count(LoRaTalkHistory* history);

/** Message i, 0 = oldest (lock held). */
const LoRaTalkMessage* loratalk_history_get(LoRaTalkHistory* history, size_t i);

/** True if messages are being saved to the SD card. */
bool loratalk_history_is_persistent(LoRaTalkHistory* history);
