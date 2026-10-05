# Profiling report — Home Assistant dashboard (WT32-SC01), 2026-10-03

Measured on 2026-10-03, builds **15346-15354**, env `wt32_sc01_test1`. Every number in this file
comes from a real measurement of the device, not from an estimate. The screens, the groups and the
A/B builds were measured with 15346-15348; the loop function profile (§5) needs 15351; the log
file/watchdog A/B (§8) is 15351 (the firmware pass 0 ran on), 15352, 15353 and 15354. Raw captures:
`logs/perf_sweep_*.txt`, `logs/perf_loopfn*.txt`, `logs/perf_pass0..3.txt`, `logs/perf_logfs.txt`,
`logs/perf_nochk.txt` (gitignored).

This report is a historical record: the monitoring build that produced the numbers has been removed
from the firmware again, the two fixes it led to are in the tree (§5, §8). It merges the former
`profiling.md` (the report) and `PERF_MONITOR.md` (the definitions of the groups), the duplicated
tables of the two are kept once.

## 1. Setup

| Item | Value |
| --- | --- |
| Panel | ST7796S 480x320, HSPI **write only** (3 wire, no MISO), 16 bpp, RGB565 byte swapped (`LV_COLOR_16_SWAP 1`) |
| Panel clock | 80 MHz (`IOT_WT32_SC01_TFT_SPI_FREQUENCY`) = 10 MB/s ideal |
| Draw buffers | 2 x 153600 px (`LVGL_BUFFER_LINES = IOT_WT32_SC01_TFT_HEIGHT`, one full screen each) in **PSRAM** |
| Flush | blocking `pushImage()` - a PSRAM buffer cannot be read by the SPI DMA, so no DMA and no overlap of render and transfer |
| LVGL | 8.4, `LV_DISP_DEF_REFR_PERIOD = 5` ms, `LV_USE_PERF_MONITOR = 1`, 32 KB heap pool in PSRAM |
| Main loop | Arduino `loop()` = every loop function on every iteration + the event scheduler after each |
| Measured loop rate | 3700-11000 iterations/s depending on the log file and the watchdog fix (§8), period mean 0.09-0.29 ms |

### The panel arithmetic (the hard ceiling)

| | pixels | bytes | wire time at 80 MHz |
| --- | --- | --- | --- |
| full screen 480x320 | 153600 | 307200 | **30.7 ms** |
| one 4x2 grid cell 110x148 | 16280 | 32560 | 3.3 ms |
| one 4x3 area cell 110x92 | 10120 | 20240 | 2.0 ms |

Measured transfer rate: **8.0-8.8 MB/s of the 10 MB/s** the clock allows (80-88 %), the rest is the
per call protocol overhead (set window + commands). A refresh that invalidates most of the display
therefore costs at least ~31 ms of transfer alone, which is the ceiling of the screens that draw a
large area (panels and the quick settings sheet).

## 2. The instrument and how to read it

All numbers are one second averages: one display refresh split into its groups (`[perf] refresh`),
and one main loop iteration split into its groups (`[perf] loop`).

* **The on screen FPS of LVGL is not a frame rate.** LVGL 8.4 (`src/core/lv_refr.c`,
  `LV_USE_PERF_MONITOR`) computes `1000 * frames / elapsed_ms` over refreshes that invalidated **more
  than 5000 px**, where `elapsed` is the duration of that one refresh (render + flush). `20 fps` means
  *the refresh took 50 ms*, and cheap refreshes are not counted at all. The `[perf] refresh ... lvgl X
  fps` field printed the same number next to the groups, so screen and log could be compared.
* `refresh/s` is the real rate of counted refreshes and `dirty` the average invalidated area - both are
  what turns a per refresh time into a visible impression.
* The `[perf] loop` groups are **milliseconds per second of wall time** and add up to ~1000, so the
  share of each group is directly readable; a per iteration average would hide the groups that only
  run a few times per second (§10). `[perf] refresh` values are **per refresh**.
* The per callback (`[perf] loopfn`) numbers carry roughly **±20 %** uncertainty: the two `micros()`
  calls are part of the measured interval. `displayLoop` reads ~335 ms/s from the outside while its
  own groups sum to ~283 ms/s. Ranking and shares are stable, absolute values are not exact.
