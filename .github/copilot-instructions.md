# Project Guidelines

**kfc_fw** is a plugin-based ESP8266/ESP32 firmware (PlatformIO, C++17) with a WebUI, an AT-command
console, MQTT/Home Assistant integration and an optional LVGL touch UI. These rules apply to every task
in this workspace.

- Main entry point: `src/kfc_firmware.cpp` (`setup()` / `loop()`); plugins under `src/plugins/**`.
- Build configuration: `platformio.ini` + `conf/**` (one file per env in `conf/envs/`); build scripts in `scripts/`.
- WebUI sources: `Resources/**`, built by the Python web builder (`lib/KFCLibrary/KFCWebBuilder/kfc_webbuilder`,
  config `KFCWebBuilder.yaml`) into `data/webui/**`.
- Feature overview in [README.md](../README.md), recent changes in [CHANGELOG.md](../CHANGELOG.md).

## Topic files

The rules of a specific area live in `.github/instructions/`. Read the matching file before working in that
area - its rules apply like the ones in this file:

- [`build-system`](instructions/build-system.instructions.md): `platformio.ini`, `conf/**`, `scripts/**` -
  library declarations and `lib_ignore`, compiler flags, clean builds, the build number, `compiledb`.
- [`plugins`](instructions/plugins.instructions.md): adding or changing a plugin - registration, AT commands
  versus the status page.
- [`config`](instructions/config.instructions.md): a config struct or parameter (`include/kfc_fw_config/**`) -
  a layout change resets the parameter once, by design; NVS.
- [`webui`](instructions/webui.instructions.md): `Resources/**`, the web builder and its preprocessor, help
  texts, the hashed files in `data/webui/`, server-rendered forms.
- [`device-testing`](instructions/device-testing.instructions.md): flashing and testing a device - serial
  port and logs, the PC tools, resets, WebUI login.
- [`esp8266-test-env`](instructions/esp8266-test-env.instructions.md): the `led_strip_xmas_serial` test env,
  the WebUI test suite, heap and leak diagnostics.
- [`crash-debugging`](instructions/crash-debugging.instructions.md): a crashed device - ESP32 core dumps,
  ESP8266 exceptions, build archives (`-t buildarchive`).
- [`clock-leds`](instructions/clock-leds.instructions.md): the clock plugin (`src/plugins/clock/**`) - pixel
  type and color math, LED power after a crash, the NeoPixelBus RMT/I2S transport of
  `wled_esp32_controller_neopixelbus`.
- [`lvgl`](instructions/lvgl.instructions.md): the LVGL touch UI (`src/plugins/lvgl/**`) - widget threading,
  LVGL 8.4 traps, screenshots over HTTP.

## Conventions

- **American English** in code, comments, UI labels and docs: `color`, not `colour`.
- **The build decides, not the IDE squiggles** - verify a change with a real compile.
- **No option without an effect**: a form field, AT command, MQTT entity or WebUI control must be compiled out
  (`#if`) in builds that cannot act on it, and a config field needs a reader. Keep the config bit itself when other
  builds use it, so the stored layout stays the same.
- **Show a design/icon preview before implementing UI work** and wait for the approval.
- The firmware is **C++17** (`-std=gnu++17` in `conf/common.ini`). Match the surrounding style; the repo's
  `.clang-format` is the reference (WebKit base, 4 spaces, no tabs).
- **Initialize members in the constructor, not in the class body** - no in-class default member
  initializers (`uint8_t _x{0};`). Write the constructor **inline in the header** when the type has none in a
  `.cpp`; a type that already has one in a `.cpp` keeps it there. Keep the initializer list in declaration
  order, and do not list a member twice
- Both platforms use the **stock** PlatformIO Arduino cores (`espressif8266`, `espressif32`) again - only
  upstream APIs are available, no fork-only `String` helpers (`rtrim/ltrim`, `*IgnoreCase`, `startsWith(char)`,
  chained `trim()/toLowerCase()`). Use the project's `StrView`/`StrWrapper`
  (`lib/KFCLibrary/KFCBaseLibrary/include/StrView.h`) instead, and put the flash string on the left:
  `F("...") == str`, never `str == F("...")`.
- **Changelog with every change**: `CHANGELOG.md`, an entry under today's date (`### YYYY-MM-DD`, newest first,
  inside `## Unreleased (<version>)`), under Added, Changed, Fixed or Removed, saying what a user or developer
  sees. `* ` bullets wrapped at 100 columns. A change that reverses an entry of the same day replaces it
  instead of adding a second one. The owner maintains the wording.
