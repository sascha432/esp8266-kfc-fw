# KFC Firmware - changelog

Every change to the firmware, the WebUI, the tools and the docs, newest first, under the day it
was made; a release gets its version as the heading. Each entry says what a user or a developer
sees: a new feature under Added, a different behavior under Changed, a repaired one under Fixed,
and what is gone under Removed. Releases up to 0.0.8 have no dates.

## Unreleased (0.0.9)

### 2026-10-30

Changed

* Build: the indexed form field names (`PROGMEM_DEF_LOCAL_VARNAMES`, `F_VAR`) are one table per
  name created by the compiler in flash instead of a pointer array built on the stack, the count
  can be any constant expression (no integer literal needed anymore)

Removed

* Build: the Boost library (`include/boost_1_80_0`, 11 MB), the few preprocessor macros it was used
  for are replaced by `pp_tools.h` in KFCLibrary

### 2026-10-29

Changed

* WebUI: the network configuration is split into pages, one per WiFi station (Configuration,
  Network), the single form did not fit the ESP8266 with 1024 pixels (503)

Fixed

* The phone controller ignoring the MQTT `/allow/set` and `/answer` topics (inverted check since the
  StrView migration)
* "Export Settings" downloading an empty file on a low heap (ESP8266 with 1024 pixels), it answers
  503 if the export cannot be built
* The web server not answering after saving the network settings: the restart could not bind port 80
  while the browser still had connections on it, a network save does not restart the web server
  anymore and a failed bind is retried
* A crash (Exception 3) when saving the Remote Access form
* The WiFi station form saving "<No SSID>" as the SSID of an enabled station without one

### 2026-10-28

Changed

* WiFi watchdog: checks once per minute and switches to the next network after 5 minutes with
  `WiFi.begin()` instead of resetting the adapter (a reset during the SDK's own reconnect scan left
  the ESP8266 station dead, devices stayed offline for hours while the networks were up), logs the
  WiFi state while down, restarts the device after 30 minutes without a connection, the error count
  is reset while connected (errors of earlier outages added up and moved the devices to the backup
  network after a short router restart)

Fixed

* ESP32: the boot menu (multiple resets) fed no watchdog during its 15 second wait, the task
  watchdog reset the device and the menu came up again (boot loop)
