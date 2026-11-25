#include "../loratalk_app_i.h"

static void clear_history_callback(DialogExResult result, void* context) {
    LoRaTalkApp* app = context;
    if(result == DialogExResultLeft) {
        view_dispatcher_send_custom_event(app->view_dispatcher, LoRaTalkEventDialogLeft);
    } else if(result == DialogExResultRight) {
        view_dispatcher_send_custom_event(app->view_dispatcher, LoRaTalkEventDialogRight);
    }
}

void loratalk_scene_clear_history_on_enter(void* context) {
    LoRaTalkApp* app = context;
    DialogEx* dialog = app->dialog;

    dialog_ex_reset(dialog);
    dialog_ex_set_header(dialog, "Clear chat history?", 64, 10, AlignCenter, AlignCenter);
    dialog_ex_set_text(
        dialog, "Deletes every message\non this Flipper.", 64, 32, AlignCenter, AlignCenter);
    dialog_ex_set_left_button_text(dialog, "Cancel");
    dialog_ex_set_right_button_text(dialog, "Clear");
    dialog_ex_set_result_callback(dialog, clear_history_callback);
    dialog_ex_set_context(dialog, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, LoRaTalkViewDialog);
}

bool loratalk_scene_clear_history_on_event(void* context, SceneManagerEvent event) {
    LoRaTalkApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == LoRaTalkEventDialogRight) {
        loratalk_history_clear(app->history);
        app->unread = 0;
        chat_view_refresh(app->chat);
        notification_message(app->notifications, &sequence_success);
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    if(event.event == LoRaTalkEventDialogLeft) {
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    return false;
}

void loratalk_scene_clear_history_on_exit(void* context) {
    LoRaTalkApp* app = context;
    dialog_ex_reset(app->dialog);
}
