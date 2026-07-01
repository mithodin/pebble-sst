# Pebble watchapp for SimpleTimeTracker — Bootstrap

## Goal

Bootstrap a Pebble watchapp (C, SDK 4.x) that will eventually act as a
client to the Android SimpleTimeTracker companion's `feature_pebble` RPC
server. This PR delivers only a static dummy screen plus the project
scaffold, a flake input pinning the API source-of-truth, and a Unity test
harness.

The eventual app scope (informed the scaffold, **not** built here):
view running timers, start/stop timers, repeat last activity.

## Non-goals (this PR)

- No AppMessage/RPC client logic.
- No real UI screens (running timers, picker).
- No state model, no pagination.
- No statistics, settings, or tag handling.

## Context

### Pebble SDK + toolchain

The project uses [`pebble-dev/pebble.nix`](https://github.com/pebble-dev/pebble.nix).
Two outputs matter:

- `pebbleEnv.${system}` — `mkShellNoCC` wrapping `pebble-tool`,
  `pebble-toolchain-bin` (ARM cross-compiler for the watch), `pebble-qemu`,
  `nodejs`. Used for `pebble build` / `pebble install` / `pebble emulate`.
- `buildPebbleApp.${system}` — a derivation that cross-compiles a `.pbw`
  via `pebble build` under a pre-seeded `~/.pebble-sdk/SDKs/4.3`. Intended
  for app-store bundling; **not used in this PR** because it asserts many
  app-store fields (banner, icons, screenshots) that aren't ready yet.

The dev shell (`flake.nix` → `devShells.default`) is what drives both
watch builds and host-side tests.

### Companion protocol (source-of-truth)

Pinned via a flake input so we can read the wire format directly from the
upstream branch rather than copy-paste constants:

```nix
inputs.simpletimetracker = {
  url = "github:mithodin/Android-SimpleTimeTracker/feature/pebble-integration";
  flake = false;
};
```

`flake = false` because the upstream repo has no `flake.nix` — Nix fetches
it as a plain source tree (available via `outPath`), per the [Nix flake
input docs](https://nixos.org/manual/nix/stable/command-ref/new-cli/nix3-flake.html#flake-inputs).
GitHub inputs are fetched as tarballs (not full clones), so it's already
cheap. The input is a build/dev-time reference only; it is **not** linked
into the watch binary. It exists so that:

1. Future work can generate `protocol/keys.h` from the Kotlin source.
2. Reviewers can `nix flake lock` and `nix run .#inspect-api` (or just
   `nix shell` and `rg`) to cross-check field counts against the latest
   upstream commit without leaving the repo.

The relevant upstream files (read during this design, to be re-read on
sync):

| Upstream file | Role |
|---|---|
| `features/feature_pebble/.../PebbleRequests.kt` | Method IDs + pagination key constants |
| `features/feature_pebble/.../PebbleDictionaryMapper.kt` | Wire format: tuple layout per method, field counts per item |
| `features/feature_pebble/.../PebbleRPCServer.kt` | Server dispatch (mirrors method ids; documents ack/nack semantics) |
| `wearable_api/.../WearableDTO.kt` | Data shapes (activities, current state, statistics, tags, settings) |

### Wire protocol summary (for the eventual client)

- AppMessage dictionaries keyed by `UInt`.
- `KEY_METHOD_ID = 0` carries a `UInt8` method id.
- Paginated responses use `KEY_TOTAL_ITEMS=1`, `KEY_OFFSET=2`,
  `KEY_RETURNED_COUNT=3` (key 3 is dual-purpose: requested `LIMIT` in
  requests, returned count in responses), `KEY_ITEMS_START=4`.
- Inbox budget is `APP_MESSAGE_INBOX_SIZE_MINIMUM = 124` bytes; the server
  packs as many items as fit using `sizeInBytes` estimation.
- The server can only Ack/Nack inline; query responses arrive as separate
  pushes keyed by the same `KEY_METHOD_ID` — the client correlates by
  method id.
- `DATA_UPDATED` (13) is a server-initiated push; the watch refreshes on
  receipt.

Method ids in scope for the eventual app:
`QUERY_ACTIVITIES=1`, `QUERY_CURRENT_ACTIVITIES=2`, `START_ACTIVITY=4`,
`STOP_ACTIVITY=5`, `REPEAT_ACTIVITY=6`, `DATA_UPDATED=13`.

## Architecture

```
pepple-sst/
├── flake.nix                 # pebbleEnv + Unity + simpletimetracker input
├── package.json              # Pebble manifest (modern SDK 4 format)
├── wscript                   # waf build script (globs src/c/**/*.c)
├── src/
│   └── c/
│       ├── main.c           # pebble_main: init/deinit, push dummy window
│       ├── ui/
│       │   ├── dummy.c      # static Window + TextLayer
│       │   └── dummy.h
│       └── protocol/
│           └── keys.h       # mirrors PebbleRequests.kt (full set, future-proof)
├── tests/
│   ├── Makefile             # host gcc: links Unity + fakes + src (minus main.c)
│   ├── test_dummy.c         # asserts dummy_push creates window + text
│   ├── test_main.c          # Unity runner (UNITY_BEGIN/END)
│   └── fakes/
│       ├── pebble_sdk_fake.c   # stubs for Window/TextLayer/Layer API
│       └── pebble_sdk_fake.h
└── docs/superpowers/specs/  # this design
```

### Why this layout

- **`src/protocol/keys.h`** is the single source of truth mirroring
  `PebbleRequests.kt`. Populated fully now (all 13 method ids + pagination
  keys + budget) so follow-up RPC work doesn't touch it. Unused constants
  are fine — they're `#define`s, cost nothing, and prevent drift.
- **`src/ui/`** separated from `src/main.c` so each future screen
  (`running.c`, `picker.c`) gets its own file. `main.c` stays small and
  owns only app lifecycle + root window selection.
- **`tests/fakes/`** is the seam: `pebble_sdk_fake.c` stubs the Pebble API
  surface used by `src/` so tests can link against `src/*.c` on the host
  with plain `gcc`. The fake surface grows with the codebase; for this PR
  it's ~6 functions (`window_create`, `window_destroy`, `window_stack_push`,
  `text_layer_create`, `text_layer_set_text`, `layer_add_child`) plus call
  counters + last-arg capture.

### Test harness: Unity + hand-rolled fakes

Chosen over Ceedling (Ruby dep, awkward under Nix), cmocka (more verbose),
and Criterion (overkill). Unity is the Pebble-community standard, is a
single `.c`/`.h` pair, and the Pebble mock surface is small enough that
hand-rolled fakes are clearer than auto-generated mocks.

Unity is vendored via a flake input (not committed to the repo):

```nix
inputs.unity = {
  url = "github:ThrowTheSwitch/Unity";
  flake = false;
};
```

The test runner is a plain `Makefile` under `tests/`:

```make
# tests/Makefile
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
```

`UNITY_PATH` is passed in from the flake's `test` app so the path to the
pinned Unity source is controlled by Nix, not hard-coded.

### Flake outputs

```nix
{
  devShells.default = pebble.pebbleEnv.${system} {
    packages = with pkgs; [ gcc gnumake ];
  };

  apps.test = {
    type = "app";
    program = "${pkgs.writeShellScript "test-runner" ''
      export UNITY_PATH=${unity.outPath}
      make -C tests test
    ''}";
  };
}
```

`gcc` + `gnumake` are added to the dev shell so `make -C tests test` works
inside `nix develop`; the `apps.test` entry lets CI run `nix run .#test`.
Both `simpletimetracker` and `unity` are non-flake inputs, accessed via
`outPath` in the outputs function.

## Deliverable (this PR)

1. **`flake.nix`** — add `simpletimetracker` + `unity` non-flake inputs (`flake = false`);
   add `gcc`/`gnumake` to the dev shell; add `apps.test`.
2. **`package.json`** — modern Pebble manifest (SDK 4 format: top-level
   `name`/`version`/`author` + a `pebble` object). The `pebble` object
   contains: generated `uuid`, `sdkVersion: "4.3"`, `targetPlatforms`
   (all platforms), `watchapp: { watchface: false }`, `messageKeys: []`.
   (The legacy `appinfo.json` format is for SDK 2/3; modern C projects use
   `package.json` — verified in `pebble_tool/sdk/project.py`:
   `NpmProject` parses `package.json`'s `pebble` key for all project types,
   and `pebble new-project` scaffolds `package.json` for C projects.)
3. **`wscript`** — waf build script (from the official `app/wscript`
   template): globs `src/c/**/*.c` per platform, bundles into `.pbw`.
4. **`src/c/main.c`** — `main()` calling `dummy_push()` then
   `app_event_loop()`.
5. **`src/c/ui/dummy.c` / `dummy.h`** — `dummy_push()` creates a `Window`,
   adds a `TextLayer` with "SimpleTimeTracker" title + "TODO" body, pushes
   onto the window stack.
6. **`src/c/protocol/keys.h`** — all 13 method ids + pagination keys + inbox
   budget, with a header comment pointing at `PebbleRequests.kt` and noting
   the sync procedure.
7. **`tests/`** — `test_main.c`, `test_dummy.c`, `fakes/pebble_sdk_fake.{c,h}`,
   `Makefile`. One test: `dummy_push` calls `window_stack_push` and sets the
   text layer text.
8. **`.gitignore`** — `build/`, `*.pbw`, `test_runner`.

## Testing

- `nix develop` then `pebble build` → must produce `build/*.pbw`.
- `nix run .#test` → Unity runs the dummy test, exits 0.
- No Pebble emulator assertions in this PR (the dummy screen has no
  behavior to assert beyond window creation, which the host test covers).

## Future work (out of scope, listed for context)

- `src/rpc.c/h` — AppMessage client: `app_message_open`, outbox send,
  inbox receive, correlation by method id, pagination fetching.
- `src/state.c/h` — in-memory model: activities list, current timers,
  last record for repeat.
- `src/ui/running.c`, `src/ui/picker.c` — real screens.
- Generate `protocol/keys.h` from `simpletimetracker` flake input at build
  time (replacing the manual sync step).
- Fakes for `app_message_*` so RPC decode logic gets host tests.

## Open questions

None — scope and harness choice are settled.
