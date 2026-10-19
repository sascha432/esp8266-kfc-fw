# WebUI stress test: crashes, reproduction and test procedure

State 2026-10-18. Found with `scripts/tools/webui_test.py` while the serial console of both test devices
was logged. Fixed crashes are listed for regression testing, open ones need work.

## Test devices

| env                                 | chip    | address       | WebUI user  | serial port | flash with                   |
|-------------------------------------|---------|---------------|-------------|-------------|------------------------------|
| `wled_esp32_controller_neopixelbus` | ESP32   | 192.168.0.196 | `KFC0A4271` | `com7`      | `-t buildarchive` (serial)   |
| `led_strip_xmas_serial`             | ESP8266 | 192.168.0.195 | `KFC43946D` | `com6`      | `-t upload` (serial)         |

Password 12345678 on both. No OTA on either device. The ESP8266 test device is `led_strip_xmas_serial`
(it ran `bme280_serial` before, do not use that env any more).

## Rules for the tests

- Never save a form, change brightness/volume, WiFi, passwords or accounts, and never start an
  LED/motor/sound action. Publishing MQTT (auto discovery included) is allowed on the two test devices to
  test the link, on no other device. `webui_test.py` only sends invalid form POSTs (all real fields
  missing, the firmware discards them); keep it that way.
- A crash reboots the device; on the ESP32 board the LED power is switched off and on and the log shows
  "The LED subsystem has been locked cause of a crash or hard reset".
- Several resets in a row can leave a device in safe mode (plugins off): the status page shows it next to
  the host name, one more reset clears it.
- Do not paste raw serial logs into reports: builds before 2026-10-18 logged POST parameters including the
  login password.

## Flash

```
pio run -e wled_esp32_controller_neopixelbus -t buildarchive   # builds, erases the core dump, flashes, archives elf/
pio run -e led_strip_xmas_serial -t upload
```

- Close every serial monitor/logger on com6/com7 first, an open port blocks the upload.
- The ESP32 link sometimes fails with `reopening .../libNeoPixelBus.a: No such file or directory` (file
  locked by another process): run the same command again.
- Wait ~15 s after the flash before testing; requests in the first seconds after a boot can fail.
- Shared/core changes must build for both envs.

## Serial log while testing

The ports reset the boards when DTR/RTS are toggled. Open them with DTR/RTS low (pyserial is in PlatformIO's
Python, `%USERPROFILE%\.platformio\penv\Scripts\python.exe`):

```python
# serial_log.py <port> <seconds> <outfile>
import sys, time, serial
port, seconds, out = sys.argv[1], float(sys.argv[2]), sys.argv[3]
ser = serial.Serial(); ser.port = port; ser.baudrate = 115200; ser.timeout = 0.2
ser.dtr = False; ser.rts = False          # before open(): no auto reset
ser.open()
end, line = time.time() + seconds, b''
with open(out, 'w', encoding='utf8', errors='replace') as f:
    while time.time() < end:
        line += ser.read(4096)
        while b'\n' in line:
            text, line = line.split(b'\n', 1)
            f.write('%.3f %s\n' % (time.time(), text.decode('utf8', 'replace').rstrip('\r'))); f.flush()
ser.close()
```

Run one per port in the background, then the tests. The ESP8266 output contains NUL bytes: `tr -d '\000'`
before grepping.

## Test suite

```
.venv\Scripts\python scripts/tools/webui_test.py --env wled_esp32_controller_neopixelbus --env led_strip_xmas_serial --newlib
.venv\Scripts\python scripts/tools/webui_test.py --env led_strip_xmas_serial --newlib --repeat 3      # intermittent crashes
.venv\Scripts\python scripts/tools/webui_test.py --env wled_esp32_controller_neopixelbus --groups forms --max-form-size 16384 --form-heap-share 3
```

The suite checks uptime and free heap after every form and group and reports `reboot detected` with the form
or group that caused it. `--json <file>` stores the results. See the docstring of the script for the groups.

## Decoding a crash