* The numbers depend on the state of the dashboard (how many tiles changed since the last refresh),
  so compare `draw`/`flush` only between phases with a similar `dirty`.

### Groups of one refresh

| Group | What it covers | Where it was measured |
| --- | --- | --- |
| `draw` | LVGL renders the invalid area into the draw buffer: fills, alpha blends, rounded corner and arc masks, font/MDI glyph decode, images | `render_start_cb` until the first flush |
| `flush` | the transfer to the panel (`pushImage()`, blocking) | inside the flush callback |
| `overhead` | the part of `flush` that the CPU driven transfer adds on top of the ideal wire time (`pixels * 2 bytes * 8 / panel clock`) - the price of a PSRAM draw buffer the SPI DMA cannot read | derived |
| `MB/s` | achieved transfer rate of `flush` | derived |
| `dirty` | invalidated pixels per refresh, the number the on screen FPS uses | `monitor_cb` |

### Groups of one main loop iteration

`loop` and `iter` describe one iteration, `rep` the cost of one summary, the rest are ms/s:

| Group | What it covers | Where it was measured |
| --- | --- | --- |
| `loop` | the period of one main loop iteration (everything, the groups below are the parts of it) | entry of `displayLoop()` |
| `iter` | iterations of the lvgl loop function per second; a small number means the loop is blocked (compare with `other`) | derived |
| `rep` | the cost of the summary itself (the logger can block on a flash write); removed from the period of the next iteration, otherwise the instrument would report itself | around the two summary lines |
| `lvgl` | `lv_timer_handler()` without `draw`/`flush`: animations, timers, style invalidation, layout | around `WT32_SC01::loop()` |
| `ui` | `HassScreen::update()` without the rebuilds and the HA update | `HassScreen::update()` |
| `rebuild` | a widget tree is built or destroyed (page, panel, quick settings) | `HassScreen::create()`/`_rebuild()` |
| `ha` | `HomeAssistant::Dashboard::update()` | `Dashboard::update()` |
| `in` | touch read (FT6336U over I2C) and the gesture handling | `LVGLScreenManager::_handleInput()` |
| `tick` | `LVGLScreenManager::tick()` outside the input handling: the auto rotation and the update interval check | around `screenManager.tick()` |
| `rest` | the lvgl loop function outside the scopes above (the screenshot debug hook and the power/brightness loop) | around `displayLoop()` |
| `other` | the rest of `loop`: WiFi, AsyncWebServer, MQTT, the other plugins and any idle time | derived |

`cpu` is LVGL's own `100 - lv_timer_get_idle()` (it only counts the time inside `lv_timer_handler`).

### The loop function lines

Each registered loop function was timed with two `micros()` calls and reported in two extra lines:

```
[perf] loopfn 0 id=3ffc68bc          (once per boot, plus name= for the ones that register a name)
[perf] loopfn ms/s: total=1000.3 0=240.4 1=15.5 2=5.1 3=334.8 4=48.5 sched=305.7 n=5
```

`total` is the wall time of the main loop (1000 ms/s, the instrument cost is included in `rest`), the
indices are the position in the loop function vector and `sched` the sum of **all** scheduler passes
(the one after every callback plus the final one). The ids of the measured build: `0` =
`&serialHandler`, `1` = `KFCFWConfiguration::loop`, `2` = MDNS, `3` = the LVGL `displayLoop`, `4` =
weather2. They were resolved from the ELF (`xtensa-esp32-elf-nm`): `0x3ffc68bc` = `serialHandler` (a
global object in DRAM), `0x3ffc5f18` = the weather2 `plugin` (which is how the naming was validated).

## 3. Per screen, steady state

From `logs/perf_sweep_default.txt` (per refresh, 480x320 landscape, builds 15346-15348):

