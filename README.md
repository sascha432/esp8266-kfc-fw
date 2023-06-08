# esp8266-kfc-fw

Firmware for ESP8266/ESP32 and IoT devices

The firmware offers a plugin interface to easily extend its functionality. The plugins can be configured via web interface and have access to the network, file system, logging, serial console, I2C bus, GPIO, GPIO expanders, EEPROM, improved and stable internal ADC, RTC memory access, a (more or less) precise RTC during deep sleep, a maximum of 30 days deep sleep, an event/task scheduler, direct access to flash storage (copy on write) without file system overhead, crash logs with stack traces, etc... via API functions. Support for deep sleep, quick boot (~37ms) and WiFi Quick Connect (<230ms) after wake up is also available

## Required flash size

### ESP8266

1M with most features activated, no OTA updates<br>2M with OTA

### ESP32

4M and 8M with OTA

**OTA for 4M is work in progress**

## ChangeLog

**UPDATE**: First version with support for ESP32 with Arduino Core 2.0.0 is working, not all plugins are supported yet.

**SPIFFS** has been replaced with **LittleFS** starting with version 0.0.4.7604

Starting with version 0.0.5, a modified Arduino framework is being used.
https://github.com/sascha432/Arduino/tree/master

[Version 0.0.8 (master)](./CHANGELOG.md)

