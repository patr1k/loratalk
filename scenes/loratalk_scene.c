#include "loratalk_scene.h"

// Generate scene on_enter handlers array
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
void (*const loratalk_scene_on_enter_handlers[])(void*) = {
#include "loratalk_scene_config.h"
};
#undef ADD_SCENE

// Generate scene on_event handlers array
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
bool (*const loratalk_scene_on_event_handlers[])(void* context, SceneManagerEvent event) = {
#include "loratalk_scene_config.h"
};
#undef ADD_SCENE

// Generate scene on_exit handlers array
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
void (*const loratalk_scene_on_exit_handlers[])(void* context) = {
#include "loratalk_scene_config.h"
};
#undef ADD_SCENE

// Initialize scene handlers configuration structure
const SceneManagerHandlers loratalk_app_scene_handlers = {
    .on_enter_handlers = loratalk_scene_on_enter_handlers,
    .on_event_handlers = loratalk_scene_on_event_handlers,
    .on_exit_handlers = loratalk_scene_on_exit_handlers,
    .scene_num = LoRaTalkSceneCount,
};
