# esp8266-kfc-fw

Firmware for ESP8266/ESP32 and IoT devices

The firmware offers a plugin interface to easily extend its functionality. The plugins can be configured via web interface and have access to the network, file system, logging, serial console, I2C bus, GPIO, GPIO extenders, EEPROM, improved and stable internal ADC, RTC memory, a (more or less) precise RTC during deep sleep, a maximum of 30 days deep sleep, an event/task scheduler, direct access to flash storage (copy on write) without file system overhead, crash logs with stack traces, etc... via API functions. Support for deep sleep, quick boot (~37ms) and WiFi Quick Connect (<230ms) after wake up is also available

## Required flash size

1M with most features activated, no OTA updates

2M with OTA

## ChangeLog

**SPIFFS** has been replaced with **LittleFS** starting with version 0.0.4.7604

[Version 0.0.4](./CHANGELOG.md)

[Version 0.0.3](https://github.com/sascha432/esp8266-kfc-fw/blob/v0.0.3/CHANGELOG.md)

[Version 0.0.2](https://github.com/sascha432/esp8266-kfc-fw/blob/027305464fb486606840fa821595f7bce6e6ef1d/CHANGELOG.md)

## Stable versions

I started to create branches of stable versions for my different devices.

[7 Channel Relay Board - Version 0.0.4.7598](https://github.com/sascha432/esp8266-kfc-fw/tree/relay_board_0.0.4.7598)

[BME280 Sensor - Version 0.0.4.7552](https://github.com/sascha432/esp8266-kfc-fw/tree/bme280_0.0.4.7552)

[Blinds Control - Version 0.0.4.7591](https://github.com/sascha432/esp8266-kfc-fw/tree/blinds_control_0.0.4.7591)

[Remote Control - Version 0.0.4.7545](https://github.com/sascha432/esp8266-kfc-fw/tree/remote_control_0.0.4.7545)

## Libraries

### KFCSyslog

Send messages to a syslog server via UDP, TCP and TLS

### KFCWebBuilder

Framework to build WebUIs with bootstrap and store them mostly compressed in a virtual file system. Combined with server side includes, complex dynamic web pages/forms with a low memory footprint can be created

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
