#include "../loratalk_app_i.h"

enum ConfigItem {
    ConfigItemAddress,
    ConfigItemFrequency,
};

const char* baud_rates[] =
    {"300", "1200", "4800", "9600", "19200", "28800", "38400", "57600", "115200"};
const char* bandwidths[] = {"125 KHz", "250 KHz", "500 KHz"};
const char* coding_rates[] = {"4/5", "4/6", "4/7", "4/8"};
const char* spreading_factors[] = {"7", "8", "9", "10", "11"};
const char* preambles[] = {"4", "5", "6", "7", "8"};

void enter_callback(void* context, uint32_t index) {
    furi_assert(context);
    LoRaTalkApp* app = context;
    if(index == ConfigItemAddress) {
        view_dispatcher_send_custom_event(app->view_dispatcher, LoRaTalkView_Config_Address);
    } else if(index == ConfigItemFrequency) {
        view_dispatcher_send_custom_event(app->view_dispatcher, LoRaTalkView_Config_Frequency);
    }
}

void baud_rate_change_callback(VariableItem* item) {
    LoRaTalkApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, baud_rates[index]);
    app->baud_rate = index;
}

void loratalk_scene_config_on_enter(void* context) {
    furi_assert(context);
    LoRaTalkApp* app = context;
    variable_item_list_reset(app->config_view);

    variable_item_list_set_enter_callback(app->config_view, enter_callback, app);

    VariableItem* address_item = variable_item_list_add(app->config_view, "Address", 1, NULL, app);

    variable_item_set_current_value_index(address_item, 0);
    char address[6];
    itoa(app->address, address, 10);
    variable_item_set_current_value_text(address_item, address);

    VariableItem* baud_rate_item = variable_item_list_add(
        app->config_view, "Baud Rate", COUNT_OF(baud_rates), baud_rate_change_callback, app);

    variable_item_set_current_value_index(baud_rate_item, app->baud_rate);
    variable_item_set_current_value_text(baud_rate_item, baud_rates[app->baud_rate]);

    view_dispatcher_switch_to_view(app->view_dispatcher, LoRaTalkView_Config);
}

bool loratalk_scene_config_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    //LoRaTalkApp* app = context;
    bool consumed = false;
    switch(event.type) {
    case SceneManagerEventTypeCustom:
        switch(event.event) {}
        break;
    default:
        consumed = false;
        break;
    }
    return consumed;
}

void loratalk_scene_config_on_exit(void* context) {
    furi_assert(context);
    LoRaTalkApp* app = context;
    variable_item_list_reset(app->config_view);
}
