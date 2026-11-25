#include "lora_module.h"

#include <furi.h>
#include <furi_hal.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAG "LoRaModule"

#define RX_STREAM_SIZE   (1024)
#define JOB_QUEUE_SIZE   (4)
#define WORKER_STACK     (3 * 1024)
#define POLL_INTERVAL_MS (20)

/* Per-command timeouts (UART transfer time is added on top) */
#define CMD_TIMEOUT_MS    (1000)
#define PROBE_TIMEOUT_MS  (300)
#define RESET_TIMEOUT_MS  (3000)
#define SEND_TIMEOUT_MS   (2000)
#define CMD_BUF_SIZE      (LORA_MAX_PAYLOAD + 32)
#define RESPONSE_BUF_SIZE (64)

typedef enum {
    WorkerFlagStop = (1 << 0),
    WorkerFlagJob = (1 << 1),
} WorkerFlag;

typedef enum {
    LoRaJobConnect,
    LoRaJobApply,
    LoRaJobSend,
} LoRaJobType;

typedef struct {
    LoRaJobType type;
    uint32_t id;
    uint32_t baud;
    uint16_t address;
    bool has_config;
    LoRaConfig config;
    char text[LORA_MAX_PAYLOAD + 1];
} LoRaJob;

typedef enum {
    WaitNone,
    WaitResponse, /* first +OK / +ERR / +VALUE line */
    WaitReady, /* +READY after AT+RESET */
} WaitMode;

struct LoRaModule {
    LoRaEventCallback callback;
    void* context;

    FuriHalSerialHandle* serial;
    uint32_t baud;
    FuriStreamBuffer* rx_stream;
    FuriThread* worker;
    FuriMessageQueue* jobs;
    volatile bool stopping;

    /* Line assembly (worker thread only) */
    char line[LORA_LINE_MAX + 1];
    size_t line_len;
    bool line_overflow;

    /* Pending command (worker thread only) */
    WaitMode wait;
    bool got_response;
    char response[RESPONSE_BUF_SIZE];

    /* Scratch space kept off the worker stack */
    LoRaEvent event;
    LoRaJob job;
    char cmd[CMD_BUF_SIZE];

    /* Shared with other threads */
    FuriMutex* info_mutex;
    LoRaModuleInfo info;
};

/* ---------- UART ---------- */

static void lora_uart_rx_cb(
    FuriHalSerialHandle* handle,
    FuriHalSerialRxEvent event,
    size_t size,
    void* context) {
    LoRaModule* module = context;
    if(event & (FuriHalSerialRxEventData | FuriHalSerialRxEventIdle)) {
        uint8_t data[FURI_HAL_SERIAL_DMA_BUFFER_SIZE];
        while(size) {
            size_t chunk = size > sizeof(data) ? sizeof(data) : size;
            size_t got = furi_hal_serial_dma_rx(handle, data, chunk);
            if(!got) break;
            furi_stream_buffer_send(module->rx_stream, data, got, 0);
            size -= got;
        }
    }
}

static void lora_uart_set_baud(LoRaModule* module, uint32_t baud) {
    if(!module->serial) return;
    furi_hal_serial_dma_rx_stop(module->serial);
    furi_hal_serial_set_br(module->serial, baud);
    module->baud = baud;
    furi_hal_serial_dma_rx_start(module->serial, lora_uart_rx_cb, module, false);
}

static void lora_uart_tx(LoRaModule* module, const char* text) {
    furi_hal_serial_tx(module->serial, (const uint8_t*)text, strlen(text));
    furi_hal_serial_tx(module->serial, (const uint8_t*)"\r\n", 2);
    furi_hal_serial_tx_wait_complete(module->serial);
}

/* Time to move len bytes over the UART (10 bits per byte), in ms */
static uint32_t lora_uart_time_ms(LoRaModule* module, size_t len) {
    uint32_t baud = module->baud ? module->baud : LORA_BAUD_DEFAULT;
    return (uint32_t)((len * 10UL * 1000UL) / baud) + 1;
}

