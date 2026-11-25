#include "../loratalk_app_i.h"

static void connect_error_button_callback(GuiButtonType result, InputType type, void* context) {
    LoRaTalkApp* app = context;
    if(result == GuiButtonTypeRight && type == InputTypeShort) {
        view_dispatcher_send_custom_event(app->view_dispatcher, LoRaTalkEventRetry);
    }
}

void loratalk_scene_connect_error_on_enter(void* context) {
    LoRaTalkApp* app = context;

    widget_reset(app->widget);
    widget_add_string_element(
        app->widget,
        64,
        2,
        AlignCenter,
        AlignTop,
        FontPrimary,
        lora_module_error_str(app->last_error));

    const char* hint;
    if(app->last_error == LORA_ERR_UART_BUSY) {
        hint = "Another app is using\nthe GPIO UART.\nClose it and retry.";
    } else {
        hint = "Flipper -> RYLR998:\n13 TX->RXD  14 RX->TXD\n9 3V3->VDD  GND->GND";
    }
    widget_add_string_multiline_element(
        app->widget, 64, 15, AlignCenter, AlignTop, FontSecondary, hint);
    widget_add_button_element(
        app->widget, GuiButtonTypeRight, "Retry", connect_error_button_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, LoRaTalkViewWidget);
}

bool loratalk_scene_connect_error_on_event(void* context, SceneManagerEvent event) {
    LoRaTalkApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom && event.event == LoRaTalkEventRetry) {
        // Back to the connect scene, which starts a new attempt
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_stop(app->scene_manager);
        view_dispatcher_stop(app->view_dispatcher);
        consumed = true;
    }
    return consumed;
}

void loratalk_scene_connect_error_on_exit(void* context) {
    LoRaTalkApp* app = context;
    widget_reset(app->widget);
}
