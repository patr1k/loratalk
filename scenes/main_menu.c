#include "../loratalk_app_i.h"

typedef enum {
    MainMenuChat,
    MainMenuSettings,
    MainMenuInfo,
    MainMenuClearHistory,
    MainMenuAbout,
} MainMenuIndex;

static void main_menu_callback(void* context, uint32_t index) {
    LoRaTalkApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, LoRaTalkEventItem + index);
}

static const char* main_menu_chat_label(LoRaTalkApp* app) {
    if(app->unread == 0) return "Chat";
    furi_string_printf(app->text, "Chat (%u new)", app->unread);
    return furi_string_get_cstr(app->text);
}

void loratalk_scene_main_menu_on_enter(void* context) {
    LoRaTalkApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    submenu_add_item(submenu, main_menu_chat_label(app), MainMenuChat, main_menu_callback, app);
    submenu_add_item(submenu, "Module Settings", MainMenuSettings, main_menu_callback, app);
    submenu_add_item(submenu, "Module Info", MainMenuInfo, main_menu_callback, app);
    submenu_add_item(
        submenu, "Clear Chat History", MainMenuClearHistory, main_menu_callback, app);
    submenu_add_item(submenu, "Wiring & About", MainMenuAbout, main_menu_callback, app);
    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, LoRaTalkSceneMainMenu));

    view_dispatcher_switch_to_view(app->view_dispatcher, LoRaTalkViewSubmenu);
}

bool loratalk_scene_main_menu_on_event(void* context, SceneManagerEvent event) {
    LoRaTalkApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == LoRaTalkEventMessageReceived) {
            submenu_change_item_label(app->submenu, MainMenuChat, main_menu_chat_label(app));
            return true;
        }
        if(event.event < LoRaTalkEventItem) return false;

        uint32_t index = event.event - LoRaTalkEventItem;
        scene_manager_set_scene_state(app->scene_manager, LoRaTalkSceneMainMenu, index);
        consumed = true;
        switch(index) {
        case MainMenuChat:
            scene_manager_next_scene(app->scene_manager, LoRaTalkSceneChat);
            break;
        case MainMenuSettings:
            // Fresh editing session
            scene_manager_set_scene_state(app->scene_manager, LoRaTalkSceneSettings, 0);
            app->edit_selected = 0;
            scene_manager_next_scene(app->scene_manager, LoRaTalkSceneSettings);
            break;
        case MainMenuInfo:
            scene_manager_next_scene(app->scene_manager, LoRaTalkSceneInfo);
            break;
        case MainMenuClearHistory:
            scene_manager_next_scene(app->scene_manager, LoRaTalkSceneClearHistory);
            break;
        case MainMenuAbout:
            scene_manager_next_scene(app->scene_manager, LoRaTalkSceneAbout);
            break;
        default:
            consumed = false;
            break;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_stop(app->scene_manager);
        view_dispatcher_stop(app->view_dispatcher);
        consumed = true;
    }
    return consumed;
}

void loratalk_scene_main_menu_on_exit(void* context) {
    LoRaTalkApp* app = context;
    submenu_reset(app->submenu);
}
