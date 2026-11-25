#pragma once

#include "../loratalk_history.h"

#include <gui/view.h>

typedef struct ChatView ChatView;

typedef enum {
    ChatViewEventCompose, /**< OK: write a message */
    ChatViewEventDestination, /**< Long OK: change who messages go to */
} ChatViewEvent;

typedef void (*ChatViewCallback)(ChatViewEvent event, void* context);

ChatView* chat_view_alloc(LoRaTalkHistory* history);
void chat_view_free(ChatView* chat);
View* chat_view_get_view(ChatView* chat);

void chat_view_set_callback(ChatView* chat, ChatViewCallback callback, void* context);
void chat_view_set_destination(ChatView* chat, uint16_t destination);

/** Show a short notice in the header (e.g. a send error) until the next key press. */
void chat_view_set_notice(ChatView* chat, const char* notice);

/** Call after the history changed; scrolls back to the newest message. */
void chat_view_refresh(ChatView* chat);
