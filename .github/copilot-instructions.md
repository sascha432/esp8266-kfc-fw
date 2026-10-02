# Project Guidelines

**kfc_fw** is a plugin-based ESP8266/ESP32 firmware (PlatformIO, C++17) with a WebUI, an AT-command
console, MQTT/Home Assistant integration and an optional LVGL touch UI. These rules apply to every task
in this workspace.

- Main entry point: `src/kfc_firmware.cpp` (`setup()` / `loop()`); plugins under `src/plugins/**`.
- Build configuration: `platformio.ini` + `conf/**` (one file per env in `conf/envs/`); build scripts in `scripts/`.
- WebUI sources: `Resources/**`, built by `KFCWebBuilder.json` into `data/webui/**`.
- Feature overview in [README.md](../README.md), recent changes in [CHANGELOG.md](../CHANGELOG.md).

## Conventions

- **American English** in code, comments, UI labels and docs: `color`, not `colour`.
- **The build decides, not the IDE squiggles** - verify a change with a real compile.
- **Show a design/icon preview before implementing UI work** and wait for the approval.
- **Do not modify the libraries** - other folders under `lib/` are out of bounds and require separate approval.
- The firmware is **C++17** (`-std=gnu++17` in `conf/common.ini`). Match the surrounding style; the repo's
  `.clang-format` is the reference (WebKit base, 4 spaces, no tabs).
- Both platforms use the **stock** PlatformIO Arduino cores (`espressif8266`, `espressif32`) again - only
  upstream APIs are available, no fork-only `String` helpers (`rtrim/ltrim`, `*IgnoreCase`, `startsWith(char)`,
  chained `trim()/toLowerCase()`). Use the project's `StrView`/`StrWrapper`
  (`lib/KFCLibrary/KFCBaseLibrary/include/StrView.h`) instead, and put the flash string on the left:
  `F("...") == str`, never `str == F("...")`.
- Keep `CHANGELOG.md` entries to one line per change - the owner maintains the wording. Add to the top of the file.

## Filesystem and the WebUI

- **The WebUI is a separate image**: help texts and pages need `-t rebuildfs` and `-t uploadfs`, a plain `pio run` does not
  rebuild them.
- **`-t uploadfs` formats the filesystem.** Everything that was uploaded at runtime, `/hass.yaml`
  included, is gone afterwards.
- If the user forgot to disconnect their terminal or if the device becomes unresponsive, reset it via
  serial - that requires terminating the user's process. Don't use web uploads/OTA if serial programming
  is selected. Don't terminate uploads over serial that block, wait for them.
- Built pages and assets are content-hashed (`data/webui/<hash>`), JS/CSS are gzip-compressed on disk and
  `data/webui/.listings.txt` maps a hash back to its original name - look a file up there, not by grep.

## Build, flash and verify

- `pio run -e <env> -t upload` builds and flashes in one step (`upload` runs the build first). No separate
  `pio run` step is needed; the COM port and OTA support per environment are in the environment sections below.
- **`-t buildfs` does not compile the firmware** - always run a plain `pio run -e <env>` as well.
- Shared/core changes must be built for **both platforms**, e.g. `pio run -e bme280_serial` (the ESP8266 test
  env) next to the ESP32 env - a change can compile on one and fail on the other.
- **There are no unit tests** - verification happens on the hardware: a trace line, a log or a screenshot has
  to show what the change did, "it compiles" is not a verification. `logs/check_pages.py` checks pages and
  endpoints in bulk; the environment sections list the panel/debug key recipes.
- Serial capture/reset:
  `python logs/serial_capture.py --port <COMx> --seconds 60 --no-reset | Out-File -Encoding utf8 logs/x.txt`
  - **not with `>`**, a redirected file becomes UTF-16 and greps find nothing in it.
- Free the serial port first: a leftover `platformio.exe ... monitor` blocks the upload
  (`Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -match 'platformio.exe.*monitor' }`).
  Several resets in a row can leave the device in **safe mode** (plugins off) - one more reset clears it.
- The WebUI **user name is the device name**; the default password is 12345678.
- **A clean build is the only proof** after a migration or flag change: PlatformIO rebuilds on content
  signature, so stale objects in `.pio/build/<env>` can keep a broken build green.
  `pio run -e <env> -t clean` deletes the tracked `data/webui/placeholder.txt` - restore it with git.
- For a single TU, `pio run -e <env> -t compiledb` writes `compile_commands.json`; the recorded command
  is longer than the Windows command line limit, so run it through a GCC **response file** (`@args.rsp`)
  and convert `\` to `/` only in `-I/-isystem/-include/-o` style path switches (a global replace
  destroys the `\"` inside `-D` macros).
- VS Code freezing for ~40 s after a build is the STM32Cube Build Analyzer parsing `firmware.elf`;
  `files.watcherExclude` already hides `**/.pio/**` - do not "fix" that here.