- ESP32: `.venv\Scripts\python scripts/tools/kfc_coredump.py trace --host 192.168.0.196 -u KFC0A4271 -p 12345678`
  decodes the stored core dump with the archive of the flashed build (`-t buildarchive` stores it); the
  result is `logs/crash_<env>_<build>.txt` with the stacks of all tasks. The dump only holds the last crash.
  Without an archive pass `--elf .pio/build/<env>/firmware.elf` if it is still the flashed build (compare
  the sha256 prefix of the dump). Panics with a corrupted backtrace (task watchdog) need the dump.
- ESP8266: the exception is printed on serial (`Exception (NN)`, `epc1`, `>>>stack>>>`). Resolve epc1 and
  the `40xxxxxx` stack words with
  `%USERPROFILE%\.platformio\packages\toolchain-xtensa\bin\xtensa-lx106-elf-addr2line.exe -pfiaC -e .pio/build/led_strip_xmas_serial/firmware.elf <addresses>`.
  The stack dump contains stale frames, read it with care.

## Crashes

### OPEN-1: AsyncTCP deadlock under parallel requests (task watchdog)

- env: `wled_esp32_controller_neopixelbus` (all ESP32 envs use `AsyncTCP@1.1.1`, `conf/common_esp32.ini`)
- symptom: `task_wdt: - async_tcp (CPU 1)`, both CPUs idle, `abort()` from `task_wdt_isr`, backtrace corrupted
- reproduce: 3 parallel GET `/status.html` (logged in) in rounds of 6, crashed after ~15 rounds; the robustness
  group of the suite (`parallel requests`) hits it in most runs:
  ```python
  import requests, concurrent.futures
  s = requests.Session(); s.post('http://192.168.0.196/index.html', data={'username': 'KFC0A4271', 'password': '12345678'})
  get = lambda _: s.get('http://192.168.0.196/status.html', timeout=20).status_code
  for n in range(30):
      with concurrent.futures.ThreadPoolExecutor(max_workers=3) as p: print(n, list(p.map(get, range(6))))
  ```
- cause (core dump): the lwIP thread `tiT` blocks in `_prepend_async_event()` (`xQueueSendToFront(..., portMAX_DELAY)`)
  for an accept event because the 32 entry event queue of `async_tcp` is full, while `async_tcp` waits in
  `tcpip_api_call(_tcp_write)` for `tiT`. Queue size (32) and `portMAX_DELAY` are hardcoded in AsyncTCP 1.1.1
  (`.pio/libdeps/<env>/AsyncTCP@1.1.1/src/AsyncTCP.cpp:98-147`). Not caused by the ESPAsyncWebServer changes.
- fix needs a decision of the owner: own patched AsyncTCP (like the NeoPixelBus fork) or a maintained fork.
  Do not edit `.pio/libdeps` (not persistent).

- 2026-10-19: the maintained fork ESP32Async/AsyncTCP 3.x (a 3.5.0 copy already sits unused in
  `.pio/libdeps/<env>/AsyncTCP`, installed 2026-10-04 - the build links `AsyncTCP@1.1.1`, see the dependency graph)
  has a configurable queue (`CONFIG_ASYNC_TCP_QUEUE_SIZE`, default 64), drops poll/ack events instead of blocking
  when the queue fills up, resets the pcb callbacks and purges the queued events of a client inside
  `tcpip_api_call()` (`_tcp_close_api()`, `_remove_events_for_client()`). OPEN-2 and OPEN-8 are the same 1.1.1
  code, so the decision covers three crashes. Still the owner's decision, nothing was changed.
- 2026-10-19, owner decision: switched to `esp32async/AsyncTCP@3.5.0` (`[lib_asynctcp]` in `conf/libs.ini`,
  referenced by `conf/common_esp32.ini`; the manifest of `lib/ESPAsyncWebServer` depends on `esp32async/AsyncTCP
  ^3.5.0` instead of `me-no-dev/AsyncTCP ^1.1.1`). Clean build of `wled_esp32_controller_neopixelbus` links 3.5.0;
  the `close(bool)` deprecation warnings are harmless (3.x ignores `now`, as 1.1.1 did). Covers OPEN-2 and
  OPEN-8 as well.