- **Report every change to an external library** to the developer, naming the library and the files: the
  junctions to separate repos (`lib/KFCLibrary`, `lib/ESPAsyncWebServer`, `lib/NeoPixelEspEx`, `lib/libcrc16` -
  committed and pushed separately), the patched core copy `lib/ESP8266mDNS`, and anything under `.pio/libdeps/`
  or the PlatformIO packages (overwritten on the next install - the fix belongs into the library's repo or fork).

## Build, flash and verify

Before flashing, logging or testing a device, read
[`device-testing`](instructions/device-testing.instructions.md).

- `pio run -e <env> -t upload` builds and flashes in one step (`upload` runs the build first). No separate
  `pio run` step is needed.
- **The serial port is the env's `monitor_port`** (`conf/envs/*.ini`) - never a hardcoded COM port. When testing
  an env whose `monitor_port` is missing, **ask the user which port to use** before flashing or opening a monitor.
- **Serial only, no OTA**: `wled_esp32_controller`, `wled_esp32_controller_neopixelbus` and `wt32_sc01_test1` (the
  partition table has a single `factory` app partition and no OTA slots) and `led_strip_xmas_serial`.
- Shared/core changes must be built for **both platforms**, e.g. `pio run -e led_strip_xmas_serial` (the ESP8266
  test env) next to the ESP32 env - a change can compile on one and fail on the other.
- **There are no unit tests** - verification happens on the hardware: a trace line, a log or a screenshot has
  to show what the change did, "it compiles" is not a verification.
- **Device tests**: never save a form, change brightness/volume, WiFi, passwords or accounts, never start an
  LED/motor/sound action - send invalid form POSTs only (every real field missing, the firmware discards them).
  MQTT publishing is allowed on the two test boards only. Never send hand-made mDNS packets to the multicast
  group: one malformed packet rebooted every unpatched ESP8266 on the LAN.

## Filesystem and the WebUI

- **The WebUI is a separate image**: help texts and pages need `-t uploadfs` (or `-t buildfs`), a plain `pio run` does not
  rebuild them. Both build the WebUI into `data/` before the image is packed, skipped if no source changed;
  `-t rebuildfs` forces the WebUI build (without packing the image).
- **`-t buildfs` does not compile the firmware** - always run a plain `pio run -e <env>` as well.
- **`-t uploadfs` formats the filesystem.** Everything that was uploaded at runtime is gone afterwards.

## Architecture

- **Which plugins compile is decided by the build, not at runtime**: `conf/common.ini`
  `build_src_filter` excludes all of `src/plugins/` and each env re-adds the folders it wants, plus `-D`
  feature flags. A plugin that no env adds is never compiled - check before editing it.
- **The main loop is the single writer for display/UI state.** Work requested from another task
  (AsyncTCP/WebSocket, MQTT, HTTP, IR, buttons, AT) is queued with `TaskQueue`
  (`lib/KFCLibrary/KFCEventScheduler`, docs in `lib/KFCLibrary/docs/KFCEventScheduler/`) and drained
  from `loop()`. Naming rule: `...Deferred()` may be called from any task (it queues), `...Queued()` is
  queue-only, no suffix runs in the loop task and may be called directly from loop-task code.
- **LVGL and `FastLED.show()` are not re-entrant**: only the display-owning task may call them (LVGL details
  in the `lvgl` topic file).

## Pitfall checklist

Recurring bug classes - check these before blaming a change:

- **ESP32 core macros**: `DEFAULT`, `DISABLED`, `ON`, `OFF`, ... are `#define`s - an enum member with
  such a name breaks every TU that includes `Arduino.h`. Prefix enumerators.
- On ESP32 `F()/PSTR()/PROGMEM` are no-ops and `F("...")` does not convert to `const char*`; on ESP8266
  they move literals to flash (plain literals cost RAM there). Use `PSTR()` only together with the `_P`
  readers (`snprintf_P`, `strncmp_P`, ...).
- A NULL `%s` argument crashes newlib; plain appends use `+=`/`concat`, not a `printf` family call.
- **ESP8266 has no C++ exceptions**: a failed `new` aborts the device (`Unhandled C++ exception: OOM`) and the SDK
  crashes in the WiFi code when a beacon cannot be allocated. Check `ESP.getFreeHeap()`/`getMaxFreeBlockSize()`
  before large allocations and answer 503 instead (see the web server guards).
- **Unsigned `millis()` arithmetic**: use a signed (clamped) difference - a timestamp written later in
  the same tick underflows and expires a timeout instantly.
- Deeper dated findings (crash analyses, vendor-landmine details, measured numbers) are in the agent
  memory files `/memories/repo/*.md` - read them before a risky change.
