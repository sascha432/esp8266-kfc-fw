# esp8266-kfc-fw

Firmware for ESP8266/ESP32 and IoT devices

The firmware offers a plugin interface to easily extend its functionality. The plugins can be configured via web interface and have access to the network, file system, logging, serial console, I2C bus, GPIO, GPIO expanders, NVS flash, improved and stable internal ADC, RTC memory access, a (more or less) precise RTC during deep sleep, a maximum of 30 days deep sleep, an event/task scheduler, crash logs with stack traces, etc... via API functions. Support for deep sleep, quick boot (~37ms) and WiFi Quick Connect (<230ms) after wake up is also available

## Required flash size

### ESP8266

1M with most features activated, no OTA updates<br>2M with OTA

Running on a modified framework-arduinoespressif8266 v3.1.2

### ESP32

4M and 8M with OTA

Running on a modified framework-arduinoespressif32 v2.0.9

## ChangeLog

[Version 0.0.9 (master)](./CHANGELOG.md)

[Version 0.0.8](https://github.com/sascha432/esp8266-kfc-fw/blob/v0.0.8/CHANGELOG.md)

[Version 0.0.7](https://github.com/sascha432/esp8266-kfc-fw/blob/v0.0.7/CHANGELOG.md)

[Version 0.0.6](https://github.com/sascha432/esp8266-kfc-fw/blob/v0.0.6/CHANGELOG.md)

[Version 0.0.5](https://github.com/sascha432/esp8266-kfc-fw/blob/v0.0.5/CHANGELOG.md)

**SPIFFS** has been replaced with **LittleFS** starting with version 0.0.4.7604

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
- Modified framework-arduinoespressif8266 v3.1.2-mod

Since this device has not enough memory and CPU power, a SSL webserver is not possible for most environments. To secure a connection, use haproxy with a certificate from https://letsencrypt.org/. Something like *.mydomain.com and redirect all traffic to the devices. Like https://bathroom.mydomain.com to 192.168.0.88, Like https://kitchen.mydomain.com to 192.168.0.77, etc...

The modified version of the core is available on github and used by default

| System Info | |
|---|---|
| Hardware | ESP8266 4.00MB Flash 80MHz@DIO, 160MHz, Free RAM 19.41KB |
| Framework | Arduino ESP8266 3.1.2-g1c34ef21-dev |
| SDK | 2.2.2-dev(38a443e) |
| Core | 3.0.2-17-g5266f22b=30002017 |
| lwIP | lwIP:STABLE-2_1_3_RELEASE |

### ESP32

- GCC 8.4.x with gnu++17
- Modified framework-arduinoespressif32 v2.0.9-mod

| System Info | |
|---|---|
| Hardware | ESP32 4.00MB Flash 80MHz@QIO, 2x240MHz, Free RAM 136.53KB, Temperature 35.6°C |
| Framework | Arduino ESP32 2.0.9 |
| SDK | ESP-IDF Version v4.4.4  |

## Libraries

The libraries have found a new home

[https://github.com/sascha432/KFCLibrary](https://github.com/sascha432/KFCLibrary)
[https://github.com/sascha432/ESP8266_NvsFlash_NONOS_SDK](https://github.com/sascha432/ESP8266_NvsFlash_NONOS_SDK)

## Plugins

### AT Mode

Configure and control the device with AT commands via serial interface or WebUI

### MQTT Client

MQTT Client with API to easily add components that work with Home Assistant, as well as auto discovery

### SSDP auto discovery

Shows up as device and links to the WebUI

## MDNS auto discovery with zeroconf support

MDNS discovery and zeroconf support for MQTT, Syslog and Home Assistant

### Http2Serial

Serial console access over the WebUI using web sockets

### Serial2TCP

Serial console redirection via TCP in client or server mode

### STK500v1

Fully asynchronous STK500v1 programmer over WiFi, serial or web server upload...

### NTP Client

NTP Client with timezone support

### Syslog

Send log messages to a syslog server via TCP/UDP

### Ping Monitor

Ping remote hosts over the WebUI and run background ping statistics

### I2C Scanner

Scan GPIO pins for I2C devices

### Sensor

Plugin for different sensors. Supply Voltage/Battery with charging indicator, BME280, BME680, CCS811, HLW8012, LM75A, DS3231, INA219, DHT11, DHT22, AM2301, BH1750FVI, Motion sensors and system metrics
Support for native WebUI and MQTT

### Mpr121Touchpad

Support for capacitive touch pads using the MPR121 sensor. With gestures detection

### Switch

Plugin for relays and other devices connected to GPIO pins, for example Sonoff basic
Support for native WebUI and MQTT.

### Alarm

Plugin for up to 10 different alarms, single alarm or repeated on daily basis

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

WLED compatible ESP32 controller, 4 outputs, 160W, max. 4096 LEDs (4x512 recommended for 60fps)
[ESP32 WLED Controller](https://oshwlab.com/sascha23095123423/wled-controller_copy)

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

### FastLED (Clock/LED matrix plugin)

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
