#include "loratalk_history.h"
#include "loratalk_settings.h"

#include <furi.h>
#include <furi_hal_rtc.h>
#include <storage/storage.h>
#include <toolbox/stream/file_stream.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAG "LoRaTalkHistory"

/* Rewrite the log with only the RAM contents once it grows past this */
#define HISTORY_COMPACT_SIZE (32 * 1024)

/*
 * Log format, one message per line, tab separated:
 *   <I|O> <unix time> <address> <rssi> <snr> <status> <text>
 * Text never contains tabs or newlines (control characters are replaced).
 */

struct LoRaTalkHistory {
    FuriMutex* mutex;
    LoRaTalkMessage messages[LORATALK_HISTORY_MAX];
    size_t head; /* index of oldest */
    size_t count;
    bool persistent;
};

LoRaTalkHistory* loratalk_history_alloc(void) {
    LoRaTalkHistory* history = malloc(sizeof(LoRaTalkHistory));
    memset(history, 0, sizeof(LoRaTalkHistory));
    history->mutex = furi_mutex_alloc(FuriMutexTypeRecursive);
    return history;
}

static void history_clear_ram(LoRaTalkHistory* history) {
    for(size_t i = 0; i < LORATALK_HISTORY_MAX; i++) {
        free(history->messages[i].text);
        history->messages[i].text = NULL;
    }
    history->head = 0;
    history->count = 0;
}

void loratalk_history_free(LoRaTalkHistory* history) {
    history_clear_ram(history);
    furi_mutex_free(history->mutex);
    free(history);
}

void loratalk_history_lock(LoRaTalkHistory* history) {
    furi_mutex_acquire(history->mutex, FuriWaitForever);
}

void loratalk_history_unlock(LoRaTalkHistory* history) {
    furi_mutex_release(history->mutex);
}

size_t loratalk_history_count(LoRaTalkHistory* history) {
    return history->count;
}

const LoRaTalkMessage* loratalk_history_get(LoRaTalkHistory* history, size_t i) {
    if(i >= history->count) return NULL;
    return &history->messages[(history->head + i) % LORATALK_HISTORY_MAX];
}

bool loratalk_history_is_persistent(LoRaTalkHistory* history) {
    return history->persistent;
}

/* Append to RAM, dropping the oldest message when full. Takes ownership of text. */
static LoRaTalkMessage* history_push(LoRaTalkHistory* history, const LoRaTalkMessage* msg) {
    size_t slot;
    if(history->count < LORATALK_HISTORY_MAX) {
        slot = (history->head + history->count) % LORATALK_HISTORY_MAX;
        history->count++;
    } else {
        slot = history->head;
        free(history->messages[slot].text);
        history->head = (history->head + 1) % LORATALK_HISTORY_MAX;
    }
    history->messages[slot] = *msg;
    return &history->messages[slot];
}

static char* history_strdup(const char* text) {
    size_t len = strlen(text);
    char* copy = malloc(len + 1);
    memcpy(copy, text, len + 1);
    lora_proto_sanitize(copy);
    return copy;
}

static void history_format(const LoRaTalkMessage* msg, FuriString* out) {
    furi_string_printf(
        out,
        "%c\t%lu\t%u\t%d\t%d\t%u\t%s\n",
        msg->dir == LoRaTalkMessageIn ? 'I' : 'O',
        msg->timestamp,
        msg->address,
        msg->rssi,
        msg->snr,
        msg->status,
        msg->text);
}

static bool history_parse(const char* line, LoRaTalkMessage* msg) {
    char dir = 0;
    unsigned long timestamp = 0;
    unsigned address = 0, status = 0;
    int rssi = 0, snr = 0, offset = 0;
    if(sscanf(
           line,
           "%c\t%lu\t%u\t%d\t%d\t%u\t%n",
           &dir,
           &timestamp,
           &address,
           &rssi,
           &snr,
           &status,
           &offset) < 6 ||
       offset == 0) {
        return false;
    }
    if(dir != 'I' && dir != 'O') return false;

    memset(msg, 0, sizeof(*msg));
    msg->dir = dir == 'I' ? LoRaTalkMessageIn : LoRaTalkMessageOut;
    msg->timestamp = timestamp;
    msg->address = (uint16_t)address;
    msg->rssi = (int16_t)rssi;
    msg->snr = (int16_t)snr;
    msg->status = (uint8_t)status;
    if(msg->status > LoRaTalkMessageStatusFailed) msg->status = LoRaTalkMessageStatusNone;
    // A message still "sending" when the app closed never got a result
    if(msg->status == LoRaTalkMessageStatusSending) msg->status = LoRaTalkMessageStatusFailed;

    // Strip the newline
    char text[LORA_MAX_PAYLOAD + 1];
    strlcpy(text, line + offset, sizeof(text));
    size_t len = strlen(text);
    while(len && (text[len - 1] == '\n' || text[len - 1] == '\r'))
        text[--len] = '\0';
    msg->text = history_strdup(text);
    return true;
}

