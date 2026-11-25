#include "../loratalk_app_i.h"

/* Shared number entry; the scene state says what is being edited. */

static void number_input_callback(void* context, int32_t number) {
    LoRaTalkApp* app = context;
    app->number_result = number;
    view_dispatcher_send_custom_event(app->view_dispatcher, LoRaTalkEventNumberDone);
}

void loratalk_scene_number_input_on_enter(void* context) {
    LoRaTalkApp* app = context;
    LoRaTalkNumberTarget target =
        scene_manager_get_scene_state(app->scene_manager, LoRaTalkSceneNumberInput);

    const char* header = "";
    int32_t value = 0, min = 0, max = 65535;
    switch(target) {
    case LoRaTalkNumberAddress:
        header = "Address (0-65535)";
        value = app->edit.address;
        break;
    case LoRaTalkNumberFrequency:
        header = "Frequency kHz (902-928M)";
        value = (int32_t)(app->edit.band_hz / 1000);
        min = LORA_BAND_MIN_HZ / 1000;
        max = LORA_BAND_MAX_HZ / 1000;
        break;
    case LoRaTalkNumberDestination:
        header = "Send to address (0=all)";
        value = app->settings.destination;
        break;
    }

    number_input_set_header_text(app->number_input, header);
    number_input_set_result_callback(
        app->number_input, number_input_callback, app, value, min, max);
    view_dispatcher_switch_to_view(app->view_dispatcher, LoRaTalkViewNumberInput);
}

bool loratalk_scene_number_input_on_event(void* context, SceneManagerEvent event) {
    LoRaTalkApp* app = context;
    if(event.type != SceneManagerEventTypeCustom || event.event != LoRaTalkEventNumberDone) {
        return false;
    }

    LoRaTalkNumberTarget target =
        scene_manager_get_scene_state(app->scene_manager, LoRaTalkSceneNumberInput);
    int32_t n = app->number_result;

    switch(target) {
    case LoRaTalkNumberAddress:
        if(n >= 0 && n <= 65535 && (uint16_t)n != app->edit.address) {
            app->edit.address = (uint16_t)n;
            app->edit_dirty = true;
        }
        break;
    case LoRaTalkNumberFrequency: {
        uint32_t hz = (uint32_t)n * 1000UL;
        if(hz >= LORA_BAND_MIN_HZ && hz <= LORA_BAND_MAX_HZ && hz != app->edit.band_hz) {
            app->edit.band_hz = hz;
            app->edit_dirty = true;
        }
        break;
    }
    case LoRaTalkNumberDestination:
        if(n >= 0 && n <= 65535) {
            app->settings.destination = (uint16_t)n;
            chat_view_set_destination(app->chat, app->settings.destination);
            loratalk_app_save_settings(app);
        }
        break;
    }

    scene_manager_previous_scene(app->scene_manager);
    return true;
}

void loratalk_scene_number_input_on_exit(void* context) {
    UNUSED(context);
}
