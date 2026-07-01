# Pebble Watchapp Bootstrap Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Bootstrap a Pebble watchapp (C, SDK 4.3) for SimpleTimeTracker with a static dummy screen, a flake input pinning the upstream API source-of-truth, and a Unity test harness.

**Architecture:** A `pebbleEnv` dev shell drives `pebble build` for the watch binary and `make` for host-side Unity tests. Two non-flake inputs (`simpletimetracker`, `unity`) are fetched as plain source trees via `flake = false`. The watch app is a single window with a TextLayer; `protocol/keys.h` is fully populated now so future RPC work doesn't touch it.

**Tech Stack:** Pebble SDK 4.3 (C), `pebble-tool` 5.0.x, `pebble-toolchain-bin` 4.9.169 (ARM cross-compiler), Unity test framework, Nix flakes.

## Global Constraints

- Pebble SDK version is `4.3` (the version `pebble.nix` fetches and what `buildPebbleApp.nix` hardcodes as `SDKs/4.3`).
- Project manifest is `package.json` (modern SDK 4 format with a `pebble` key) — NOT legacy `appinfo.json`.
- Source files live under `src/c/` (the `wscript` globs `src/c/**/*.c`).
- `pebble` and `make` are available inside `nix develop` (the dev shell).
- `flake = false` for inputs whose repos have no `flake.nix` (Unity, simpletimetracker).
- No comments in code unless explicitly requested.
- C standard: C11 (host tests); the Pebble toolchain has its own settings.

---

### Task 1: Add non-flake inputs and update dev shell

**Files:**
- Modify: `flake.nix`
- Modify: `flake.lock` (auto-updated by `nix flake lock`)

**Interfaces:**
- Produces: flake inputs `simpletimetracker` (non-flake, `outPath` available) and `unity` (non-flake, `outPath` available); dev shell gains `gcc` + `gnumake`; flake exposes `apps.test`.

- [ ] **Step 1: Edit `flake.nix` to add inputs**

Replace the entire `inputs` block and `outputs` function. The new flake adds `simpletimetracker` and `unity` as non-flake inputs, adds `gcc`/`gnumake` to the dev shell, and defines `apps.test`.

```nix
{
  description = "Pebble watchapp + for SimpleTimeTracker";

  inputs = {
    pebble.url = "github:pebble-dev/pebble.nix";
    flake-utils.url = "github:numtide/flake-utils";
    simpletimetracker = {
      url = "github:mithodin/Android-SimpleTimeTracker/feature/pebble-integration";
      flake = false;
    };
    unity = {
      url = "github:ThrowTheSwitch/Unity";
      flake = false;
    };
  };

  outputs =
    {
      pebble,
      flake-utils,
      nixpkgs,
      unity,
      ...
    }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs {
          inherit system;
        };
      in
      {
        devShells.default = pebble.pebbleEnv.${system} {
          packages = with pkgs; [
            gcc
            gnumake
          ];
        };

        apps.test = {
          type = "app";
          program = "${pkgs.writeShellScript "pepple-sst-test-runner" ''
            export UNITY_PATH=${unity.outPath}
            make -C tests test
          ''}";
        };
      });
}
```

- [ ] **Step 2: Lock the new inputs**

Run:
```bash
nix flake lock
```
Expected: `flake.lock` gains `simpletimetracker` and `unity` nodes with `"flake": false`. No errors.

- [ ] **Step 3: Verify the dev shell and inputs resolve**

Run:
```bash
nix develop --command sh -c 'echo gcc=$(which gcc) make=$(which make) pebble=$(which pebble)'
nix eval .#apps.test.program --raw
```
Expected: prints paths for `gcc`, `make`, `pebble`, and a store path for the test runner script. No errors.

- [ ] **Step 4: Verify the non-flake inputs are fetchable**

Run:
```bash
nix build --no-link --print-out-paths .#inputs.simpletimetracker.outPath 2>/dev/null || true
ls "$(nix eval .#inputs.simpletimetracker.outPath --raw)/features/feature_pebble/src/main/java/com/example/util/simpletimetracker/feature_pebble/PebbleRequests.kt"
ls "$(nix eval .#inputs.unity.outPath --raw)/src/unity.c"
```
Expected: `PebbleRequests.kt` and `unity.c` exist at those paths. No "file not found".