/* ---------- Line handling ---------- */

static void lora_emit(LoRaModule* module, const LoRaEvent* event) {
    if(module->callback) module->callback(event, module->context);
}

static void lora_handle_line(LoRaModule* module, const char* line) {
    LoRaLineType type = lora_proto_classify(line);
    FURI_LOG_D(TAG, "<< %s", line);

    if(type == LoRaLineReceive) {
        memset(&module->event, 0, sizeof(module->event));
        module->event.type = LoRaEventReceived;
        if(lora_proto_parse_rcv(line, &module->event.rx)) {
            lora_emit(module, &module->event);
        } else {
            FURI_LOG_W(TAG, "Unparseable +RCV");
        }
        return;
    }

    switch(module->wait) {
    case WaitResponse:
        // Unsolicited boot messages are not answers to our command
        if(type == LoRaLineReady || type == LoRaLineReset) return;
        strlcpy(module->response, line, sizeof(module->response));
        module->got_response = true;
        break;
    case WaitReady:
        if(type == LoRaLineReady) {
            strlcpy(module->response, line, sizeof(module->response));
            module->got_response = true;
        }
        break;
    case WaitNone:
    default:
        break;
    }
}

static void lora_feed(LoRaModule* module, const uint8_t* data, size_t len) {
    for(size_t i = 0; i < len; i++) {
        char c = (char)data[i];
        if(c == '\n') {
            if(!module->line_overflow && module->line_len > 0) {
                module->line[module->line_len] = '\0';
                lora_handle_line(module, module->line);
            }
            module->line_len = 0;
            module->line_overflow = false;
        } else if(c == '\r') {
            // ignored; lines end with CR LF
        } else if(module->line_len < LORA_LINE_MAX) {
            module->line[module->line_len++] = c;
        } else {
            module->line_overflow = true;
        }
    }
}

/* Read whatever the UART has, waiting at most timeout_ms for the first byte */
static void lora_pump(LoRaModule* module, uint32_t timeout_ms) {
    uint8_t buf[64];
    size_t got = furi_stream_buffer_receive(module->rx_stream, buf, sizeof(buf), timeout_ms);
    while(got) {
        lora_feed(module, buf, got);
        got = furi_stream_buffer_receive(module->rx_stream, buf, sizeof(buf), 0);
    }
}

static bool lora_should_stop(LoRaModule* module) {
    return module->stopping;
}

/* ---------- Commands ---------- */

/**
 * Send one AT command and wait for its reply. Received messages that arrive
 * in the meantime are still dispatched.
 *
 * @param reply  optional, receives the reply line (e.g. "+ADDRESS=5")
 * @return LORA_OK, a module +ERR code (> 0) or a local error (< 0)
 */
static int lora_command_ex(
    LoRaModule* module,
    const char* cmd,
    WaitMode wait,
    uint32_t timeout_ms,
    char* reply,
    size_t reply_size) {
    if(!module->serial) return LORA_ERR_UART_BUSY;
    if(lora_should_stop(module)) return LORA_ERR_ABORTED;

    // Handle anything already received before we start waiting
    lora_pump(module, 0);

    module->wait = wait;
    module->got_response = false;
    module->response[0] = '\0';

    FURI_LOG_D(TAG, ">> %s", cmd);
    lora_uart_tx(module, cmd);

    // The command itself has already been sent; allow for the reply's transfer time
    timeout_ms += lora_uart_time_ms(module, RESPONSE_BUF_SIZE);
    uint32_t start = furi_get_tick();
    while(!module->got_response && !lora_should_stop(module) &&
          (furi_get_tick() - start) < furi_ms_to_ticks(timeout_ms)) {
        lora_pump(module, POLL_INTERVAL_MS);
    }
    module->wait = WaitNone;

    if(!module->got_response) {
        return lora_should_stop(module) ? LORA_ERR_ABORTED : LORA_ERR_TIMEOUT;
    }
    if(reply) strlcpy(reply, module->response, reply_size);

    switch(lora_proto_classify(module->response)) {
    case LoRaLineOk:
    case LoRaLineValue:
    case LoRaLineReady:
        return LORA_OK;
    case LoRaLineError: {
        int code = lora_proto_error_code(module->response);
        return code > 0 ? code : LORA_ERR_GARBAGE;
    }
    default:
        return LORA_ERR_GARBAGE;
    }
}

