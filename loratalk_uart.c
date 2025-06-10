#include "loratalk_uart.h"

struct LoRaTalk_UART {
    LoRaTalkApp* app;
    FuriHalSerialHandle* serial_handle;
    FuriThread* rx_thread;
    FuriStreamBuffer* rx_stream;
    uint8_t rx_buf[RX_BUF_SIZE + 1];
    void (*handle_rx_data_cb)(uint8_t* buf, size_t len, void* context);
};

typedef enum {
    WorkerEvtStop = (1 << 0),
    WorkerEvtRxDone = (1 << 1),
} WorkerEvtFlags;

void loratalk_uart_set_handle_rx_data_cb(
    LoRaTalk_UART* uart,
    void (*handle_rx_data_cb)(uint8_t*, size_t, void*)) {
    furi_assert(uart);
    uart->handle_rx_data_cb = handle_rx_data_cb;
}

#define WORKER_ALL_RX_EVENTS (WorkerEvtStop | WorkerEvtRxDone)

static void loratalk_uart_on_irq_cb(
    FuriHalSerialHandle* handle,
    FuriHalSerialRxEvent ev,
    size_t size,
    void* context) {
    LoRaTalk_UART* uart = (LoRaTalk_UART*)context;

    if(ev & (FuriHalSerialRxEventData | FuriHalSerialRxEventIdle)) {
        uint8_t data[FURI_HAL_SERIAL_DMA_BUFFER_SIZE] = {0};
        while(size) {
            size_t ret = furi_hal_serial_dma_rx(
                handle,
                data,
                (size > FURI_HAL_SERIAL_DMA_BUFFER_SIZE) ? FURI_HAL_SERIAL_DMA_BUFFER_SIZE : size);
            furi_stream_buffer_send(uart->rx_stream, data, ret, 0);
            size -= ret;
        };
        furi_thread_flags_set(furi_thread_get_id(uart->rx_thread), WorkerEvtRxDone);
    }
}

static int32_t uart_worker(void* context) {
    LoRaTalk_UART* uart = (void*)context;

    while(1) {
        uint32_t events =
            furi_thread_flags_wait(WORKER_ALL_RX_EVENTS, FuriFlagWaitAny, FuriWaitForever);
        furi_check((events & FuriFlagError) == 0);
        if(events & WorkerEvtStop) break;
        if(events & WorkerEvtRxDone) {
            size_t len = furi_stream_buffer_receive(uart->rx_stream, uart->rx_buf, RX_BUF_SIZE, 0);
            if(len > 0) {
                if(uart->handle_rx_data_cb) uart->handle_rx_data_cb(uart->rx_buf, len, uart->app);
            }
        }
    }

    furi_stream_buffer_free(uart->rx_stream);
    return 0;
}

void loratalk_uart_tx(uint8_t* data, size_t len, void* context) {
    LoRaTalk_UART* uart = (LoRaTalk_UART*)context;
    furi_hal_serial_tx(uart->serial_handle, data, len);
}

LoRaTalk_UART* loratalk_uart_init(LoRaTalkApp* app) {
    LoRaTalk_UART* uart = malloc(sizeof(LoRaTalk_UART));
    uart->app = app;

    uart->rx_stream = furi_stream_buffer_alloc(RX_BUF_SIZE, 1);
    uart->rx_thread = furi_thread_alloc_ex("LoRaTalk_UartWorker", 1024, uart_worker, uart);
    furi_thread_start(uart->rx_thread);

    if(app->baud_rate == 0) {
        app->baud_rate = 115200;
    }

    furi_assert(!uart->serial_handle);
    uart->serial_handle = furi_hal_serial_control_acquire(FuriHalSerialIdUsart);
    furi_assert(uart->serial_handle);

    furi_hal_serial_init(uart->serial_handle, app->baud_rate);
    furi_hal_serial_dma_rx_start(uart->serial_handle, loratalk_uart_on_irq_cb, uart, false);

    return uart;
}

void loratalk_uart_free(LoRaTalk_UART* uart) {
    furi_assert(uart);

    furi_thread_flags_set(furi_thread_get_id(uart->rx_thread), WorkerEvtStop);
    furi_thread_join(uart->rx_thread);
    furi_thread_free(uart->rx_thread);

    furi_hal_serial_deinit(uart->serial_handle);
    furi_hal_serial_control_release(uart->serial_handle);
    uart->serial_handle = NULL;

    free(uart);
}
