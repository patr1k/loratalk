#pragma once

#include "loratalk_app.h"

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/dialog_ex.h>
#include <gui/modules/number_input.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <notification/notification_messages.h>
#include <expansion/expansion.h>

#include "loratalk_custom_event.h"
#include "loratalk_history.h"
#include "loratalk_settings.h"
#include "lora/lora_module.h"
#include "scenes/loratalk_scene.h"
#include "views/chat_view.h"

#define LORATALK_VERSION "1.0"

typedef enum {
    LoRaTalkViewSubmenu,
    LoRaTalkViewVariableItemList,
    LoRaTalkViewTextInput,
    LoRaTalkViewNumberInput,
    LoRaTalkViewWidget,
    LoRaTalkViewDialog,
    LoRaTalkViewChat,
} LoRaTalkView;

/** What the shared number input scene is editing (its scene state) */
typedef enum {
    LoRaTalkNumberAddress,
    LoRaTalkNumberFrequency,
    LoRaTalkNumberDestination,
} LoRaTalkNumberTarget;

struct LoRaTalkApp {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    NotificationApp* notifications;
    Expansion* expansion;

    Submenu* submenu;
    VariableItemList* variable_item_list;
    TextInput* text_input;
    NumberInput* number_input;
    Widget* widget;
    DialogEx* dialog;
    ChatView* chat;

    LoRaModule* lora;
    FuriMessageQueue* lora_events;
    LoRaModuleInfo info; /**< Snapshot taken after connect / apply */
    int last_error; /**< Result of the last connect / apply */

    LoRaTalkSettings settings;
    LoRaTalkHistory* history;
    uint32_t next_message_id;
    uint16_t unread;

    /* Settings editor */
    LoRaConfig edit;
    bool edit_dirty;
    uint8_t edit_selected;
    VariableItem* item_sf;
    VariableItem* item_preamble;
    VariableItem* item_apply;

    /* Input buffers */
    char text_buf[LORA_MAX_PAYLOAD + 1];
    char password_buf[LORA_PASSWORD_LEN + 1];
    char header_buf[32];
    int32_t number_result;

    FuriString* text;
};

/** Remember the module's current settings on the SD card. */
void loratalk_app_save_settings(LoRaTalkApp* app);

/** Format a frequency in Hz as "915.000". */
void loratalk_format_mhz(char* out, size_t size, uint32_t hz);