static int lora_command(LoRaModule* module, const char* cmd) {
    return lora_command_ex(module, cmd, WaitResponse, CMD_TIMEOUT_MS, NULL, 0);
}

static int lora_commandf(LoRaModule* module, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(module->cmd, sizeof(module->cmd), fmt, args);
    va_end(args);
    return lora_command(module, module->cmd);
}

/* Query "AT+<key>?" and return a pointer to the value in out (or NULL). */
static const char*
    lora_query(LoRaModule* module, const char* key, const char* reply_key, char* out, size_t size) {
    snprintf(module->cmd, sizeof(module->cmd), "AT+%s?", key);
    if(lora_command_ex(module, module->cmd, WaitResponse, CMD_TIMEOUT_MS, out, size) != LORA_OK)
        return NULL;
    const char* value = lora_proto_value(out, reply_key);
    if(!value) value = lora_proto_value(out, key);
    return value;
}

/* Is the module answering at the current baud rate? */
static bool lora_probe(LoRaModule* module) {
    for(int attempt = 0; attempt < 2; attempt++) {
        int result = lora_command_ex(module, "AT", WaitResponse, PROBE_TIMEOUT_MS, NULL, 0);
        if(result == LORA_OK) return true;
        // An +ERR means the baud rate is right but the module's input buffer
        // had junk in it (from probing at other rates); try once more.
        if(result <= 0) return false;
    }
    return false;
}

static bool lora_detect(LoRaModule* module, uint32_t preferred) {
    static const uint32_t order[] = {115200, 9600, 57600, 38400, 19200, 28800, 4800, 1200, 300};

    if(lora_config_baud_valid(preferred)) {
        lora_uart_set_baud(module, preferred);
        if(lora_probe(module)) return true;
    }
    for(size_t i = 0; i < COUNT_OF(order); i++) {
        if(lora_should_stop(module)) return false;
        if(order[i] == preferred) continue;
        FURI_LOG_I(TAG, "Probing %lu baud", order[i]);
        lora_uart_set_baud(module, order[i]);
        furi_delay_ms(10);
        if(lora_probe(module)) return true;
    }
    return false;
}

/* Read every setting into cfg. */
static int lora_read_config(LoRaModule* module, LoRaConfig* cfg, char* version, char* uid) {
    char reply[RESPONSE_BUF_SIZE];
    const char* v;

    lora_config_default(cfg);
    cfg->baud = module->baud;

    if(!(v = lora_query(module, "ADDRESS", "ADDRESS", reply, sizeof(reply))))
        return LORA_ERR_TIMEOUT;
    cfg->address = (uint16_t)strtoul(v, NULL, 10);

    // The reply prefix is "+NETWORK=" (not "+NETWORKID=")
    if(!(v = lora_query(module, "NETWORKID", "NETWORK", reply, sizeof(reply))))
        return LORA_ERR_TIMEOUT;
    cfg->network_id = (uint8_t)strtoul(v, NULL, 10);

    if(!(v = lora_query(module, "BAND", "BAND", reply, sizeof(reply)))) return LORA_ERR_TIMEOUT;
    cfg->band_hz = strtoul(v, NULL, 10);

    if(!(v = lora_query(module, "PARAMETER", "PARAMETER", reply, sizeof(reply))))
        return LORA_ERR_TIMEOUT;
    unsigned sf = 0, bw = 0, cr = 0, pre = 0;
    if(sscanf(v, "%u,%u,%u,%u", &sf, &bw, &cr, &pre) == 4) {
        cfg->sf = (uint8_t)sf;
        cfg->bw = (uint8_t)bw;
        cfg->cr = (uint8_t)cr;
        cfg->preamble = (uint8_t)pre;
    }

    if(!(v = lora_query(module, "CRFOP", "CRFOP", reply, sizeof(reply)))) return LORA_ERR_TIMEOUT;
    cfg->rf_power = (uint8_t)strtoul(v, NULL, 10);

    // Optional ones
    cfg->password[0] = '\0';
    if((v = lora_query(module, "CPIN", "CPIN", reply, sizeof(reply)))) {
        // "No Password!" when unset
        if(lora_config_password_valid(v) && strlen(v) == LORA_PASSWORD_LEN) {
            strlcpy(cfg->password, v, sizeof(cfg->password));
        }
    }
    version[0] = '\0';
    if((v = lora_query(module, "VER", "VER", reply, sizeof(reply)))) strlcpy(version, v, 32);
    uid[0] = '\0';
    if((v = lora_query(module, "UID", "UID", reply, sizeof(reply)))) strlcpy(uid, v, 32);

    return LORA_OK;
}