- [ ] **Step 5: Commit**

```bash
git add flake.nix flake.lock
git commit -m "flake: add simpletimetracker + unity non-flake inputs, test runner

simpletimetracker pins the feature/pebble-integration branch as the
API source-of-truth (read-only reference for protocol/keys.h sync).
unity provides the test framework, fetched as a plain source tree.
gcc/gnumake added to the dev shell for host-side test compilation."
```

---

### Task 2: Create the Pebble project files (package.json, wscript, .gitignore)

**Files:**
- Create: `package.json`
- Create: `wscript`
- Modify: `.gitignore`

**Interfaces:**
- Produces: a valid Pebble project root that `pebble build` can compile. `package.json` carries the UUID, `sdkVersion: "4.3"`, `targetPlatforms`, and `watchapp: { watchface: false }`.

- [ ] **Step 1: Generate a UUID for the app**

Run:
```bash
python3 -c 'import uuid; print(uuid.uuid4())'
```
Copy the output — it goes into `package.json` in the next step.

- [ ] **Step 2: Write `package.json`**

Use the UUID from Step 1. This mirrors the official `pebble new-project` template at `pebble_tool/sdk/templates/app/package.json`, with `sdkVersion: "4.3"` and `messageKeys: []` (empty — keys are defined in `src/c/protocol/keys.h`, not duplicated here until AppMessage lands).

```json
{
  "name": "pepple-sst",
  "author": "lucas",
  "version": "1.0.0",
  "keywords": ["pebble-app"],
  "private": true,
  "dependencies": {},
  "pebble": {
    "displayName": "SimpleTimeTracker",
    "uuid": "<UUID FROM STEP 1>",
    "sdkVersion": "4.3",
    "enableMultiJS": true,
    "targetPlatforms": [
      "aplite",
      "basalt",
      "chalk",
      "diorite",
      "emery"
    ],
    "watchapp": {
      "watchface": false
    },
    "messageKeys": [],
    "resources": {
      "media": []
    }
  }
}
```

- [ ] **Step 3: Write `wscript`**

Copy verbatim from the official template at `pebble_tool/sdk/templates/app/wscript` (this globs `src/c/**/*.c` per platform and bundles the `.pbw`):

```python
#
# This file is the default set of rules to compile a Pebble application.
#
# Feel free to customize this to your needs.
#
import os.path

top = '.'
out = 'build'


def options(ctx):
    ctx.load('pebble_sdk')


def configure(ctx):
    """
    This method is used to configure your build. ctx.load(`pebble_sdk`) automatically configures
    a build for each valid platform in `targetPlatforms`. Platform-specific configuration: add your
    change after calling ctx.load('pebble_sdk') and make sure to set the correct environment first.
    Universal configuration: add your change prior to calling ctx.load('pebble_sdk').
    """
    ctx.load('pebble_sdk')


def build(ctx):
    ctx.load('pebble_sdk')

    build_worker = os.path.exists('worker_src')
    binaries = []

    cached_env = ctx.env
    for platform in ctx.env.TARGET_PLATFORMS:
        ctx.env = ctx.all_envs[platform]
        ctx.set_group(ctx.env.PLATFORM_NAME)
        app_elf = '{}/pebble-app.elf'.format(ctx.env.BUILD_DIR)
        ctx.pbl_build(source=ctx.path.ant_glob('src/c/**/*.c'), target=app_elf, bin_type='app')

        if build_worker:
            worker_elf = '{}/pebble-worker.elf'.format(ctx.env.BUILD_DIR)
            binaries.append({'platform': platform, 'app_elf': app_elf, 'worker_elf': worker_elf})
            ctx.pbl_build(source=ctx.path.ant_glob('worker_src/c/**/*.c'),
                          target=worker_elf,
                          bin_type='worker')
        else:
            binaries.append({'platform': platform, 'app_elf': app_elf})
    ctx.env = cached_env

    ctx.set_group('bundle')
    ctx.pbl_bundle(binaries=binaries,
                   js=ctx.path.ant_glob(['src/pkjs/**/*.js',
                                         'src/pkjs/**/*.json',
                                         'src/common/**/*.js']),
                   js_entry_file='src/pkjs/index.js')
```