static void history_append_file(LoRaTalkHistory* history, const LoRaTalkMessage* msg) {
    if(!history->persistent) return;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(storage_sd_status(storage) == FSE_OK) {
        storage_simply_mkdir(storage, LORATALK_DATA_DIR);
        File* file = storage_file_alloc(storage);
        if(storage_file_open(file, LORATALK_HISTORY_PATH, FSAM_WRITE, FSOM_OPEN_APPEND)) {
            FuriString* line = furi_string_alloc();
            history_format(msg, line);
            storage_file_write(file, furi_string_get_cstr(line), furi_string_size(line));
            furi_string_free(line);
        } else {
            FURI_LOG_E(TAG, "Can't open history log");
        }
        storage_file_close(file);
        storage_file_free(file);
    }
    furi_record_close(RECORD_STORAGE);
}

/* Rewrite the log with just the messages held in RAM */
static void history_compact(LoRaTalkHistory* history, Storage* storage) {
    FURI_LOG_I(TAG, "Compacting history log");
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, LORATALK_HISTORY_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        FuriString* line = furi_string_alloc();
        for(size_t i = 0; i < history->count; i++) {
            history_format(loratalk_history_get(history, i), line);
            storage_file_write(file, furi_string_get_cstr(line), furi_string_size(line));
        }
        furi_string_free(line);
    }
    storage_file_close(file);
    storage_file_free(file);
}

void loratalk_history_load(LoRaTalkHistory* history) {
    loratalk_history_lock(history);
    history_clear_ram(history);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    history->persistent = storage_sd_status(storage) == FSE_OK;

    if(history->persistent) {
        Stream* stream = file_stream_alloc(storage);
        FuriString* line = furi_string_alloc();
        size_t size = 0;
        if(file_stream_open(stream, LORATALK_HISTORY_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
            size = stream_size(stream);
            while(stream_read_line(stream, line)) {
                LoRaTalkMessage msg;
                if(history_parse(furi_string_get_cstr(line), &msg)) history_push(history, &msg);
            }
        }
        file_stream_close(stream);
        stream_free(stream);
        furi_string_free(line);

        if(size > HISTORY_COMPACT_SIZE) history_compact(history, storage);
        FURI_LOG_I(TAG, "Loaded %u messages", history->count);
    } else {
        FURI_LOG_I(TAG, "No SD card, history won't be saved");
    }

    furi_record_close(RECORD_STORAGE);
    loratalk_history_unlock(history);
}

void loratalk_history_add_incoming(LoRaTalkHistory* history, const LoRaRxMessage* rx) {
    LoRaTalkMessage msg = {
        .id = 0,
        .timestamp = furi_hal_rtc_get_timestamp(),
        .address = rx->address,
        .rssi = rx->rssi,
        .snr = rx->snr,
        .dir = LoRaTalkMessageIn,
        .status = LoRaTalkMessageStatusNone,
        .text = history_strdup(rx->data),
    };
    loratalk_history_lock(history);
    LoRaTalkMessage* stored = history_push(history, &msg);
    LoRaTalkMessage copy = *stored;
    loratalk_history_unlock(history);
    // copy.text stays valid: only this (GUI) thread removes messages
    history_append_file(history, &copy);
}

void loratalk_history_add_outgoing(
    LoRaTalkHistory* history,
    uint32_t id,
    uint16_t address,
    const char* text) {
    LoRaTalkMessage msg = {
        .id = id,
        .timestamp = furi_hal_rtc_get_timestamp(),
        .address = address,
        .dir = LoRaTalkMessageOut,
        .status = LoRaTalkMessageStatusSending,
        .text = history_strdup(text),
    };
    loratalk_history_lock(history);
    history_push(history, &msg);
    loratalk_history_unlock(history);
    // Logged to SD once the result is known
}

void loratalk_history_set_status(
    LoRaTalkHistory* history,
    uint32_t id,
    LoRaTalkMessageStatus status) {
    LoRaTalkMessage copy;
    bool found = false;

    loratalk_history_lock(history);
    for(size_t i = 0; i < history->count; i++) {
        LoRaTalkMessage* msg = &history->messages[(history->head + i) % LORATALK_HISTORY_MAX];
        if(msg->dir == LoRaTalkMessageOut && msg->id == id) {
            msg->status = status;
            copy = *msg;
            found = true;
            break;
        }
    }
    loratalk_history_unlock(history);

    if(found) history_append_file(history, &copy);
}

void loratalk_history_clear(LoRaTalkHistory* history) {
    loratalk_history_lock(history);
    history_clear_ram(history);
    loratalk_history_unlock(history);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_remove(storage, LORATALK_HISTORY_PATH);
    furi_record_close(RECORD_STORAGE);
}