## Environment `wt32_sc01_test1`

- Flashed over **COM9**; **OTA is not available** (the partition table has a single `factory` app partition
  and no OTA slots).
- **Custom file `/hass.yaml`**: after a firmware/FS upload, upload it with `scripts\tools\hass_config.bat`
  (it posts `include\retracted\custom_config\hass.yaml` and prints "Upload successful"). Validate it offline
  first with `python scripts/tools/hass_config.py --file <file> --validate-only` - it prints the tiles with
  the cell of both orientations and a map per page and per orientation; exit code 1 means warnings, not an
  error.
- **Panel verification** with the debug screenshot API (no finger needed): the keys are listed in
  `weather_station2.cpp` (`hasspage`, `hasspanel`, `hassview`, `hassrange`, `hassrot`, `hasssettings`,
  `hassfull`) and documented in `src/plugins/weather_station2/docs/hass_config.md` ("Debug keys of the
  screenshot feature"):
  `python scripts/tools/hass_screenshot.py --host <ip> -u <device name> -p <password> --screen HASS
  --set "hasspage:23" --set "hassrot:1" -o logs/x.png`
- **A GUI change reviews and updates the GUI docs in the same step**: `src/plugins/weather_station2/docs/`
  (`hass_config.md`, `MIGRATION_LVGL.md`, `screens/`) is part of the change, not a follow-up.
- `include/retracted/custom_config/hass.yaml` holds the real Home Assistant token (gitignored) - never print,
  log or commit its contents.
- The Home Assistant side is probed from the PC (`logs/probe_hass_ws.py`, `logs/probe_hass_resub.py`,
  `logs/probe_hass_burst.py`) before a firmware change is blamed for a protocol problem.

## Environment `wled_esp32_controller`

- Flashed over **COM7**; **OTA is not available** (the partition table has a single `factory` app partition
  and no OTA slots).

## Environment `bme280_serial` (ESP8266 test env)

- Flashed over **COM6**; `board = nodemcuv2` with `upload_protocol = esptool` - **serial only, no OTA**. It
  extends `debug_esp8266`, so `DEBUG` is on and the serial log is verbose (env definition:
  `conf/envs/environmental_sensor.ini`).
- This is the **ESP8266 verification env** for shared/core changes (see the build section): build it next to
  the ESP32 env, because a change can compile on one platform and fail on the other.
- Page/endpoint checks against it use the same PC tools as for the panel (`logs/check_pages.py`,
  `logs/fetch_page.py`, `logs/bw_test.py`).

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

## WebUI and forms

- `Resources/html|js|css/**` are built into `data/webui/<hash>` (+ `.listings.txt`) by the PHP builder
  driven by `KFCWebBuilder.json`. Its preprocessor is `<!--#if MACRO-->` / `<!--#else-->` /
  `<!--#endif-->` in HTML and `/*--#if MACRO--*/` in JS/CSS; write `defined(NAME)` when one macro name is
  a prefix of another, otherwise the condition silently drops files.
- Page-level help texts live in `Resources/html/<page>.html` as
  `div.form-help-block > div[data-target="#field_id"]`; `Resources/js/forms.js` applies them.
- Server-rendered forms come from a plugin's `createConfigureForm()` (`FormUI::Form::BaseForm`) and are
  streamed via `include/templates.h` + `src/templates.cpp` (`TemplateDataProvider`/`SSIProxyStream`).
  **Form names are globally unique** - a URL directory does not scope them.
- Never build a whole large form before the response is streamed: that stalls the web server. Split it
  into several pages, as `/weather2*.html` does.

## Pitfall checklist

Recurring bug classes - check these before blaming a change:

- **ESP32 core macros**: `DEFAULT`, `DISABLED`, `ON`, `OFF`, ... are `#define`s - an enum member with
  such a name breaks every TU that includes `Arduino.h`. Prefix enumerators.
- On ESP32 `F()/PSTR()/PROGMEM` are no-ops and `F("...")` does not convert to `const char*`; on ESP8266
  they move literals to flash (plain literals cost RAM there). Use `PSTR()` only together with the `_P`
  readers (`snprintf_P`, `strncmp_P`, ...).
- A NULL `%s` argument crashes newlib; plain appends use `+=`/`concat`, not a `printf` family call.
- **Unsigned `millis()` arithmetic**: use a signed (clamped) difference - a timestamp written later in
  the same tick underflows and expires a timeout instantly.
- **LVGL 8.4**: `lv_obj_get_x/y/width()` are layout-dependent (0 before the first layout pass - use
  `lv_obj_get_style_*`); `lv_label_set_text()` reallocates and restarts scroll animations when the
  pointer changes; every object is clickable by default; never call `lv_chart_remove_series()`.
- Deeper dated findings (crash analyses, vendor-landmine details, measured numbers) are in the agent
  memory files `/memories/repo/*.md` - read them before a risky change.