- verified 2026-10-19, build 15814 (archived): the OPEN-1 reproducer above 30 rounds = 180x 200 (1.1.1 crashed
  after ~15 rounds), `webui_test.py --newlib --repeat 3` 801 passed, `--newlib --parallel 1 --repeat 3` 801
  passed (hit OPEN-2 and OPEN-8 within 3 runs on 1.1.1); no reboot (uptime 28 s -> 857 s in
  `logs/asynctcp3_15814_serial.txt`), no core dump.

### OPEN-2: lwIP assert in `tcp_recved()`

- env: `wled_esp32_controller_neopixelbus`, seen once in the robustness group (`--repeat 3`)
- symptom: `assert failed: tcp_update_rcv_ann_wnd ... tcp.c:951 (new_rcv_ann_wnd <= 0xffff)` in `tiT`, called
  from `AsyncClient::_recv()` -> `_tcp_recved()` (AsyncTCP.cpp:920)
- meaning: more received bytes were acknowledged than arrived (window > 64 KB)
- suspect: `AsyncClient::close()` acks `_rx_ack_len` without resetting it (AsyncTCP.cpp:733); the only user of
  `ackLater()` is the WebSocket close handshake in `lib/ESPAsyncWebServer/src/AsyncWebSocket.cpp` (`_onData`,
  opcode `WS_DISCONNECT`). Not proven. Reproduce: robustness group repeated (unmasked frame -> close 1002,
  WebSocket close tests).

- reproduced 2026-10-19, build 15803 (`--newlib --parallel 1 --repeat 3`, run 3, websocket group, right after
  `WS /webui-ws[1]: Client connected`), core dump `logs/crash_wled_esp32_controller_neopixelbus_15803.txt`: the
  `async_tcp` task is in `AsyncClient::close(now=true)` <- `AsyncWebSocketClient::_onAck()` (the close frame was
  acknowledged, AsyncWebSocket.cpp:574) and called `_tcp_recved(pcb=0x3ffe3d70, closed_slot=-1, len=3331310600)`:
  `_rx_ack_len` and `_closed_slot` of the AsyncClient are garbage, the object was already freed. The `_sent` event
  that led there was a stale entry of the AsyncTCP event queue.
- cause (AsyncTCP 1.1.1, not ESPAsyncWebServer): only `AsyncClient::_close()` removes the queued events of a client
  (`_tcp_clear_events()`), `_error()` and `_lwip_fin()` do not. An error event is *prepended* to the queue, the
  client is deleted in its discard callback (`AsyncWebSocketClient` deletes the AsyncClient in `onDisconnect`) and
  the SENT/RECV events that were queued before and now sit behind it run on the freed object. The WebSocket close
  handshake is only the first caller that uses the stale `_rx_ack_len`. The fix lives in AsyncTCP: 3.x purges the
  events in `_tcp_close_api()` under the lwIP lock - same decision as OPEN-1.

### OPEN-3: ESP8266 heap exhausted by the 46 field form POSTs, crash in the WiFi stack

- env: `led_strip_xmas_serial`
- symptom: `Exception (29)` (StoreProhibited), `epc1=0x4000df64` (ROM memcpy), `excvaddr=0x00000000`,
  `ctx: sys`; the stack is the SDK WiFi code (`ieee80211_parse_beacon` / `scan_parse_beacon` /
  `cnx_update_bss_more`), the SDK copies into a NULL allocation. Earlier decodes that showed
  `lfs_dir_commit*` read stale frames of the loop stack.
- reproduce: `webui_test.py --env led_strip_xmas_serial --newlib --repeat 12` rebooted in 10 of 12 runs, always
  on the second POST ("16 fields/1024B" or multipart) of a 46 field form: `/led-matrix/ani-fire-reactive.html`,
  `ani-plasma-reactive.html`, `ani-spectrum-*.html`. A single POST of the same data usually works.
- measured (heap watch of `src/lfs_reentry_guard.cpp`): free heap 1.2-4 KB (largest block 200-2700 bytes)
  right before every crash, ~6.7 KB after boot. Once a 676 byte allocation in `nvs::PageManager::load()` failed
  first (see OPEN-5).
