# LED libraries and build flags

Reference for the LED/clock plugin: which LED library does what, which environment builds which
transport, and which `-D` switches select or influence them (including the I2S microphone of the
visualizer).

The transports themselves (RMT memory blocks, mixed I2S+RMT, measured frame times) are described in
[`.github/copilot-instructions.md`](../.github/copilot-instructions.md), section
`Environment wled_esp32_controller_neopixelbus`. This page is the library and flag reference.

## 1. LED libraries

| Library | Source / version | Role in this firmware | Built into |
| --- | --- | --- | --- |
| FastLED (fork) | `sascha432/FastLED` (reports 3.4.1) | Transport (its own ESP32 RMT driver) + color math + animations + dithering + fork-only frame retry counters | `wled_esp32_controller`, `wled_esp32_controller_rmt`, `wled_esp32_s3_controller`, ESP8266 `ledmatrix_*`, `weather_station*`, `7segment_clock` |
| FastLED (official) | `FastLED/FastLED#3.9.20` | Color math + animations + `CRGB`/`CHSV` only - no `addLeds()`/`FastLED.show()`, so no RMT/I2S driver of FastLED is installed | `wled_esp32_controller_neopixelbus` |
| NeoPixelBus (fork) | `sascha432/NeoPixelBus`, branch `kfc-rmt-mem-blocks` / tag `kfc-rmt1`, based on upstream `master` `882b804`; `lib_deps` URL dependency (`#kfc-rmt1`, no local checkout) | Transport: RMT mux (`RMT_CHANNEL_0` with all 8 memory blocks, one segment after another) and/or I2S (DMA, up to 2 ports in parallel) | `wled_esp32_controller_neopixelbus` |
| NeoPixelEspEx | `sascha432/NeoPixelEspEx` (0.0.3), checkout in `lib/NeoPixelEspEx` | Transport (`IOT_LED_MATRIX_NEOPIXEL_EX_SUPPORT`) and the built-in WS2812 status LED when `HAVE_FASTLED=0` | ESP8266 `ledmatrix_*`, `weather_station*`, ESP32 `ledmatrix_base_esp32` |
| FastLED 3.10.x | - | **Does not build here** (its `platforms/arduino` layer needs a `Serial` with `begin()`/`operator bool()`, `serial_compat.h` force-includes `extern Stream &Serial`) - FastLED is therefore pinned | - |

The LED dependencies are declared **once** in `conf/envs/led_matrix.ini` and referenced by the
environments with `${...lib_deps}`:

