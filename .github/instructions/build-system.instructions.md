---
applyTo: "platformio.ini,conf/**,scripts/**,include/build_number.txt"
description: "Library declarations, lib_ignore, compiler flags and optimize pragmas, the build number, compiledb"
---

# Build system

Details for `platformio.ini`, `conf/**` and `scripts/**`. The general build, flash and verify rules are in
[`.github/copilot-instructions.md`](../copilot-instructions.md).

## Libraries

- Shared library URLs are declared **once** and referenced with `${name.lib_deps}` - never repeat a URL in an env:
  LED libraries in `conf/envs/led_matrix.ini` (`[led_lib_neopixelex]`, `[led_lib_fastled]`, `[led_lib_neopixelbus]`),
  everything else in `conf/libs.ini` (`[lib_*]`).
- A `lib_ignore` must start from `${<parent>.lib_ignore}` - `${env.lib_ignore}` resolves to nothing (a no-op, not
  the inherited list). The one exception that keeps an explicit list is `conf/envs/weather_station.ini` (needs
  `Adafruit GFX Library`, which the base ignores).
- FastLED and the NeoPixelBus fork: see
  [`.github/instructions/clock-leds.instructions.md`](clock-leds.instructions.md).

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
- `KFCLibrary/scripts/extra_script.py` reads the counter for `data/.pvt/build` (it is a separate git repo -
  changes there must be committed/pushed separately).
- The library's `SaveCrash::Data::FirmwareVersion::build` bitfield is 16 bit - only the crash log and the
  config-version comparison truncate there.

## Compiler flags

- PlatformIO adds the core's flags **after** an env's `build_flags`: both cores add `-Os`, the ESP32 core also
  `-ggdb`, `-fstack-protector` and `-std=gnu++11`. A `build_flags` entry that contradicts one of them has no
  effect - a different level needs `-Os` in `build_unflags` (it runs after the core's build script) plus the new
  `-O` in `build_flags`, `-fstack-protector-all` needs `-fstack-protector` in `build_unflags`. Check the result
  with `pio run -e <env> -t envdump` (`CCFLAGS`), it does not compile.
- A hot path gets `O3` from `#pragma GCC push_options`/`optimize` in a `.cpp` **after** its includes, or from an
  `optimize` attribute on the function - never around a header or its includes: GCC does not inline a function
  with its own optimize options into a caller with other options, so every small accessor becomes a call.

## ESP8266 IRAM

- The instruction cache is **16 KB or 32 KB**, so IRAM is 32 KB (default) or 48 KB. More IRAM only comes from the
  core presets `-D PIO_FRAMEWORK_ARDUINO_MMU_CACHE16_IRAM48` (or `..._SECHEAP_SHARED`, the unused IRAM becomes a
  second heap). Never use `PIO_FRAMEWORK_ARDUINO_MMU_CUSTOM` with other `MMU_IRAM_SIZE`/`MMU_ICACHE_SIZE`
  values: the core keeps the 32 KB cache for any `MMU_ICACHE_SIZE` other than `0x4000`, the build links and the
  cache overwrites the IRAM above 32 KB at runtime (tested with `0x9000`/`0x7000`: every word above `0x40108000`
  was wrong right after boot).
- The 16 KB cache makes code in flash slower (`weather_station.ini`: the SECHEAP preset lagged). Check the free
  IRAM in `firmware.elf` (`_text_end`, `0x40108000` = 32 KB) before switching an env.

## Compiling a single TU, IDE

- For a single TU, `pio run -e <env> -t compiledb` writes `compile_commands.json`; the recorded command
  is longer than the Windows command line limit, so run it through a GCC **response file** (`@args.rsp`)
  and convert `\` to `/` only in `-I/-isystem/-include/-o` style path switches (a global replace
  destroys the `\"` inside `-D` macros).
- VS Code freezing for ~40 s after a build is the STM32Cube Build Analyzer parsing `firmware.elf`;
  `files.watcherExclude` already hides `**/.pio/**` - do not "fix" that here.
