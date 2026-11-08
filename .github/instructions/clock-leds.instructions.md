---
applyTo: "src/plugins/clock/**,conf/envs/wled_board.ini,conf/envs/led_matrix.ini"
description: "Clock plugin pixels, LED power after a crash, the NeoPixelBus RMT/I2S transport"
---

# Clock plugin LEDs and the NeoPixelBus transport

Details for the clock plugin (`src/plugins/clock/**`) and its LED transports. The library and `-D` flag reference
is [`docs/LED_Libraries.md`](../../docs/LED_Libraries.md); the general rules are in
[`.github/copilot-instructions.md`](../copilot-instructions.md).

## Pixel type and color math

- FastLED: only the fork is used, and only as LED transport - the clock plugin's pixel type and color math
  are `src/plugins/clock/pixel_color.h` (`Clock::PixelRGB`/`PixelHSV`, `blend()`, `nscale8()`, ...) in every env.
  Do not use FastLED's `CRGB`/`CHSV` or lib8tion in the clock plugin; outside the `!HAVE_NEOPIXELBUS` transport
  code it is not included.

## LED power after a crash or reset

- WS2812 LEDs keep their last frame until they get new data or lose power. Boards with a standby pin
  (`IOT_LED_MATRIX_STANDBY_PIN`, MOSFET/relay) switch the LED power off where the transport cannot be used:
  `Clock::LedPower::bootOff()` at boot before the safe mode is selected (ESP8266 `preinit()`, ESP32 start of
  `setup()`), `Clock::LedPower::crashOff()` in the ESP8266 crash callback (`ClockPluginClearPixels()`, the name is
  kept for `lib/KFCLibrary`'s `save_crash.cpp`) and in the ESP32 panic handler (`__wrap_esp_panic_handler`, needs
  `-Wl,--wrap=esp_panic_handler` in the env). `crashOff()` writes registers only - no Arduino, RTOS or flash
  calls in that path. Boards without a standby pin do nothing; the data lines are left floating.
- Between the reset and `bootOff()` the pin is an unconfigured input: the MOSFET gate needs a pull resistor to the
  "off" level on the board.
- `ClockPlugin::clearPixels()` blanks the pixels with the transport (restart, `ClockPluginShutdownPixels()`);
  `+LMC=cl` does the same from the console.

## Environment `wled_esp32_controller_neopixelbus`

- Same board, partition table and filesystem as `wled_esp32_controller`, but the LED transport is **NeoPixelBus**
  (`HAVE_NEOPIXELBUS=1`) instead of FastLED. **FastLED is not part of this env** (no `lib_deps` entry): the pixel
  type and color math come from `pixel_color.h`, `<FastLED.h>` is only included under `!HAVE_NEOPIXELBUS`
  (`clock_base.h`) and every `FastLED.` call in the clock plugin sits in such a branch. The brightness lives in
  `PixelDisplay::_brightness` (`dump()` reads `getBrightness()`). Keep it that way - new `FastLED.` calls need the
  same guard. `HAVE_FASTLED` is unrelated: it only switches the built-in WS2812 LED.
- Show method is a runtime toggle (`+LMC=met,nrmt|ni2s`, Display Method form), default **NeoPixelBus RMT**.
  `ShowMethodType::FASTLED` stays in the enum (the value is stored in the config, removing it would renumber
  NEOBUS_RMT/I2S) but cannot be selected: `_sanitizeConfig()` and `_setShowMethod()` map it to NEOBUS_RMT,
  `_toggleShowMethod()` skips it and `+LMC=met,fast` is not compiled in. All
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
  more) - `https://github.com/sascha432/NeoPixelBus.git#kfc-rmt1`, branch
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
- The status page only shows the show method and fps (plus the `<n> I2S + <n> RMT pins` split); the transport
  diagnostics are printed by `+LMC=fr` (sticky since boot): `rmt blocks`, `max. overrun` for the worst frame that
  took longer than its pixels need (the RMT memory ran empty and stale data was transmitted, i.e. visible
  flicker), `transmissions`/`over` for the segment transmissions that were late and `tx timeouts` for frames that
  never finished. `+LMC=st` prints the state, power level, IR receiver counters, deferred task queue and which
  diagnostics are compiled in.
- Frame timing in `+LMC=fr` (averages of the last frames, from `ClockPlugin::_loop()`): `wire` is the
  shortest frame the configured segments allow (sum of the segments at 30 us per pixel), `show` is what the loop
  really spends on the transport (filling + transmitting + stalls) and `anim` is the rest of the frame
  (animations, matrix/hexagon mapping, power limit) - the frame time is `anim` + `show`, so `show - wire` is the
  loss inside the transport and `anim` is the CPU cost of the animation. `max. write` is the longest time the
  driver needed to translate and copy a frame into the RMT memory (that time is part of the frame, not of the
  transmission). `max. overrun`/`over` compare the transmission window (end of the write until the
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
    collection): the transport recovers on the next frame and only the `max. overrun` (`+LMC=fr`) maximum grows, so this is
    not a bug to fix. Freeing filesystem space or stopping the log writer only reduces the stutter. A growing
    `max. overrun` (`+LMC=fr`) *without* WebUI or filesystem activity would mean the interrupt latency is above what the RMT
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
  FastLED's fork-only power management is not compiled in. Temporal dithering does not exist (NeoPixelBus has none):
  the form field and `+LMC=dit` are not compiled in, `getDither()` returns `false`; the `dithering` config bit is kept.
