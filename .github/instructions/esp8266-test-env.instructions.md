---
applyTo: "conf/envs/led_strip.ini,scripts/tools/webui_*.py,include/web_server.h"
description: "The ESP8266 test env led_strip_xmas_serial, the WebUI test suite, heap and leak diagnostics"
---

# Environment `led_strip_xmas_serial` (ESP8266 test env)

The test rules for devices (no form saves, no actions, MQTT, mDNS) are in
[`.github/copilot-instructions.md`](../copilot-instructions.md), section "Build, flash and verify".

- NodeMCU (`board = nodemcuv2`), **serial only, no OTA**; use the env's `monitor_port` (`conf/envs/led_strip.ini`).
  It extends the ESP-01 LED strip base but unflags the standby pin 3 and `ESP8266_USE_UART_RX_AS_OUTPUT`, so the
  UART RX and the serial console work - keep those unflags.
- The **HLK-LD2420** radar (OT1 on D7, RX on D4) borrows UART0 RX while a
  `+LD2420` command runs, including the read about 1 s after boot: the serial console ignores input meanwhile and
  prints the held output afterwards. Send the next serial command after the previous one has answered.
- **1024 pixels on purpose**: a low-memory demo (~20 KB idle heap) for the heap guards of the web server
  (`WEBSERVER_*_MIN_FREE_*` in `include/web_server.h`). The visualizer forms (~14 KB to build) only fit while
  the heap is idle and are answered with 503 otherwise - expected, not a regression. Big forms are split into
  pages, as the network form is (one page per WiFi station, `/network/station-<n>.html`).
- This is the **ESP8266 verification env** for shared/core changes: build it next to the ESP32 env.
- **WebUI test suite** (workspace venv): `.venv\Scripts\python scripts/tools/webui_test.py --env led_strip_xmas_serial
  --newlib` (`--env wled_esp32_controller_neopixelbus` for the ESP32 board, both `--env` at once work). It checks
  uptime and free heap after every form and group and reports `reboot detected` with the cause; `--json <file>`
  stores the results, the docstring lists the groups. Intermittent problems need **at least 12 runs**
  (`--repeat 12`) with the serial port logged. A 503 of the heap guards is a warning, not a failure.
  The suite only sends invalid form POSTs (every real field missing, the firmware discards them) - keep it that way.
- **Heap diagnostics** (ESP8266, DEBUG build): `-D WEBSERVER_LOG_HEAP=1 -D UMM_STATS_FULL=1 -D UMM_INFO=1` adds
  `heap=<free>/<largest block> min=<low water mark>` to the access log and a `template done` line after every
  template page; `scripts/tools/webui_heap_analyze.py <serial log>` tabulates it per request. For a leak, read
  the free heap with `+HEAP=0` over serial before and after N requests (the status page value includes the
  page's own allocations), then take `+HEAPDUMP` before and after and compare with
  `scripts/tools/webui_heapdump_parse.py <a> <b>`: it lists the block kinds that grew. A flash address in such a
  block is resolved with `xtensa-lx106-elf-nm -C -n .pio/build/<env>/firmware.elf` (the first word of a C++
  object is its vtable + 8).
- **ESP8266 crash**: decoding the exception is described in
  [`.github/instructions/crash-debugging.instructions.md`](crash-debugging.instructions.md).