- not LittleFS: the LittleFS re-entry (FIXED-7) is fixed and logged 0 re-entries in these 12 runs, the crash is
  unchanged.
- ideas: limit the POST body/field count on the ESP8266 before it is parsed (413), render the form with less
  heap (the page is ~30 KB), reserve a heap margin for the SDK (refuse requests below ~6 KB free).
- related: on these forms the rendered page is sometimes incomplete (`PrintArgs` drops entries when it cannot
  allocate, response size varies 29-34 KB).

- measured 2026-10-19 (build 15799, `WEBSERVER_LOG_HEAP=1` + `UMM_STATS_FULL=1` + `UMM_INFO=1` in
  `led_strip_xmas_serial`: the access log prints `heap=<free>/<largest block> min=<low water mark since the
  previous line>`, `template done ...` prints the low water mark of the render): idle heap 20-22 KB. Creating a
  form costs ~325 byte per field: `/network.html` 13.3 KB, the 44 field visualizer forms 13.7 KB, `/alarm.html`
  5 KB, the small animation forms 3-4 KB. Rendering a form page costs 3.1-3.6 KB on top (1 KB frame buffer, TCP
  send buffer, PrintArgs), the error page of a POST up to 4.7 KB. FormUI already deduplicates its strings and
  keeps PROGMEM labels in flash, the cost is spread over many small objects (field, value, BaseUI, storage
  vector, validators) - reducing it is a library refactor.
- the first guard (8 KB free after the form was built, before the template and the response) refused
  `/network.html` on a plain GET (7.0-7.4 KB after creation). A trial with 7 KB (build 15802) crashed 3x in 7
  runs: the POST error page needs 4.7 KB and the SDK fails on fragmentation - one crash had 5.9 KB free but a
  648 byte largest block, another went down to 1.4 KB free / 232 byte block.
- guard rework (build 15805): one check after the form, the template and the response were created
  (`Plugin::_canRenderForm()`: free >= `WEBSERVER_FORM_MIN_FREE_HEAP` 8 KB and largest block >=
  `WEBSERVER_FORM_MIN_FREE_BLOCK` 4 KB, and `config.hasNvsReadFailed()` false, see OPEN-5), the 503 carries
  `Retry-After: 5`. 12 runs: 116 refused renders, **no SDK crash**, but 3 reboots of a new kind: `Unhandled C++
  exception: OOM` while the `/network.html` form was *built* (`operator new` of 20-32 bytes in
  `BaseForm::_add<Group>` / `BaseUI` / `_beginFileResponse`) - the idle heap had dropped to 13-15 KB (OPEN-9) and
  the core aborts on a failed `new`. Hence `Plugin::_canCreateForm()` (build 15807): free >=
  `WEBSERVER_FORM_CREATE_MIN_FREE_HEAP` 16 KB before a form is created (GET, POST, WebSocket form action).