static int lora_refresh_info(LoRaModule* module) {
    LoRaModuleInfo info;
    memset(&info, 0, sizeof(info));
    int result = lora_read_config(module, &info.config, info.version, info.uid);
    info.connected = (result == LORA_OK);

    furi_mutex_acquire(module->info_mutex, FuriWaitForever);
    if(info.connected) {
        module->info = info;
    } else {
        module->info.connected = false;
    }
    furi_mutex_release(module->info_mutex);
    return result;
}

static int lora_reset(LoRaModule* module) {
    return lora_command_ex(module, "AT+RESET", WaitReady, RESET_TIMEOUT_MS, NULL, 0);
}

static int lora_change_baud(LoRaModule* module, uint32_t baud) {
    uint32_t old_baud = module->baud;
    if(baud == old_baud) return LORA_OK;

    // The module answers "+IPR=<baud>" at the old rate
    int result = lora_commandf(module, "AT+IPR=%lu", baud);
    if(result != LORA_OK) return result;

    furi_delay_ms(50);
    lora_uart_set_baud(module, baud);
    if(lora_probe(module)) return LORA_OK;

    // The new rate may only take effect after a reset (the AT guide doesn't
    // say). It's stored in flash, so reset at the old rate and listen at the
    // new one.
    FURI_LOG_W(TAG, "No reply at %lu, trying reset", baud);
    lora_uart_set_baud(module, old_baud);
    if(lora_probe(module)) {
        lora_uart_tx(module, "AT+RESET");
        lora_uart_set_baud(module, baud);
        module->wait = WaitReady;
        module->got_response = false;
        uint32_t start = furi_get_tick();
        while(!module->got_response && !lora_should_stop(module) &&
              (furi_get_tick() - start) < furi_ms_to_ticks(RESET_TIMEOUT_MS)) {
            lora_pump(module, POLL_INTERVAL_MS);
        }
        module->wait = WaitNone;
        if(lora_probe(module)) return LORA_OK;
    }

    // Last resort: find it wherever it is
    if(lora_detect(module, baud) && module->baud == baud) return LORA_OK;
    return LORA_ERR_VERIFY;
}

/*
 * Write every setting in desired that differs from cur. cur is updated as
 * settings are written. Stops at the first error.
 */
