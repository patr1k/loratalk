/**
 * RYLR998 configuration model: allowed values, validation rules and helpers.
 *
 * Pure C (no Furi dependencies) so it can be unit-tested on a PC.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* US 902-928 MHz ISM band */
#define LORA_BAND_MIN_HZ     (902000000UL)
#define LORA_BAND_MAX_HZ     (928000000UL)
#define LORA_BAND_DEFAULT_HZ (915000000UL)

#define LORA_SF_MIN (5)
#define LORA_SF_MAX (11)

/* AT+PARAMETER bandwidth codes */
#define LORA_BW_125 (7)
#define LORA_BW_250 (8)
#define LORA_BW_500 (9)

#define LORA_CR_MIN (1) /* 4/5 */
#define LORA_CR_MAX (4) /* 4/8 */

#define LORA_PREAMBLE_MIN     (4)
#define LORA_PREAMBLE_MAX     (24)
#define LORA_PREAMBLE_DEFAULT (12)

/* Only NETWORKID 18 may use a preamble other than 12 */
#define LORA_NETWORK_ID_DEFAULT (18)

#define LORA_POWER_MAX (22)

#define LORA_BAUD_DEFAULT (115200UL)

/** Password: 8 hex digits; empty string means "no password" */
#define LORA_PASSWORD_LEN (8)

typedef struct {
    uint16_t address;
    uint8_t network_id;
    uint32_t band_hz;
    uint8_t sf;
    uint8_t bw; /**< 7 = 125 kHz, 8 = 250 kHz, 9 = 500 kHz */
    uint8_t cr; /**< 1..4 = 4/5..4/8 */
    uint8_t preamble;
    uint8_t rf_power; /**< dBm, 0..22 */
    uint32_t baud;
    char password[LORA_PASSWORD_LEN + 1];
} LoRaConfig;

extern const uint32_t lora_baud_rates[];
extern const size_t lora_baud_rates_count;

extern const uint8_t lora_network_ids[];
extern const size_t lora_network_ids_count;

/** Factory defaults (per the AT command guide), with the band set to 915 MHz. */
void lora_config_default(LoRaConfig* cfg);

/** Largest spreading factor the module accepts at a bandwidth code. */
uint8_t lora_config_sf_max(uint8_t bw);

/** Bandwidth code -> kHz (0 if invalid). */
uint16_t lora_config_bw_khz(uint8_t bw);

bool lora_config_preamble_locked(uint8_t network_id);
bool lora_config_network_id_valid(uint8_t network_id);
bool lora_config_baud_valid(uint32_t baud);

/** Empty, or exactly 8 hex digits and not 00000000. */
bool lora_config_password_valid(const char* password);

/**
 * Force every field into a value the module accepts.
 * @return true if nothing had to change
 */
bool lora_config_sanitize(LoRaConfig* cfg);

/** Estimated time on air for a payload, in milliseconds. */
uint32_t lora_config_airtime_ms(const LoRaConfig* cfg, size_t payload_len);

/** True if the radio settings (everything two peers must agree on) match. */
bool lora_config_radio_equal(const LoRaConfig* a, const LoRaConfig* b);

#ifdef __cplusplus
}
#endif
