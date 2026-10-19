# Handoff plan: remaining WebUI crash items

State 2026-10-19 02:30 PDT. Written for another Claude Code session picking up this work cold, it has no
memory of the two investigations that produced `docs/webui_crash_report.md`. Read that file first; it has the
background, the measured numbers and the OPEN/FIXED item numbers this plan refers to.

## Already done, do not redo

- **OPEN-4, OPEN-5, OPEN-6, OPEN-7 and FIXED-1..7 are closed**, see the report. OPEN-7 (the mDNS packet that
  rebooted every ESP8266 on the LAN) is committed (`39c87e0d`, `lib/ESP8266mDNS` 1.2.0-kfc2). Never test it
  with multicast packets again (memory `project-mdns-legacy-unicast-crash` if your harness has it).
- **OPEN-3 (ESP8266 heap exhaustion by the big forms) is as far as a guard can take it** - see below for what
  is left and why it is the owner's decision.
- **ESP32 build verification is done**: `wled_esp32_controller_neopixelbus` compiles with every shared change
  of both sessions (it did not before: `lib/ESP8266mDNS` was pulled into the ESP32 build by the LDF, fixed with
  `lib_ignore` in `conf/common_esp32.ini`). Build 15803 was flashed (`-t buildarchive`, archive in `elf/`) and
  ran `--newlib --parallel 1 --repeat 3`: 756 passed, no 503 of the heap guard, two crashes = OPEN-2 and the
  new OPEN-8, both AsyncTCP 1.1.1 (below). The *final* firmware of this session (build 15809 on the ESP8266)
  was only compiled for the ESP32, not flashed - the changes after 15803 are the form guard rework and the
  `write()` return value, nothing ESP32 specific.
- The test suite `scripts/tools/webui_test.py` handles the guard: a 503 is a WARN in the pages/forms/robustness
  groups, the "values unchanged" check skips a refused form, a non-JSON answer is a FAIL instead of a Python
  traceback (it aborted a 12 run test in run 7 before).

## Nothing is committed - run `git status --short` and `git -C lib/KFCLibrary status --short` first

Both repos have pending changes the owner has not reviewed. Some files are staged, some not - the owner did
that, leave the index alone. **Do not commit** unless the owner explicitly asks (standing rule, memory
`feedback-no-commits`). Files of this session on top of the previous one:

- `include/web_server.h`, `src/web_server.cpp` - the reworked guard (`_canCreateForm()`, `_canRenderForm()`,
  the thresholds `WEBSERVER_FORM_MIN_FREE_HEAP` / `..._MIN_FREE_BLOCK` / `..._CREATE_MIN_FREE_HEAP` with the
  measured reasoning in the header), a failed `config.write()` answers 503 instead of the "applying" page, the
  `WEBSERVER_LOG_HEAP` access log prints `heap=<free>/<largest block> min=<low water mark since the previous
  line>` (ESP8266 with `UMM_STATS_FULL`)
- `src/async_web_response.cpp` - `template done heap=... min=...` after every template response (`WEBSERVER_LOG_HEAP`)
- `include/kfc_fw_config.h`, `src/kfc_fw_config.cpp` - `KFCFWConfiguration::write()` returns the result
- `lib/KFCLibrary/KFCConfiguration/include/Configuration.h` - `hasNvsReadFailed()` getter (the rest of the
  OPEN-5 fix is committed in KFCLibrary as `24e5e05`)
- `conf/common_esp32.ini` - `lib_ignore` `ESP8266mDNS`
- `conf/envs/led_strip.ini` - diagnostics `UMM_STATS_FULL=1` + `UMM_INFO=1` added next to `WEBSERVER_LOG_HEAP=1`,
  the test env keeps its UART RX (`build_unflags` of the standby pin 3 and `ESP8266_USE_UART_RX_AS_OUTPUT`)
  (the file was LF in the working tree, it is CRLF again like the index - keep it CRLF)
- `scripts/tools/webui_test.py`, `CHANGELOG.md`, `docs/webui_crash_report.md`, this file

