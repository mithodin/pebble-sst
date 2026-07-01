#include "pebble_sdk_fake.h"
#include <string.h>
#include <stdlib.h>

static int s_window_create_count = 0;
static int s_window_destroy_count = 0;
static int s_window_stack_push_count = 0;
static char s_last_text[256] = {0};
static TextLayer *s_first_text_layer = NULL;

struct Window { WindowHandlers handlers; };
struct TextLayer { char text[256]; };
struct Layer { int dummy; };

void fake_reset(void) {
  s_window_create_count = 0;
  s_window_destroy_count = 0;
  s_window_stack_push_count = 0;
  s_last_text[0] = '\0';
  s_first_text_layer = NULL;
}

Window *window_create(void) {
  s_window_create_count++;
  Window *w = calloc(1, sizeof(Window));
  return w;
}

void window_destroy(Window *window) {
  s_window_destroy_count++;
  free(window);
}

void window_set_window_handlers(Window *window, WindowHandlers handlers) {
  if (window) window->handlers = handlers;
}

void window_stack_push(Window *window, bool animated) {
  (void)animated;
  s_window_stack_push_count++;
  fake_trigger_window_load(window);
}

Layer *window_get_root_layer(Window *window) {
  (void)window;
  static Layer root = {0};
  return &root;
}

GRect layer_get_bounds(Layer *layer) {
  (void)layer;
  GRect bounds = GRect(0, 0, 144, 168);
  return bounds;
}

void layer_add_child(Layer *parent, Layer *child) {
  (void)parent;
  (void)child;
}

TextLayer *text_layer_create(GRect bounds) {
  (void)bounds;
  TextLayer *l = calloc(1, sizeof(TextLayer));
  if (!s_first_text_layer) s_first_text_layer = l;
  return l;
}

void text_layer_destroy(TextLayer *layer) {
  free(layer);
}

void text_layer_set_text(TextLayer *layer, const char *text) {
  if (layer && text) {
    strncpy(layer->text, text, sizeof(layer->text) - 1);
    if (layer == s_first_text_layer) {
      strncpy(s_last_text, text, sizeof(s_last_text) - 1);
    }
  }
}

void text_layer_set_text_alignment(TextLayer *layer, GTextAlignment alignment) {
  (void)layer;
  (void)alignment;
}

void text_layer_set_font(TextLayer *layer, GFont font) {
  (void)layer;
  (void)font;
}

Layer *text_layer_get_layer(TextLayer *layer) {
  (void)layer;
  static Layer l = {0};
  return &l;
}

GFont fonts_get_system_font(int font_key) {
  (void)font_key;
  GFont f;
  return f;
}

void app_event_loop(void) {
  /* no-op for tests */
}

int fake_window_create_count(void) { return s_window_create_count; }
int fake_window_destroy_count(void) { return s_window_destroy_count; }
int fake_window_stack_push_count(void) { return s_window_stack_push_count; }
const char *fake_last_text_layer_text(void) { return s_last_text; }

void fake_trigger_window_load(Window *window) {
  if (window && window->handlers.load) window->handlers.load(window);
}

void fake_trigger_window_unload(Window *window) {
  if (window && window->handlers.unload) window->handlers.unload(window);
}
