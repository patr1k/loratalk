#include "../loratalk_app_i.h"

static void settings_unsaved_callback(DialogExResult result, void* context) {
    LoRaTalkApp* app = context;
    if(result == DialogExResultLeft) {
        view_dispatcher_send_custom_event(app->view_dispatcher, LoRaTalkEventDialogLeft);
    } else if(result == DialogExResultRight) {
        view_dispatcher_send_custom_event(app->view_dispatcher, LoRaTalkEventDialogRight);
    }
}

void loratalk_scene_settings_unsaved_on_enter(void* context) {
    LoRaTalkApp* app = context;
    DialogEx* dialog = app->dialog;

    dialog_ex_reset(dialog);
    dialog_ex_set_header(dialog, "Unsaved changes", 64, 10, AlignCenter, AlignCenter);
    dialog_ex_set_text(
        dialog, "Write them to\nthe module?", 64, 32, AlignCenter, AlignCenter);
    dialog_ex_set_left_button_text(dialog, "Discard");
    dialog_ex_set_right_button_text(dialog, "Apply");
    dialog_ex_set_result_callback(dialog, settings_unsaved_callback);
    dialog_ex_set_context(dialog, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, LoRaTalkViewDialog);
}

bool loratalk_scene_settings_unsaved_on_event(void* context, SceneManagerEvent event) {
    LoRaTalkApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == LoRaTalkEventDialogLeft) {
        app->edit_dirty = false;
        scene_manager_set_scene_state(app->scene_manager, LoRaTalkSceneSettings, 0);
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, LoRaTalkSceneMainMenu);
        return true;
    }
    if(event.event == LoRaTalkEventDialogRight) {
        scene_manager_next_scene(app->scene_manager, LoRaTalkSceneSettingsApply);
        return true;
    }
    return false;
}

void loratalk_scene_settings_unsaved_on_exit(void* context) {
    LoRaTalkApp* app = context;
    dialog_ex_reset(app->dialog);
}
