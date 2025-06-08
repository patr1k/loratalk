#include "../loratalk_app_i.h"

typedef enum {
    MainMenuIndexConfig,
    MainMenuIndexChat,
} MainMenuIndex;

void loratalk_app_menu_callback_main_menu(void* context, uint32_t index) {
    furi_assert(context);
    LoRaTalkApp* app = context;
    switch(index) {
    case MainMenuIndexConfig:
        scene_manager_handle_custom_event(app->scene_manager, LoRaTalkEvent_ShowConfig);
        break;
    case MainMenuIndexChat:
        scene_manager_handle_custom_event(app->scene_manager, LoRaTalkEvent_ShowChat);
        break;
    }
}

void loratalk_scene_main_menu_on_enter(void* context) {
    furi_assert(context);
    LoRaTalkApp* app = context;
    submenu_reset(app->main_menu);

    submenu_add_item(
        app->main_menu,
        "Configure Module",
        MainMenuIndexConfig,
        loratalk_app_menu_callback_main_menu,
        app);
    submenu_add_item(
        app->main_menu, "Chat", MainMenuIndexChat, loratalk_app_menu_callback_main_menu, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, LoRaTalkView_MainMenu);
}

bool loratalk_scene_main_menu_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    LoRaTalkApp* app = context;
    bool consumed = false;
    switch(event.type) {
    case SceneManagerEventTypeCustom:
        switch(event.event) {
        case LoRaTalkEvent_ShowConfig:
            scene_manager_next_scene(app->scene_manager, LoRaTalkScene_Config);
            consumed = true;
            break;
        }
        break;
    default:
        consumed = false;
        break;
    }
    return consumed;
}

void loratalk_scene_main_menu_on_exit(void* context) {
    furi_assert(context);
    LoRaTalkApp* app = context;
    submenu_reset(app->main_menu);
}