| Group | URL | Used by |
| --- | --- | --- |
| `[led_lib_neopixelex]` | `sascha432/NeoPixelEspEx` | the ESP8266/ESP32 LED base envs |
| `[led_lib_fastled]` | `sascha432/FastLED` (fork) | envs where FastLED drives the LEDs (the power limit path needs the fork's `m_pPowerFunc`) |
| `[led_lib_fastled_official]` | `FastLED/FastLED#3.9.20` | the NeoPixelBus env (color math only) |
| `[led_lib_neopixelbus]` | `sascha432/NeoPixelBus#kfc-rmt1` | `wled_esp32_controller_neopixelbus` |

`HAVE_NEOPIXELBUS=1` is exclusive with `IOT_LED_MATRIX_NEOPIXEL_EX_SUPPORT` - `clock_def.h` raises an
`#error` if both are set.

Adafruit NeoPixel support was removed (2026-10-04): `IOT_LED_MATRIX_NEOPIXEL_SUPPORT`,
`ShowMethodType::AF_NEOPIXEL`, the `+LMC=met,neo` command and `include/Adafruit_NeoPixelEx.h` are
gone, `NeoPixelEx` is the only NeoPixel-style transport (the `NEOPIXEL_*` tuning macros belong to it).

## 2. Transport selection flags

| Flag | Default | Effect |
| --- | --- | --- |
| `IOT_CLOCK` | 0 | Compiles the clock/LED plugin (`IOT_LED_MATRIX` requires it) |
| `IOT_LED_MATRIX` | 0 | 2D matrix mode instead of clock displays |
| `IOT_LED_MATRIX_OUTPUT_PIN`..`PIN3` | per env | One output pin per segment; derives `IOT_LED_MATRIX_CHANNELS` (1..4, `include/global.h`) |
| `IOT_CLOCK_NUM_PIXELS` | `cols*rows + offset` | Pixel buffer size = `Clock::MatrixValidation::kMaxPixels` |
| `IOT_LED_MATRIX_CONFIGURABLE` | 1 | Runtime (`DynamicPixelMapping`) instead of compile-time pixel mapping |
| `IOT_LED_MATRIX_NEOPIXEL_EX_SUPPORT` | 1 | NeoPixelEx transport, adds `ShowMethodType::NEOPIXEL_EX` |
| `HAVE_NEOPIXELBUS` | 0 | NeoPixelBus backend, adds `ShowMethodType::NEOBUS_RMT` and `NEOBUS_I2S` |
| `IOT_LED_MATRIX_FASTLED_ONLY` | derived | 1 when no other transport is compiled in (`HAVE_NEOPIXELBUS` and `IOT_LED_MATRIX_NEOPIXEL_EX_SUPPORT` are 0) |
| `IOT_CLOCK_SHOW_METHOD_MAX` | derived | Highest selectable `ShowMethodType`: 1 with FastLED only, 2 with NeoPixelEx, 3 with NeoPixelBus. A stored value above it would call a transport that is not compiled in |
| `IOT_CLOCK_SHOW_METHOD_DEFAULT` | derived | New config's method: 2 (`NEOBUS_RMT`) with NeoPixelBus, otherwise 1 (`FASTLED`) |
| `FASTLED_LED_CONTROLLER` | `NEOPIXEL` | FastLED chipset (`pixel_display.h`); `7segment_clock` uses `WS2813_GRB` |
| `HAVE_FASTLED` | undefined (0) | Selects FastLED instead of NeoPixelEx for the built-in WS2812 status LED (`blink_led_timer.h`) and the weather station; must be set by the env, the firmware defines no default |
| `HAVE_FASTLED_RMT` | - | Set by `wled_esp32_controller` and `wled_esp32_controller_rmt`; no consumer in the firmware sources nor in the pinned FastLED (3.4.1 fork / 3.9.20) - effectively a no-op |
| `FASTLED_ESP32_I2S` | - | FastLED drives the LEDs over I2S; conflicts with the visualizer microphone on the same port (see below) |

## 3. Flags that influence the transports

### NeoPixelBus / RMT

| Flag / symbol | Default | Effect |
| --- | --- | --- |
| `NEOPIXELBUS_RMT_INT_FLAGS` | `ESP_INTR_FLAG_IRAM \| ESP_INTR_FLAG_LEVEL3` (fork patch, keeps its `#ifndef`) | RMT ISR priority/flags; `-D` overrides it for A/B tests |
| `gNeoPixelBusRmtMemBlocks` | 1 (library) / 8 (set by `NeoBusRmtMux`) | Memory blocks per RMT channel; all 8 on one channel give the refill interrupt its ~320 us window |
| - | - | `NeoBusRmtMux` routes `RMT_CHANNEL_0` with `rmt_set_gpio()` to the pin of the segment it transmits, `rmt_wait_tx_done()` frames the segments |

### I2S microphone and the I2S LED transport

| Flag | Default | Effect |
| --- | --- | --- |
| `IOT_LED_MATRIX_ENABLE_VISUALIZER` | 0 | Compiles the audio visualizer animations |
| `IOT_LED_MATRIX_ENABLE_VISUALIZER_I2S_MICROPHONE` | 0 | Compiles the INMP441 microphone (I2S, ESP32/ESP8266 build for it) |
| `IOT_LED_MATRIX_I2S_PORT` | `I2S_NUM_0` | Port the microphone installs the ESP-IDF driver on |
| `HAVE_NEOPIXELBUS_SUPPORT_MIC` | 0 | Reserves the microphone's port; the LED transport uses the remaining I2S port(s) and transmits the segments no port is left for with RMT (mixed mode). A NeoPixelBus build with the microphone and without this flag is an `#error` (`pixel_output_neobus.h`) |
| `IOT_LED_MATRIX_I2S_MICROPHONE_SD/WS/SCK` | 32 / 15 / 14 | Microphone data/word-select/clock pins |
| `Clock::MatrixValidation::kMaxI2sStrips` | 2 with `HAVE_NEOPIXELBUS`, else 0 | I2S ports on the ESP32 |
| `Clock::MatrixValidation::kMicI2sPort` | `IOT_LED_MATRIX_I2S_PORT` or 0xff | Port that is never used by the LED transport |
| `Clock::MatrixValidation::kAvailableI2sStrips` | `kMaxI2sStrips` minus the reserved port | Ports the LEDs may use |
| `Clock::MatrixValidation::kMaxStrips` | 4 | Segments/pins the transports can drive (`NeoBusRmtMux::kMaxPins`, `NeoBusStrips::kMaxStrips`) |

Rules of thumb (ESP32 has exactly two I2S ports and three possible consumers):

| Configuration | Result |
| --- | --- |
| 1 segment | I2S (stall proof) or RMT |
| 2 segments, microphone off | Both I2S ports, parallel (2 chains, one per port) |
| 2 segments, microphone on | 1 I2S + 1 RMT in the same frame (mixed mode) |
| 3-4 segments | RMT only (serial, frame rate drops with the total pixel count) |
| `FASTLED_ESP32_I2S=1` + microphone on port 0 | `#error` in `i2s_microphone.cpp` - set `IOT_LED_MATRIX_I2S_PORT=I2S_NUM_1` or disable the microphone |

### FastLED (fork) knobs

| Flag | Default | Effect |
| --- | --- | --- |
| `FASTLED_ESP32_FLASH_LOCK` | 0 | Holds `spi_flash_op_lock()` around `FastLED.show()` (same mechanism `NeoBusStrips::show()` uses) |
| `FASTLED_ALLOW_INTERRUPTS`, `INTERRUPT_THRESHOLD`, `FASTLED_INTERRUPT_RETRY_COUNT` | 1 / 0 / 2 | Interrupt handling and frame retry behavior |
| `FASTLED_DEBUG_COUNT_FRAME_RETRIES` | 1 (`ledmatrix_pixels`) | Fork-only retry counters; forced to 0 in NeoPixelBus builds (`clock_def.h`) |
| `FASTLED_RMT_MAX_CHANNELS` / `FASTLED_RMT_MEM_BLOCKS` / `FASTLED_RMT_BUILTIN_DRIVER` | 8 / 2 / false | FastLED's own RMT driver (not used by the NeoPixelBus backend) |
| `FASTLED_ESP8266_RAW_PIN_ORDER` | - | ESP8266 pin numbering (`weather_station`) |

### NeoPixelEx knobs

| Flag | Effect |
| --- | --- |
| `NEOPIXEL_ALLOW_INTERRUPTS`, `NEOPIXEL_INTERRUPT_RETRY_COUNT` | Interrupt handling during output |
| `NEOPIXEL_USE_PRECACHING` | Pre-computed bit patterns (removed by `ledmatrix_base_esp32` via `build_unflags`) |
| `NEOPIXEL_HAVE_BRIGHTHNESS`, `NEOPIXEL_HAVE_STATS`, `NEOPIXEL_DEBUG` | Brightness scaling, statistics, debug output |
| `NEOPIXEL_DEBUG_TRIGGER_PIN`, `NEOPIXEL_DEBUG_TRIGGER_PIN2` | 14 / 15 (`ledmatrix_pixels`) - pins toggled by the debug code |
| `NEOPIXEL_CHIPSET` | Chipset (commented out in `ledmatrix_pixels`) |

## 4. Numeric and behavior flags

| Flag | Default | Effect |
| --- | --- | --- |
| `IOT_CLOCK_HAVE_POWER_LIMIT` | 0 | Software power limiter (`ClockPlugin::_getNeoBusBrightness()`), applied in the loop - FastLED's fork-only power management is not compiled in |
| `IOT_CLOCK_DISPLAY_POWER_CONSUMPTION` | 0 | Power/current display |
| `IOT_CLOCK_OPTIMIZE_LOOP` | 1 | Shorter loop path, mostly relevant for the ESP8266 FastLED frame rate |
| `IOT_CLOCK_POWER_CORRECTION_OUTPUT` | `PF(0.01)` | Assumed non-linear loss at high currents, used by the power limiter/display |
| `IOT_CLOCK_PIXEL_MAPPING_TYPE` | derived from `IOT_LED_MATRIX_CONFIGURABLE` | `DynamicPixelMapping` vs `PixelMapping` |

## 5. Recommended transports

| Platform | Use | Why |
| --- | --- | --- |
| ESP8266 | FastLED (fork, `sascha432/FastLED`) | It is the transport the ESP8266 envs are built and tested with and it is the fastest of the compiled ones. NeoPixelBus is not an option on this platform: its ESP8266 sources need a `Serial` with `begin()`/`operator bool()`, while the firmware force-includes `serial_compat.h` with `extern Stream &Serial;` (`NO_GLOBAL_SERIAL`) - that is why `NeoPixelBus` sits in the `lib_ignore` of `conf/common_esp8266.ini` |
| ESP32 (xtensa core) | NeoPixelBus with the **I2S** transport (`ShowMethodType::NEOBUS_I2S`) | DMA driven: `NeoEsp32I2sMethodBase::Update()` encodes the whole frame into one DMA buffer and starts it, so there is no refill interrupt to miss - a flash write only delays the next frame instead of corrupting the current one - and up to 2 segments are transmitted in parallel (measured 118 fps at 2048 pixels / 2 segments). The RMT path is serial (a frame costs the sum of its segments, 512 pixels are ~15.4 ms) and stall sensitive |

The NeoPixelBus build defaults to the RMT transport (`IOT_CLOCK_SHOW_METHOD_DEFAULT`); select I2S with the
Display Method form field `dm`, or at runtime with `+LMC=met,ni2s`. With
`IOT_LED_MATRIX_ENABLE_VISUALIZER_I2S_MICROPHONE=1` one I2S port is reserved for the microphone, so with 2
segments the second one is transmitted by RMT in the same frame (mixed mode, see section 3).

### FastLED RMT on the ESP32 (good, but with an open caveat)

FastLED's own RMT driver is a good alternative as well - it transmits at the wire-limited frame rate - but it has
a rare random crash that is still unfixed:

- `ESP32RMTController::showPixels()` waits `portMAX_DELAY` on the TX-done semaphore. If that interrupt never
  arrives, the loop task blocks and the task watchdog panics; the panic path then faults while the flash lock is
  held, so the core dump is incomplete and the culprit task is missing.
- The suspected cause is the flash lock (`FASTLED_ESP32_FLASH_LOCK` holds `spi_flash_op_lock()` around
  `FastLED.show()`) meeting a flash write (NVS config save, LittleFS or syslog write): a flash operation masks
  interrupts on both cores for its whole duration, which can delay the RMT interrupt past what the driver
  tolerates.
- Observed twice so far, once ~217 s after boot and once after 1 h 51 m - it is a race, not an uptime or
  filesystem size problem.

The transports, the measured frame times and the flash-stall behavior are described in
[`.github/copilot-instructions.md`](../.github/copilot-instructions.md), section
`Environment wled_esp32_controller_neopixelbus`.
