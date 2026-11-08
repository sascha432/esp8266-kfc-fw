# Project Guidelines

**kfc_fw** is a plugin-based ESP8266/ESP32 firmware (PlatformIO, C++17) with a WebUI, an AT-command
console, MQTT/Home Assistant integration and an optional LVGL touch UI. These rules apply to every task
in this workspace.

- Main entry point: `src/kfc_firmware.cpp` (`setup()` / `loop()`); plugins under `src/plugins/**`.
- Build configuration: `platformio.ini` + `conf/**` (one file per env in `conf/envs/`); build scripts in `scripts/`.
  Shared library URLs are declared **once** (`conf/libs.ini`, LED libraries in `conf/envs/led_matrix.ini`) and
  referenced with `${name.lib_deps}` - never repeat a URL in an env.
- WebUI sources: `Resources/**`, built by the Python web builder (`lib/KFCLibrary/KFCWebBuilder/kfc_webbuilder`,
  config `KFCWebBuilder.yaml`) into `data/webui/**`.
- Feature overview in [README.md](../README.md), recent changes in [CHANGELOG.md](../CHANGELOG.md).

## Topic files

The rules of a specific area live in `.github/instructions/`. Read the matching file before working in that
area - its rules apply like the ones in this file:

- [`.github/instructions/build-system.instructions.md`](instructions/build-system.instructions.md):
  `platformio.ini`, `conf/**`, `scripts/**` - library declarations and `lib_ignore`, compiler flags and
  optimize pragmas, the build number, `compiledb`.
- [`.github/instructions/webui.instructions.md`](instructions/webui.instructions.md): `Resources/**`, the web
  builder and its preprocessor, help texts, the hashed files in `data/webui/`.
- [`.github/instructions/crash-debugging.instructions.md`](instructions/crash-debugging.instructions.md): a
  crashed device - ESP32 core dumps, ESP8266 exceptions, build archives (`-t buildarchive`).
- [`.github/instructions/esp8266-test-env.instructions.md`](instructions/esp8266-test-env.instructions.md): the
  `led_strip_xmas_serial` test env, the WebUI test suite, heap and leak diagnostics.
- [`.github/instructions/clock-leds.instructions.md`](instructions/clock-leds.instructions.md): the clock plugin
  (`src/plugins/clock/**`) - pixel type and color math, LED power after a crash, the NeoPixelBus RMT/I2S
  transport of `wled_esp32_controller_neopixelbus`.

## Conventions

- **American English** in code, comments, UI labels and docs: `color`, not `colour`.
- **The build decides, not the IDE squiggles** - verify a change with a real compile.
- **AT mode is the developer console, the status page is user UI.** AT commands (serial/WebSocket console) may print
  verbose diagnostics in plain text (counters, timings, internal state); a plugin's `getStatus()` is shown in the WebUI
  and keeps to a short summary in HTML. Both only list what the build compiles in - no counters of a transport or
  feature that is not part of the build. Document AT commands in `docs/AtModeHelp.md`.
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

- `pio run -e <env> -t upload` builds and flashes in one step (`upload` runs the build first). No separate
  `pio run` step is needed.
- **The serial port is the env's `monitor_port`** (`conf/envs/*.ini`) - never a hardcoded COM port. When testing
  an env whose `monitor_port` is missing, **ask the user which port to use** before flashing or opening a monitor.
- **Serial only, no OTA**: `wled_esp32_controller`, `wled_esp32_controller_neopixelbus` and `wt32_sc01_test1` (the
  partition table has a single `factory` app partition and no OTA slots) and `led_strip_xmas_serial`.
- **`-t buildfs` does not compile the firmware** - always run a plain `pio run -e <env>` as well.
- Shared/core changes must be built for **both platforms**, e.g. `pio run -e led_strip_xmas_serial` (the ESP8266
  test env) next to the ESP32 env - a change can compile on one and fail on the other.
