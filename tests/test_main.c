#include "unity.h"

void test_dummy_push_creates_and_pushes_window(void);
void test_dummy_sets_title_text_on_load(void);

void setUp(void) {}
void tearDown(void) {}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_dummy_push_creates_and_pushes_window);
  RUN_TEST(test_dummy_sets_title_text_on_load);
  return UNITY_END();
}
