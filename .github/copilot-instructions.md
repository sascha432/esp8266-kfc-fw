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
- **Initialize members in the constructor, not in the class body** - no in-class default member
  initializers (`uint8_t _x{0};`). Write the constructor **inline in the header** when the type has none in a
  `.cpp`; a type that already has one in a `.cpp` keeps it there. Keep the initializer list in declaration
  order, and do not list a member twice
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

## Build number

- `include/build_number.txt` holds the persistent counter **and** a readable history (build date + environments).
  It is a plain text file for humans/AI - it is never compiled, never uploaded to the device and the history is
  never truncated.
- `src/build_number.cpp` is **generated** by `scripts/build_number.py` and is the only translation unit the
  number is compiled into (a build recompiles just that file). Never edit it by hand.
- `scripts/pre_script.py` increments **before compiling** so the new number ends up in the binary. A build that
  never reaches the link step did not create a firmware - its number is taken back on the next run
  (`.pio/build_number.pending`), so a failed build does not consume a number.
- There is no build-command detection: **every environment build advances the counter by one**, so a two-env
  `pio run` uses two numbers. `pio run -t newbuild` increments once without a build confirming it.
- Targets that do not compile the firmware (`clean`, `fullclean`, `compiledb`, `buildfs`, `rebuildfs`,
  `uploadfs`, `uploadfsota`, `envdump`, `monitor`, `nobuild`) do not advance the counter.
- `KFCFWConfiguration::getBuildNumber()` / `getBuildNumberString()` expose it; the short firmware version is
  `<major>.<minor>.<revision> Build <number>` and the build string shares that flash string's location
  (`+ strlen(FIRMWARE_VERSION_STR " Build ")`).
- `lib/KFCLibrary/scripts/extra_script.py` reads the counter for `data/.pvt/build` (it is a separate git repo -
  changes there must be committed/pushed separately).
- The library's `SaveCrash::Data::FirmwareVersion::build` bitfield is 16 bit - only the crash log and the
  config-version comparison truncate there.

## Build archive

- `-t upload` / `-t uploadota` only add `erase_core_dump` (ESP32 only) before flashing, in `scripts/extra_script.py` -
  they do **not** archive. `-t buildarchive` builds, uploads and archives in one step and runs the archive step
  only after a successful upload, so a failed build or upload never leaves an archive behind.
- The ESP32 core dump is erased over **serial** (`esptool erase_region <coredump offset> <size>` taken from
  `$PARTITIONS_TABLE_CSV`) - no WebUI, no credentials, and it works when the device has crashed.
- A build is archived as `elf/<env>_<build>.tar.gz`: `firmware.elf`, `firmware.bin`, `filesystem.bin` (the
  `spiffs` partition read back from the device) plus `source/` (project + `lib/KFCLibrary`) and `info.txt`
  (build, env, git revisions, checksums, restore hints). `elf/archive.log` lists every archive; `elf/` is gitignored.
- **Archives are kept indefinitely**: every `-t buildarchive` leaves its own `elf/<env>_<build>.tar.gz` behind and
  nothing is ever deleted, so a crash can be decoded as long as that build was archived.
- The tar is written **straight from the working tree** (no staging copy, no temp directory) with
  `compresslevel=6`, and the esptool reads use `$UPLOAD_SPEED` with a fallback to esptool's 115200.

## Build, flash and verify

- `pio run -e <env> -t upload` builds and flashes in one step (`upload` runs the build first). No separate
  `pio run` step is needed.
- **The serial port is the env's `monitor_port`** (`conf/envs/*.ini`) - never a hardcoded COM port. When testing
  an env whose `monitor_port` is missing, **ask the user which port to use** before flashing or opening a monitor.
- **`-t buildfs` does not compile the firmware** - always run a plain `pio run -e <env>` as well.
- Shared/core changes must be built for **both platforms**, e.g. `pio run -e bme280_serial` (the ESP8266 test
  env) next to the ESP32 env - a change can compile on one and fail on the other.
- **There are no unit tests** - verification happens on the hardware: a trace line, a log or a screenshot has
  to show what the change did, "it compiles" is not a verification. `logs/check_pages.py` checks pages and
  endpoints in bulk; the environment sections list the panel/debug key recipes.