- [ ] **Step 4: Update `.gitignore`**

Append Pebble build artifacts and the test binary to the existing `.gitignore`:

```
build/
dist/
dist.zip
.lock-waf*
node_modules/
tests/test_runner
```

- [ ] **Step 5: Commit**

```bash
git add package.json wscript .gitignore
git commit -m "scaffold: pebble project files (package.json, wscript)

Modern SDK 4 format: package.json with a 'pebble' key (not legacy
appinfo.json). wscript globs src/c/**/*.c per platform and bundles
the .pbw. UUID generated for the watchapp."
```

---

### Task 3: Write `protocol/keys.h` (mirrors PebbleRequests.kt)

**Files:**
- Create: `src/c/protocol/keys.h`

**Interfaces:**
- Produces: `KEY_METHOD_ID`, `METHOD_*` constants (all 13), `KEY_TOTAL_ITEMS`/`KEY_OFFSET`/`KEY_RETURNED_COUNT`/`KEY_LIMIT`/`KEY_ITEMS_START`, `INBOX_BUDGET_BYTES`. All `#define`s — unused ones cost nothing and prevent drift with upstream.

- [ ] **Step 1: Write `keys.h`**

Mirror the constants from `PebbleRequests.kt` exactly (values verified against `features/feature_pebble/.../PebbleRequests.kt` on the `feature/pebble-integration` branch). The header comment documents the sync procedure.

```c
#ifndef PEPPEL_SST_PROTOCOL_KEYS_H
#define PEPPEL_SST_PROTOCOL_KEYS_H

/*
 * AppMessage key constants mirroring PebbleRequests.kt from
 * mithodin/Android-SimpleTimeTracker (branch: feature/pebble-integration).
 *
 * Sync procedure: diff this file against
 *   features/feature_pebble/src/main/java/com/example/util/simpletimetracker/feature_pebble/PebbleRequests.kt
 * in the simpletimetracker flake input (nix eval .#inputs.simpletimetracker.outPath).
 * Method ids and pagination keys must match exactly.
 */

#define KEY_METHOD_ID        0u

/* Method ids (mirror WearableRequests paths / PebbleRequests.kt) */
#define METHOD_QUERY_ACTIVITIES                  1u
#define METHOD_QUERY_CURRENT_ACTIVITIES          2u
#define METHOD_QUERY_STATISTICS                  3u
#define METHOD_START_ACTIVITY                    4u
#define METHOD_STOP_ACTIVITY                     5u
#define METHOD_REPEAT_ACTIVITY                   6u
#define METHOD_QUERY_TAGS_FOR_ACTIVITY           7u
#define METHOD_QUERY_SHOULD_SHOW_TAG_SELECTION   8u
#define METHOD_QUERY_SHOULD_SHOW_TAG_VALUE_SELECTION 9u
#define METHOD_QUERY_SETTINGS                   10u
#define METHOD_SET_SETTINGS                     11u
#define METHOD_OPEN_PHONE_APP                   12u
#define METHOD_DATA_UPDATED                     13u

/* Pagination keys (in request and response) */
#define KEY_TOTAL_ITEMS     1u
#define KEY_OFFSET          2u
#define KEY_RETURNED_COUNT 3u
#define KEY_ITEMS_START     4u

/* Key 3 is dual-purpose: requested limit (UInt8) in paginated requests,
 * returned count (UInt8) in paginated responses. */
#define KEY_LIMIT           3u

/* Byte budget for response payload (APP_MESSAGE_INBOX_SIZE_MINIMUM) */
#define INBOX_BUDGET_BYTES  124

#endif /* PEPPEL_SST_PROTOCOL_KEYS_H */
```

- [ ] **Step 2: Verify it cross-checks against the source of truth**

Run:
```bash
STT="$(nix eval .#inputs.simpletimetracker.outPath --raw)"
grep -E 'const val (QUERY_|START_|STOP_|REPEAT_|DATA_|OPEN_|SET_|KEY_|INBOX_BUDGET)' \
  "$STT/features/feature_pebble/src/main/java/com/example/util/simpletimetracker/feature_pebble/PebbleRequests.kt"
```
Expected: the printed Kotlin constants match the `#define` values in `keys.h`. No mismatches.

