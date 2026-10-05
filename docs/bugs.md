# Known bugs

Bugs found in reviews and on the hardware. Newest first within each section. Move an entry to **Fixed**
when the fix is committed, with the commit hash or the date.

Affected env for the WS2 entries: `wt32_sc01_test1` (the only env that builds `src/plugins/weather_station2`).
Other entries name their envs.

## Open

### WS2-6 World clock switches the device's time zone
- **Where:** `src/plugins/weather_station2/ws2_screen_world_clock.cpp:238` (`WorldClockScreen::update()`, `_formatClock()`)
- **Symptom:** after the world clock screen was shown, the device runs in the time zone of a configured clock.
- **Cause:** `deviceTz = getenv("TZ")` keeps a pointer into the environment. On ESP32 `safeSetTZ()` is a plain
  `setenv()`. When the clock's TZ string is not longer than the device's, newlib overwrites the old value in place,
  so `deviceTz` then points at the clock's TZ and the "restore" sets it again. When the clock's TZ is longer,
  `setenv()` allocates a new entry and leaks the old one, every second for each clock while the screen is visible.
- **Fix:** copy the device TZ into a local buffer before the loop (the 1.x plugin copied it into a `String`).

### WS2-5 World clock ignores name and time zone changes
- **Where:** `src/plugins/weather_station2/ws2_screen_world_clock.cpp:94` (`_readClocks()`, early return at line 123)
- **Symptom:** a new name or time zone from the "World Clock" form does not show until reboot, unless enable or
  12/24h of a clock is changed as well.
- **Cause:** the rows are re-read only when `_signature` (enabled clocks + 12/24h flags) changes; it does not
  include name/TZ, and `create()` (line 78) never resets it.
- **Fix:** invalidate `_signature` in `create()` (e.g. `0xffff`).

### WS2-4 Misleading error text for connection failures (OpenWeatherMap)
- **Where:** `src/plugins/weather_station2/shared/open_weather_map/open_weather_map_client.cpp:123`
- **Symptom:** a failed connection (`status < 0`, 0 bytes) is reported as "HTTP -1: no weather data in the response".
- **Cause:** the parser's error is reported even though nothing was received.
- **Fix:** for `status < 0` report `HTTPClient::errorToString(status)`.

### WS2-3 Debug output enabled by default
- **Where:** `#define DEBUG_WEATHER_STATION2 1` in `ws2_form.cpp:14`, `ws2_screen_hass.cpp:26`,
  `shared/open_weather_map/open_weather_map_client.cpp:15` and `shared/home_assistant/`
  `hass_client.cpp:14`, `hass_config.cpp:9`, `hass_dashboard.cpp:12`, `hass_socket.cpp:11`.
- **Symptom:** verbose serial log, e.g. a trace on every HASS screen tick. All other plugin files default to `0`.
- **Fix:** default to `0` (some files enable it on purpose, see their comments - decide per file).

### WS2-2 Power monitor keeps the old connection after a host change
- **Where:** `src/plugins/weather_station2/shared/power_monitor/power_monitor_client.cpp:230`
- **Symptom:** changing or removing the remote host keeps the existing connection until it drops. A server that
  goes silent while the TCP connection stays up is shown as "online" with stale values.
- **Cause:** the target is only read again on reconnect; the read loop has no idle timeout.
- **Fix:** leave the read loop when the target changed, and drop the connection after a few seconds without a frame.

### WS2-1 HASS: latent overflow and reload race (low priority)
- **Where:** `src/plugins/weather_station2/shared/home_assistant/hass_dashboard.cpp:132` (`_decodeStringValue()`),
  `:230` (`_copyValue()`), `:503` (`Dashboard::_reload()`)
- **Overflow:** both decoders always write the terminator (`*write++ = 0`). Once the detail buffer is full, every
  further value writes one byte past its end. Unlikely with the current sizing (payload + 32 bytes), but unguarded.
- **Race:** `_reload()` calls `_client.stop()` and reloads `_config` even when the request task did not stop within
  its timeout (`hass_client.cpp:344`), while that task can still read the old configuration.

## Fixed

### CLK-1 Dithering option shown but without effect on NeoPixelBus
- **Affected envs:** `HAVE_NEOPIXELBUS=1`, currently `wled_esp32_controller_neopixelbus`.
- **Symptom:** the "FastLED Temporal Dithering" switch and `+LMC=dit` could be changed and reported the new state,
  but the output did not change (NeoPixelBus has no temporal dithering, `getDither()` had only FastLED-only readers).
- **Fix:** 2026-10-05, not committed yet. The form field (`clock_form.cpp`) and `+LMC=dit` (`atmode.cpp`) are
  `#if !HAVE_NEOPIXELBUS`, `PixelDisplay` has no `_dither` there and `getDither()` returns `false`. The help text in
  `led-matrix.html` needs no change, `forms.js` only attaches it to an existing field. The `dithering` config bit is
  kept, the stored layout is the same in every env.

### TLS handshake fails with -30592 (mbedtls allocations in PSRAM)
- **Where:** `src/plugins/weather_station2/weather_station2.cpp:201` (`_installTlsPsramAllocator()`)
- **Symptom:** every HTTPS request to api.openweathermap.org failed with
  `(-30592) SSL - A fatal alert message was received from our peer`, 0 bytes received.
- **Cause:** the PSRAM-first allocator for mbedtls (`mbedtls_platform_set_calloc_free()`). The ESP sent a corrupt
  handshake message, likely the ESP32 rev1 PSRAM cache issue. It depends on the PSRAM layout: commit `b7fd87a7`
  worked before and failed later with the same code. Server, network and mbedtls 2.28 were verified from the PC.
- **Fix:** 2026-10-05, call commented out, verified on the device. Not committed yet; the allocator code
  (`_installTlsPsramAllocator()`, `_psramCalloc()`, `_psramFree()`, the includes) and the TLS comments in
  `open_weather_map_client.cpp` still describe it and should be removed.

### Weather units wrong (wind always, everything in imperial)
- **Where:** `src/plugins/weather_station2/ws2_data.cpp` (`_requestLoop()`, `formatWind()`)
- **Symptom:** metric wind was 3.6x too low (m/s printed as km/h); imperial values were converted twice
  (70 °F shown as 158 °F).
- **Cause:** the API was asked for the configured units, the format helpers then converted from metric again.
- **Fix:** `225c6abe` - the API is always asked for metric, `formatWind()` converts m/s to km/h or mph.
  Verified on the device.
