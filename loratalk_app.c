#include "loratalk_app_i.h"

#include <stdio.h>

#define TAG "LoRaTalk"

#define LORA_EVENT_QUEUE_SIZE (16)

static const NotificationSequence sequence_message_received = {
    &message_display_backlight_on,
    &message_vibro_on,
    &message_blue_255,
    &message_delay_100,
    &message_vibro_off,
    &message_blue_0,
    NULL,
};

void loratalk_format_mhz(char* out, size_t size, uint32_t hz) {
    uint32_t khz = hz / 1000;
    snprintf(out, size, "%lu.%03lu", khz / 1000, khz % 1000);
}

void loratalk_app_save_settings(LoRaTalkApp* app) {
    if(app->info.connected) {
        app->settings.config = app->info.config;
        app->settings.has_config = true;
    }
    loratalk_settings_save(&app->settings);
}

/* Runs on the LoRa worker thread: hand the event to the GUI thread. */
static void loratalk_app_lora_callback(const LoRaEvent* event, void* context) {
    LoRaTalkApp* app = context;
    if(furi_message_queue_put(app->lora_events, event, 100) != FuriStatusOk) {
        FURI_LOG_W(TAG, "Dropped LoRa event %d", event->type);
        return;
    }
    view_dispatcher_send_custom_event(app->view_dispatcher, LoRaTalkEventLoRa);
}

/* GUI thread: app-wide handling, then let the current scene react. */
static void loratalk_app_handle_lora_event(LoRaTalkApp* app, const LoRaEvent* event) {
    uint32_t scene_event = 0;

    switch(event->type) {
    case LoRaEventReceived:
        FURI_LOG_I(TAG, "Message from %u (%d dBm)", event->rx.address, event->rx.rssi);
        loratalk_history_add_incoming(app->history, &event->rx);
        if(scene_manager_get_current_scene(app->scene_manager) != LoRaTalkSceneChat) {
            app->unread++;
        }
        notification_message(app->notifications, &sequence_message_received);
        chat_view_refresh(app->chat);
        scene_event = LoRaTalkEventMessageReceived;
        break;

    case LoRaEventSendDone:
        loratalk_history_set_status(
            app->history,
            event->id,
            event->error == LORA_OK ? LoRaTalkMessageStatusSent : LoRaTalkMessageStatusFailed);
        chat_view_set_notice(
            app->chat, event->error == LORA_OK ? NULL : lora_module_error_str(event->error));
        scene_event = LoRaTalkEventSendDone;
        break;

    case LoRaEventConnected:
    case LoRaEventApplyDone:
        lora_module_get_info(app->lora, &app->info);
        app->last_error = event->error;
        if(app->info.connected) loratalk_app_save_settings(app);
        scene_event = event->type == LoRaEventConnected ? LoRaTalkEventConnected :
                                                          LoRaTalkEventApplyDone;
        break;

    case LoRaEventConnectFailed:
        lora_module_get_info(app->lora, &app->info);
        app->last_error = event->error;
        scene_event = LoRaTalkEventConnectFailed;
        break;
    }

    if(scene_event) scene_manager_handle_custom_event(app->scene_manager, scene_event);
}

static bool loratalk_app_custom_event_callback(void* context, uint32_t custom_event) {
    furi_assert(context);
    LoRaTalkApp* app = context;

    if(custom_event == LoRaTalkEventLoRa) {
        LoRaEvent event;
        while(furi_message_queue_get(app->lora_events, &event, 0) == FuriStatusOk) {
            loratalk_app_handle_lora_event(app, &event);
        }
        return true;
    }
    return scene_manager_handle_custom_event(app->scene_manager, custom_event);
}

static bool loratalk_app_back_event_callback(void* context) {
    furi_assert(context);
    LoRaTalkApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void loratalk_app_tick_event_callback(void* context) {
    furi_assert(context);
    LoRaTalkApp* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

static LoRaTalkApp* loratalk_app_alloc(void) {
    LoRaTalkApp* app = malloc(sizeof(LoRaTalkApp));
    memset(app, 0, sizeof(LoRaTalkApp));

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    // The expansion module service listens on the same USART; take it over
    app->expansion = furi_record_open(RECORD_EXPANSION);
    expansion_disable(app->expansion);

    app->text = furi_string_alloc();

    loratalk_settings_load(&app->settings);
    app->history = loratalk_history_alloc();
    loratalk_history_load(app->history);

    app->scene_manager = scene_manager_alloc(&loratalk_app_scene_handlers, app);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, loratalk_app_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, loratalk_app_back_event_callback);
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, loratalk_app_tick_event_callback, 250);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, LoRaTalkViewSubmenu, submenu_get_view(app->submenu));
    app->variable_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        LoRaTalkViewVariableItemList,
        variable_item_list_get_view(app->variable_item_list));
    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, LoRaTalkViewTextInput, text_input_get_view(app->text_input));
    app->number_input = number_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, LoRaTalkViewNumberInput, number_input_get_view(app->number_input));
    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, LoRaTalkViewWidget, widget_get_view(app->widget));
    app->dialog = dialog_ex_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, LoRaTalkViewDialog, dialog_ex_get_view(app->dialog));
    app->chat = chat_view_alloc(app->history);
    view_dispatcher_add_view(
        app->view_dispatcher, LoRaTalkViewChat, chat_view_get_view(app->chat));
    chat_view_set_destination(app->chat, app->settings.destination);

    app->lora_events = furi_message_queue_alloc(LORA_EVENT_QUEUE_SIZE, sizeof(LoRaEvent));
    app->lora = lora_module_alloc(loratalk_app_lora_callback, app);
    lora_config_default(&app->info.config);

    return app;
}

static void loratalk_app_free(LoRaTalkApp* app) {
    // Stop the worker first so no more events arrive
    lora_module_free(app->lora);

    view_dispatcher_remove_view(app->view_dispatcher, LoRaTalkViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, LoRaTalkViewVariableItemList);
    view_dispatcher_remove_view(app->view_dispatcher, LoRaTalkViewTextInput);
    view_dispatcher_remove_view(app->view_dispatcher, LoRaTalkViewNumberInput);
    view_dispatcher_remove_view(app->view_dispatcher, LoRaTalkViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, LoRaTalkViewDialog);
    view_dispatcher_remove_view(app->view_dispatcher, LoRaTalkViewChat);
    submenu_free(app->submenu);
    variable_item_list_free(app->variable_item_list);
    text_input_free(app->text_input);
    number_input_free(app->number_input);
    widget_free(app->widget);
    dialog_ex_free(app->dialog);
    chat_view_free(app->chat);

    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    furi_message_queue_free(app->lora_events);
    loratalk_history_free(app->history);
    furi_string_free(app->text);

    expansion_enable(app->expansion);
    furi_record_close(RECORD_EXPANSION);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    free(app);
}

int32_t loratalk_app(void* arg) {
    UNUSED(arg);

    LoRaTalkApp* app = loratalk_app_alloc();
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    scene_manager_next_scene(app->scene_manager, LoRaTalkSceneConnect);
    view_dispatcher_run(app->view_dispatcher);
    loratalk_app_free(app);

    return 0;
}
