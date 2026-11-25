#include "../loratalk_app_i.h"

static void chat_callback(ChatViewEvent event, void* context) {
    LoRaTalkApp* app = context;
    view_dispatcher_send_custom_event(
        app->view_dispatcher,
        event == ChatViewEventCompose ? LoRaTalkEventCompose : LoRaTalkEventDestination);
}

void loratalk_scene_chat_on_enter(void* context) {
    LoRaTalkApp* app = context;
    app->unread = 0;
    chat_view_set_callback(app->chat, chat_callback, app);
    chat_view_set_destination(app->chat, app->settings.destination);
    view_dispatcher_switch_to_view(app->view_dispatcher, LoRaTalkViewChat);
}

bool loratalk_scene_chat_on_event(void* context, SceneManagerEvent event) {
    LoRaTalkApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == LoRaTalkEventCompose) {
            scene_manager_next_scene(app->scene_manager, LoRaTalkSceneChatCompose);
            consumed = true;
        } else if(event.event == LoRaTalkEventDestination) {
            scene_manager_set_scene_state(
                app->scene_manager, LoRaTalkSceneNumberInput, LoRaTalkNumberDestination);
            scene_manager_next_scene(app->scene_manager, LoRaTalkSceneNumberInput);
            consumed = true;
        } else if(event.event == LoRaTalkEventMessageReceived) {
            app->unread = 0;
            consumed = true;
        }
    }
    return consumed;
}

void loratalk_scene_chat_on_exit(void* context) {
    LoRaTalkApp* app = context;
    chat_view_set_notice(app->chat, NULL);
}
