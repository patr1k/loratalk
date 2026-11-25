#include "loratalk_settings.h"

#include <flipper_format/flipper_format.h>
#include <furi.h>
#include <storage/storage.h>

#define TAG "LoRaTalkSettings"

#define SETTINGS_FILETYPE "LoRaTalk Settings"
#define SETTINGS_VERSION  (1)

void loratalk_settings_default(LoRaTalkSettings* settings) {
    memset(settings, 0, sizeof(*settings));
    settings->has_config = false;
    lora_config_default(&settings->config);
    settings->destination = 0;
}

bool loratalk_sd_available(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    bool ok = storage_sd_status(storage) == FSE_OK;
    furi_record_close(RECORD_STORAGE);
    return ok;
}

static bool read_u32(FlipperFormat* ff, const char* key, uint32_t* value) {
    // Keys are optional; rewind so their order in the file doesn't matter
    flipper_format_rewind(ff);
    return flipper_format_read_uint32(ff, key, value, 1);
}

bool loratalk_settings_load(LoRaTalkSettings* settings) {
    loratalk_settings_default(settings);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_file_alloc(storage);
    FuriString* str = furi_string_alloc();
    bool ok = false;

    do {
        if(!flipper_format_file_open_existing(ff, LORATALK_SETTINGS_PATH)) break;
        uint32_t version = 0;
        if(!flipper_format_read_header(ff, str, &version)) break;
        if(furi_string_cmp_str(str, SETTINGS_FILETYPE) != 0 || version != SETTINGS_VERSION) break;

        LoRaConfig* c = &settings->config;
        uint32_t v;
        if(read_u32(ff, "Baud", &v)) c->baud = v;
        if(read_u32(ff, "Destination", &v)) settings->destination = (uint16_t)v;
        if(read_u32(ff, "Address", &v)) c->address = (uint16_t)v;
        if(read_u32(ff, "NetworkId", &v)) c->network_id = (uint8_t)v;
        if(read_u32(ff, "Frequency", &v)) c->band_hz = v;
        if(read_u32(ff, "SpreadingFactor", &v)) c->sf = (uint8_t)v;
        if(read_u32(ff, "Bandwidth", &v)) c->bw = (uint8_t)v;
        if(read_u32(ff, "CodingRate", &v)) c->cr = (uint8_t)v;
        if(read_u32(ff, "Preamble", &v)) c->preamble = (uint8_t)v;
        if(read_u32(ff, "Power", &v)) c->rf_power = (uint8_t)v;
        flipper_format_rewind(ff);
        if(flipper_format_read_string(ff, "Password", str)) {
            strlcpy(c->password, furi_string_get_cstr(str), sizeof(c->password));
        }
        flipper_format_rewind(ff);
        bool has_config = false;
        if(flipper_format_read_bool(ff, "HasConfig", &has_config, 1)) {
            settings->has_config = has_config;
        }
        lora_config_sanitize(c);
        ok = true;
    } while(false);

    if(!ok) FURI_LOG_I(TAG, "No usable settings, using defaults");

    furi_string_free(str);
    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

bool loratalk_settings_save(const LoRaTalkSettings* settings) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    bool ok = false;

    if(storage_sd_status(storage) != FSE_OK) {
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    storage_simply_mkdir(storage, LORATALK_DATA_DIR);
    FlipperFormat* ff = flipper_format_file_alloc(storage);
    const LoRaConfig* c = &settings->config;

    do {
        if(!flipper_format_file_open_always(ff, LORATALK_SETTINGS_PATH)) break;
        if(!flipper_format_write_header_cstr(ff, SETTINGS_FILETYPE, SETTINGS_VERSION)) break;
        bool has_config = settings->has_config;
        if(!flipper_format_write_bool(ff, "HasConfig", &has_config, 1)) break;

        uint32_t v;
#define WRITE_U32(key, value)                                       \
    v = (uint32_t)(value);                                          \
    if(!flipper_format_write_uint32(ff, key, &v, 1)) break;
        WRITE_U32("Baud", c->baud);
        WRITE_U32("Destination", settings->destination);
        WRITE_U32("Address", c->address);
        WRITE_U32("NetworkId", c->network_id);
        WRITE_U32("Frequency", c->band_hz);
        WRITE_U32("SpreadingFactor", c->sf);
        WRITE_U32("Bandwidth", c->bw);
        WRITE_U32("CodingRate", c->cr);
        WRITE_U32("Preamble", c->preamble);
        WRITE_U32("Power", c->rf_power);
#undef WRITE_U32
        if(!flipper_format_write_string_cstr(ff, "Password", c->password)) break;
        ok = true;
    } while(false);

    if(!ok) FURI_LOG_E(TAG, "Failed to save settings");

    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);
    return ok;
}
