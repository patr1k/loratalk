#include "loratalk_app_i.h"
#include "scenes/loratalk_scene.h"

static bool loratalk_app_custom_event_callback(void* context, uint32_t custom_event) {
    furi_assert(context);
    LoRaTalkApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, custom_event);
}

static bool loratalk_app_back_event_callback(void* context) {
    furi_assert(context);
    LoRaTalkApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void loratalk_app_tick_event_callback(void* context) {
    furi_assert(context);
    LoRaTalkApp* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

void loratalk_app_scene_manager_init(LoRaTalkApp* app) {
    app->scene_manager = scene_manager_alloc(&loratalk_app_scene_handlers, app);
}

void loratalk_app_view_dispatcher_init(LoRaTalkApp* app) {
    app->view_dispatcher = view_dispatcher_alloc();

    app->loading = loading_alloc();
    app->main_menu = submenu_alloc();
    app->config = variable_item_list_alloc();
    app->config_address = text_input_alloc();

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, loratalk_app_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, loratalk_app_back_event_callback);
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, loratalk_app_tick_event_callback, 100);

    view_dispatcher_add_view(
        app->view_dispatcher, LoRaTalkView_Loading, loading_get_view(app->loading));
    view_dispatcher_add_view(
        app->view_dispatcher, LoRaTalkView_MainMenu, submenu_get_view(app->main_menu));
    view_dispatcher_add_view(
        app->view_dispatcher, LoRaTalkView_Config, variable_item_list_get_view(app->config));
    view_dispatcher_add_view(
        app->view_dispatcher,
        LoRaTalkView_Config_Address,
        text_input_get_view(app->config_address));
}

LoRaTalkApp* loratalk_app_alloc() {
    LoRaTalkApp* app = malloc(sizeof(LoRaTalkApp));

    loratalk_app_scene_manager_init(app);
    loratalk_app_view_dispatcher_init(app);

    app->baud_rate = BaudRate_9600;
    app->address = 14721;

    return app;
}

void loratalk_app_free(LoRaTalkApp* app) {
    scene_manager_free(app->scene_manager);
    view_dispatcher_remove_view(app->view_dispatcher, LoRaTalkView_Loading);
    view_dispatcher_remove_view(app->view_dispatcher, LoRaTalkView_MainMenu);
    view_dispatcher_remove_view(app->view_dispatcher, LoRaTalkView_Config);
    view_dispatcher_remove_view(app->view_dispatcher, LoRaTalkView_Config_Address);
    view_dispatcher_free(app->view_dispatcher);
    loading_free(app->loading);
    submenu_free(app->main_menu);
    variable_item_list_free(app->config);
    free(app);
}

int32_t loratalk_app(void* arg) {
    UNUSED(arg);

    LoRaTalkApp* app = loratalk_app_alloc();

    Gui* gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    scene_manager_next_scene(app->scene_manager, LoRaTalkScene_MainMenu);
    view_dispatcher_run(app->view_dispatcher);

    furi_record_close(RECORD_GUI);
    loratalk_app_free(app);

    return 0;
}
