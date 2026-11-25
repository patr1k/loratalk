#include "../loratalk_app_i.h"

static void chat_compose_callback(void* context) {
    LoRaTalkApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, LoRaTalkEventTextDone);
}

void loratalk_scene_chat_compose_on_enter(void* context) {
    LoRaTalkApp* app = context;
    TextInput* text_input = app->text_input;

    if(app->settings.destination == 0) {
        snprintf(app->header_buf, sizeof(app->header_buf), "Message to all:");
    } else {
        snprintf(
            app->header_buf, sizeof(app->header_buf), "Message to %u:", app->settings.destination);
    }

    app->text_buf[0] = '\0';
    text_input_reset(text_input);
    text_input_set_header_text(text_input, app->header_buf);
    text_input_set_minimum_length(text_input, 1);
    text_input_set_result_callback(
        text_input, chat_compose_callback, app, app->text_buf, sizeof(app->text_buf), true);
    view_dispatcher_switch_to_view(app->view_dispatcher, LoRaTalkViewTextInput);
}

bool loratalk_scene_chat_compose_on_event(void* context, SceneManagerEvent event) {
    LoRaTalkApp* app = context;

    if(event.type == SceneManagerEventTypeCustom && event.event == LoRaTalkEventTextDone) {
        if(app->text_buf[0]) {
            uint32_t id = ++app->next_message_id;
            loratalk_history_add_outgoing(
                app->history, id, app->settings.destination, app->text_buf);
            chat_view_refresh(app->chat);
            lora_module_send(app->lora, id, app->settings.destination, app->text_buf);
        }
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    return false;
}

void loratalk_scene_chat_compose_on_exit(void* context) {
    LoRaTalkApp* app = context;
    text_input_reset(app->text_input);
}