| Screen | dirty | draw | flush | draw+flush | MB/s | counter shows |
| --- | --- | --- | --- | --- | --- | --- |
| main grid (first area) | 20.7 kpx | 13.2 ms | 4.9 ms | 18.1 ms | 8.0 | 53 fps |
| `Rooms` area page | 31.9 kpx | 13.8 ms | 7.2 ms | 21.0 ms | 8.4 | 47 fps |
| sensor panel (idle, graph drawn once) | 8.2 kpx | 8.4 ms | 2.0 ms | 10.4 ms | 7.7 | 87 fps |
| dimmer/light panel - level | 66.3 kpx | 32.3 ms | 14.9 ms | 47.2 ms | 8.5 | 21 fps |
| dimmer/light panel - color wheel | 49.0 kpx | 40.8 ms | 11.0 ms | 51.8 ms | 8.5 | 19 fps |
| dimmer/light panel - color temperature | 58.8 kpx | 24.1 ms | 13.2 ms | 37.3 ms | 8.5 | 27 fps |
| dimmer/light panel - effects | 71.6 kpx | 21.3 ms | 16.0 ms | 37.3 ms | 8.5 | 27 fps |
| climate panel - arc | 54.4 kpx | 27.6 ms | 12.3 ms | 39.9 ms | 8.5 | 25 fps |
| climate panel - mode list | 60.9 kpx | 17.4 ms | 13.5 ms | 30.9 ms | 8.6 | 32 fps |
| climate panel - preset list | 60.4 kpx | 21.5 ms | 13.4 ms | 34.9 ms | 8.6 | 29 fps |
| climate panel - fan list | 61.8 kpx | 18.0 ms | 13.7 ms | 31.7 ms | 8.6 | 31 fps |
| **quick settings - tiles** | 44.7 kpx | **74.1 ms** | 10.5 ms | 84.6 ms | 8.1 | **12 fps** |
| quick settings - editor | 20.9 kpx | 25.8 ms | 5.0 ms | 30.8 ms | 7.9 | 32 fps |

`draw` costs **0.5-0.65 us per invalidated pixel** (~120-160 cycles at 240 MHz; 0.49 us/px on the
light panel, 0.64 on the main grid) and **1.66 us/px** on the quick settings sheet. That sheet is the
worst screen by far: its root is a full screen container with `LV_OPA_60`, so every redrawn pixel of
the tile grid behind it is alpha blended instead of copied. It is also the screen whose counter swings
the most (10-80 fps).

Only the *invalidated* area is paid for - the panels invalidate 49-72 kpx per refresh, the grid pages
8-32 kpx. That single number separates fast and slow screens, not the widget count.

### Transition cost (what a tap costs)

Measured in the `[build]` window right after the request, before the values settle. The window is
4 s long and contains exactly one rebuild, so the *peak* one-second value is the cost of that
rebuild (the mean is the cost divided over the 3-4 reports of the window):

| Transition | rebuild, peak 1 s | mean over the window |
| --- | --- | --- |
| main grid (page 0) | **83 ms** | 27.7 ms/s (3 reports) |
| `Rooms` page (grid with 12 tiles) | **84 ms** | 84.2 ms/s (1 report) |
| dimmer/light panel (level) | **89 ms** | 29.6 ms/s (3 reports) |
| dimmer/light panel (color wheel) | **67 ms** | 22.4 ms/s (3 reports) |

So a page *or* panel switch costs **67-89 ms** of widget tree construction and first paint before
anything is visible - the panel is not cheaper than the page, both rebuild nearly the whole display.
In the steady state `rebuild` is 0.

## 4. Where the wall time goes

Milliseconds per second, three screens (`logs/perf_sweep_default.txt`):

| group | main grid | light panel (level) | quick settings (tiles) |
| --- | --- | --- | --- |
| `other` (everything outside the lvgl plugin) | 651.6 | 560.1 | 463.0 |
| `draw` (LVGL renders into the buffer) | 85.8 | 201.8 | 348.5 |
| `lvgl` (timer handler: animations, layout, style) | 102.8 | 70.1 | 66.8 |
| `flush` (transfer to the panel) | 32.0 | 92.8 | 49.5 |
| `rest` (loop fn outside its scopes) | 53.6 | 36.5 | 32.6 |
| `tick` (screen manager) | 40.0 | 26.9 | 24.0 |
| `ha` (dashboard update) | 18.5 | 13.8 | 11.8 |
| `in` (touch + gestures) | 8.7 | 6.1 | 5.5 |
| `ui` (HASS screen update) | 7.3 | 4.8 | 10.9 |
| `rebuild` | 0.0 | 0.0 | 0.0 |
| sum | 1000.3 | 1012.8 | 1012.6 |