static int lora_write_config(LoRaModule* module, LoRaConfig* cur, const LoRaConfig* desired_in) {
    LoRaConfig desired = *desired_in;
    lora_config_sanitize(&desired);
    int result;

    // A password can only be removed by resetting the module
    if(desired.password[0] == '\0' && cur->password[0] != '\0') {
        result = lora_reset(module);
        if(result != LORA_OK) return result;
        char version[32], uid[32];
        result = lora_read_config(module, cur, version, uid);
        if(result != LORA_OK) return result;
        cur->password[0] = '\0';
    }

    bool params_differ = cur->sf != desired.sf || cur->bw != desired.bw ||
                         cur->cr != desired.cr || cur->preamble != desired.preamble;

    // A preamble other than 12 needs NETWORKID 18, so order the two writes so
    // that the combination is valid at every step.
    bool params_first = desired.network_id != LORA_NETWORK_ID_DEFAULT;

    if(params_first && params_differ) {
        result = lora_commandf(
            module,
            "AT+PARAMETER=%u,%u,%u,%u",
            desired.sf,
            desired.bw,
            desired.cr,
            desired.preamble);
        if(result != LORA_OK) return result;
        cur->sf = desired.sf;
        cur->bw = desired.bw;
        cur->cr = desired.cr;
        cur->preamble = desired.preamble;
    }

    if(cur->network_id != desired.network_id) {
        result = lora_commandf(module, "AT+NETWORKID=%u", desired.network_id);
        if(result != LORA_OK) return result;
        cur->network_id = desired.network_id;
    }

    if(!params_first && params_differ) {
        result = lora_commandf(
            module,
            "AT+PARAMETER=%u,%u,%u,%u",
            desired.sf,
            desired.bw,
            desired.cr,
            desired.preamble);
        if(result != LORA_OK) return result;
        cur->sf = desired.sf;
        cur->bw = desired.bw;
        cur->cr = desired.cr;
        cur->preamble = desired.preamble;
    }

    if(cur->address != desired.address) {
        result = lora_commandf(module, "AT+ADDRESS=%u", desired.address);
        if(result != LORA_OK) return result;
        cur->address = desired.address;
    }

    if(cur->band_hz != desired.band_hz) {
        // ",M" stores the frequency in flash (module firmware 1.2.0+)
        result = lora_commandf(module, "AT+BAND=%lu,M", desired.band_hz);
        if(result > 0) result = lora_commandf(module, "AT+BAND=%lu", desired.band_hz);
        if(result != LORA_OK) return result;
        cur->band_hz = desired.band_hz;
    }

    if(cur->rf_power != desired.rf_power) {
        result = lora_commandf(module, "AT+CRFOP=%u", desired.rf_power);
        if(result != LORA_OK) return result;
        cur->rf_power = desired.rf_power;
    }

    if(desired.password[0] && strcmp(cur->password, desired.password) != 0) {
        result = lora_commandf(module, "AT+CPIN=%s", desired.password);
        if(result != LORA_OK) return result;
        strlcpy(cur->password, desired.password, sizeof(cur->password));
    }

    // Last, so a failure here can't leave the other settings unwritten
    if(cur->baud != desired.baud) {
        result = lora_change_baud(module, desired.baud);
        if(result != LORA_OK) return result;
        cur->baud = desired.baud;
    }

    return LORA_OK;
}

static void lora_emit_simple(LoRaModule* module, LoRaEventType type, int error, uint32_t id) {
    memset(&module->event, 0, sizeof(module->event));
    module->event.type = type;
    module->event.error = error;
    module->event.id = id;
    lora_emit(module, &module->event);
}

static void lora_current_config(LoRaModule* module, LoRaConfig* cfg) {
    furi_mutex_acquire(module->info_mutex, FuriWaitForever);
    *cfg = module->info.config;
    furi_mutex_release(module->info_mutex);
}

/* ---------- Jobs ---------- */

