#include "chat_view.h"

#include <furi.h>
#include <gui/elements.h>

#include <stdio.h>
#include <string.h>

/*
 * Layout (128x64):
 *   +--------------------------------+
 *   |To: All                 -87dBm  |  header (inverted)
 *   |1234: hey, are you there?       |  incoming: "<from>: text", left aligned
 *   |                     yes, here! |  outgoing: right aligned
 *   |                  on my way ... |  " ..." while sending, " (!)" if failed
 *   +--------------------------------+
 * Up/Down scroll by a line, Left/Right by a page.
 */

#define HEADER_HEIGHT (11)
#define LINE_HEIGHT   (10)
#define BODY_TOP      (HEADER_HEIGHT + 1)
#define VISIBLE_LINES ((64 - BODY_TOP) / LINE_HEIGHT)
#define TEXT_WIDTH    (120)
#define RIGHT_EDGE    (123) /* leave room for the scrollbar */
#define MARGIN        (2)
#define LINE_BUF      (64)
#define MSG_BUF       (LORA_MAX_PAYLOAD + 24)

struct ChatView {
    View* view;
    ChatViewCallback callback;
    void* context;
};

typedef struct {
    LoRaTalkHistory* history;
    uint16_t destination;
    int16_t scroll; /* lines up from the bottom */
    int16_t max_scroll; /* computed while drawing */
    char notice[24];
} ChatViewModel;

/* Length of the next line of text that fits in max_width, breaking at spaces. */
static size_t wrap_next(Canvas* canvas, const char* text, uint16_t max_width) {
    size_t i = 0, last_space = 0;
    uint16_t width = 0;
    while(text[i]) {
        width += canvas_glyph_width(canvas, (uint8_t)text[i]);
        if(width > max_width) break;
        if(text[i] == ' ') last_space = i;
        i++;
    }
    if(!text[i]) return i; // everything fits
    if(i == 0) return 1; // a single glyph wider than the line
    return last_space ? last_space + 1 : i;
}

static void format_message(const LoRaTalkMessage* msg, char* out, size_t size) {
    if(msg->dir == LoRaTalkMessageIn) {
        snprintf(out, size, "%u: %s", msg->address, msg->text);
    } else {
        const char* suffix = "";
        if(msg->status == LoRaTalkMessageStatusSending) suffix = " ...";
        if(msg->status == LoRaTalkMessageStatusFailed) suffix = " (!)";
        snprintf(out, size, "%s%s", msg->text, suffix);
    }
}

static size_t count_lines(Canvas* canvas, const char* text) {
    size_t lines = 0;
    while(*text) {
        text += wrap_next(canvas, text, TEXT_WIDTH);
        lines++;
    }
    return lines ? lines : 1;
}

static void draw_header(Canvas* canvas, ChatViewModel* model, int16_t last_rssi, bool have_rssi) {
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, HEADER_HEIGHT);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);

    char buf[24];
    if(model->destination == 0) {
        snprintf(buf, sizeof(buf), "To: All");
    } else {
        snprintf(buf, sizeof(buf), "To: %u", model->destination);
    }
    canvas_draw_str(canvas, MARGIN, 9, buf);

    if(model->notice[0]) {
        canvas_draw_str_aligned(canvas, 126, 9, AlignRight, AlignBottom, model->notice);
    } else if(have_rssi) {
        snprintf(buf, sizeof(buf), "%ddBm", last_rssi);
        canvas_draw_str_aligned(canvas, 126, 9, AlignRight, AlignBottom, buf);
    }
    canvas_set_color(canvas, ColorBlack);
}