- [ ] **Step 3: Commit**

```bash
git add src/c/protocol/keys.h
git commit -m "protocol: keys.h mirroring PebbleRequests.kt

All 13 method ids + pagination keys + inbox budget, mirroring the
upstream feature/pebble-integration branch. Fully populated now so
future RPC work doesn't touch this file. Header documents the sync
procedure against the simpletimetracker flake input."
```

---

### Task 4: Write the dummy screen and main.c

**Files:**
- Create: `src/c/ui/dummy.h`
- Create: `src/c/ui/dummy.c`
- Create: `src/c/main.c`

**Interfaces:**
- Produces: `dummy_push()` (creates + pushes the dummy window), `dummy_pop()` (destroys it). `main()` calls `dummy_push()` then `app_event_loop()`.

- [ ] **Step 1: Write `dummy.h`**

```c
#ifndef PEPPEL_SST_UI_DUMMY_H
#define PEPPEL_SST_UI_DUMMY_H

void dummy_push(void);
void dummy_pop(void);

#endif /* PEPPEL_SST_UI_DUMMY_H */
```

- [ ] **Step 2: Write `dummy.c`**

A static Window + TextLayer. The load handler creates the TextLayer with a title and a "TODO" body. Modeled on the official `app/main.c` template structure.

```c
#include <pebble.h>
#include "dummy.h"

static Window *s_window;
static TextLayer *s_title_layer;
static TextLayer *s_body_layer;

static void prv_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_title_layer = text_layer_create(GRect(0, 50, bounds.size.w, 30));
  text_layer_set_text(s_title_layer, "SimpleTimeTracker");
  text_layer_set_text_alignment(s_title_layer, GTextAlignmentCenter);
  text_layer_set_font(s_title_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  layer_add_child(window_layer, text_layer_get_layer(s_title_layer));

  s_body_layer = text_layer_create(GRect(0, 90, bounds.size.w, 20));
  text_layer_set_text(s_body_layer, "TODO");
  text_layer_set_text_alignment(s_body_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_body_layer));
}

static void prv_window_unload(Window *window) {
  text_layer_destroy(s_title_layer);
  text_layer_destroy(s_body_layer);
}

void dummy_push(void) {
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  const bool animated = true;
  window_stack_push(s_window, animated);
}

void dummy_pop(void) {
  window_destroy(s_window);
}
```

- [ ] **Step 3: Write `main.c`**

```c
#include <pebble.h>
#include "ui/dummy.h"

int main(void) {
  dummy_push();
  app_event_loop();
  dummy_pop();
}
```

- [ ] **Step 4: Build the watchapp**

Run inside `nix develop`:
```bash
nix develop --command pebble build
```
Expected: `'build' finished successfully` and `build/pepple-sst.pbw` exists. No compile errors.

- [ ] **Step 5: Commit**

```bash
git add src/c/ui/dummy.h src/c/ui/dummy.c src/c/main.c
git commit -m "feat: static dummy screen + main entry point

dummy_push() creates a Window with a 'SimpleTimeTracker' title and a
'TODO' body TextLayer. main() pushes it and enters the event loop.
Proves the toolchain compiles and links a .pbw end-to-end."
```

---

### Task 5: Write the Unity test harness and fakes

**Files:**
- Create: `tests/fakes/pebble_sdk_fake.h`
- Create: `tests/fakes/pebble_sdk_fake.c`
- Create: `tests/test_dummy.c`
- Create: `tests/test_main.c`
- Create: `tests/Makefile`

**Interfaces:**
- Consumes: `dummy_push()` from Task 4; Unity from the `unity` flake input (path via `$UNITY_PATH`).
- Produces: `make -C tests test` builds and runs `tests/test_runner`; `nix run .#test` does the same via the flake app.

- [ ] **Step 1: Write the fake Pebble SDK header**

This stubs the Window/TextLayer/Layer API surface used by `dummy.c`. It provides call counters + last-arg capture so tests can assert behavior without a real Pebble.