The sums overshoot 1000 by ~1.3 % - the window correction for the summary's own cost is not applied
to the per group values. Treat the table as shares.

**Reading it:** the LVGL work is 220-465 ms/s, the rest of the main loop ~460-650 ms/s (WiFi,
AsyncWebServer, MQTT/websocket, the logger, the other plugins). On the level panel a refresh costs
47 ms of LVGL work, but only ~2.5 of them fit into a second because the rest of the time is taken:
the refresh rate (6.3-6.5 per second everywhere) follows from that split, not from the panel.

## 5. Inside `other`: the main loop callbacks

`src/kfc_firmware.cpp:loop()` calls every registered callback on every iteration, and
`__Scheduler.run(Event::PriorityType::NORMAL)` **after each of them** plus once more at the end
(6 passes per iteration with 5 callbacks).

First area page, build 15351, 5283 iterations/s, 5 callbacks. Mean over a 47 s capture:

| item | ms/s | what it is |
| --- | --- | --- |
| `#3` `displayLoop` | **334.8** | all LVGL work (equals `lvgl+draw+flush+tick+rest+ui+in`) |
| `sched` event scheduler | **305.7** | 6 calls per iteration, see below; the sum of the list traversals **and** the work of the timers that were dispatched |
| `#0` `&serialHandler` | **240.4** | SerialHandler loop function |
| `#4` `weather2(data+ha)` | 48.5 | `DataSource::update()` + `Dashboard::update()` |
| `#1` `KFCFWConfiguration::loop` | 15.5 | |
| `#2` `MDNSPlugin::loop` | 5.1 | |
| loop bookkeeping + instrument | 50.3 | the loop body outside the callbacks |
| total | 1000.3 | = one wall second |

### What the 6 scheduler calls are (and are not)

`loop()` calls the scheduler 6 times per iteration, but they are **two different functions** and no
timer callback is ever invoked twice per traversal:

| call | function | cost |
| --- | --- | --- |
| 5x, `__Scheduler.run(NORMAL)` before every callback | `Scheduler::_run(PriorityType)`, `Scheduler.hpp:112` | `if (_hasEvent > runAbovePriority)` - O(1) when nothing above `NORMAL` is pending. Otherwise **one** traversal of `_timers` that invokes only the timers with `_priority > NORMAL` whose `_callbackScheduled` is set (`_callbackScheduled` is cleared before the invoke, so at most once each) |
| 1x, `__Scheduler.run()` at the end | `Scheduler::_run()`, `Scheduler.cpp:161` | one traversal invoking the due timers with `_priority <= NORMAL` (budget `kMaxRuntimeLimit` = 250 ms), then a **second traversal that only recomputes `_hasEvent`** - it invokes nothing |

So "walks the list twice" applies to the once-per-iteration `_run()` only, and the second walk is
a read-only scan. `PriorityType` is `NONE=-127, LOWEST=-64, LOWER=-32, LOW=-16, NORMAL=0, HIGH=16,
HIGHER=32, HIGHEST=64, TIMER=126` (`lib/KFCLibrary/KFCEventScheduler/include/Event.h`), so
`_hasEvent > NORMAL` is true as soon as any HIGH or TIMER event is pending - which is the normal
case here, so the 5 in-loop calls do traverse the list.

The measured `sched` value is **not pure traversal overhead**: it contains the execution of every
timer the scheduler dispatched in that second (the logger's flush timer is one of them, see the
alternating pattern below).

> Note for anyone re-measuring: the **first** `[perf]` window after a flash spans the rest of
> `setup()` and shows `iter 4/s`, `period 260 ms`. That is not a stall, the loop is not running yet.

### Two alternating patterns - it is the log file (§8 is the A/B proof)

The steady windows are not uniform, they alternate between two mixes (both totalling 1000 ms/s).
Pass 3 (log file on, watchdog fix, HA websocket down), 14 consecutive windows:

| second | `iter` | `period` | `sched` | `#0` serialHandler | `#3` displayLoop |
| --- | --- | --- | --- | --- | --- |
| light | 9300-11100 | 0.09-0.10 ms | **43-56** | 253-304 | 372-490 |
| heavy | 3700-5800 | 0.17-0.27 ms | **500-585** | 100-160 | 232-294 |