## OPEN-3: what the guard does now, and what is left (owner decision)

Measured on `led_strip_xmas_serial` (ESP8266, 1024 pixels, ~20-22 KB idle heap): building a form costs ~325
byte per field (`/network.html` 13.3 KB, the 44 field visualizer forms 13.7 KB), rendering it 3.1-3.6 KB more,
the error page of a POST 4.7 KB. The SDK crashes in the WiFi code when a beacon arrives and it cannot
allocate; the core aborts when a `new` fails while a form is built. Three guards in `src/web_server.cpp`:

1. `_canCreateForm()` before a form is built (GET, POST, WebSocket form action): free heap >= 16 KB
2. `_canRenderForm()` after the form, the template and the response exist: free >= 8 KB and largest block >=
   4 KB, and the configuration was read (`config.hasNvsReadFailed()` false, otherwise the form would show zeros)
3. a `config.write()` that did not return SUCCESS answers 503 (write() discards the changes when a parameter
   could not be read, KFCLibrary `24e5e05`)
4. `_canHandleRequest()` at the start of `_handleFileRead()`: below 4 KB free every file request is answered
   503 before the request path allocates anything (the core aborts on any failed `new`), and guard 2 applies
   to every template page (status, login, ...), not only to forms - two requests at once (the suite's health
   check next to a form render) crashed the device twice at 2-3 KB free

Every refusal is a plain-text 503 with `Retry-After: 5`. Results: 12 runs on build 15805 (guards 2+3): no SDK
crash, 116 refused renders, 3 reboots = OOM abort while building `/network.html` at 13-15 KB free -> guard 1.
The final 12 run verification with all four guards (build 15809) - see the end of this file.

**What is left is not a guard problem:** on this env the network and visualizer forms need 13-14 KB to build
plus 8 KB to render and do not fit next to the pixels, so they are refused (503) once the heap is below ~22 KB,
and the heap leaks (OPEN-9). Options for the owner: split those forms into pages (the project's own rule,
`/weather2*.html` is the precedent - a UI change, show a preview first), reduce the per-field cost in FormUI
(library refactor), or accept the 503 on this env (envs with fewer pixels have the heap). Do not lower the
thresholds to make the test pass: 7 KB was tried (build 15802) and crashed 3x in 7 runs.

## OPEN-9: the ESP8266 heap leaks ~1 KB per forms pass (new, not fixed)

- the idle heap at the start of every 12x pass declined 20.7 -> 19.0 -> 14.9 KB (reboot), 20.3 -> 19.1 -> 12.9
  (reboot), 20.4 -> 19.1 -> 17.7 -> 16.2 -> 15.6 -> 14.4 KB (build 15805, session data, not in the repo); the status
  page's *total* free heap declines, not only the largest block, so it is a leak (or a growing container), not
  fragmentation
- per group (`--groups <g> --repeat 4` each, build 15805): only the **forms** group leaks: pass starts 16.5 ->
  15.5 -> 14.6 -> 13.7 KB (~0.9 KB per pass = ~20 forms x (2 GET + 4-5 invalid POST + health GET)); auth,
  pages, assets, api, websocket, robustness, newlib are flat within noise
- per form the deltas are noisy (-50..-700 byte per form, `matrix.html`, `ani-Xmas.html`, `ani-plasma.html`
  largest), no single form stands out
- the forms group only posts invalid forms (every real field missing -> `validate()` fails -> DISCARD +
  `config.discard()` -> the error page is rendered or refused with 503). Candidates to check: everything in
  that path that outlives the request (`AsyncWebServerRequest` params/`_tempObject`, the FormUI error list,
  `StringDeduplicator`, `config.discard()`/`_nvs_close()` and its deinit timer, the response objects of the 503
  path `delete response` + `_beginLowMemoryResponse()`), and the GET snapshot of a form page
