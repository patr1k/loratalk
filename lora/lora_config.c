#include "lora_config.h"

#include <ctype.h>
#include <string.h>

const uint32_t lora_baud_rates[] = {300, 1200, 4800, 9600, 19200, 28800, 38400, 57600, 115200};
const size_t lora_baud_rates_count = sizeof(lora_baud_rates) / sizeof(lora_baud_rates[0]);

const uint8_t lora_network_ids[] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 18};
const size_t lora_network_ids_count = sizeof(lora_network_ids) / sizeof(lora_network_ids[0]);

void lora_config_default(LoRaConfig* cfg) {
    memset(cfg, 0, sizeof(*cfg));
    cfg->address = 0;
    cfg->network_id = LORA_NETWORK_ID_DEFAULT;
    cfg->band_hz = LORA_BAND_DEFAULT_HZ;
    cfg->sf = 9;
    cfg->bw = LORA_BW_125;
    cfg->cr = 1;
    cfg->preamble = LORA_PREAMBLE_DEFAULT;
    cfg->rf_power = LORA_POWER_MAX;
    cfg->baud = LORA_BAUD_DEFAULT;
    cfg->password[0] = '\0';
}

uint8_t lora_config_sf_max(uint8_t bw) {
    // "SF7 to SF9 at 125kHz, SF7 to SF10 at 250kHz, and SF7 to SF11 at 500kHz"
    switch(bw) {
    case LORA_BW_250:
        return 10;
    case LORA_BW_500:
        return 11;
    case LORA_BW_125:
    default:
        return 9;
    }
}

uint16_t lora_config_bw_khz(uint8_t bw) {
    switch(bw) {
    case LORA_BW_125:
        return 125;
    case LORA_BW_250:
        return 250;
    case LORA_BW_500:
        return 500;
    default:
        return 0;
    }
}

bool lora_config_preamble_locked(uint8_t network_id) {
    return network_id != LORA_NETWORK_ID_DEFAULT;
}

bool lora_config_network_id_valid(uint8_t network_id) {
    for(size_t i = 0; i < lora_network_ids_count; i++) {
        if(lora_network_ids[i] == network_id) return true;
    }
    return false;
}

bool lora_config_baud_valid(uint32_t baud) {
    for(size_t i = 0; i < lora_baud_rates_count; i++) {
        if(lora_baud_rates[i] == baud) return true;
    }
    return false;
}

bool lora_config_password_valid(const char* password) {
    size_t len = strlen(password);
    if(len == 0) return true;
    if(len != LORA_PASSWORD_LEN) return false;
    bool all_zero = true;
    for(size_t i = 0; i < len; i++) {
        if(!isxdigit((unsigned char)password[i])) return false;
        if(password[i] != '0') all_zero = false;
    }
    return !all_zero;
}

bool lora_config_sanitize(LoRaConfig* cfg) {
    LoRaConfig before = *cfg;

    if(!lora_config_network_id_valid(cfg->network_id)) cfg->network_id = LORA_NETWORK_ID_DEFAULT;
    if(cfg->band_hz < LORA_BAND_MIN_HZ || cfg->band_hz > LORA_BAND_MAX_HZ)
        cfg->band_hz = LORA_BAND_DEFAULT_HZ;
    if(cfg->bw < LORA_BW_125 || cfg->bw > LORA_BW_500) cfg->bw = LORA_BW_125;
    if(cfg->sf < LORA_SF_MIN) cfg->sf = LORA_SF_MIN;
    if(cfg->sf > lora_config_sf_max(cfg->bw)) cfg->sf = lora_config_sf_max(cfg->bw);
    if(cfg->cr < LORA_CR_MIN || cfg->cr > LORA_CR_MAX) cfg->cr = LORA_CR_MIN;
    if(lora_config_preamble_locked(cfg->network_id)) {
        cfg->preamble = LORA_PREAMBLE_DEFAULT;
    } else if(cfg->preamble < LORA_PREAMBLE_MIN || cfg->preamble > LORA_PREAMBLE_MAX) {
        cfg->preamble = LORA_PREAMBLE_DEFAULT;
    }
    if(cfg->rf_power > LORA_POWER_MAX) cfg->rf_power = LORA_POWER_MAX;
    if(!lora_config_baud_valid(cfg->baud)) cfg->baud = LORA_BAUD_DEFAULT;

    cfg->password[LORA_PASSWORD_LEN] = '\0';
    for(char* p = cfg->password; *p; p++)
        *p = (char)toupper((unsigned char)*p);
    if(!lora_config_password_valid(cfg->password)) cfg->password[0] = '\0';

    return memcmp(&before, cfg, sizeof(before)) == 0;
}

uint32_t lora_config_airtime_ms(const LoRaConfig* cfg, size_t payload_len) {
    // Semtech SX127x/SX126x time-on-air formula, explicit header, CRC on.
    // The module adds 8 bytes of its own framing to every payload.
    uint32_t bw_hz = (uint32_t)lora_config_bw_khz(cfg->bw) * 1000UL;
    if(bw_hz == 0) bw_hz = 125000UL;
    int32_t sf = cfg->sf;
    int32_t pl = (int32_t)payload_len + 8;

    // Symbol time in microseconds
    uint32_t t_sym_us = (uint32_t)(((uint64_t)1 << sf) * 1000000ULL / bw_hz);
    int32_t de = t_sym_us > 16000 ? 1 : 0; // low data rate optimisation

    int32_t num = 8 * pl - 4 * sf + 28 + 16;
    int32_t den = 4 * (sf - 2 * de);
    if(den <= 0) den = 1;
    int32_t payload_symbols = 8;
    if(num > 0) payload_symbols += ((num + den - 1) / den) * (cfg->cr + 4);

    // Preamble is (n + 4.25) symbols; work in quarter symbols to stay integer
    uint64_t quarter_symbols = (uint64_t)(cfg->preamble * 4 + 17) + (uint64_t)payload_symbols * 4;
    uint64_t us = quarter_symbols * t_sym_us / 4;
    return (uint32_t)((us + 999) / 1000);
}

bool lora_config_radio_equal(const LoRaConfig* a, const LoRaConfig* b) {
    return a->network_id == b->network_id && a->band_hz == b->band_hz && a->sf == b->sf &&
           a->bw == b->bw && a->cr == b->cr && a->preamble == b->preamble &&
           strcmp(a->password, b->password) == 0;
}
