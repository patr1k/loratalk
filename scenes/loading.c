#include "../loratalk_app_i.h"

void loratalk_scene_loading_on_enter(void* context) {
    furi_assert(context);
    LoRaTalkApp* app = context;

    view_dispatcher_switch_to_view(app->view_dispatcher, LoRaTalkView_Loading);
}

bool loratalk_scene_loading_on_event(void* context, SceneManagerEvent event) {
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

    case SceneManagerEventTypeBack:
        view_dispatcher_stop(app->view_dispatcher);
        consumed = true;
        break;

    default:
        consumed = false;
        break;
    }
    return consumed;
}

void loratalk_scene_loading_on_exit(void* context) {
    UNUSED(context);
}