- build 15807 under two clients at once (a leak loop and the suite's robustness group overlapped): a status
  page rendered next to a refused form POST at 2.7 KB free aborted in `HttpHeaders::addNoCache()` (a 28 byte
  `new`), and a status page rendered next to a 44 field form render took the heap from 4.9 KB to 2.1 KB /
  808 byte block and crashed the SDK (`Exception (29)`). Hence (build 15809): `_canRenderTemplate()` checks
  **every** template page, not only forms, and `_canHandleRequest()` answers 503 at the start of
  `_handleFileRead()` below `WEBSERVER_REQUEST_MIN_FREE_HEAP` (4 KB) before the request path allocates anything
- final verification (build 15809, all four checks): 12 runs, **no crash, no reboot**; the guards refused 407
  form creations, 126 page renders and 3 requests while the heap leaked from 23 to 10.5 KB (OPEN-9)
- conclusion for `led_strip_xmas_serial` (1024 pixels): the network and the visualizer forms need 13-14 KB to
  build plus 8 KB to render and do not fit next to the ~20 KB idle heap, the guard only keeps the device alive
  (503). The fix is splitting these forms into pages (project rule, as `/weather2*.html` does) - a UI change
  for the owner to decide. Envs with fewer pixels have the heap.
- ESP32: `WEBSERVER_FORM_MIN_FREE_HEAP` 32 KB / block 16 KB / create 48 KB, 3 runs on build 15803 without a
  single 503.

### OPEN-4: ESP8266 OOM abort in the core mDNS responder (`/mdns-discovery.html`)

- env: `led_strip_xmas_serial`, build 15782; seen by the owner on `/mdns-discovery.html` and reproduced with a
  single `GET /mdns_discovery` (the data request of that page) right after a login, without the test suite
- symptom: `User exception (panic/abort/assert)`, `Unhandled C++ exception: OOM`, `ctx: sys`,
  `last failed alloc call: 40280261(36)` (owner) and `4028050A(624)` (reproduced) with ~4.5 KB free heap and
  a 2.2 KB largest block
- cause: the discovery installs an mDNS service query (`MDNSPlugin::mdnsDiscoveryHandler()`,
  `MDNS.installServiceQuery("kfcmdns", "udp")`), the answers of the other KFC devices are stored by the core's
  LEAmDNS in the network context (`udp_input` -> `MDNSResponder::_parseMessage()` -> `_processAnswers()` ->
  `_processPTRAnswer()` / `_processAAnswer()`) with a plain `new`. The core's `operator new` (`abi.cpp:47`)
  calls `__unhandled_exception("OOM")` when `malloc()` fails, so one failed allocation reboots the device.
  The ESP8266 starts with only ~6.7 KB free heap on this env (1024 LEDs), see OPEN-3.
- the core library cannot be changed here. Ideas: refuse the discovery below a free heap / block limit
  (answer 503), stop the query early when the heap drops (flag from the answer callback, remove the query
  from the loop, not from the callback), limit the number of answers.
- second bug on the same endpoint: `AsyncMDNSResponse` (`include/async_web_response.h`) never sets `_code`,
  every other response of `src/async_web_response.cpp` sets 200. The response starts with `HTTP/1.1 0`, the
  Python client rejects it (`BadStatusLine('HTTP/1.1 0')`, 10 of 10 requests after the reboot).
- the suite only loads the page `/mdns-discovery.html`, not `/mdns_discovery`, that is why it never crashed
  there

### OPEN-5: config read fails when the NVS cannot be initialized (low heap)

- env: `led_strip_xmas_serial`, seen once right before an OPEN-3 crash:
  `_nvs_open: cannot open NVS namespace=kfcfw_config mode=0 err=00001101` (`ESP_ERR_NVS_NOT_INITIALIZED`), then
  `_readData: cannot read data handle=0d3d size=9 read=9 err=1107` (`ESP_ERR_NVS_INVALID_HANDLE`)
- cause: the NVS partition is initialized on demand and `nvs::PageManager::load()` failed to allocate 676 bytes
- an invalid or unreadable config must not crash the device (owner): check what the callers of a failed read do
  (defaults vs. garbage, a later write of the defaults over the stored config) and retry the init when heap is
  available again. The crash that followed this time was OPEN-3.

- 2026-10-19, build 15802 (ESP8266): seen 7x in a 12 run test (`_nvs_open: cannot open NVS namespace=kfcfw_config
  mode=0 err=00001101`). Cause: `NVS_DEINIT_PARTITION_ON_CLOSE=1` (ESP8266 default) deinitializes the NVS 250 ms
  after every access to free its RAM, the next access runs `nvs_flash_init()` again and `nvs::PageManager::load()`
  needs a 676 byte block - with 5.4 KB free and a 648 byte largest block (fragmented by the 44 field forms) the init
  fails and every parameter read fails. The forms were rendered with zeros/defaults (`values changed: v_fabe, ...`
  of `webui_test.py`), a save would have stored them.
- fixes: KFCLibrary `24e5e05` (`write()` discards the changes when a parameter could not be read), and in
  `src/web_server.cpp`: `_canRenderForm()` answers 503 when `config.hasNvsReadFailed()` (new getter in
  `Configuration.h`) so a form with zeros is never shown, and a `config.write()` that did not return SUCCESS
  answers 503 instead of the "applying" page.

### OPEN-6: `/file_manager/list` without a session answers 404 instead of 403

- env: `led_strip_xmas_serial`, auth group of `webui_test.py`, runs 5-12 of the 12 run test (runs 1-4 passed),
  needs a check

### OPEN-7: mDNS packets from the test device crashed dozens of devices on the LAN

- reported by the owner on 2026-10-18 ~20:05 PDT: many KFC devices on the network crashed at the same time,
  several times, while a new ESP8266 discovery was tested (`src/plugins/mdns/mdns_query.cpp`, now disabled:
  `Output::begin()` returns false)
- what was sent (times PDT): a PTR query for `_kfcmdns._udp.local` to 224.0.0.251:5353 from an ephemeral port
  ("legacy unicast", RFC 6762 6.7), two packets per discovery request:
  - 19:51 and 19:58: one valid query each from the PC (Python raw socket, ID 0 and 0x1235)
  - 19:57-19:58 (10 requests), 19:59 (1), 20:01 (1): `udp_sendto()` was called twice with the same pbuf.
    lwIP leaves the UDP header it added in the pbuf, the second packet most likely contained the previous
    IP/UDP headers in front of the DNS query - a malformed mDNS packet that every device parses
  - 20:03 (1) and 20:04-20:05 (11): a new pbuf per packet, valid (checked with a capture on the PC), 16-19
    devices answered every request 5 s apart, which speaks against the valid query as the cause
- confirmed by the SaveCrash log of a deployed ESP8266 (build 14767): `User exception`, `last failed alloc call:
  40270731(57344)` at 19:57:10, 19:57:45, 19:59:41 and 20:01:20 - exactly the requests with the reused pbuf,
  none during the valid queries at 20:03-20:05. The stack holds the received payload `08 00 45 00 00 41`: an
  IPv4 header inside the mDNS message
- cause in the core: LEAmDNS allocates the RDATA of an unknown record type with the RDLength of the packet
  (`MDNSResponder::_readRRAnswerGeneric()`: `new unsigned char[RDLength]`, LEAmDNS_Transfer.cpp) without
  checking it against the packet size, and the core aborts on a failed `new`. Every ESP8266 firmware with the
  stock core has this, one packet multicast to 224.0.0.251:5353 reboots all of them
- fix: the ESP8266mDNS library (now a local copy) rejects the record in `_readRRAnswer()` when
  `u16RDLength > m_pUDPContext->getSize()`, and `_readRRAnswerTXT()` bounds every TXT item inside the RDATA and
  searches the `=` with `memchr` over the item length instead of `strchr` on a non-terminated buffer
  (version `1.2.0-kfc1`). This is the real fix, where the allocation happens.
- an earlier workaround wrapped lwIP's `udp_input()` in the firmware (`src/esp8266_mdns_guard.cpp`,
  `-Wl,--wrap=udp_input`) and dropped malformed packets before the responder saw them. It was verified
  (A/B below) and then **removed** once the library was patched - the two must not both be applied