- tool: `scripts/tools/webui_leak_isolate.py` (100 GETs of a small form page, 100 invalid POSTs with 1 and with 14
  fields, 100 GETs of a plain page, free heap from the status page before/after each loop) - its result is
  in the last section of this file. A `malloc - free` live count was tried and removed again: umm counts
  realloc frees but not the matching allocations, the number drifts
- the ESP8266 core's `UMM_STATS_FULL` + `WEBSERVER_LOG_HEAP` access log (`heap=free/largest min=low water`)
  is in the flashed build and in `conf/envs/led_strip.ini`, `scripts/tools/webui_heap_analyze.py` tabulates it per
  request (`python -I scripts/tools/webui_heap_analyze.py <serial log>`), `scripts/tools/serial_log.py` is the
  logger of the report (`serial_log.py com6 3600 logs/x.txt`, PlatformIO's Python has pyserial)

## OPEN-1 / OPEN-2 / OPEN-8: three AsyncTCP 1.1.1 crashes on the ESP32 - owner decision, no code

**Decided 2026-10-19: switched to `esp32async/AsyncTCP@3.5.0`** (see the report) - verified on build 15814 (OPEN-1
reproducer 180/180, suite 801/801 with default and `--parallel 1` x3, no reboot, no core dump); revert `conf/libs.ini`, `conf/common_esp32.ini` and `lib/ESPAsyncWebServer/library.json` if it fails.

All three are in `.pio/libdeps/<env>/AsyncTCP@1.1.1` (what `conf/common_esp32.ini` links), evidence in the
report: the event queue deadlock (OPEN-1), a use-after-free of the AsyncClient through a stale queued event
(OPEN-2, core dump `logs/crash_wled_esp32_controller_neopixelbus_15803.txt`: `close()` with
`_rx_ack_len=3331310600`, `_closed_slot=-1`) and the pcb callbacks reset without the lwIP lock (OPEN-8,
`_lwip_fin(this=NULL)`). The maintained fork ESP32Async/AsyncTCP 3.x fixes all three by design (configurable
queue, event purge and callback reset inside `tcpip_api_call()`); a 3.5.0 copy already sits unused in
`.pio/libdeps/<env>/AsyncTCP` (installed 2026-10-04, the build links 1.1.1 - check the dependency graph of a
build before assuming anything). **Stop and ask the owner** which way: the fork from the registry
(`esp32async/AsyncTCP`), a local patched copy (like `lib/ESP8266mDNS`), or something else. Test device
`wled_esp32_controller_neopixelbus` (192.168.0.196, com7, `-t buildarchive`); `--parallel 1` keeps the suite
clear of OPEN-1 so the other two can be reproduced (`--newlib --parallel 1 --repeat 3` hit both within 3 runs).

## Cleanup before this is ready to commit (ask the owner)

- `conf/envs/led_strip.ini`: `KFC_LFS_REENTRY_GUARD=1` (+ the `-Wl,--wrap=lfs_*` lines, `src/lfs_reentry_guard.cpp`),
  `WEBSERVER_LOG_HEAP=1`, `UMM_STATS_FULL=1`, `UMM_INFO=1` are diagnostics. The heap log is what the OPEN-9 hunt
  needs, so leave them until that is done, then ask whether to keep them (they only add serial output)
- the mDNS discovery query `src/plugins/mdns/mdns_query.cpp` is still disabled (`Output::begin()` returns
  `false` at the top) - owner's call whether to re-enable it now that OPEN-7 is fixed
- the orphan `.pio/libdeps/<env>/AsyncTCP` (3.5.0) folder is harmless but confusing; it goes away with the
  AsyncTCP decision

## Test devices and procedure

See the report ("Test devices", "Serial log while testing", "Test suite"). Practical notes from this session:

- a serial logger on `com6`/`com7` blocks the upload: stop it first
  (`Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -match 'serial_log\.py' } | Stop-Process`),
  `wmic` is not available
- `led_strip_xmas_serial` inherited `IOT_LED_MATRIX_STANDBY_PIN=3` + `ESP8266_USE_UART_RX_AS_OUTPUT=1` from the
  ESP-01 base, which opens the UART TX-only (no AT commands over serial). The env now unflags both (no standby
  pin, the NodeMCU has no MOSFET) and the serial console works again (build 15811, `+HEAP=0` answers)
- never edit a header while a `pio run` is compiling: GCC's `#pragma once` sees two different files and every
  TU fails with "multiple definition"
- the suite's stdout is block buffered when redirected to a file; `--json` is written at the end, watch the
  serial log for progress
- the standing rule for intermittent crashes is >= 12 repeats (`--repeat 12`, ~4 min per pass)

## Order suggestion

1. OPEN-9 leak (self-contained, ESP8266 test device, no owner decision needed) - start from
   `leak_isolate.py`'s per-request numbers at the end of this file
2. OPEN-3 owner decision (split the big forms?) - a UI change needs a preview first
3. OPEN-1/2/8 - stop and ask for the AsyncTCP decision before writing code
4. Cleanup, then hand back for review before any commit

## Results of the last runs

### Final verification: `--newlib --repeat 12` on build 15809 (all four guards), 2026-10-19 02:15-02:37

- **0 crashes, 0 reboots** in 12 passes (serial log checked: no exception, no reset). 1219 passed, 31 failed,
  222 warnings - every failure is a 503 of the guards on the leaked-down heap (parallel requests, 40 extra
  headers, forms skipped) plus `/export-settings` answering an empty body at low heap (0 bytes instead of JSON
  or 503 - minor, not looked at) and one "logout removes the session" (the logout itself got a 503)
- the guards fired 407x before a form was built, 126x before a page was rendered, 3x before a request was
  handled; the idle heap went 23.1 -> 19.8 -> 18.8 -> 17.6 -> 16.3 -> 17.3 -> 16.3 -> 14.0 -> 13.2 -> 12.1 ->
  11.4 -> 10.5 KB (OPEN-9), so from pass 4 on the 13-14 KB forms were refused and from pass 8 on most forms
- the run took 20 min instead of 50 because refused pages answer fast

### Leak isolation (`scripts/tools/webui_leak_isolate.py`, build 15807, 100 requests per loop)

- the free heap of the status page before/after each loop, 3 s settle time (first run, device fresh after the
  forms passes): GET of the form page `/led-matrix/ani-solid.html` **-18.2 byte per request**, invalid POST with
  1 field +4.4, invalid POST with 14 fields/955 B +6.8, GET of the plain page `/about.html` -2.7 (noise)
- the access log confirms it inside the GET loop: `heap=` at the GET line 18168 -> 16784 over 100 GETs, the
  largest block 14144 -> 11408; inside the POST loop the value is constant (16960)
- caveat: the configuration keeps the readable/writeable copies of a form's parameters until
  `KFCFWConfiguration::gc()` releases them 5 s after the last read (`_garbageCollectionCycleDelay`), a POST
  releases them at once with `config.discard()`. A 3 s settle time therefore counts the copies of the last
  GET; a second run with 8 s settle time is worthless: the 12 run suite of the previous chain was still
  running against the device (two clients, 6.4 KB free at the start, two crashes) - repeat it alone on a
  freshly booted device and read the per-request trend from the serial log rather than from the before/after
  values
- so: a GET of a settings form page loses ~14-18 byte that a POST of the same page does not. What only the GET
  path does: `new SettingsForm(nullptr)` (no request data), `createConfigureForm(CREATE_GET)`, and it never
  calls `config.discard()` - the copies are released later by `gc()`/`release()`
  (`lib/KFCLibrary/KFCConfiguration/src/Configuration.cpp`), which keeps a changed writeable parameter and
  releases the others. Start there: compare `release()` with `discard()` for a parameter that `hasDataChanged()`
  reports as changed although nothing was saved (e.g. a form getter that normalizes a value), and the
  `_nvs_get_blob_with_open()` -> `_nvs_init()` path that `hasDataChanged()` runs for every parameter
