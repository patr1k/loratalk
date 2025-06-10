#include "../loratalk_app_i.h"

typedef enum {
    ConfigItemAddress,
    ConfigItemFrequency,
} ConfigItem;

bool on_menu = true;

const char* baud_rates[] =
    {"300", "1200", "4800", "9600", "19200", "28800", "38400", "57600", "115200"};
const char* bandwidths[] = {"125 KHz", "250 KHz", "500 KHz"};
const char* coding_rates[] = {"4/5", "4/6", "4/7", "4/8"};
const char* spreading_factors[] = {"7", "8", "9", "10", "11"};
const char* preambles[] = {"4", "5", "6", "7", "8"};

char address_input[16];
VariableItem* address_item;

static void enter_callback(void* context, uint32_t index) {
    furi_assert(context);
    LoRaTalkApp* app = context;
    if(index == ConfigItemAddress) {
        view_dispatcher_send_custom_event(app->view_dispatcher, LoRaTalkView_Config_Address);
    } else if(index == ConfigItemFrequency) {
        view_dispatcher_send_custom_event(app->view_dispatcher, LoRaTalkView_Config_Frequency);
    }
}

static void baud_rate_change_callback(VariableItem* item) {
    LoRaTalkApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, baud_rates[index]);
    app->baud_rate = atoi(baud_rates[index]);
}

static bool address_validate(const char* text, FuriString* error, void* context) {
    UNUSED(context);

    if(strlen(text) < 1) {
        furi_string_set(error, "Cannot be\nempty!");
        return false;
    }

    while(*text) {
        if(!isdigit((unsigned char)*text++)) {
            furi_string_set(error, "Must be an\ninteger!");
            return false;
        }
    }

    uint32_t value = atoi(text);
    if(value > 65535) {
        furi_string_set(error, "Range is:\n0-65535");
        return false;
    }

    return true;
}

static void address_change(void* context) {
    furi_assert(context);
    LoRaTalkApp* app = context;
    app->address = atoi(address_input);
    variable_item_set_current_value_text(address_item, address_input);
    on_menu = true;
    view_dispatcher_switch_to_view(app->view_dispatcher, LoRaTalkView_Config);
}

void loratalk_scene_config_on_enter(void* context) {
    furi_assert(context);
    LoRaTalkApp* app = context;
    variable_item_list_reset(app->config);

    variable_item_list_set_enter_callback(app->config, enter_callback, app);

    address_item = variable_item_list_add(app->config, "Address", 1, NULL, app);

    variable_item_set_current_value_index(address_item, 0);
    char address[6];
    itoa(app->address, address, 10);
    variable_item_set_current_value_text(address_item, address);

    VariableItem* baud_rate_item = variable_item_list_add(
        app->config, "Baud Rate", COUNT_OF(baud_rates), baud_rate_change_callback, app);

    uint8_t index = 0;
    for(uint8_t i = 0; i < COUNT_OF(baud_rates); i++) {
        if((uint32_t)atoi(baud_rates[i]) == app->baud_rate) {
            index = i;
            break;
        }
    }
    variable_item_set_current_value_index(baud_rate_item, index);
    variable_item_set_current_value_text(baud_rate_item, baud_rates[index]);

    view_dispatcher_switch_to_view(app->view_dispatcher, LoRaTalkView_Config);
}

bool loratalk_scene_config_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    LoRaTalkApp* app = context;
    bool consumed = false;
    switch(event.type) {
    case SceneManagerEventTypeCustom:
        switch(event.event) {
        case LoRaTalkView_Config_Address:
            text_input_reset(app->config_address);
            text_input_set_header_text(app->config_address, "Address");
            text_input_set_minimum_length(app->config_address, 1);
            text_input_set_validator(app->config_address, address_validate, app);
            itoa(app->address, address_input, 10);
            text_input_set_result_callback(
                app->config_address,
                address_change,
                app,
                address_input,
                sizeof(address_input),
                false);
            on_menu = false;
            view_dispatcher_switch_to_view(app->view_dispatcher, LoRaTalkView_Config_Address);
            consumed = true;
            break;
        }
        break;
    case SceneManagerEventTypeBack:
        if(on_menu) {
            scene_manager_previous_scene(app->scene_manager);
        } else {
            on_menu = true;
            view_dispatcher_switch_to_view(app->view_dispatcher, LoRaTalkView_Config);
        }
        consumed = true;
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
    variable_item_list_reset(app->config);
}