static void lora_job_connect(LoRaModule* module, const LoRaJob* job) {
    if(!module->serial) {
        lora_emit_simple(module, LoRaEventConnectFailed, LORA_ERR_UART_BUSY, 0);
        return;
    }
    if(!lora_detect(module, job->baud)) {
        lora_emit_simple(
            module,
            LoRaEventConnectFailed,
            lora_should_stop(module) ? LORA_ERR_ABORTED : LORA_ERR_NO_MODULE,
            0);
        return;
    }
    FURI_LOG_I(TAG, "Module found at %lu baud", module->baud);

    int result = lora_refresh_info(module);
    if(result != LORA_OK) {
        lora_emit_simple(module, LoRaEventConnectFailed, result, 0);
        return;
    }

    int reapply_result = LORA_OK;
    if(job->has_config) {
        LoRaConfig cur;
        lora_current_config(module, &cur);
        // Only the settings that may not survive a power cycle
        LoRaConfig desired = cur;
        desired.band_hz = job->config.band_hz;
        desired.sf = job->config.sf;
        desired.bw = job->config.bw;
        desired.cr = job->config.cr;
        desired.preamble = job->config.preamble;
        desired.rf_power = job->config.rf_power;
        strlcpy(desired.password, job->config.password, sizeof(desired.password));
        if(lora_config_preamble_locked(cur.network_id)) desired.preamble = cur.preamble;
        lora_config_sanitize(&desired);

        bool differs = !lora_config_radio_equal(&cur, &desired) ||
                       cur.rf_power != desired.rf_power;
        if(differs) {
            FURI_LOG_I(TAG, "Re-applying saved radio settings");
            reapply_result = lora_write_config(module, &cur, &desired);
            lora_refresh_info(module);
        }
    }

    lora_emit_simple(module, LoRaEventConnected, reapply_result, 0);
}

static void lora_job_apply(LoRaModule* module, const LoRaJob* job) {
    LoRaConfig cur;
    lora_current_config(module, &cur);
    int result = lora_write_config(module, &cur, &job->config);
    int refresh = lora_refresh_info(module);
    if(result == LORA_OK) result = refresh;
    lora_emit_simple(module, LoRaEventApplyDone, result, 0);
}

static void lora_job_send(LoRaModule* module, const LoRaJob* job) {
    size_t len = strlen(job->text);
    LoRaConfig cfg;
    lora_current_config(module, &cfg);

    snprintf(module->cmd, sizeof(module->cmd), "AT+SEND=%u,%u,%s", job->address, len, job->text);
    uint32_t timeout = SEND_TIMEOUT_MS + lora_config_airtime_ms(&cfg, len);

    int result = lora_command_ex(module, module->cmd, WaitResponse, timeout, NULL, 0);
    if(result == 17) {
        // "Last TX was not completed": wait one packet time and retry
        furi_delay_ms(lora_config_airtime_ms(&cfg, LORA_MAX_PAYLOAD));
        result = lora_command_ex(module, module->cmd, WaitResponse, timeout, NULL, 0);
    }
    lora_emit_simple(module, LoRaEventSendDone, result, job->id);
}

static int32_t lora_worker(void* context) {
    LoRaModule* module = context;

    while(true) {
        uint32_t flags = furi_thread_flags_get();
        if(flags & WorkerFlagStop) break;
        if(flags & WorkerFlagJob) furi_thread_flags_clear(WorkerFlagJob);

        while(!module->stopping &&
              furi_message_queue_get(module->jobs, &module->job, 0) == FuriStatusOk) {
            switch(module->job.type) {
            case LoRaJobConnect:
                lora_job_connect(module, &module->job);
                break;
            case LoRaJobApply:
                lora_job_apply(module, &module->job);
                break;
            case LoRaJobSend:
                lora_job_send(module, &module->job);
                break;
            }
        }

        if(module->serial) {
            lora_pump(module, POLL_INTERVAL_MS);
        } else {
            furi_thread_flags_wait(
                WorkerFlagStop | WorkerFlagJob, FuriFlagWaitAny | FuriFlagNoClear, 100);
        }
    }
    return 0;
}

/* ---------- Public API ---------- */