`tests/fakes/pebble_sdk_fake.h`:
```c
#ifndef PEBBLE_SDK_FAKE_H
#define PEBBLE_SDK_FAKE_H

#include <stdint.h>
#include <stdbool.h>

typedef struct Window Window;
typedef struct TextLayer TextLayer;
typedef struct Layer Layer;

typedef struct { int x, y, w, h; } GRect;
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
```

- [ ] **Step 2: Write the fake implementation**

`tests/fakes/pebble_sdk_fake.c`:
```c
#include "pebble_sdk_fake.h"
#include <string.h>
#include <stdlib.h>

static int s_window_create_count = 0;
static int s_window_destroy_count = 0;
static int s_window_stack_push_count = 0;
static char s_last_text[256] = {0};

struct Window { WindowHandlers handlers; };
struct TextLayer { char text[256]; };
struct Layer { int dummy; };

void fake_reset(void) {
  s_window_create_count = 0;
  s_window_destroy_count = 0;
  s_window_stack_push_count = 0;
  s_last_text[0] = '\0';
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
  GRect bounds = {0, 0, 144, 168};
  return bounds;
}

void layer_add_child(Layer *parent, Layer *child) {
  (void)parent;
  (void)child;
}

TextLayer *text_layer_create(GRect bounds) {
  (void)bounds;
  return calloc(1, sizeof(TextLayer));
}

void text_layer_destroy(TextLayer *layer) {
  free(layer);
}

void text_layer_set_text(TextLayer *layer, const char *text) {
  if (layer && text) {
    strncpy(s_last_text, text, sizeof(s_last_text) - 1);
    strncpy(layer->text, text, sizeof(layer->text) - 1);
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
  GFont f = {0};
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
```

- [ ] **Step 3: Write `test_dummy.c`**

One test: `dummy_push()` creates a window, pushes it onto the stack, and the load handler sets the title text.

`tests/test_dummy.c`:
```c
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
```

- [ ] **Step 4: Write `test_main.c` (Unity runner)**

`tests/test_main.c`:
```c
#include "unity.h"

void test_dummy_push_creates_and_pushes_window(void);
void test_dummy_sets_title_text_on_load(void);

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_dummy_push_creates_and_pushes_window);
  RUN_TEST(test_dummy_sets_title_text_on_load);
  return UNITY_END();
}
```

- [ ] **Step 5: Write `tests/Makefile`**

`UNITY_PATH` is passed in from the flake `apps.test` (or set manually for `nix develop`). Compiles all `src/c/ui/*.c` (excluding `main.c`), the fakes, Unity, and the tests.

```make
CC      = gcc
CFLAGS  = -I../src/c -I../src/c/ui -I../src/c/protocol -I. -Ifakes \
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
```

- [ ] **Step 6: Run the tests and verify they pass**

Run:
```bash
nix run .#test
```
Expected: Unity output with `2 Tests 0 Failures 0 Ignored` and exit code 0.

- [ ] **Step 7: Commit**

```bash
git add tests/
git commit -m "test: Unity harness + Pebble SDK fakes for dummy screen

Hand-rolled fakes for the Window/TextLayer/Layer API surface used by
src/c/ui/dummy.c, so host-side gcc can link and test the UI logic
without a Pebble. Two tests assert dummy_push creates+pushes a window
and sets the title text. make -C tests test or nix run .#test runs it."
```

---

### Task 6: Final verification

**Files:** none (verification only)

- [ ] **Step 1: Verify the watchapp still builds**

Run:
```bash
nix develop --command pebble build
```
Expected: `'build' finished successfully`. `build/pepple-sst.pbw` exists.

- [ ] **Step 2: Verify tests still pass**

Run:
```bash
nix run .#test
```
Expected: `2 Tests 0 Failures 0 Ignored`, exit 0.

- [ ] **Step 3: Verify the flake is clean**

Run:
```bash
nix flake check --no-build
```
Expected: no evaluation errors. (May warn about missing `overlay`/`packages` — that's fine, we don't expose those.)

- [ ] **Step 4: Verify git status is clean**

Run:
```bash
git status
```
Expected: nothing uncommitted except possibly `build/` (which is gitignored).

- [ ] **Step 5: Final commit if anything was missed**

If `git status` shows untracked files that should be committed (not `build/`), commit them. Otherwise, no commit needed — the work is complete.
