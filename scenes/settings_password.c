#include "../loratalk_app_i.h"

#include <ctype.h>

static void settings_password_callback(void* context) {
    LoRaTalkApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, LoRaTalkEventTextDone);
}

static bool settings_password_validator(const char* text, FuriString* error, void* context) {
    UNUSED(context);
    if(lora_config_password_valid(text)) return true;
    furi_string_set(error, "8 hex digits\n(0-9, A-F)\nor empty=off");
    return false;
}

void loratalk_scene_settings_password_on_enter(void* context) {
    LoRaTalkApp* app = context;
    TextInput* text_input = app->text_input;

    strlcpy(app->password_buf, app->edit.password, sizeof(app->password_buf));
    text_input_reset(text_input);
    text_input_set_header_text(text_input, "Password (empty = off)");
    text_input_set_minimum_length(text_input, 0);
    text_input_set_validator(text_input, settings_password_validator, app);
    text_input_set_result_callback(
        text_input,
        settings_password_callback,
        app,
        app->password_buf,
        sizeof(app->password_buf),
        false);
    view_dispatcher_switch_to_view(app->view_dispatcher, LoRaTalkViewTextInput);
}

bool loratalk_scene_settings_password_on_event(void* context, SceneManagerEvent event) {
    LoRaTalkApp* app = context;
    if(event.type == SceneManagerEventTypeCustom && event.event == LoRaTalkEventTextDone) {
        for(char* p = app->password_buf; *p; p++)
            *p = (char)toupper((unsigned char)*p);
        if(strcmp(app->password_buf, app->edit.password) != 0) {
            strlcpy(app->edit.password, app->password_buf, sizeof(app->edit.password));
            app->edit_dirty = true;
        }
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    return false;
}

void loratalk_scene_settings_password_on_exit(void* context) {
    LoRaTalkApp* app = context;
    text_input_reset(app->text_input);
}