LoRaModule* lora_module_alloc(LoRaEventCallback callback, void* context) {
    LoRaModule* module = malloc(sizeof(LoRaModule));
    memset(module, 0, sizeof(LoRaModule));
    module->callback = callback;
    module->context = context;
    module->info_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    module->rx_stream = furi_stream_buffer_alloc(RX_STREAM_SIZE, 1);
    module->jobs = furi_message_queue_alloc(JOB_QUEUE_SIZE, sizeof(LoRaJob));
    lora_config_default(&module->info.config);

    module->serial = furi_hal_serial_control_acquire(FuriHalSerialIdUsart);
    if(module->serial) {
        module->baud = LORA_BAUD_DEFAULT;
        furi_hal_serial_init(module->serial, module->baud);
        furi_hal_serial_dma_rx_start(module->serial, lora_uart_rx_cb, module, false);
    } else {
        FURI_LOG_E(TAG, "USART is busy");
    }

    module->worker = furi_thread_alloc_ex("LoRaWorker", WORKER_STACK, lora_worker, module);
    furi_thread_start(module->worker);
    return module;
}

void lora_module_free(LoRaModule* module) {
    furi_assert(module);
    module->stopping = true;
    furi_thread_flags_set(furi_thread_get_id(module->worker), WorkerFlagStop);
    furi_thread_join(module->worker);
    furi_thread_free(module->worker);

    if(module->serial) {
        furi_hal_serial_dma_rx_stop(module->serial);
        furi_hal_serial_deinit(module->serial);
        furi_hal_serial_control_release(module->serial);
    }

    furi_message_queue_free(module->jobs);
    furi_stream_buffer_free(module->rx_stream);
    furi_mutex_free(module->info_mutex);
    free(module);
}

static void lora_submit(LoRaModule* module, const LoRaJob* job) {
    if(furi_message_queue_put(module->jobs, job, 0) != FuriStatusOk) {
        FURI_LOG_W(TAG, "Job queue full");
        // Report the failure so the UI isn't left waiting
        LoRaEvent event = {0};
        event.error = LORA_ERR_TIMEOUT;
        event.id = job->id;
        switch(job->type) {
        case LoRaJobConnect:
            event.type = LoRaEventConnectFailed;
            break;
        case LoRaJobApply:
            event.type = LoRaEventApplyDone;
            break;
        case LoRaJobSend:
            event.type = LoRaEventSendDone;
            break;
        }
        lora_emit(module, &event);
        return;
    }
    furi_thread_flags_set(furi_thread_get_id(module->worker), WorkerFlagJob);
}

void lora_module_connect(LoRaModule* module, uint32_t preferred_baud, const LoRaConfig* reapply) {
    LoRaJob job;
    memset(&job, 0, sizeof(job));
    job.type = LoRaJobConnect;
    job.baud = preferred_baud;
    if(reapply) {
        job.has_config = true;
        job.config = *reapply;
    }
    lora_submit(module, &job);
}

void lora_module_apply(LoRaModule* module, const LoRaConfig* desired) {
    LoRaJob job;
    memset(&job, 0, sizeof(job));
    job.type = LoRaJobApply;
    job.has_config = true;
    job.config = *desired;
    lora_submit(module, &job);
}

void lora_module_send(LoRaModule* module, uint32_t id, uint16_t address, const char* text) {
    LoRaJob job;
    memset(&job, 0, sizeof(job));
    job.type = LoRaJobSend;
    job.id = id;
    job.address = address;
    strlcpy(job.text, text, sizeof(job.text));
    lora_proto_sanitize(job.text);
    lora_submit(module, &job);
}

void lora_module_get_info(LoRaModule* module, LoRaModuleInfo* info) {
    furi_mutex_acquire(module->info_mutex, FuriWaitForever);
    *info = module->info;
    furi_mutex_release(module->info_mutex);
}

const char* lora_module_error_str(int error) {
    switch(error) {
    case LORA_OK:
        return "OK";
    case LORA_ERR_TIMEOUT:
        return "No response";
    case LORA_ERR_NO_MODULE:
        return "Module not found";
    case LORA_ERR_UART_BUSY:
        return "UART in use";
    case LORA_ERR_GARBAGE:
        return "Bad reply";
    case LORA_ERR_VERIFY:
        return "Baud change failed";
    case LORA_ERR_ABORTED:
        return "Cancelled";
    default:
        return error > 0 ? lora_proto_error_str(error) : "Error";
    }
}