The logger's queue flush is an `Event::Timer`, so the LittleFS write is dispatched **inside**
`__Scheduler.run()` and lands in `sched` - half a second of the second is nothing but the file write.
`#0` and `#3` are per-iteration costs and simply scale with the iteration rate, `#4` weather2 with
the work it does.

`setFileLogging(false)` in the same firmware removes the alternation completely (`sched` 54 ms/s
constant, `iter` 11000/s, §8). The 2:1 pattern is the log file - not the drawing, not the watchdog.

### Nothing blocks a single call, but a second can

`period` is the **window average** (1000 ms / iterations), it says nothing about the distribution
inside a window. It stays at 0.09-0.29 ms mean and 0.11-0.44 ms p95 across the four passes of §8, so
the loop completes thousands of short iterations per second in every state - but a single long call
inside one window would be invisible here, it would only show up as fewer iterations in that window.
That is exactly what the log write does: 0.5 s of every other second (§8, where the per block
measurement shows it is one long call).

What is certain: the loop spins at 3700-11000 iterations/s and pays fixed per-iteration overhead
(6 scheduler passes + 5 callbacks + `lv_timer_handler`). That overhead is 600-700 ms/s and it caps the
display at 3.7-6.5 refreshes/s.

### The SerialHandler watchdog defect (fixed, cost measured in §8)

`src/serial_handler.cpp` (`Wrapper::_loop()`, registered as a loop function), before:

```cpp
esp_err_t err = esp_task_wdt_status(NULL);
if (err == ESP_ERR_NOT_FOUND) { esp_task_wdt_add(NULL); deleteWdt = true; }
... _pollSerial(); _transmitClientsRx(); _transmitClientsTx();
if (deleteWdt) { esp_task_wdt_delete(NULL); }
```

It **subscribed the task to the Task Watchdog and removed it again on every loop iteration** - three
TWDT calls with their spinlocks, 5300-11000 times per second, for nothing. An early estimate of
~45 us per iteration was too high; the measurement in §8 (pass 1 against pass 2, same screen, same
flags) is **22.2 us on every iteration**, 17 % of the loop at the time.

The task is now subscribed once in `addLoop()` and unsubscribed in `removeLoop()`
(`include/serial_handler.h`) and fed by a single `esp_task_wdt_reset()` at the top of `_loop()`.
The three `esp_task_wdt_reset()` calls inside the transfer code stay - they keep the watchdog fed
during a long transfer.

## 6. Why the FPS number swings (50 -> 20 fps)

It is not the frame rate, it is the invalidated area. Same screen, one capture
(`logs/perf_loopfn2.txt`, 47 refreshes):

| invalid area | refreshes | draw | flush | counter shows |
| --- | --- | --- | --- | --- |
| 10 kpx (a few labels) | 26 | 5.5 ms | 2.2 ms | ~131 fps |
| 30 kpx (two tiles) | 9 | 17.4 ms | 7.6 ms | ~40 fps |
| 40 kpx | 6 | 21.2 ms | 9.6 ms | ~32 fps |
| 50 kpx | 2 | 23.8 ms | 11.2 ms | ~29 fps |
| 80 kpx (page/panel) | 4 | 39.6 ms | 17.7 ms | ~17 fps |

* `draw` is a linear function of the invalidated area (~0.5 us/px) in every bucket, `flush` tracks it
  at 8.4-8.6 MB/s.
* The number of counted refreshes per second stays at 3.6-6.5 the whole time.
* `dirty p95` is ~3x the mean (25 vs 80 kpx): most refreshes are small, a few are huge.

So a single value change in a distant tile can grow the bounding box from 10 to 80 kpx and the
displayed counter drops by a factor of 8 without anything being slower.

### The second cause of the swing: the log file stalls the whole loop

The table above is about the cost of **one refresh**. The refresh **rate** swings for a different
reason: while the log file is enabled, half a second of every other second is spent inside
`__Scheduler.run()` writing to LittleFS (§5, §8), and in those seconds the loop runs half as many
iterations (pass 3: 3700-5800/s against 9300-11100/s). Fewer iterations means `lv_timer_handler`
runs less often, invalidations pile up (dirty 20.9 kpx against 16.6 kpx with the log file off) and
the counter drops with them. Both effects hit the same screen at the same time, which is why the
number looks so unstable.

