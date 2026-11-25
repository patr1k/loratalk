#include "../loratalk_app_i.h"

/*
 * Module settings editor. Changes go into app->edit and are only written to
 * the module by "Apply". Scene state: 0 = load app->edit from the module
 * snapshot on enter, 1 = keep editing (returning from a sub-screen).
 */

typedef enum {
    SettingsItemAddress,
    SettingsItemNetworkId,
    SettingsItemFrequency,
    SettingsItemSpreadingFactor,
    SettingsItemBandwidth,
    SettingsItemCodingRate,
    SettingsItemPreamble,
    SettingsItemPower,
    SettingsItemPassword,
    SettingsItemBaud,
    SettingsItemApply,
} SettingsItem;

static const char* const coding_rate_text[] = {"4/5", "4/6", "4/7", "4/8"};
static const char* const bandwidth_text[] = {"125kHz", "250kHz", "500kHz"};

static void settings_mark_dirty(LoRaTalkApp* app) {
    app->edit_dirty = true;
    variable_item_set_current_value_text(app->item_apply, "*");
}

static void settings_set_number_text(VariableItem* item, const char* fmt, unsigned value) {
    char buf[16];
    snprintf(buf, sizeof(buf), fmt, value);
    variable_item_set_current_value_text(item, buf);
}

static void settings_update_sf(LoRaTalkApp* app) {
    uint8_t max = lora_config_sf_max(app->edit.bw);
    if(app->edit.sf > max) app->edit.sf = max;
    if(app->edit.sf < LORA_SF_MIN) app->edit.sf = LORA_SF_MIN;
    variable_item_set_values_count(app->item_sf, max - LORA_SF_MIN + 1);
    variable_item_set_current_value_index(app->item_sf, app->edit.sf - LORA_SF_MIN);
    settings_set_number_text(app->item_sf, "SF%u", app->edit.sf);
}

static void settings_update_preamble(LoRaTalkApp* app) {
    if(lora_config_preamble_locked(app->edit.network_id)) {
        // Only NETWORKID 18 may change the preamble
        app->edit.preamble = LORA_PREAMBLE_DEFAULT;
        variable_item_set_values_count(app->item_preamble, 1);
        variable_item_set_current_value_index(app->item_preamble, 0);
    } else {
        if(app->edit.preamble < LORA_PREAMBLE_MIN) app->edit.preamble = LORA_PREAMBLE_MIN;
        if(app->edit.preamble > LORA_PREAMBLE_MAX) app->edit.preamble = LORA_PREAMBLE_MAX;
        variable_item_set_values_count(
            app->item_preamble, LORA_PREAMBLE_MAX - LORA_PREAMBLE_MIN + 1);
        variable_item_set_current_value_index(
            app->item_preamble, app->edit.preamble - LORA_PREAMBLE_MIN);
    }
    settings_set_number_text(app->item_preamble, "%u", app->edit.preamble);
}

static void settings_network_changed(VariableItem* item) {
    LoRaTalkApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->edit.network_id = lora_network_ids[index];
    settings_set_number_text(item, "%u", app->edit.network_id);
    settings_update_preamble(app);
    settings_mark_dirty(app);
}

static void settings_sf_changed(VariableItem* item) {
    LoRaTalkApp* app = variable_item_get_context(item);
    app->edit.sf = LORA_SF_MIN + variable_item_get_current_value_index(item);
    settings_set_number_text(item, "SF%u", app->edit.sf);
    settings_mark_dirty(app);
}

static void settings_bw_changed(VariableItem* item) {
    LoRaTalkApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->edit.bw = LORA_BW_125 + index;
    variable_item_set_current_value_text(item, bandwidth_text[index]);
    settings_update_sf(app);
    settings_mark_dirty(app);
}

static void settings_cr_changed(VariableItem* item) {
    LoRaTalkApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->edit.cr = LORA_CR_MIN + index;
    variable_item_set_current_value_text(item, coding_rate_text[index]);
    settings_mark_dirty(app);
}

static void settings_preamble_changed(VariableItem* item) {
    LoRaTalkApp* app = variable_item_get_context(item);
    if(lora_config_preamble_locked(app->edit.network_id)) return;
    app->edit.preamble = LORA_PREAMBLE_MIN + variable_item_get_current_value_index(item);
    settings_set_number_text(item, "%u", app->edit.preamble);
    settings_mark_dirty(app);
}

static void settings_power_changed(VariableItem* item) {
    LoRaTalkApp* app = variable_item_get_context(item);
    app->edit.rf_power = variable_item_get_current_value_index(item);
    settings_set_number_text(item, "%udBm", app->edit.rf_power);
    settings_mark_dirty(app);
}

static void settings_baud_changed(VariableItem* item) {
    LoRaTalkApp* app = variable_item_get_context(item);
    app->edit.baud = lora_baud_rates[variable_item_get_current_value_index(item)];
    settings_set_number_text(item, "%u", (unsigned)app->edit.baud);
    settings_mark_dirty(app);
}

static void settings_enter_callback(void* context, uint32_t index) {
    LoRaTalkApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, LoRaTalkEventItem + index);
}

