#pragma once

#include "loratalk_app_i.h"

#include <furi_hal.h>

#define RX_BUF_SIZE (320)

typedef struct LoRaTalk_UART LoRaTalk_UART;

void loratalk_uart_set_handle_rx_data_cb(
    LoRaTalk_UART* uart,
    void (*handle_rx_data_cb)(uint8_t* buf, size_t len, void* context));

void loratalk_uart_tx(uint8_t* data, size_t len, void* context);
LoRaTalk_UART* loratalk_uart_init(LoRaTalkApp* app);
void loratalk_uart_free(LoRaTalk_UART* uart);