## 7. A/B: PSRAM vs internal RAM vs DMA

Same screen (dimmer/light panel, level view, ~66 kpx dirty), per refresh:

| draw buffer | flush | draw | MB/s | overhead | counter |
| --- | --- | --- | --- | --- | --- |
| PSRAM x2, blocking `pushImage()` (default) | 14.9 ms | 32.3 ms | 8.5 | 1.62 ms | 21 fps |
| internal RAM x2 (16 lines), blocking | 15.4 ms | 28.1 ms | 8.0 | 2.45 ms | 23 fps |
| internal RAM x2 (16 lines), `pushImageDMA()` | 14.4 ms | 28.0 ms | 8.8 | 1.10 ms | 23 fps |

* **DMA buys ~3 %** on the flush and the PSRAM buffer costs ~13 % on `draw`.
* The 16 line internal buffers flush in ~20 chunks, which *lowers* throughput (8.0 MB/s) and adds
  per call overhead (2.45 ms) - a small buffer is worse than one full screen buffer.
* The transfer is wire limited, not CPU limited, so the remaining ~15 % is protocol overhead.

Memory bandwidth (one time `memcpy` benchmark, 8 KB blocks, reported by the monitor):

| | MB/s |
| --- | --- |
| PSRAM -> PSRAM | **299.6** |
| internal RAM -> internal RAM | **361.0** |

The PSRAM is only 17 % slower for a bulk copy, which is why moving the draw buffer changes `draw` by
13 % and not by a factor.

LVGL heap: **1344 B used of the 32 KB pool (5 %), 1 % fragmentation** - the pool size is generous.

## 8. A/B: the LittleFS log file and the SerialHandler watchdog fix

Four passes of the same firmware on the same screen, 43-44 windows each: `logs/perf_pass0.txt` ..
`logs/perf_pass3.txt`. Pass 0 was captured while HA was still connected (live values, 28.5 kpx dirty),
the other three with the HA websocket down (placeholder tiles, 16.6-20.9 kpx) - the websocket proxy
(port 9123) refused connections from then on. The clean pairs are therefore 1/2 (log off, watchdog
fix) and 2/3 (watchdog fix, log file), each pair differing in exactly one variable.

| pass | file log | wdt fix | live data | `iter` | `period` mean | p95 | `sched` |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 0 | on | no | yes | 4148/s | 0.29 ms | 0.44 | 301 ms/s |
| 1 | off | no | no | 7524/s | 0.13 ms | 0.15 | 50 ms/s |
| 2 | off | yes | no | **11042/s** | **0.09 ms** | 0.11 | 54 ms/s |
| 3 | on | yes | no | 7231/s | 0.19 ms | 0.27 | 322 ms/s |

Per loop iteration - the honest unit for a saturated loop, because the groups always sum to
1000 ms/s: a change can only ever show up as more iterations, never as idle CPU.

| pass | per iteration | `#0` serialHandler | `#3` displayLoop | `sched` |
| --- | --- | --- | --- | --- |
| 0 | 241 us | 51.8 us | 95.1 us | 72.6 us |
| 1 | 133 us | 50.9 us | 51.3 us | 6.6 us |
| 2 | **91 us** | **28.7 us** | 40.3 us | 4.9 us |
| 3 | 138 us | 27.6 us | 47.8 us | 44.5 us |

### The watchdog fix is worth 22 us on every single iteration

Pass 1 -> 2 (identical screen, identical flags): `#0` 50.9 -> 28.7 us per call, i.e. **22.2 us per
iteration**, ~17 % of the loop, `iter` 7524 -> 11042 (+47 %). Pass 3 confirms 27.6 us with the log
file on - the remaining cost is now independent of the iteration rate, it is the polling work itself.
The old code paid the constant on every iteration, 5300-11000 times per second.

### The log file costs 250-270 ms/s, ~25 % of the loop, and causes the 2:1 alternation