[Version 0.0.7](https://github.com/sascha432/esp8266-kfc-fw/blob/v0.0.7/CHANGELOG.md)

[Version 0.0.6](https://github.com/sascha432/esp8266-kfc-fw/blob/v0.0.6/CHANGELOG.md)

[Version 0.0.5](https://github.com/sascha432/esp8266-kfc-fw/blob/v0.0.5/CHANGELOG.md)

[Version 0.0.4](https://github.com/sascha432/esp8266-kfc-fw/blob/v0.0.4/CHANGELOG.md)

[Version 0.0.3](https://github.com/sascha432/esp8266-kfc-fw/blob/v0.0.3/CHANGELOG.md)

[Version 0.0.2](https://github.com/sascha432/esp8266-kfc-fw/blob/027305464fb486606840fa821595f7bce6e6ef1d/CHANGELOG.md)

## Requirements

 - Visual Studio Code for Windows or Linux
 - PlatformIO 6.1.4 (Linux compiles way faster, see [compile_time.md](compile_time.md))
 - WSL2 Virtual Linux and WSL remote extension. Speeds up compiling with many cores by 10-30%
 - See ``Building the VFS`` for more requirements

### ESP8266

- GCC 10.x
- Arduino Core 3.x with modified WString class

Since this device has not enough memory and CPU power, a SSL webserver is not possible for most environments. To secure a connection, use haproxy with a certificate from https://letsencrypt.org/. Something like *.mydomain.com and redirect all traffic to the devices. Like https://bathroom.mydomain.com to 192.168.0.88, Like https://kitchen.mydomain.com to 192.168.0.77, etc...

The modified version of the core is available on github and used by default

| System Info | |
|---|---|
| Hardware | ESP8266 4.00MB Flash, 80 Mhz, Free RAM 22.56KB |
| Software | KFC FW 0.0.5 Build 9509.3.0.2-17-g5266f22b-dev Aug 17 2021 18:12:25 |
| Framework | Arduino ESP8266 3.0.1-dev 3.0.2-17-g5266f22b 0x5266f22b |
| SDK | 2.2.2-dev(38a443e) |
| Core | 3.0.2-17-g5266f22b=30002017 |
| lwIP | STABLE-2_1_2_RELEASE/glue:1.2-48-g7421258/BearSSL:6105635 |

### ESP32

- GCC 8.x with gnu++17
- Arduino Core 2.x with modified WString class and ESP8266 compatibility extensions

The modified version of the core is available on github and used by default. `framework-arduinoespressif32#feature/arduino-upstream` is used in order to get the GCC 8.x toolchain instead of 5.x

| System Info | |
|---|---|
| Hardware  | ESP32 4.00MB Flash, 240 Mhz, Free RAM 154.59KB, Temperature 36.7°C |
| Software | KFC FW 0.0.5 Build 9681 Aug 22 2021 18:18:35<br>ESP-IDF Version v4.4-dev-2313-gc69f0ec32  |
| Framework | Arduino ESP32 2.0.0 |

## Libraries

### KFCSyslog

Send messages to a syslog server via UDP, TCP and TLS

### KFCWebBuilder

Framework to build WebUIs with bootstrap and store them mostly compressed in a virtual file system. Combined with server side includes, complex dynamic web pages/forms with a low memory footprint can be created

#### Building the VFS

Following software is required to build the virtual file system. Compatible versions might work as well, the listed ones have been tested...

 - Java JRE 8 / openjdk-18-jre-headless
 - NodeJS v12 or v16
 - Install uglify-js (3.15.3) using ``npm install uglify-js`` inside the project directory. For linux create a symlink ``sudo ln -s $(pwd)/node_modules/uglify-js/bin/uglifyjs /usr/bin/uglifyjs``
 - PHP 7.4.x (8.x is not supported yet)
 - mklittlefs - If it is not being installed automatically, run ``pio pkg install -t platformio/tool-mklittlefs``

Executables must be in ``PATH``. If you have PHP 8.x, install PHP 7.x from source and ``export PHPEXE=/home/sascha/Desktop/php-7.4.30/sapi/cli/php``. For Windows, create an environment variable ``PHPEXE`` pointing to php.exe

More details about binaries can be found/changed in [lib/KFCWebBuilder/KFCWebBuilder.json](lib/KFCWebBuilder/KFCWebBuilder.json)

### KFCVirtualFileSystem

~~Read only file system with long filename support, optimized for low memory environments~~
Due to constantly changing file system implementations of the Arduno frameworks currently replaced with long filename support on top of LittleFS, but no transparent access for Dir()/File() anymore. Replacement classes ListDir and FSWrapper.

Support for overriding read only files by uploading to a special directory.

### KFCJson

Library to read streamed JSON documents and create JSON streams with small buffer size

### KFCEventScheduler

Platform indepentend timer, scheduler with prioritized queue, WiFi callbacks and loop functions with basic support for calculating load average

### KFCResetDetector

Detect crashes and offer safe mode by pressing reset button during boot, count number of resets for extended functionality (reset configuration, open public hotspot etc...)
It also offers a RTC memory mananger to store data identified by an unqiue id and crc check

### KFCConfiguration

Library to handle configurations stored in the EEPROM, read on demand to save memory, manage changes in structure automatically, export and import configuration as JSON

### KFCRestApi

Library to handle RESTful APIs using asyncHTTPrequest/KFCJson

### PinMonitor

Support for push buttons with debouncing and rotary encoders using interrupts

### Mpr121Touchpad

Support for capacitive touchpads using the MPR121 sensor. With gestures detection

## Plugins

### AT Mode

Configure and control the device with AT commands via serial interface or WebUI

### MQTT Client

MQTT Client with API to easily add components that work with Home Assistant, as well as auto discovery

### SSDP auto discovery

Shows up as device and links to the WebUI

## MDNS auto disovery with zeroconf support

MDNS discovery and zeroconf support for MQTT, Syslog and Home Assistant

### Http2Serial

Serial console access over the WebUI using web sockets

### Serial2TCP

Serial console redirection via TCP in client or server mode

### STK500v1

Fully asynchronous STK500v1 programmer over WiFi, serial or web server upload...

### RF24 Master

Secure communication (no encryption though) with NRF24L01+ modules. Support for multiple channels/6 devices per channel.

### NTP Client

NTP Client with timezone support

### Syslog

Send log messages to a syslog server via TCP/UDP

### Ping Monitor

Ping remote hosts over the WebUI and run background ping statistics

### I2C Scanner

Scan GPIO pins for I2C devices

### Sensor

Plugin for different sensors. Supply Voltage/Battery with charging indicator, BME280, BME680, CCS811, HLW8012, LM75A, DS3231, INA219, DHT11, DHT22, AM2301 and system metrics.
Support for native WebUI and MQTT.

### Switch

Plugin for relays and other devices connected to GPIO pins, for example Sonoff basic.
Support for native WebUI and MQTT.

### Alarm

Plugin for up to 10 different alarms, single alarm or repeated on daily basis.

### Clock

Plugin for my WS2812 Based 7 Segment Clocks

[ESP8266-WiFi-Clock](https://github.com/sascha432/ESP8266-WiFi-Clock)

[EasyEDA 7-Segment WiFi Display](https://easyeda.com/sascha23095123423/iot_wifi_7segment)

[EasyEDA WS2812 WiFi Controller](https://easyeda.com/sascha23095123423/iot_wifi_clock_controller)

It includes a python tool to generate a translation table to address any 7 segment display.

### LED Matrix

Plugin based on the clock plugin to control a WS2812 LED matrix or LED string. The plugin is using FastLED and supports all the available LED types.
Support for motion detection sensors, LED power limit and temperature protection. A maximum of 1024 LEDs are supported.

Compatible with my [EasyEDA WS2812 WiFi Controller](https://easyeda.com/sascha23095123423/iot_wifi_clock_controller)

### Dimmer

Plugin to control my trailing edge WiFi dimmer with energy monitor

[ATmega328P Firmware - Trailing Edge Dimmer](https://github.com/sascha432/trailing_edge_dimmer)

[EasyEDA 4-Channel Dimmer with Power Monitor](https://easyeda.com/sascha23095123423/iot-4-channel-dimmer-with-pm)

[EasyEDA 1-Channel in-wall or plugin dimmer with Power Monitor](https://easyeda.com/sascha23095123423/iot_1ch_dimmer_copy_copy_copy)

[EasyEDA Control Module for 1-Channel Dimmer](https://easyeda.com/sascha23095123423/esp12e_iot_module_copy)


### Blinds Controller

Plugin for my 2 channel blinds controller

[ESP8266-WiFi-Blinds](https://github.com/sascha432/ESP8266-WiFi-Blinds)

[EasyEDA Blinds Controller](https://easyeda.com/sascha23095123423/iot_blinds_controller)

### WiFi Remote Control

Plugin for my 4 button ultra low power WiFi remote control. 16.5µA standby, 250ms wakeup time to send first UDP packets and 300ms for a fully established MQTT session with QoS.
Supports MQTT / Homeassistant Device Triggers and UDP packets.

[ESP8266-WiFi-Remote-Control](https://github.com/sascha432/ESP8266-WiFi-Remote-Control)

[EasyEDA 4Channel Remote Control](https://easyeda.com/sascha23095123423/iot_4ch_remote)

### Weather Station

Weather Station for TFT displays with openweathermap.org API.
Live remote view over WebUI and screenshots in .BMP format

[ESP8266-WiFi-Weather-Station](https://github.com/sascha432/ESP8266-WiFi-Weather-Station)

[EasyEDA Weather Station](https://easyeda.com/sascha23095123423/iot_weather_station)

### File Manager

WebUI to explore and modify LittleFS and KFCVirtualFileSystem

### SaveCrash

Store crash logs with checksum of the firmware, version and other details directly on flash memory. Does not allocate any memory and uses copy on write to ensure no data is lost. WebUI to review crash reports and download stack traces. The OTA tool supports archiving .ELF files that were uploaded, including file system images and configuration files/git revision...

## Required third party libraries

### ESPAsyncWebServer

[ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer)

### asyncHTTPrequest

[https://github.com/boblemaire/asyncHTTPrequest](https://github.com/boblemaire/asyncHTTPrequest)

### Arduino Cryptography Library

[https://github.com/rweather/arduinolibs](https://github.com/rweather/arduinolibs)

### AsyncMqttClient (MQTT plugin)

[async-mqtt-client](https://github.com/marvinroger/async-mqtt-client)

### AsyncWebSocket (Http2Serial plugin)

[ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer/blob/master/src/AsyncWebSocket.h)

### AsyncPing (Ping Monitor plugin)

https://github.com/akaJes/AsyncPing

~~### RF24 (RF24 Master)~~

~~https://github.com/nRF24/RF24~~

### FastLED (Clock plugin)

[FastLED](https://github.com/FastLED/FastLED)

### ESP32 SSDP

[ESP32 Simple Service Discovery](https://github.com/luc-github/ESP32SSDP.git)

### Adafruit Libraries (Various plugins)

[Adafruit_Sensor](https://github.com/adafruit/Adafruit_Sensor)

[Adafruit_BME280_Library](https://github.com/adafruit/Adafruit_BME280_Library)

[Adafruit_CCS811.git](https://github.com/adafruit/Adafruit_CCS811.git)

[Adafruit_INA219](https://github.com/adafruit/Adafruit_INA219)

[Adafruit-GFX-Library](https://github.com/adafruit/Adafruit-GFX-Library)

[Adafruit-ST7735-Library](https://github.com/adafruit/Adafruit-ST7735-Library)

[Adafruit_SSD1306](https://github.com/adafruit/Adafruit_SSD1306)

[adafruit/RTClib](https://github.com/adafruit/RTClib)

[Adafruit_MPR121](https://github.com/adafruit/Adafruit_MPR121)

[Adafruit_NeoPixel](https://github.com/adafruit/Adafruit_NeoPixel)

### Rotary encoder handler for arduino. v1.1

[buxtronix/arduino/Rotary.cpp](https://github.com/buxtronix/arduino/blob/master/libraries/Rotary/Rotary.cpp)