- the deployed ESP8266 devices stay vulnerable until they are updated with the patched library
- reproduced and verified 2026-10-18 by unicast to single devices (never the group): the trigger is a QUERY
  (QR=0) with QDCOUNT=1 and ANCOUNT=1, the known-answer record being an unknown type (e.g. 99) with
  RDLENGTH=0xe000. LEAmDNS reads the known-answer section during query parsing and allocates
  `new unsigned char[0xe000]` -> OOM abort. A clean RESPONSE (QR=1) does not trigger it (`_parseResponse` is
  skipped without an active query). Confirmed still unfixed on esp8266/Arduino master (`_readRRAnswerGeneric`,
  the `_udpReadBuffer` size check is commented out).
- A/B on the owner's devices with the identical packet: unpatched 192.168.0.32 rebooted (uptime 0:00:55 after),
  patched 192.168.0.17 and 192.168.0.195 stayed up and logged `malformed mDNS packet ... dropped`
- the discovery query of `mdns_query.cpp` is disabled (`Output::begin()` returns false) until the owner decides;
  its valid legacy unicast query crashed nothing

### OPEN-8: `AsyncClient::_lwip_fin()` with a NULL client (ESP32)

- env: `wled_esp32_controller_neopixelbus`, build 15803, 2026-10-19 (`--newlib --parallel 1 --repeat 3`, run 2,
  forms group while `/sensor.html` was posted) - a crash that the parallel-request deadlock (OPEN-1) hid before
