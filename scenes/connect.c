#include "../loratalk_app_i.h"

/* Startup: find the module and read its settings. */

void loratalk_scene_connect_on_enter(void* context) {
    LoRaTalkApp* app = context;

    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 22, AlignCenter, AlignCenter, FontPrimary, "Connecting...");
    widget_add_string_element(
        app->widget, 64, 38, AlignCenter, AlignCenter, FontSecondary, "Looking for the RYLR998");
    widget_add_string_element(
        app->widget, 64, 56, AlignCenter, AlignCenter, FontSecondary, "Back: exit");
    view_dispatcher_switch_to_view(app->view_dispatcher, LoRaTalkViewWidget);

    // Until a module has been read once, adopt its settings instead of
    // pushing the Flipper's defaults onto it.
    lora_module_connect(
        app->lora,
        app->settings.config.baud,
        app->settings.has_config ? &app->settings.config : NULL);
}

bool loratalk_scene_connect_on_event(void* context, SceneManagerEvent event) {
    LoRaTalkApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == LoRaTalkEventConnected) {
            if(app->last_error != LORA_OK) {
                FURI_LOG_W(
                    "LoRaTalk",
                    "Saved settings not re-applied: %s",
                    lora_module_error_str(app->last_error));
            }
            scene_manager_next_scene(app->scene_manager, LoRaTalkSceneMainMenu);
            consumed = true;
        } else if(event.event == LoRaTalkEventConnectFailed) {
            scene_manager_next_scene(app->scene_manager, LoRaTalkSceneConnectError);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_stop(app->scene_manager);
        view_dispatcher_stop(app->view_dispatcher);
        consumed = true;
    }
    return consumed;
}

void loratalk_scene_connect_on_exit(void* context) {
    LoRaTalkApp* app = context;
    widget_reset(app->widget);
}