Pass 2 -> 3 (identical screen and flags, only the file logging differs): `sched` 54 -> 322 ms/s,
`iter` 11042 -> 7231/s. In pass 3 the `sched` value is 43-56 ms/s in one second and 500-585 ms/s in
the next, forever (§5). Pass 0 -> 1 shows the same magnitude (301 -> 50 ms/s) in a completely
different screen state, so the number is not a screen artifact.

The mechanism is the filesystem, not formatting the messages: `_flushQueue()` runs as an
`Event::Timer` and therefore executes **inside** `__Scheduler.run()` - that is why it lands in `sched`
and in no callback. It is not the amount of data either: the capture contains 150 log lines, 140 of
them the monitor's own summary (3.4/s), so roughly 250 ms of CPU per second is spent on 4 short
lines. The block measurement below shows what it is: the **free space check** (`getFSInfo()`) before
the write and the LittleFS **metadata commit** on the close. Both of them read or erase flash, and an
erase on the ESP32 **disables the flash cache**, which stalls code fetches and interrupts for its
whole duration.

Consequences:

* Every measurement of the loop or the FPS has to keep the log file in mind: with it enabled the
  monitor writes its own summary to flash and largely measures that write - that is most of what
  "pattern A" was. `Logger::setFileLogging(false)` switches the file output off at runtime, the
  serial output is not affected.
* The same lever applies in production without the monitor: any NOTICE-level traffic that reaches the
  log file costs ~250 ms/s while it is being written, i.e. the GUI freezes for half a second every
  other second. The cost is per **commit**, not per byte, so the lever is fewer flushes (a longer
  `writeDelay`), not less text.
* The watchdog fix is a plain win and cannot make anything slower.

### Inside the log write: the free space check is 78 % of it, the write itself is nothing

The file write path was measured per block (the four blocks of `_flushQueue()` as they were marked in
`src/logger.cpp` at the time), same screen, `logs/perf_logfs.txt`, 41 windows:

| block | what it does | ms/s | per flush |
| --- | --- | --- | --- |
| block1 `space` | `getFSInfo()` - the free space check before writing anything | **402.5** | ~400 ms |
| `_closeLog` `close` | `File::close()`, plus a rotation if the file is too big | 82.0 | ~80 ms |
| `__openLog` `open` | `createFileRecursive()` | 30.7 | ~31 ms |
| block3 `write` | `File::write()` + `println()` per message | **0.2** | ~0.2 ms |
| block3 `yield` | `optimistic_yield(10000)` | 0.0 | - |
| block2 `sort` | `tmp.sort()` of the queue (4 messages) | 0.0 | - |
| | `_flushQueue()` total | **517.4** | ~500 ms |

* 1.0 flushes/s, 4.2 messages/s, **worst single flush 685.8 ms** - it is one long call, not many
  short ones.
* The write is free, and the sort and the yield do not matter. `getFSInfo()` is the cost: on the
  ESP32 core **both** `totalBytes()` and `usedBytes()` call `esp_littlefs_info()`, i.e. the
  filesystem is traversed **twice** per flush, reading every block with the flash cache disabled.
  That is the half second.
* `_closeLog()` is the LittleFS metadata commit (~80 ms for one file close, cache disabled as
  well) - the messages only reach the file cache until then.
* The decision that followed: the free space check is **removed** - a full traversal of the file
  system for a protection that only has to catch a nearly full one. The LittleFS divide-by-zero in
  `lfs_alloc()` that it guarded (the ESP32 core should return `LFS_ERR_NOSPC` instead) is unguarded
  again, so a full file system means the log files have to be cleared.
* The same firmware measured 517 ms/s here against 322 ms/s `sched` in pass 3: the traversal cost
  grows with the contents of the filesystem, so the longer the device has been logging, the more of
  the loop it loses. Treat any log-file number as "size at the time of the measurement".

**After removing the check** (`logs/perf_nochk.txt`, 47 windows, same screen): the whole path is
**115.6 ms/s**, the worst single flush 281.0 ms against 685.8 ms - exactly the 402 ms/s of block1 is
gone, `close` (81.3), `open` (33.8) and `write` (0.1) are unchanged. `sched` fell from 538.5 to
155.7 ms/s in that capture. The loop rate is not directly comparable there (the HASS screen was busy:
`ui` 11.1 ms/s, `draw` 145.9 ms/s, dirty 29.8 kpx against 20.4 in the capture before), but the
per flush numbers are load independent.