- Serial capture/reset (pass the env's `monitor_port`):
  `python logs/serial_capture.py --port <monitor_port> --seconds 60 --no-reset | Out-File -Encoding utf8 logs/x.txt`
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

## Crash dumps and stack traces

- `scripts/tools/kfc_coredump.py info|download|trace|erase` is the only way to touch a device crash. It
  authenticates with the WebUI session id (user = device name = `KFC` + the last 3 bytes of the MAC upper
  hex, e.g. `arp -a | Select-String <ip>` -> `c4-4f-33-0a-42-71` -> `KFC0A4271`; default password 12345678);
  the address and environment are in `conf/envs/*.ini`.
- `trace` is the workflow: download `/coredump.bin`, match it against `elf/<env>_<build>.tar.gz` (the crash
  summary carries the first 16 hex of the ELF sha256), decode with `esp-coredump` plus the PlatformIO xtensa
  gdb and resolve the panic addresses with `addr2line`. Artifacts: `logs/coredump_<env>_<build>.{bin,elf}`
  and `logs/crash_<env>_<build>.{json,txt}`. `--erase` deletes the dump on the device afterwards.
- The download is a **20 byte flash header plus an ELF core dump** - do not hand `/coredump.bin` to
  `esp-coredump` as it is and `-t raw` is wrong for these dumps (`-t elf` on the stripped file is correct).
- Read the `reason:` line: `abort() was called at PC <addr>` names the trigger (`task_wdt_isr` = task
  watchdog, something starved IDLE0; `esp_core_dump_do_write_elf_pass` = the panic path itself faulted and the
  dump is incomplete, so the culprit task is missing - `IDLE0` is then only the panic context).
- Only `-t buildarchive` stores an archive, so a trace needs the crashed build to have been archived (a plain
  `-t upload` leaves none) - or `--elf` with the exact `firmware.elf` of that build, never a rebuilt one (the
  addresses would not match).
- `/trace-crash <ip>` (`.github/prompts/trace-crash.prompt.md`) runs capture, decode, root cause and fix
  from the chat.
- PowerShell pipes into a file (`>`, `Tee-Object`) write **UTF-16** - use `Out-File -Encoding ascii` for
  captured tool output or greps find nothing in it.

## Environment `wt32_sc01_test1`

- **OTA is not available** (the partition table has a single `factory` app partition and no OTA slots); use the
  env's `monitor_port` (`conf/envs/wt32_sc01.ini`).
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

- **OTA is not available** (the partition table has a single `factory` app partition and no OTA slots); use the
  env's `monitor_port` (`conf/envs/wled_board.ini`).

## Environment `wled_esp32_controller_neopixelbus`

- Same board, partition table and filesystem as `wled_esp32_controller`, but the LED transport is **NeoPixelBus**
  (`HAVE_NEOPIXELBUS=1`) instead of FastLED. FastLED is still used for `CRGB`/`CHSV` and the animations only -
  no `addLeds()`/`FastLED.show()`, so no FastLED RMT driver is initialized.
- FastLED is pinned to the **official 3.9.20**. 3.10.x does not build here: its `platforms/arduino` layer needs a
  `Serial` with `begin()`/`operator bool()`, while the firmware force-includes `serial_compat.h` with
  `extern Stream &Serial;` (`NO_GLOBAL_SERIAL`).
- Show method is a runtime toggle (`+LMC=met,nrmt|ni2s`, Display Method form), default **NeoPixelBus RMT**. All
  RMT outputs share one channel (see below), so up to 4 output pins work; I2S drives one port per segment
  (2 on the ESP32) and the port of the I2S microphone visualizer is reserved for it - the segments that no I2S
  port is left for are transmitted by RMT in the same frame (mixed mode, `HAVE_NEOPIXELBUS_SUPPORT_MIC`, below).
- **All RMT pins are transmitted on ONE channel with all 8 memory blocks.** The RMT channels share the 8 blocks
  and the ESP-IDF driver refills a channel from its threshold interrupt, which is set to half of its memory: two
  channels of 4 blocks each only leave **~160 us** per refill, one channel with all 8 blocks has **~320 us**. A
  late interrupt lets the hardware replay stale memory, the strip latches in the middle of a frame and the rest
  of the frame is shifted (random colors / "moving start LED"). Measured on the device: 2 channels/4 blocks
  flickers, 1 channel/8 blocks is stable. `NeoBusRmtMux` therefore transmits the segments one after another on
  `RMT_CHANNEL_0` and routes the RMT output to the pin of the segment that is transmitted (`rmt_set_gpio`); the
  previous pin is left as GPIO output low = idle level, so its strip latches. `rmt_wait_tx_done()` between the
  frames waits for the channel *and* is non-destructive (the driver gives the semaphore back), so it doubles as
  the frame timer. A frame costs the sum of its segments (128 pixels are ~3.84 ms): 2 segments ~78 fps, 4 ~65 fps.
- **NeoPixelBus is our fork of Makuna/NeoPixelBus**, pulled in as a `lib_deps` URL (not a local checkout any
  more) - `https://github.com/sascha432/NeoPixelBus.git#kfc-rmt1` in `conf/envs/wled_board.ini`, branch
  `kfc-rmt-mem-blocks`, tag `kfc-rmt1`, based on upstream `master` `882b804` (the three commits after the
  `2.8.4` tag: #894/#910/#911). The fork's `ReadMe.md` documents issue #921 / PRs #922/#923. Two patches in
  `src/internal/methods/NeoEsp32RmtMethod.{h,cpp}`:
  1. `NEOPIXELBUS_RMT_INT_FLAGS` is `ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL3` instead of the upstream
     `ESP_INTR_FLAG_LOWMED` (not IRAM safe, lowest priority). Same values FastLED's ESP32 RMT driver uses. It
     keeps its `#ifndef`, so `-D NEOPIXELBUS_RMT_INT_FLAGS=...` still overrides it for A/B tests.
  2. `mem_block_num` is no longer hardcoded to 1 - it reads `gNeoPixelBusRmtMemBlocks` (defined in the lib,
     default 1). `NeoBusRmtMux` sets it to all 8 blocks before `Begin()`.
  `NeoBusStrips::show()` additionally holds `spi_flash_op_lock()` around the transmit (the same mechanism as
  `FASTLED_ESP32_FLASH_LOCK`). Do not replace the fork URL with the upstream/registry version (the two patches
  would be lost); `NeoPixelBus` stays in `conf/common_esp8266.ini` `lib_ignore` - that is what keeps it out of
  the ESP8266 builds (LDF cannot see `#if HAVE_NEOPIXELBUS`), the fork keeps upstream's `platforms: "*"` manifest.
- Status page diagnostics for the RMT transport (sticky since boot): `..., NeoPixelBus RMT, <fps>, <n> blocks`,
  then `, frame +<us>` for the worst frame that took longer than its pixels need (the RMT memory ran empty and
  stale data was transmitted, i.e. visible flicker), `, <n>% over` for the share of segment transmissions that
  were late and `, <n> tx timeouts` for frames that never finished.
- Frame timing on the status page (averages of the last frames, from `ClockPlugin::_loop()`): `wire` is the
  shortest frame the configured segments allow (sum of the segments at 30 us per pixel), `show` is what the loop
  really spends on the transport (filling + transmitting + stalls) and `anim` is the rest of the frame
  (animations, matrix/hexagon mapping, power limit) - the frame time is `anim` + `show`, so `show - wire` is the
  loss inside the transport and `anim` is the CPU cost of the animation. `write +<us>` is the longest time the
  driver needed to translate and copy a frame into the RMT memory (that time is part of the frame, not of the
  transmission). `frame +<us>`/`<n>% over` compare the transmission window (end of the write until the
  transmit-done interrupt) with the wire time; it contains the task wakeup latency of the done semaphore as well
  as every replay, so the **maximum** is the value to watch - a high percentage alone is expected, and the
  maximum grows while the WebUI is used (accepted, see below).
- **Throughput and restrictions of the transports** (30 us per pixel = 1.25 us per bit is the WS2812 wire time,
  a segment is limited to `NeoBusRmtMux::kMaxPins`/`kMaxStrips` = 4):
  - **RMT transmits one wire at a time**, so a frame costs the **sum of all segments**: 512 pixels ~15.4 ms
    (~65 fps ceiling), 2048 pixels ~61 ms (~16 fps) - measured 48.1 fps (2 segments), 33.8 (3), 6.5 (4 segments
    of 2048 pixels). Running the pins on separate RMT channels would make them parallel, but then each channel
    only gets 4 of the 8 memory blocks and flickers (see above) - with RMT there is no parallel operation.
  - **RMT is sensitive to flash writes**: NVS/LittleFS/WiFi operations mask interrupts on both cores for their
    whole duration (a 4 KB page program is ~1-3 ms, a sector erase 20-45 ms) and the RMT replays its memory
    meanwhile, so that frame takes longer *and* shows garbage. `spi_flash_op_lock()` only excludes writers that
    honor it (the WiFi driver uses it), so it does not prevent this. `SyslogFile` writes `/.logs/messages` with
    open/append/close per message and rotates it at 16 KB, so a single log line or a WebUI request can stall a
    frame; measured `frame +5016us` at 512 pixels / 2 segments and `frame +27292us` at 2048 pixels / 4 segments
    (the "worst case" values, the filesystem being ~97% full makes every write start a garbage collection that
    erases 4 KB blocks).
  - **The stutter while the WebUI is used is accepted** (config saves, log writes, filesystem garbage
    collection): the transport recovers on the next frame and only the `frame +<us>` maximum grows, so this is
    not a bug to fix. Freeing filesystem space or stopping the log writer only reduces the stutter. A growing
    `frame +<us>` *without* WebUI or filesystem activity would mean the interrupt latency is above what the RMT
    can cover - that output then needs a DMA transport (I2S).
  - **I2S is DMA driven and parallel**: `NeoEsp32I2sMethodBase::Update()` encodes the whole frame into a DMA
    buffer (`heap_caps_malloc(..., MALLOC_CAP_DMA)`) and starts it, there is no refill interrupt - a delayed
    interrupt only delays the *next* frame instead of glitching the current one - and up to **2 segments are
    transmitted in parallel** (`NeoBusChannel` selects the I2S port) at the cost of one. Measured 118 fps at
    2048 pixels / 2 segments. Restrictions: only two ports exist and the visualizer's I2S microphone needs one of
    them. **Never share a port with the microphone**: NeoPixelBus installs its own I2S driver with the DMA
    interrupt of the port (`Esp32_i2s.c`, `i2sInit()` cannot report a failure), the microphone installs the
    ESP-IDF driver on `IOT_LED_MATRIX_I2S_PORT` (I2S0 by default). The IDF install then fails
    (`i2s_dma_intr_init(): Register I2S Interrupt error`), its cleanup resets the peripheral and the LED driver
    is left without a DMA interrupt - `NeoEsp32I2sMethodBase::Update()` spins in
    `while (!IsReadyToUpdate()) yield();` and the task watchdog resets the device in a loop (observed
    2026-10-04, build 15441: `task_wdt: - loopTask (CPU 1)` -> `abort()`). `HAVE_NEOPIXELBUS_SUPPORT_MIC=1`
    (set in `conf/envs/wled_board.ini`, default and documentation in `clock_def.h`) reserves
    `IOT_LED_MATRIX_I2S_PORT` for the microphone - the build errors out if the microphone is compiled in
    without it. A build that wants both I2S ports for LEDs has to disable
    `IOT_LED_MATRIX_ENABLE_VISUALIZER_I2S_MICROPHONE`. The DMA buffer of every segment must fit into internal DMA
    capable RAM (`MALLOC_CAP_DMA`); a segment that fails to initialize is transmitted by the RMT transport.
  - **Mixed I2S + RMT**: a segment that no free I2S port is left for is transmitted by the RMT mux in the same
    frame (`NeoBusStrips::update()`), the I2S segments run in parallel on the DMA while the RMT segments are sent
    one after another - a mixed frame costs the RMT segments plus the longest I2S segment. The status page shows
    the split as `<n> I2S + <n> RMT pins`. There is no I2S+FastLED mix.
  - Rule of thumb: 1 segment -> I2S (stall proof) or RMT; 2 segments -> I2S unless the microphone reserves a
    port (then 1 I2S + 1 RMT); 3-4 segments -> RMT (serial, the frame rate drops with the total pixel count).
- The power limit is a software limiter (`ClockPlugin::_getNeoBusBrightness()`) applied in `ClockPlugin::_loop()`;
  FastLED's fork-only power management is not compiled in. Temporal dithering is a no-op (NeoPixelBus has none).
- **OTA is not available** (single `factory` app partition); use the env's `monitor_port`.

## Environment `bme280_serial` (ESP8266 test env)

- `board = nodemcuv2` with `upload_protocol = esptool` - **serial only, no OTA**; use the env's `monitor_port`
  (`conf/envs/environmental_sensor.ini`). It extends `debug_esp8266`, so `DEBUG` is on and the serial log is verbose.
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