- symptom: `Guru Meditation Error: Core 0 panic'ed (LoadProhibited)`, `A2 = 0x00000000`, in task `tiT`:
  `AsyncClient::_lwip_fin()` (AsyncTCP.cpp:865) <- `_s_lwip_fin(arg=NULL)` <- `_tcp_recv()` (a FIN, pbuf NULL) <-
  `tcp_input` - resolved with addr2line against `elf/wled_esp32_controller_neopixelbus_15803.tar.gz`
- cause: `AsyncClient::_close()` and `_error()` reset the pcb callbacks (`tcp_arg(_pcb, NULL)`, `tcp_recv(_pcb,
  NULL)`, ...) from the `async_tcp` task without the lwIP lock. `tcp_input` on the lwIP thread (other core) reads
  `callback_arg` and `recv` one after another and calls the recv callback with the already cleared arg. AsyncTCP
  3.x resets the callbacks inside `tcpip_api_call()` (`_reset_tcp_callbacks()` in `_tcp_close_api()`). Same
  decision as OPEN-1/OPEN-2, nothing was changed.


### OPEN-9: the ESP8266 heap leaks ~1 KB per pass of the forms group

- env: `led_strip_xmas_serial`, build 15805/15807, 2026-10-19. The idle free heap of the status page (total
  free, not the largest block) declines from pass to pass of `webui_test.py --newlib --repeat 12`:
  20.7 -> 19.0 -> 14.9 KB, 20.3 -> 19.1 -> 12.9 KB, 20.4 -> 19.1 -> 17.7 -> 16.2 -> 15.6 -> 14.4 KB (each
  sequence ends in the OPEN-3 creation abort of `/network.html`)
- `--groups <g> --repeat 4` for every group: only **forms** leaks, its pass starts go 16.5 -> 15.5 -> 14.6 ->
  13.7 KB (~0.9 KB per pass of ~20 forms, each 2 GETs + 4-5 invalid POSTs + a health GET); auth, pages, assets,
  api, websocket, robustness and newlib are flat
- per form the deltas are noisy (-50..-700 byte, `matrix.html`, `ani-Xmas.html`, `ani-plasma.html` largest),
  no single form stands out. `scripts/tools/webui_leak_isolate.py` separates GET of a form page, invalid POST
  (1 and 14 fields) and GET of a plain page, 100 requests each - see the handoff plan for its numbers
- not fixed. The forms group only sends invalid forms: `createConfigureForm(CREATE_POST)` -> `validate()`
  fails -> `DISCARD` + `config.discard()` -> the error page is rendered, or refused with 503 (`delete response`
  + `config.discard()` + `_beginLowMemoryResponse()`). Check what outlives the request in that path

### FIXED-1: `String::move()` from a NULL buffer in the body parser (ESP32)

- env: `wled_esp32_controller_neopixelbus`; `LoadProhibited` in `memmove` <- `String::move` <-
  `AsyncWebServerRequest::_parsePlainPostChar`
- reproduce: POST a single 16 KB field to `/alarm.html` (forms group with `--max-form-size 16384 --form-heap-share 3`)
- fix: `lib/ESPAsyncWebServer/src/WebRequest.cpp`: the value is decoded straight from `_temp`
  (`_urlDecode()` with allocation checks), the parameter is moved; an allocation failure answers 413 and
  the rest of the body is ignored (`_bodyAllocFailed()`).

### FIXED-2: heap corruption in `PrintArgs` (ESP32)

- env: `wled_esp32_controller_neopixelbus`; `CORRUPT HEAP: Bad tail` / `multi_heap_poisoning.c:295` in
  `realloc` <- `Buffer::_changeBuffer` <- `PrintArgs::printf` while a form page is rendered
