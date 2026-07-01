CC      = gcc
CFLAGS  = -I../src/c -I../src/c/ui -I../src/c/protocol -I. -Ifakes \
          -I$(UNITY_PATH)/src \
          -DUNITY_INCLUDE_DOUBLE -Wall -Wextra -std=c11
SRC     = ../src/c/ui/dummy.c
FAKES   = fakes/pebble_sdk_fake.c
UNITY   = $(UNITY_PATH)/src/unity.c
TESTS   = test_dummy.c test_main.c

test:
	$(CC) $(CFLAGS) $(UNITY) $(FAKES) $(SRC) $(TESTS) -o test_runner
	./test_runner

clean:
	rm -f test_runner

.PHONY: test clean
