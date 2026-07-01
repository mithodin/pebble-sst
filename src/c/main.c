#include <pebble.h>
#include "ui/dummy.h"

int main(void) {
  dummy_push();
  app_event_loop();
  dummy_pop();
}