- reproduce: as FIXED-1 (large POSTs to the `led-matrix/ani-*` forms)
- fix: `lib/KFCLibrary/KFCBaseLibrary/include/PrintArgs.h` `vprintf_va_list()` returns when `reserve()`
  fails (it wrote the arguments behind a smaller buffer); `src/Buffer.cpp` keeps the old block when
  `realloc()` fails.

### FIXED-3: dangling string pointer in `PrintArgs` (ESP8266)

- env: `led_strip_xmas_serial`; `Exception (28)` in `strnlen_P` <- `PrintArgsHelper::BufferContext::initContext`
  (PrintArgs.cpp:231), `excvaddr` ends in a valid-looking low half (`0x....2918`)
- reproduce: one urlencoded POST with 16 fields of 48 bytes to `/led-matrix/ani-fire-reactive.html`
- fix: `PrintArgs.hpp` `print()` and `vprintf_P()` write an entry completely or not at all (a type byte
  without its pointer made the next bytes the string pointer).

### FIXED-4: 16 KB POST stalls the network for ~30 s (ESP32)

- env: `wled_esp32_controller_neopixelbus`; no crash, the response took 37 s / timed out, MQTT disconnected
- cause: the access log (`src/web_server.cpp`, `WEBSERVER_LOG_SERIAL`) URL-encoded every POST parameter into
  the log line (and printed the login password)
- fix: `_getUrlAndQueryFromRequest()` logs GET parameters only, values cut at 64 characters.

### FIXED-5: `async_tcp` removed from the task watchdog (ESP32)

- env: `wled_esp32_controller_neopixelbus`; log `safeSend: esp_task_wdt_add failed` followed by AsyncTCP
  `Failed to remove loop task from WDT`
- reproduce: WebSocket `/webui-ws`, authenticate, send `+get_values`
- fix: `src/web_socket.cpp` `TaskWdtScope` only removes a task it added (`safeSend()` x3, `_broadcast()`).

### FIXED-6: older ESPAsyncWebServer crashes (regression tests)

All in the robustness/newlib groups of `webui_test.py`, they fail or crash firmware built before 2026-10-18:
body beyond Content-Length (no answer), multipart boundary 100 chars (expects 400), boundary 300 chars
(endless loop/WDT on old builds), unmasked WebSocket frame (expects close 1002), WebSocket header split into
1 byte TCP segments (crashed old builds), 16 KB form bodies (heap corruption on old builds).

### FIXED-7: LittleFS re-entered during a write (ESP8266)

- env: all ESP8266 envs. LittleFS has no lock on the ESP8266 and the core's `flash_hal_read/write/erase()` call
  `optimistic_yield(10000)`: a LittleFS call of the loop task that runs longer than 10 ms (the LED show of 1024
  pixels takes ~30 ms with interrupts off) yielded inside the operation and the async web server read a static
  file with LittleFS in between
- logged with `KFC_LFS_REENTRY_GUARD=1` (`src/lfs_reentry_guard.cpp`, wraps the LittleFS API with
  `-Wl,--wrap=lfs_*`, enabled in `led_strip_xmas_serial`): every login re-entered it, e.g. inner `read
  /webui/<hash>` (sys) while outer `write /.logs/security` (cont) was running
- fix: `src/esp8266_flash_hal.cpp` replaces the three functions without the yield (`-Wl,--wrap=flash_hal_*` in
  `conf/common_esp8266.ini`), 0 re-entries in 12 suite runs afterwards
- note: the guard prints from the network context; printing the full history for every re-entry starved the
  loop task and triggered the soft WDT (`uart_do_write_char`), it prints one block per outer call now

## Not a crash, noise in the log

- `vfs_api.cpp ... open(): /littlefs/... does not exist` on the ESP32 for every page: `FileMapping` probes the
  plain path first.
- `getWritableParameter: ... resizing binary blob` (now a debug message): a stored config blob with an older
  layout size, reset to the defaults until it is saved once.
- `_sanitizeMatrixConfig: invalid matrix 0x0, using defaults`: the matrix form (GET) on a device whose stored
  clock config has an old size, only the form copy is changed.
