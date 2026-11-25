/**
 * RYLR998 driver.
 *
 * Owns the Flipper USART (pins 13 TX / 14 RX) and a worker thread that does
 * all communication with the module. Callers submit jobs (connect, apply
 * settings, send) which run asynchronously; results and received messages are
 * reported through the event callback, which is invoked on the worker thread.
 */
#pragma once

#include "lora_config.h"
#include "lora_proto.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Local (non +ERR) error codes. Positive codes are module +ERR codes. */
#define LORA_OK            (0)
#define LORA_ERR_TIMEOUT   (-1) /**< Module did not answer */
#define LORA_ERR_NO_MODULE (-2) /**< No module found at any baud rate */
#define LORA_ERR_UART_BUSY (-3) /**< USART owned by someone else */
#define LORA_ERR_GARBAGE   (-4) /**< Unreadable reply (baud rate mismatch?) */
#define LORA_ERR_VERIFY    (-5) /**< Setting did not stick */
#define LORA_ERR_ABORTED   (-6) /**< Driver shutting down */

typedef enum {
    LoRaEventConnected, /**< Module found and settings read (error = re-apply result) */
    LoRaEventConnectFailed, /**< error says why */
    LoRaEventApplyDone, /**< Settings written (error = 0 on success) */
    LoRaEventSendDone, /**< id = message id given to lora_module_send */
    LoRaEventReceived, /**< rx holds the message */
} LoRaEventType;

typedef struct {
    LoRaEventType type;
    int error;
    uint32_t id;
    LoRaRxMessage rx;
} LoRaEvent;

typedef void (*LoRaEventCallback)(const LoRaEvent* event, void* context);

typedef struct {
    bool connected;
    LoRaConfig config; /**< As last read back from the module */
    char version[32];
    char uid[32];
} LoRaModuleInfo;

typedef struct LoRaModule LoRaModule;

LoRaModule* lora_module_alloc(LoRaEventCallback callback, void* context);
void lora_module_free(LoRaModule* module);

/**
 * Find the module (trying preferred_baud first, then every supported rate)
 * and read its settings.
 *
 * @param reapply  if not NULL, the radio settings that aren't guaranteed to
 *                 survive a module power cycle (frequency, SF/BW/CR/preamble,
 *                 TX power, password) are written from here when they differ.
 */
void lora_module_connect(LoRaModule* module, uint32_t preferred_baud, const LoRaConfig* reapply);

/** Write every setting in desired that differs from the module's current value. */
void lora_module_apply(LoRaModule* module, const LoRaConfig* desired);

/** Send text (NUL terminated, max LORA_MAX_PAYLOAD bytes) to an address (0 = all). */
void lora_module_send(LoRaModule* module, uint32_t id, uint16_t address, const char* text);

/** Copy the latest module information. */
void lora_module_get_info(LoRaModule* module, LoRaModuleInfo* info);

/** Describe an error code (module +ERR or local). */
const char* lora_module_error_str(int error);

#ifdef __cplusplus
}
#endif
