#include "../loratalk_app_i.h"

/* Scene state: 0 = applying, 1 = showing an error */

void loratalk_scene_settings_apply_on_enter(void* context) {
    LoRaTalkApp* app = context;
    scene_manager_set_scene_state(app->scene_manager, LoRaTalkSceneSettingsApply, 0);

    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 26, AlignCenter, AlignCenter, FontPrimary, "Applying...");
    widget_add_string_element(
        app->widget, 64, 42, AlignCenter, AlignCenter, FontSecondary, "Writing to the module");
    view_dispatcher_switch_to_view(app->view_dispatcher, LoRaTalkViewWidget);

    lora_module_apply(app->lora, &app->edit);
}

bool loratalk_scene_settings_apply_on_event(void* context, SceneManagerEvent event) {
    LoRaTalkApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom && event.event == LoRaTalkEventApplyDone) {
        if(app->last_error == LORA_OK) {
            notification_message(app->notifications, &sequence_success);
            app->edit_dirty = false;
            // Reload the editor from what the module now reports
            scene_manager_set_scene_state(app->scene_manager, LoRaTalkSceneSettings, 0);
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, LoRaTalkSceneSettings);
        } else {
            scene_manager_set_scene_state(app->scene_manager, LoRaTalkSceneSettingsApply, 1);
            notification_message(app->notifications, &sequence_error);
            widget_reset(app->widget);
            widget_add_string_element(
                app->widget, 64, 14, AlignCenter, AlignCenter, FontPrimary, "Apply failed");
            widget_add_string_element(
                app->widget,
                64,
                32,
                AlignCenter,
                AlignCenter,
                FontSecondary,
                lora_module_error_str(app->last_error));
            widget_add_string_element(
                app->widget, 64, 54, AlignCenter, AlignCenter, FontSecondary, "Back: edit settings");
        }
        consumed = true;
    } else if(event.type == SceneManagerEventTypeBack) {
        if(scene_manager_get_scene_state(app->scene_manager, LoRaTalkSceneSettingsApply) == 1) {
            // Keep the edits so the user can fix them and try again
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, LoRaTalkSceneSettings);
        }
        // While applying, Back is ignored
        consumed = true;
    }
    return consumed;
}

void loratalk_scene_settings_apply_on_exit(void* context) {
    LoRaTalkApp* app = context;
    widget_reset(app->widget);
}
