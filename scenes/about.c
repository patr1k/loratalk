#include "../loratalk_app_i.h"

static const char about_text[] =
    "\e#LoRa Talk v" LORATALK_VERSION "\n"
    "Text chat over a REYAX\n"
    "RYLR998 LoRa module.\n"
    "\n"
    "\e#Wiring\n"
    "Flipper pin -> RYLR998\n"
    " 9  3V3 -> VDD\n"
    " 13 TX  -> RXD\n"
    " 14 RX  -> TXD\n"
    " 18 GND -> GND\n"
    "NRST can be left\n"
    "unconnected.\n"
    "\n"
    "\e#Chat\n"
    "OK: write a message\n"
    "Hold OK: recipient\n"
    "  (0 = everyone)\n"
    "Up/Down: scroll\n"
    "Left/Right: page\n"
    "\n"
    "Both ends need the same\n"
    "network ID, frequency,\n"
    "SF/BW/CR, preamble and\n"
    "password.\n"
    "\n"
    "Sent means transmitted:\n"
    "LoRa has no delivery\n"
    "receipts.";

void loratalk_scene_about_on_enter(void* context) {
    LoRaTalkApp* app = context;
    widget_reset(app->widget);
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, about_text);
    view_dispatcher_switch_to_view(app->view_dispatcher, LoRaTalkViewWidget);
}

bool loratalk_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void loratalk_scene_about_on_exit(void* context) {
    LoRaTalkApp* app = context;
    widget_reset(app->widget);
}