- **There are no unit tests** - verification happens on the hardware: a trace line, a log or a screenshot has
  to show what the change did, "it compiles" is not a verification. `scripts/tools/webui_test.py` checks pages,
  endpoints, forms and WebSockets in bulk (see the ESP8266 test env file). LVGL screens (`wt32_sc01_test1`,
  `DEBUG_LVGL_SCREENSHOT`) are captured over HTTP: `/lvgl-screen` and `/lvgl-screen.bmp`, documented in
  `src/plugins/lvgl/lvgl_debug.h`.
- **Device tests**: never save a form, change brightness/volume, WiFi, passwords or accounts, never start an
  LED/motor/sound action - send invalid form POSTs only (every real field missing, the firmware discards them).
  MQTT publishing is allowed on the two test boards only. Never send hand-made mDNS packets to the multicast
  group: one malformed packet rebooted every unpatched ESP8266 on the LAN.
- **PC tools** (`scripts/tools`) run in the workspace venv
  `.venv` (packages in `requirements.txt`, `python -m venv .venv` + `.venv\Scripts\python -m pip install -r
  requirements.txt`); the build scripts run in PlatformIO's own Python. Never add a hardcoded `sys.path` to a
  `site-packages` directory - a missing package goes into `requirements.txt`.
- Serial log (pass the env's `monitor_port`): `.venv\Scripts\python scripts/tools/serial_log.py <port> <seconds>
  logs/x.txt` opens the port with DTR/RTS low (no reset of the board) and writes UTF-8 with a timestamp per line.
  ESP8266 output contains NUL bytes (`tr -d '\000'` before grepping). PowerShell redirects (`>`) write UTF-16 -
  greps find nothing in such a file.
- Free the serial port first: a leftover `platformio.exe ... monitor` blocks the upload
  (`Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -match 'platformio.exe.*monitor' }`).
  Several resets in a row can leave the device in **safe mode** (plugins off) - one more reset clears it.
- If the user forgot to disconnect their terminal or if the device becomes unresponsive, reset it via
  serial - that requires terminating the user's process. Don't use web uploads/OTA if serial programming
  is selected. Don't terminate uploads over serial that block, wait for them.
- The WebUI **user name is the device name**; the default password is 12345678.
- **A clean build is the only proof** after a migration or flag change: PlatformIO rebuilds on content
  signature, so stale objects in `.pio/build/<env>` can keep a broken build green.
  `pio run -e <env> -t clean` deletes the tracked `data/webui/placeholder.txt` - restore it with git.
- **Build number**: every environment build advances the counter in `include/build_number.txt` by one (a two-env
  `pio run` uses two numbers); `src/build_number.cpp` is generated by `scripts/build_number.py` - never edit it by
  hand.
- `-t upload` / `-t uploadota` do **not** archive the build; only `-t buildarchive` stores the archive a crash
  trace needs (see the crash-debugging file).

## Filesystem and the WebUI

- **The WebUI is a separate image**: help texts and pages need `-t uploadfs` (or `-t buildfs`), a plain `pio run` does not
  rebuild them. Both build the WebUI into `data/` before the image is packed, skipped if no source changed;
  `-t rebuildfs` forces the WebUI build (without packing the image).
- **`-t uploadfs` formats the filesystem.** Everything that was uploaded at runtime is gone afterwards.
- Server-rendered forms come from a plugin's `createConfigureForm()`. **Form names are globally unique** - a URL
  directory does not scope them.
- Never build a whole large form before the response is streamed: that stalls the web server. Split it
  into several pages, as `/weather2*.html` does.

## Architecture

- **Plugins** derive from `PluginComponent` (`include/PluginComponent.h`), declare their metadata with
  `PROGMEM_DEFINE_PLUGIN_OPTIONS(...)` and register themselves with `REGISTER_PLUGIN(this, "Name")` in
  the constructor - the registry (`include/plugins.h`, `src/plugins.cpp`) sorts by `PriorityType` and
  calls `preSetup()` / `setup()` / `createMenu()`. There is no per-plugin `loop()`: a plugin adds a
  `LoopFunctions` callback or an `Event::Timer`.
- Smallest plugin to copy: `src/plugins/ssdp/` (one `.h` + one `.cpp`).
- **Which plugins compile is decided by the build, not at runtime**: `conf/common.ini`
  `build_src_filter` excludes all of `src/plugins/` and each env re-adds the folders it wants, plus `-D`
  feature flags. A plugin that no env adds is never compiled - check before editing it.
- **The main loop is the single writer for display/UI state.** Work requested from another task
  (AsyncTCP/WebSocket, MQTT, HTTP, IR, buttons, AT) is queued with `TaskQueue`
  (`lib/KFCLibrary/KFCEventScheduler`, docs in `lib/KFCLibrary/docs/KFCEventScheduler/`) and drained
  from `loop()`. Naming rule: `...Deferred()` may be called from any task (it queues), `...Queued()` is
  queue-only, no suffix runs in the loop task and may be called directly from loop-task code.
- **LVGL is not re-entrant**: never build or touch a widget tree from an LVGL event callback or a second
  task - record the intent and apply it in `update()`. `FastLED.show()` is not re-entrant either: only
  the display-owning task may call it.

## Configuration

- Config classes live in `include/kfc_fw_config.h` / `include/kfc_fw_config/**`; values are stored per
  parameter in NVS (`nvs2`) under a 16-bit CRC handle registered in `src/kfc_fw_config_classes.cpp`.
- A plugin implements a packed `ConfigStructType` and a handler deriving
  `ConfigGetterSetter<ConfigType, _H(...)>` (`include/kfc_fw_config/base.h`); bit fields use
  `CREATE_BITFIELD_TYPE_MIN_MAX(...)`.
- **Changing the layout resets the parameter**: `getConfig()` compares the stored size with
  `sizeof(ConfigStructType)` and falls back to the constructor defaults on a mismatch - a new or changed
  member resets that parameter **once** after flashing. That is the accepted design: put the config into
  the existing `ConfigStructType`/blob it belongs to and let it resize. Do **not** invent a separate
  parameter (its own handle + `Plugins::X` alias + `DECLARE/DEFINE_CONFIG_HANDLE_PROGMEM_STR`) or a
  nested config class just to avoid that one-time reset. Keep each bit field inside its storage unit or
  `-Wpacked-bitfield-compat` moves it silently.
- **No layout or version field for a binary config that breaks by size** (`KFCConfigurationClasses::Plugins::*`):
  the parameter is reset to the defaults (or treated as invalid) anyway, so a layout/version flag only widens
  the layout it is meant to guard - leave it out.
- **`getWriteableConfig()` creates an all-zero blob** for a parameter that does not exist yet, so a form
  can show 0 while `getConfig()` shows the constructor defaults. Add a range/default fallback in the
  getter (and note that a legal value of 0 cannot be told apart from "unset").

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
- **NVS (ESP8266)**: never deinitialize the partition while a handle is open - the handle becomes invalid (and
  older NVS library versions leaked it). `Configuration::_nvs_open()`/`_nvs_deinit()` in KFCLibrary take care of
  it; do not reopen a handle through `_nvs_close()`, it arms the deinit timer.
- **Unsigned `millis()` arithmetic**: use a signed (clamped) difference - a timestamp written later in
  the same tick underflows and expires a timeout instantly.
- **LVGL 8.4**: `lv_obj_get_x/y/width()` are layout-dependent (0 before the first layout pass - use
  `lv_obj_get_style_*`); `lv_label_set_text()` reallocates and restarts scroll animations when the
  pointer changes; every object is clickable by default; never call `lv_chart_remove_series()`.
- Deeper dated findings (crash analyses, vendor-landmine details, measured numbers) are in the agent
  memory files `/memories/repo/*.md` - read them before a risky change.