* WiFi: a link-local address (169.254.x.x, lwIP's fallback after ~4 minutes without a DHCP answer)
  does not count as connected, the watchdog reassociates to request DHCP again (the devices stayed
  offline with it until the next reboot)
* ESP8266: fixed `Exception (2)` in `igmp_tmr()` after a WiFi outage, stand-by mode switched the
  soft AP off while an mDNS multicast report for it was pending (IGMP is stopped on the AP interface
  first)
* MQTT: no connect attempt while WiFi is down, ESPAsyncTCP leaked a `tcp_pcb` (~190 byte) per
  attempt that failed without a route

### 2026-10-21

Added

* LED matrix: automatic gain for the I2S microphone, the bands are shown relative to the loudest
  band with a silence level and a dynamic range

### 2026-10-20

Fixed

* Weather station: fixed the OpenWeatherMap poll interval, a successful request scheduled the next
  one after 1/10 of the interval (2 minutes minimum) and exhausted the daily API limit
* ESP8266 mDNS discovery: a TXT item with a NUL key wrote one row behind a stack array (remote
  packet)
* Web server: the 503 of the heap guards keeps the headers of the request (a login landing on a
  refused page lost its session cookie), a saved form is not answered with 503 any more, the
  WebSocket form save reports a write that stored nothing, an unauthenticated firmware upload is
  closed at the first chunk
* ESP32: `+MDNSQ` removed the loop task from the task watchdog, the shared `TaskWdtScope` is used
  (include/task_wdt_scope.h)

### 2026-10-19

Changed

* ESP32 builds: AsyncTCP 1.1.1 (me-no-dev) is replaced by the maintained fork esp32async/AsyncTCP
  3.5.0, 1.1.1 deadlocked under parallel requests and ran stale queued events on freed clients
* Web server: a request is refused with 503 (Retry-After) when the free heap is too low to handle
  it, a settings form is only built and a template page only rendered with the heap for it, a form
  whose configuration could not be read from the NVS is not shown and a save that could not be
  stored answers 503 instead of "applying" (the ESP8266 with 1024 pixels crashed in the WiFi stack
  or aborted on a failed `new`)
* WebUI test suite: a 503 of the low-memory guard is a warning, a non-JSON answer no longer aborts
  the suite

Fixed

* ESP8266: fixed a heap leak of 48 byte per configuration access that reopened the NVS handle (the
  partition was deinitialized under an open handle, every status page leaked)
* ESP32 builds: the local ESP8266mDNS copy is ignored by the LDF (`lib_ignore`), it broke every
  ESP32 build
* ESP8266: the core's mDNS library (ESP8266mDNS/LEAmDNS) is replaced by a patched copy in
  `lib/ESP8266mDNS`, an answer with an oversized RDLength rebooted the device (the allocation is now
  capped, not just checked against the packet size)
* LittleFS corruption on the ESP8266: the file system yielded inside an operation and the web server
  accessed it in between

### 2026-10-18

Changed

* WebSocket: move String/JSON into the message buffer instead of copying
* WebUI: browser-side UI state (debug console, serial console history/filter, menu, card states,
  file manager) moved from cookies to localStorage, the Cookie header only carries the SID

Fixed

* Use after free on the ESP32 when a firmware or file manager upload was rejected

### 2026-10-14

Fixed

* MQTT client stuck in "connecting..." until the next WiFi reconnect: connect timeout (30 s +
  keepalive), the reconnect timer is not lost anymore when it is rearmed inside its own callback
* WiFi reconfiguration after "authentication expired" running inside the WiFi event handler
  (blocking scan in the event task/SDK context), it is called from the loop now
* WiFi network selection: the order of the networks is stable (no random order for equal
  priorities), the RSSI of the scan is used for networks with priority 0 (auto) and the station id
  is not used as list index anymore
* WiFi watchdog forcing network #1/#2 on every reconnect, the network selected after
  `WIFI_MAX_ERRORS` errors is kept now

### 2026-10-13

Changed

* LED Matrix: every segment is a panel (size, placement, rotation, mirror), the segment offset is
  gone, test pattern (`+LMC=dir`, `+LMC=id`, "Show All"/"Segment 1-4" buttons in the matrix form)

Fixed

* ArduinoOTA "No Answer to our Authentication": the ESP8266 FastLED driver returned with the
  interrupts locked, the loop could not yield after a frame
* Crash on restart while a WebSocket client is connected (use after free in
  AsyncWebSocket::_handleDisconnect())
* ArduinoOTA does not restart the responder of the MDNS plugin anymore (heap corruption during
  uploads)
* Crash (null pointer) of `/is-alive` without the `p` argument, a parameterless request rebooted the
  device

Removed

* The unfinished UDP video streaming of the visualizer (RGB565/RGB24 Video)
* The Python debug tool `scripts/kfcfw_tool` and its firmware support (WebSocket binary packet
  types, `+CLIENT_ID`)

### 2026-10-10

Changed

* WebUI builder rewritten in Python (YAML config `KFCWebBuilder.yaml`, PHP is no longer required,
  zopfli compression ~4% smaller)

Removed

* Home Assistant Control discontinued and moved to https://github.com/sascha432/esp32-hassctrl

### 2026-10-05

Changed

* Clock plugin: own color math instead of FastLED, LED power off at boot and after crashes, fixed
  `+LMC=tem`/`co`, removed unused options

### 2026-10-04

Added

* NeoPixelBus RMT and I2S support

Changed

* LED Matrix: unified matrix/segment validation

Removed

* The combined animations form of the clock plugin, the alarm color and flashing speed moved to the
  settings form

### 2026-10-03

Changed

* ESP32 SerialHandler: subscribe the main loop task to the task watchdog once instead of on every
  loop iteration

### 2026-10-02

Added

* New build number generator
* The MPU-6050 display rotation and tilt sensor

Changed

* Visualizer modes are selectable animations now (Spectrum Rainbow Bars, Spectrum Gradient Bars,
  Spectrum Single Color Bars, Plasma Reactive, Fire Reactive) through the WebUI, MQTT and select
  buttons

Fixed

* Web server response: fixed truncated responses, stalled connections, and excessive allocations

### 2026-10-01

Added

* Weather Station v2 with Home Assistant Control

Changed

* Updated OpenWeatherMap API to OneCall API 3.0

### 2026-09-24

Changed

* Replaced custom Arduino frameworks
* Refactored deferred calls in clock plugin using TaskQueue

Removed

* The unfinished LED matrix fan control (`IOT_LED_MATRIX_FAN_CONTROL`)

### 2026-09-23

Changed

* MQTT System Metrics Heap, Fragmentation, RSSI, Version and SaveCrash are disabled by default

Fixed

* Invalid MQTT device class for CCS811 TVOC sensor

### 2026-09-22

Added

* User configurable IR remote control actions
* ESP32 support for the IR remote control
* ESP32 core dump: the ESP-IDF core dump can be viewed, downloaded and deleted from the SaveCrash
  WebUI (ESP32)

Fixed

* An `Interrupt wdt timeout on CPU1` crash caused by `FastLED.show()` running in two tasks at the
  same time
* Broken alarm form
* A crash when switching to the next WiFi network (`+WIFI=next`) and corrected the station number
  handling of the WiFi configuration

### 2026-09-20

Added

* Audio reactive plasma and fire visualization modes for the LED matrix (speed, hue rotation, zoom,
  spectrum ripples and volume driven flames)

Removed

* The 1D visualizer modes (VU meter and rainbow spectrum)
* Serial2TCP support
* Support for the DHTxx sensor

### 2026-09-19

Fixed

* Ping monitor command crashed when hostname was not found

Removed

* ArduinoFlashStringGenerator
* The `AT_MODE_HELP_SUPPORTED` option. See [docs/AtModeHelp.md](docs/AtModeHelp.md) for the command
  reference

### 2026-09-17

Added

* Python CLI audio spectrum analyzer for the LED matrix and improved I2S microphone FFT

Fixed

* Storing unix time in RTC memory to survive resets

Removed

* The unfinished Interleaved animation

### 2026-09-16

Changed

* Reduced heap usage for file manager and MDNS Discovery

Fixed

* Savecrash issues

### 2026-09-14

Changed

* Updated MQTT sensors

### 2026-08-22

Fixed

* Invalid MQTT device class for BME680 VOC sensor and added persistent eCO2 calibration

### 2025-03-21

Added

* Intercom plugin for managing a remote doorbell via phone

### 2025-03-18

Changed

* Improved consistency in BME680 CO2 readings using the Adafruit library

### 2025-03-06

Added

* Plugin for a basic dimmer with support for X9C digitally controlled potentiometers and PWM dimming

Fixed

* Issue with the dimmer plugin's off button

### 2024-12-11

Added

* Christmas animation

Changed

* A memory queue for the logger avoids file system operations outside the main loop

Fixed

* An issue that prevented reconnecting to WiFi networks after a beacon timeout
* Stopped file system (FS) during non-atomic FS updates to prevent corruption

## 0.0.8

Added

* Real brightness for the LED matrix WebUI brightness slider and calculated power badge (auto
  brightness, temperature protection and power limit might reduce the actual output)
* Support for ESP32-S3
* Support for INMP441 microphone for the audio visualizer
* Adafruit NeoPixel support
* Support for up to 4 LED output pins
* LED Matrix and Strip support for ESP32 (wled_esp32_controller, compatible with many WLED
  controllers or my own version [EasyEDA schematics and
  PCB](https://oshwlab.com/sascha23095123423/wled-controller_copy))
* UDP video streaming to the visualizer

Changed

* Flash frequency and mode 80MHz/QIO for ESP8266
* nvs_flash is the default config storage for ESP8266
* Ported nvs_flash from ESP8266_RTOS_SDK to ESP8266_NONOS_SDK
  ([https://github.com/sascha432/ESP8266_NvsFlash_NONOS_SDK](https://github.com/sascha432/ESP8266_NvsFlash_NONOS_SDK))
* Improved visualizer and new plasma animation
* Updated ESPAsyncWebServer to fix issues with ESP32
* Merged framework-arduinoespressif8266 3.0.0 into 3.1.2-mod
* Merged framework-arduinoespressif32 2.0.0 into 2.0.9-mod
* Split KFCLibrary and added support for github or local copy in ./lib
* Missing online status and binary awake sensor fixed for remote control
* Update battery status over MQTT and UDP on a daily basis (interval can be configured to save
  battery life)
* AT+GPIO=5s shows state of the GPIO (IN/OUT/IN_PULLUP etc...)
* Updated deep sleep and quick connect for framework 3.x (200ms, quick subsequent wake-ups take
  1350ms)

Fixed

* A crash in RTCMemoryManager for ESP32
* Some issues with the visualizer
* An issue with GPIO 4 and 5 being set to INPUT_PULLUP without using I2C (add `-D
  DISABLE_TWO_WIRE=1` to disable it)

## 0.0.7

Added

* Support for 4bit bitmap images
* An external I2C bus to the Weather Station, that can be used for other sensors like BME680 (VOC
  gas), BH1750FVI (Lux), INA219 (Voltage, Current, Power) with live data on the TFT and the WebUI
* Live display of the TFT in the WebUI of the Weather Station and links to control it
* Boost 1.80.0 preprocessor

Changed

* Updates/fixes and new firmware for 4 channel dimmer
* Write MQTT auto discovery to file ``/.logs/mqtt_auto_discovery.json`` (command
  ``+MQTT=list,file``)
* Merged framework-arduinoespressif8266 3.1.0-dev
  [https://github.com/sascha432/Arduino](https://github.com/sascha432/Arduino)
* Support for BME680 updated
* More details about the software to build the VFS (Windows and Linux), can be found in
  ``README.md`` / ``Building the VFS``
* AT mode help has been moved to ``docs\AtModeHelp.md`` to save flash memory
* Most parts can be compiled under Linux now and probably OSX as well (invalid case-sensitive
  filenames mostly fixed, some python scripts might still contain Windows specific code)
* New version after a lot of issues have been fixed after upgrading to Framework Arduino ESP8266
  3.0.1

Removed

* STK500v1 updates from the WebUI ([use Serial Console](docs/AtModeHelp.md#stk500v1-programmer))
* HTTP authentication token

## 0.0.6

Added

* Analog clock screen to the weather station
* Gallery picture display (jpeg) with overlay for the weather station
* Option to enable KFC OTA and/or ArduinoOTA
* The previous and next 3 moon phases to station plugin
* World Clock screen for weather station
* New auto discovery script ``./scripts/tools/kfcfw_discovery.py``
* Option to reset factory settings and format file system, if the device keeps crashing and cannot
  be accessed anymore
* Support for Home Assistant object id
* Support for up to 8 WiFi networks (5 = default)

Changed

* Username is pre-filled at the WebUI login
* Syslog default protocol is UDP
* Updates for KFCWebBuilder (requires PHP 7.4.28)
* Updated weather station plugin for Arduino Core 3.x
* Global DNS servers shared across all WiFi networks
* Auto installation of custom framework-arduinoespressif for ESP8266 and ESP32

Fixed

* Some environments to be compiled as release (DEBUG=0), which reduces the code size by 10-15%
* FastLED and NeoPixelEx issue with Framework Arduino ESP8266 3.0.1-dev
* Invalid TZ env variable name
* Dependencies and removed ``lib-extra/``
* Fixes of the KFC OTA tool
* Issues with websocket-clients and the OTA tool
* Memory leak in GFXCanvas
* Issues with Home Assistant's name/friendly_name for templates
* Issue with corrupting the configuration during saving changes
* Memory leak with syslog (TCP only) with no WiFi connection

## 0.0.5

Added

* Support for trailing edge dimmer v2.2.2
  [https://github.com/sascha432/trailing_edge_dimmer](https://github.com/sascha432/trailing_edge_dimmer)
* Gradient animation
* CPPDEFINES dependency to KFCWebBuilder to detect any changes
* Support for MCP23017 I2C I/O-Expander
* Support for multiple I/O-Expander of the same type
* Support for BH1750FVI Ambient Light Sensor
* Support for GDBStub

Changed

* ESP32: Syslog and SSDP
* ESP32: Zeroconf and MDNS discovery
* ESP32: FreeRTOS semaphores
* ESP32: NVS storage for configuration
* ESP32: OSTimer implementation based in esp_timer
* framework-arduinoespressif8266 3.2.0
* ESP32: Arduino-ESP32 2.0.0-rc1 integration
* Merged Atomic Sun plugin into Dimmer plugin (**work in progress**)
* Linear brightness levels with cubic interpolation for the dimmer plugin
* Friendly MQTT names
* Better response for dimmer buttons and fix for disabled "long press"
* Merged Arduino Core 3.1.0-dev
* Moved KFCIOExpander to separate git repo
* Inline forms for WebUI
* Sort order for sensors
* WebServer logging to Serial
* framework-arduinoespressif8266 3.1.0 / Arduino Core 3.0.1-dev
* Cleanup of the Visual Studio mock library for 3.0.0
* New version for framework-arduinoespressif8266 3.30000.210519 (3.0.0) and GCC 10.2 changes

Fixed

* MDNS for Arduino Core 3.x
* Timing issues with the integrated STK500v1 programmer

Removed

* Persistent WiFi settings (improved stability)
* WebAlerts
* arduinoespressif8266 patch files have been removed and moved to
  [https://github.com/sascha432/Arduino/tree/master](https://github.com/sascha432/Arduino/tree/master)
* Internal memory debugger

## 0.0.4

Added

* Support for internal or external RTC
* Framework-arduinoespressif8266 3.30000.210519 (3.0.0)
* Support for TinyPwm I2C I/O-Expander
  [https://github.com/sascha432/TinyPwm](https://github.com/sascha432/TinyPwm)
* Support for PCF8574 I2C I/O-Expander
* Support for
  [https://github.com/NimmLor/IoT-Audio-Visualization-Center](https://github.com/NimmLor/IoT-Audio-Visualization-Center)
* SaveCrash WebUI to download crash logs. MD5 checksum of the firmware is included
* Support for ArduinoOTA
* Min. and max. range limits to reduce the dimmable range (Dimmer Plugin)
* Support for rotary encoders with pushbutton and capacitive touch sensor
* Option to save and restore brightness/other settings after reboot (Clock/LED Matrix plugin)
* Fire animation
* Support for loading JavaScript and CSS from remote server(s) during development
* Memory debugging and leak detection
* SSDP plugin
* Support for zeroconf (`${zeroconf:<service>.<proto>:<address|value[:port
  value]>|<fallback[:port]>}` as hostname)
* Boot option to set passwords to default, enable AP mode and web server
* Alarm plugin to Clock and Weather Station

Changed

* Improved NTP plugin
* Changes for GCC 10.2
* OTA update tool restarts device in safe mode before running any updates
* User customizable options for factory reset over the WebUI
* Human readable uptime for MQTT system metrics
* Fan control and light sensor for LED matrix
* Replaced RGB Silder with Bootstrap Color Picker Sliders
* Switched to LittleFS
* Replaced the JSON library with a more stable version that consumes less memory
* Switch plugin fixes and improvements
* Customized WString class
* Key Combo for safe mode and reset factory settings during boot
* Replace read only files from WebUI by placing files in `/.wor/*`
* 64/256KB flash storage that survives firmware and file system upgrades
* Class to write to flash storage with copy on write/basic wear leveling
* Replaced EspSaveCrash with own implementation that writes to flash storage directly without
  allocating any memory and supports multiple sectors
* Max. deep sleep time increased to up to 31 days
* Web server does not require re-registering web handlers and web sockets on reconfigure anymore
* Registered MQTT components are stored separately and do not require re-registering
* Class to send Serial output over UDP
* Colored sub ranges for rangeslider.js
* JSON schema support for MQTT light
* Upgrade to arduinoespressif8266 3.20704.0 including patch files for the platform
* Fully asynchronous MQTT auto discovery with incremental or full updates
* MQTT callback's for QoS
* Battery level indicator in % with charging detection
* MQTT device triggers for remote control
* Limit maximum power of the LEDs
* Configurable login security
* Reduce brightness depending on the temperature
* Cycling color and animation for rainbow mode (Clock/LED matrix)
* LED Matrix plugin
* Rewrite of the remote-control plugin for faster response times and longer battery life
  * Removed support for home assistant Restful API
  * Option to send UDP packets to minimize latency
  * Deep sleep up to 30 days
  * 230ms WiFi Quick Connect, sending UDP packets
  * 300ms for MQTT over TCP
* utf-8 support for web sockets
* Major improvements for forms, they are using ~10x less memory and are ~20x faster. Most need less
  than 1KB and load within 10ms
* Read ADC over web sockets with kfcfw_tool and graphical analysis
* Improved event scheduler (freed ~500 byte IRAM), memory usage and performance
* Ping monitor improvements
* Device title is being used as name for MQTT auto discovery
* Clearing EspSaveCrash EEPROM during factory reset
* 64KB flash storage (R/W) dedicated for the firmware
* AT mode command DUMPM for dumping memory or flash contents
* Disable AP mode after 15min. if default password is used (does not apply to AP mode in standby)
* Enable recovery mode after version change (AP mode enabled, default SSID, broadcast SSID, enable
  web server, reset passwords IF empty)
* JavaScript updates and fixes
* Zeroconf resolve button for forms
* MQTT auto discovery rebroadcast interval
* Open/close automation for the Blinds Plugin, audible warning while running or shortly before
* Improved HTML form generator
* Templates for configuration getters/setters
* Debug print functions for IRAM_ATTR that do not pull the entire SerialWrapper code into IRAM
* Blacklist to prevent plugins being loaded at start-up
* Improved plugin system, ~2kb more free RAM
* Rewritten SerialHandler
* Rewritten StreamWrapper (Serial, DebugSerial, SerialHandler etc...)
* KFC OTA copies firmware.elf and stores the hash in the device configuration
* MQTT metrics: uptime, heap
* NTP plugin: Check once per minute if the time is valid and call configTime() again if not
* Alarm plugin for Clock, Weather Station, ...

Fixed

* Play tone for blinds plugin
* Missing information for SSDP
* Crash during login with login security disabled
* Some environments and adapted changes of the Espressif 8266 2.3.1 framework (Arduino Core 2.7.4)
* Missing html entities encoding in forms

Removed

* Filters from syslog, reduced memory usage and improved performance
* Test mode from the blinds plugin and implemented it into kfwfc_tool

## 0.0.3

Added

* Option to enable/disable mDNS
* Support for MQTT component names instead of enumeration
* Option to turn status LED off when connected to WiFi

Changed

* Replaced remote time zone with native POSIX time zone support
* MQTT auto discovery aggregates all entities as single device
* MQTT auto discovery abbreviations
* Send calibration/config. to MQTT for recovery after firmware updates
* MQTT auto discovery queue to reduce memory usage and network load
* NTP start-up delay reduced to 1 second (from up to 5 minutes)
* Arduino ESP8266 2.7.1 release changes/fixes
* Advanced firmware configuration for dimmer plugin
* Form validator for double
* Conditional attributes for HTML forms
* Arduino ESP8266 2.6.3 release changes
* Deep sleep support can be disabled
* Device configuration page for WebUI

## 0.0.2

Added

* Option to enable SoftAP only if WiFi station mode is not connected
* Option to disable the "WebUI" page
* Support for DHT11/22 sensor
* Support for INA219 sensor
* Device title
* Bearer token for authentication (WebUI, web sockets, TCP2Serial...)
* Support for RTC DS3231
* Support for espressif8266 2.3.2/arduino 2.6.3
* Support for CCS811 sensor
* Support for BME680 sensor
* New virtual file system with reduced memory usage and long filenames

Changed

* Reboot after certain time if running in Safe Mode
* Restart WiFi subsystem once a minute if the connection failed
* Persistent message popups for the WebUI (WebAlerts)
* Multiple instances of Home Assistant supported
* Set mireds for Hass plugin
* Store crash dumps in flash memory instead of EEPROM
* RestAPI class
* Replaced manual PROGMEM strings with ArduinoFlashStringGenerator
* Ambient light sensor for clock plugin
* WebUI/MMQT update rates seperated
* Improved MMQT TCP buffer handling
* MDNS service discovery for WebUI
* MDNS service queries
* MDNS version info and device name/title
* Improved WiFi quick connect performance by ~20%
* Disabled sending MQTT auto discovery after wake up
* Server side includes and extended HTML templates
* HLW8012 sensor update rate and precision improved
* Live view of HLW8012 sensor data via kfcfw_tool
* Cli OTA update tool for Flash, SPIFFS and ATmega flash images
* Dynamic menu for WebUI
* WiFi download speed test page for WebUI
* Export configuration as JSON
* Button groups for WebUI
* KFCGFx library with compressed canvas class to fit the contents of big TFTs in memory
* STK500 programmer for AVR over serial port via WiFi or from files stored on SPIFFS
* Class to create JSON objects with streaming support with small buffer size
* Syslog over TCP support
* Started to update change log

Fixed

* WebUI "Reboot Device" checkbox "Safe Mode" fixed
* Login issues with expired session cookie
* Double menu entries after wake up from deep sleep
* Invalid timezone/abbreviatation
* MDNS interface issues with SoftAP and station mode enabled
* Missing SSID for WebUI WiFi scan

## 0.0.1

Changed

* Started to commit files
