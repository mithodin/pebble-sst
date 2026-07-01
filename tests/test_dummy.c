#include "unity.h"
#include "fakes/pebble_sdk_fake.h"
#include "ui/dummy.h"

void test_dummy_push_creates_and_pushes_window(void) {
  fake_reset();
  dummy_push();
  TEST_ASSERT_EQUAL_INT(1, fake_window_create_count());
  TEST_ASSERT_EQUAL_INT(1, fake_window_stack_push_count());
}

void test_dummy_sets_title_text_on_load(void) {
  fake_reset();
  dummy_push();
  TEST_ASSERT_EQUAL_STRING("SimpleTimeTracker", fake_last_text_layer_text());
}
