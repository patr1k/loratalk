#pragma once

typedef enum {
    /** Something arrived in app->lora_events (handled by the app, not scenes) */
    LoRaTalkEventLoRa = 1000,

    /* Forwarded to the current scene after app-level handling */
    LoRaTalkEventConnected,
    LoRaTalkEventConnectFailed,
    LoRaTalkEventApplyDone,
    LoRaTalkEventMessageReceived,
    LoRaTalkEventSendDone,

    /* UI */
    LoRaTalkEventRetry,
    LoRaTalkEventCompose,
    LoRaTalkEventDestination,
    LoRaTalkEventTextDone,
    LoRaTalkEventNumberDone,
    LoRaTalkEventDialogLeft,
    LoRaTalkEventDialogRight,

    /** Menu / list item selected: LoRaTalkEventItem + index */
    LoRaTalkEventItem = 2000,
} LoRaTalkCustomEvent;