static void chat_view_draw(Canvas* canvas, void* _model) {
    ChatViewModel* model = _model;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);

    LoRaTalkHistory* history = model->history;
    loratalk_history_lock(history);
    size_t count = loratalk_history_count(history);

    // Most recent signal strength, for the header
    int16_t last_rssi = 0;
    bool have_rssi = false;
    for(size_t i = count; i > 0; i--) {
        const LoRaTalkMessage* msg = loratalk_history_get(history, i - 1);
        if(msg->dir == LoRaTalkMessageIn) {
            last_rssi = msg->rssi;
            have_rssi = true;
            break;
        }
    }

    draw_header(canvas, model, last_rssi, have_rssi);

    if(count == 0) {
        loratalk_history_unlock(history);
        model->max_scroll = 0;
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "No messages yet");
        canvas_draw_str_aligned(canvas, 64, 46, AlignCenter, AlignCenter, "OK: write  Hold OK: To");
        return;
    }

    char text[MSG_BUF];

    // Pass 1: total line count
    size_t total = 0;
    for(size_t i = 0; i < count; i++) {
        format_message(loratalk_history_get(history, i), text, sizeof(text));
        total += count_lines(canvas, text);
    }

    int16_t max_scroll = total > VISIBLE_LINES ? (int16_t)(total - VISIBLE_LINES) : 0;
    model->max_scroll = max_scroll;
    if(model->scroll > max_scroll) model->scroll = max_scroll;
    if(model->scroll < 0) model->scroll = 0;

    // First line index shown at the top of the body
    int32_t first = (int32_t)total - VISIBLE_LINES - model->scroll;
    if(first < 0) first = 0;

    // Pass 2: draw the visible lines
    int32_t line_index = 0;
    char line[LINE_BUF];
    for(size_t i = 0; i < count && line_index < first + VISIBLE_LINES; i++) {
        const LoRaTalkMessage* msg = loratalk_history_get(history, i);
        format_message(msg, text, sizeof(text));
        const char* p = text;
        do {
            size_t len = wrap_next(canvas, p, TEXT_WIDTH);
            if(line_index >= first && line_index < first + VISIBLE_LINES) {
                size_t copy = len < sizeof(line) - 1 ? len : sizeof(line) - 1;
                memcpy(line, p, copy);
                line[copy] = '\0';
                // Don't draw the space a line was broken at
                if(copy && line[copy - 1] == ' ') line[copy - 1] = '\0';
                int32_t y = BODY_TOP + (line_index - first) * LINE_HEIGHT + 8;
                if(msg->dir == LoRaTalkMessageIn) {
                    canvas_draw_str(canvas, MARGIN, y, line);
                } else {
                    canvas_draw_str_aligned(canvas, RIGHT_EDGE, y, AlignRight, AlignBottom, line);
                }
            }
            p += len;
            line_index++;
        } while(*p);
    }
    loratalk_history_unlock(history);

    if(max_scroll > 0) {
        elements_scrollbar_pos(
            canvas, 128, BODY_TOP, 64 - BODY_TOP, max_scroll - model->scroll, max_scroll + 1);
    }
}

static bool chat_view_input(InputEvent* event, void* context) {
    ChatView* chat = context;
    bool consumed = false;

    if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
        int16_t delta = 0;
        switch(event->key) {
        case InputKeyUp:
            delta = 1;
            break;
        case InputKeyDown:
            delta = -1;
            break;
        case InputKeyLeft:
            delta = VISIBLE_LINES;
            break;
        case InputKeyRight:
            delta = -VISIBLE_LINES;
            break;
        default:
            break;
        }
        if(delta) {
            with_view_model(
                chat->view,
                ChatViewModel * model,
                {
                    model->notice[0] = '\0';
                    model->scroll += delta;
                    if(model->scroll > model->max_scroll) model->scroll = model->max_scroll;
                    if(model->scroll < 0) model->scroll = 0;
                },
                true);
            consumed = true;
        }
    }

    if(event->key == InputKeyOk) {
        if(event->type == InputTypeShort) {
            if(chat->callback) chat->callback(ChatViewEventCompose, chat->context);
            consumed = true;
        } else if(event->type == InputTypeLong) {
            if(chat->callback) chat->callback(ChatViewEventDestination, chat->context);
            consumed = true;
        }
    }

    return consumed;
}

ChatView* chat_view_alloc(LoRaTalkHistory* history) {
    ChatView* chat = malloc(sizeof(ChatView));
    chat->callback = NULL;
    chat->context = NULL;
    chat->view = view_alloc();
    view_allocate_model(chat->view, ViewModelTypeLocking, sizeof(ChatViewModel));
    view_set_context(chat->view, chat);
    view_set_draw_callback(chat->view, chat_view_draw);
    view_set_input_callback(chat->view, chat_view_input);
    with_view_model(
        chat->view,
        ChatViewModel * model,
        {
            model->history = history;
            model->destination = 0;
            model->scroll = 0;
            model->max_scroll = 0;
            model->notice[0] = '\0';
        },
        false);
    return chat;
}

void chat_view_free(ChatView* chat) {
    view_free(chat->view);
    free(chat);
}

View* chat_view_get_view(ChatView* chat) {
    return chat->view;
}

void chat_view_set_callback(ChatView* chat, ChatViewCallback callback, void* context) {
    chat->callback = callback;
    chat->context = context;
}

void chat_view_set_destination(ChatView* chat, uint16_t destination) {
    with_view_model(
        chat->view, ChatViewModel * model, { model->destination = destination; }, true);
}

void chat_view_set_notice(ChatView* chat, const char* notice) {
    with_view_model(
        chat->view,
        ChatViewModel * model,
        { strlcpy(model->notice, notice ? notice : "", sizeof(model->notice)); },
        true);
}

void chat_view_refresh(ChatView* chat) {
    with_view_model(chat->view, ChatViewModel * model, { model->scroll = 0; }, true);
}
