#ifndef PEBBLE_SDK_FAKE_H
#define PEBBLE_SDK_FAKE_H

#include <stdint.h>
#include <stdbool.h>

typedef struct Window Window;
typedef struct TextLayer TextLayer;
typedef struct Layer Layer;

typedef struct { int16_t x, y; } GPoint;
typedef struct { int16_t w, h; } GSize;
typedef struct { GPoint origin; GSize size; } GRect;
#define GRect(x, y, w, h) ((GRect){{(x), (y)}, {(w), (h)}})
#define GRectZero GRect(0, 0, 0, 0)
typedef enum { GTextAlignmentLeft, GTextAlignmentCenter, GTextAlignmentRight } GTextAlignment;
typedef struct { } GFont;

typedef struct {
  void (*load)(Window *);
  void (*unload)(Window *);
} WindowHandlers;

typedef void (*WindowHandler)(Window *);

void fake_reset(void);

Window *window_create(void);
void window_destroy(Window *window);
void window_set_window_handlers(Window *window, WindowHandlers handlers);
void window_stack_push(Window *window, bool animated);

Layer *window_get_root_layer(Window *window);
GRect layer_get_bounds(Layer *layer);
void layer_add_child(Layer *parent, Layer *child);

TextLayer *text_layer_create(GRect bounds);
void text_layer_destroy(TextLayer *layer);
void text_layer_set_text(TextLayer *layer, const char *text);
void text_layer_set_text_alignment(TextLayer *layer, GTextAlignment alignment);
void text_layer_set_font(TextLayer *layer, GFont font);
Layer *text_layer_get_layer(TextLayer *layer);

GFont fonts_get_system_font(int font_key);

void app_event_loop(void);

/* Fake inspection */
int fake_window_create_count(void);
int fake_window_destroy_count(void);
int fake_window_stack_push_count(void);
const char *fake_last_text_layer_text(void);

void fake_trigger_window_load(Window *window);
void fake_trigger_window_unload(Window *window);

#define FONT_KEY_GOTHIC_24_BOLD 0

#endif /* PEBBLE_SDK_FAKE_H */