void loratalk_scene_settings_on_enter(void* context) {
    LoRaTalkApp* app = context;
    VariableItemList* list = app->variable_item_list;
    VariableItem* item;
    char buf[16];

    if(scene_manager_get_scene_state(app->scene_manager, LoRaTalkSceneSettings) == 0) {
        app->edit = app->info.config;
        // If the module holds a value we can't represent (e.g. an 868 MHz
        // band), the corrected value needs applying
        app->edit_dirty = !lora_config_sanitize(&app->edit);
        scene_manager_set_scene_state(app->scene_manager, LoRaTalkSceneSettings, 1);
    }

    variable_item_list_reset(list);
    variable_item_list_set_enter_callback(list, settings_enter_callback, app);

    item = variable_item_list_add(list, "Address", 1, NULL, app);
    settings_set_number_text(item, "%u", app->edit.address);

    uint8_t net_index = 0;
    for(size_t i = 0; i < lora_network_ids_count; i++) {
        if(lora_network_ids[i] == app->edit.network_id) net_index = i;
    }
    item = variable_item_list_add(
        list, "Network ID", lora_network_ids_count, settings_network_changed, app);
    variable_item_set_current_value_index(item, net_index);
    settings_set_number_text(item, "%u", app->edit.network_id);

    item = variable_item_list_add(list, "Freq (MHz)", 1, NULL, app);
    loratalk_format_mhz(buf, sizeof(buf), app->edit.band_hz);
    variable_item_set_current_value_text(item, buf);

    app->item_sf = variable_item_list_add(list, "Spreading", 1, settings_sf_changed, app);
    settings_update_sf(app);

    item = variable_item_list_add(list, "Bandwidth", 3, settings_bw_changed, app);
    variable_item_set_current_value_index(item, app->edit.bw - LORA_BW_125);
    variable_item_set_current_value_text(item, bandwidth_text[app->edit.bw - LORA_BW_125]);

    item = variable_item_list_add(list, "Coding Rate", 4, settings_cr_changed, app);
    variable_item_set_current_value_index(item, app->edit.cr - LORA_CR_MIN);
    variable_item_set_current_value_text(item, coding_rate_text[app->edit.cr - LORA_CR_MIN]);

    app->item_preamble =
        variable_item_list_add(list, "Preamble", 1, settings_preamble_changed, app);
    settings_update_preamble(app);

    item = variable_item_list_add(
        list, "TX Power", LORA_POWER_MAX + 1, settings_power_changed, app);
    variable_item_set_current_value_index(item, app->edit.rf_power);
    settings_set_number_text(item, "%udBm", app->edit.rf_power);

    item = variable_item_list_add(list, "Password", 1, NULL, app);
    variable_item_set_current_value_text(item, app->edit.password[0] ? "On" : "Off");

    uint8_t baud_index = 0;
    for(size_t i = 0; i < lora_baud_rates_count; i++) {
        if(lora_baud_rates[i] == app->edit.baud) baud_index = i;
    }
    item =
        variable_item_list_add(list, "Baud Rate", lora_baud_rates_count, settings_baud_changed, app);
    variable_item_set_current_value_index(item, baud_index);
    settings_set_number_text(item, "%u", (unsigned)app->edit.baud);

    app->item_apply = variable_item_list_add(list, "Apply Changes", 1, NULL, app);
    variable_item_set_current_value_text(app->item_apply, app->edit_dirty ? "*" : "");

    variable_item_list_set_selected_item(list, app->edit_selected);
    view_dispatcher_switch_to_view(app->view_dispatcher, LoRaTalkViewVariableItemList);
}

bool loratalk_scene_settings_on_event(void* context, SceneManagerEvent event) {
    LoRaTalkApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom && event.event >= LoRaTalkEventItem) {
        uint32_t index = event.event - LoRaTalkEventItem;
        consumed = true;
        switch(index) {
        case SettingsItemAddress:
            scene_manager_set_scene_state(
                app->scene_manager, LoRaTalkSceneNumberInput, LoRaTalkNumberAddress);
            scene_manager_next_scene(app->scene_manager, LoRaTalkSceneNumberInput);
            break;
        case SettingsItemFrequency:
            scene_manager_set_scene_state(
                app->scene_manager, LoRaTalkSceneNumberInput, LoRaTalkNumberFrequency);
            scene_manager_next_scene(app->scene_manager, LoRaTalkSceneNumberInput);
            break;
        case SettingsItemPassword:
            scene_manager_next_scene(app->scene_manager, LoRaTalkSceneSettingsPassword);
            break;
        case SettingsItemApply:
            scene_manager_next_scene(app->scene_manager, LoRaTalkSceneSettingsApply);
            break;
        default:
            consumed = false;
            break;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        if(app->edit_dirty) {
            scene_manager_next_scene(app->scene_manager, LoRaTalkSceneSettingsUnsaved);
        } else {
            scene_manager_set_scene_state(app->scene_manager, LoRaTalkSceneSettings, 0);
            scene_manager_previous_scene(app->scene_manager);
        }
        consumed = true;
    }
    return consumed;
}

void loratalk_scene_settings_on_exit(void* context) {
    LoRaTalkApp* app = context;
    app->edit_selected = variable_item_list_get_selected_item_index(app->variable_item_list);
    variable_item_list_reset(app->variable_item_list);
    app->item_sf = NULL;
    app->item_preamble = NULL;
    app->item_apply = NULL;
}
