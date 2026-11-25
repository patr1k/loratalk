#include "../loratalk_app_i.h"

void loratalk_scene_info_on_enter(void* context) {
    LoRaTalkApp* app = context;
    const LoRaConfig* c = &app->info.config;
    char mhz[16];
    loratalk_format_mhz(mhz, sizeof(mhz), c->band_hz);

    FuriString* s = app->text;
    furi_string_printf(
        s,
        "Firmware: %s\n"
        "UID: %s\n"
        "Address: %u\n"
        "Network ID: %u\n"
        "Frequency: %s MHz\n"
        "SF%u, BW %ukHz, CR 4/%u\n"
        "Preamble: %u\n"
        "TX power: %u dBm\n"
        "Password: %s\n"
        "UART: %lu baud\n"
        "Airtime: %lums (20 chars)\n"
        "         %lums (240 chars)\n"
        "History: %s",
        app->info.version[0] ? app->info.version : "?",
        app->info.uid[0] ? app->info.uid : "?",
        c->address,
        c->network_id,
        mhz,
        c->sf,
        lora_config_bw_khz(c->bw),
        c->cr + 4,
        c->preamble,
        c->rf_power,
        c->password[0] ? "on" : "off",
        c->baud,
        lora_config_airtime_ms(c, 20),
        lora_config_airtime_ms(c, LORA_MAX_PAYLOAD),
        loratalk_history_is_persistent(app->history) ? "saved to SD" : "not saved (no SD)");

    widget_reset(app->widget);
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, furi_string_get_cstr(s));
    view_dispatcher_switch_to_view(app->view_dispatcher, LoRaTalkViewWidget);
}

bool loratalk_scene_info_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void loratalk_scene_info_on_exit(void* context) {
    LoRaTalkApp* app = context;
    widget_reset(app->widget);
}