What is left is the LittleFS metadata commit on every `_closeLog()` (~80 ms/s, ~8 % of the loop)
plus the open (~34 ms/s). Flushing less often is the only remaining lever for that part.

## 9. Ranked conclusions

1. **The main loop, not the display, is the limit.** 600-700 ms/s are spent outside the LVGL plugin
   while the display gets 220-465 ms/s. The refresh rate is 3.7-6.5/s on every screen because of that.
2. **The logger's file write path was 517 ms/s, and 402 ms/s of it the `getFSInfo()` free space
   check** - the ESP32 core implements both `totalBytes()` and `usedBytes()` with
   `esp_littlefs_info()`, i.e. the whole filesystem is traversed twice per flush with the flash
   cache disabled. The check is **removed** and 115.6 ms/s are left (`_closeLog` 81 ms/s = the
   LittleFS metadata commit, `__openLog` 34 ms/s, the actual `File::write()` 0.2 ms/s), §8.
   `setFileLogging(false)` removes the rest.
3. **The SerialHandler subscribed/unsubscribed the Task Watchdog every loop iteration** - 22.2 us on
   every iteration, ~17 % of the loop. Fixed in `include/serial_handler.h` / `src/serial_handler.cpp`
   and verified in §8.
4. **The event scheduler is called 6 times per loop iteration** (5x `_run(NORMAL)` + 1x `_run()`) and
   the timer list is traversed on each of them - 5-73 us per iteration depending on whether the
   logger's timer fires. `_run(NORMAL)` is O(1) only when nothing above `NORMAL` is pending.
5. **`draw` is 0.5-0.65 us/px** and dominates the LVGL cost; the only lever is a smaller invalidated
   area (2.5x on the quick settings sheet because of the `LV_OPA_60` full screen root).
6. **Do not spend effort on the flush path**: 8.0-8.8 MB/s of 10 MB/s, DMA ~3 %, buffer location
   ~13 % on `draw`. The transfer is not the problem.
7. **A tap costs 67-89 ms** of widget tree rebuild and first paint before anything is visible.

## 10. Measurements that were wrong (do not repeat them)

* **Sampled loop function timing.** Timing every 16th iteration and scaling up did not add up
  (1400 ms/s of "work" in a 1000 ms second) - the samples aliased with the periodic refreshes. The
  final instrument measures every iteration.
* **Microseconds against a millisecond constant.** The first report window compared a microsecond
  delta with `kWindowMs = 1000`, producing 2.6 summaries per second and meaningless `period`/`other`
  values (a "295 ms loop" that was an artifact).
* **Mixing window totals with per-refresh values** in the same expression inflated the derived
  `non-dma overhead` by ~8x.
* **Placing the transition marker after the HTTP request.** `/lvgl-screen.bmp` applies the values
  while the request is in flight, so the rebuild landed in the excluded window and `rebuild` read
  0.00 ms/s everywhere. The marker has to be written before the request.
* **Per-iteration averages for the loop groups.** They hide everything that runs a few times per
  second (`ui` at 5 Hz, `rebuild` once) - hence the switch to milliseconds per second.
* **Concluding "nothing blocks" from the `period` p95.** `period` is the window average
  (1000 ms / iterations). A 500 ms stall inside a window only lowers that window's iteration count
  and shows up as a slightly worse average - it is indistinguishable from many short iterations. It
  hid the 0.5 s LittleFS stall for several captures (§5, §8) and produced the too high "45 us per
  watchdog call" estimate as well.

## Appendix: tile and page indices

Not a measurement, but noted while the PC tools of the measurements were written: a PC tool
addresses the tiles and pages by the index the firmware assigns. `/hass.yaml` is parsed top to bottom
(`Config`, `hass_config.cpp`), every `- ` list item appends exactly one tile, so the `TileIndex` is
the position in the file in depth first pre-order: an area comes first, then its own tiles, then its
next sibling - a subtree is a contiguous range. An area with a `tiles:` block gets the next page
index at the same moment, so `_pages[1..n]` follow the same order (`_pages[0]` is the main page).
Nothing is renumbered afterwards.
