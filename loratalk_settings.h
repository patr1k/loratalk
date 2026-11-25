#pragma once

#include "lora/lora_config.h"

#include <storage/storage.h>

#include <stdbool.h>
#include <stdint.h>

#define LORATALK_DATA_DIR      EXT_PATH("apps_data/loratalk")
#define LORATALK_SETTINGS_PATH LORATALK_DATA_DIR "/settings.conf"
#define LORATALK_HISTORY_PATH  LORATALK_DATA_DIR "/history.txt"

/** What the Flipper remembers between runs. */
typedef struct {
    /** True once a module has been read at least once. Until then, the
     *  module's own settings are adopted instead of being overwritten. */
    bool has_config;
    /** Last known module settings; config.baud is tried first on connect. */
    LoRaConfig config;
    /** Last chat destination (0 = everyone). */
    uint16_t destination;
} LoRaTalkSettings;

void loratalk_settings_default(LoRaTalkSettings* settings);
bool loratalk_settings_load(LoRaTalkSettings* settings);
bool loratalk_settings_save(const LoRaTalkSettings* settings);

/** True if an SD card is mounted and usable. */
bool loratalk_sd_available(void);
