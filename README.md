# esp8266-kfc-fw

Firmware for ESP8266/ESP32 and IoT devices

The firmware offers a plugin interface to easily extend its functionality. The plugins can be configured via web interface and have access to the network, file system, logging, serial console, I2C bus, GPIO, EEPROM, RTC memory, event/task scheduler etc... via API functions. Support for deep sleep and WiFi quick connect (~250ms) after wake up.

## Required flash size

1M with most features activated, no OTA updates
2M with OTA

## ChangeLog

[Version 0.0.4](./CHANGELOG.md)

[Version 0.0.3](https://github.com/sascha432/esp8266-kfc-fw/blob/v0.0.3/CHANGELOG.md)

[Version 0.0.2](https://github.com/sascha432/esp8266-kfc-fw/blob/027305464fb486606840fa821595f7bce6e6ef1d/CHANGELOG.md)

## Libraries

### KFCSyslog

Send messages to a syslog server via UDP, TCP and TLS

### KFCWebBuilder

Framework to build WebUIs with bootstrap and store them mostly compressed in a virtual file system. Combined with server side includes, complex dynamic web pages with a low memory footprint can be created

### KFCVirtualFileSystem

~~Read only file system with long filename support, optimized for low memory environments~~
Due to constantly changing file system implementations in the Arduno frameworks currently replaced with long filename support on top of SPIFFS/LittleFS, but no transparent access for Dir()/File() anymore. Replacement classes ListDir and SPIFFSWrapper.

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

## Plugins

### AT Mode

Configure and control the device with AT commands via serial interface or WebUI

### MQTT Client

MQTT Client with API to easily add components that work with Home Assistant, as well as auto discovery

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

https://github.com/sascha432/ESP8266-WiFi-Clock
https://easyeda.com/sascha23095123423/iot_wifi_7segment
https://easyeda.com/sascha23095123423/iot_wifi_clock_controller

### Dimmer

Plugin to control my trailing edge WiFi dimmer with energy monitor

https://github.com/sascha432/trailing_edge_dimmer

4 Channel with power monitor
https://easyeda.com/sascha23095123423/iot-4-channel-dimmer-with-pm

1 Channel in-wall or plugin with power monitor
https://easyeda.com/sascha23095123423/iot_1ch_dimmer_copy_copy_copy
Control module
https://easyeda.com/sascha23095123423/esp12e_iot_module_copy

### Blinds Controller

Plugin for my 2 channel blinds controller

https://github.com/sascha432/ESP8266-WiFi-Blinds
https://easyeda.com/sascha23095123423/iot_blinds_controller

### Remote Control

Plugin for my 4 button ultra low power WiFi remote control

https://github.com/sascha432/ESP8266-WiFi-Remote-Control
https://easyeda.com/sascha23095123423/iot_4ch_remote

### Weather Station

Weather Station for TFT displays with openweathermap.org API.
Live remote view over WebUI and screenshots in .BMP format

https://easyeda.com/sascha23095123423/iot_weather_station
https://github.com/sascha432/ESP8266-WiFi-Weather-Station

### Home Assistant

Home Assistant RESTful API

### File Manager

WebUI to explore and modify SPIFFS and KFCVirtualFileSystem

## Required third party libraries

### ESPAsyncWebServer

https://github.com/me-no-dev/ESPAsyncWebServer

### asyncHTTPrequest

https://github.com/boblemaire/asyncHTTPrequest

### Arduino Cryptography Library

https://github.com/rweather/arduinolibs

### AsyncMqttClient (MQTT plugin)

https://github.com/marvinroger/async-mqtt-client

### AsyncWebSocket (Http2Serial plugin)

https://github.com/me-no-dev/ESPAsyncWebServer/blob/master/src/AsyncWebSocket.h

### AsyncPing (Ping Monitor plugin)

https://github.com/akaJes/AsyncPing

### EspSaveCrash (store crash dump in EEPROM)

https://github.com/krzychb/EspSaveCrash.git

### RF24 (RF24 Master)

https://github.com/nRF24/RF24

### FastLED (Clock plugin)

https://github.com/FastLED/FastLED

### Adafruit Libraries (Various plugins)

https://github.com/adafruit/Adafruit_Sensor
https://github.com/adafruit/Adafruit_BME280_Library
https://github.com/adafruit/Adafruit_CCS811.git
https://github.com/adafruit/Adafruit_INA219
https://github.com/adafruit/Adafruit-GFX-Library
https://github.com/adafruit/Adafruit-ST7735-Library
https://github.com/adafruit/Adafruit_SSD1306
https://github.com/adafruit/RTClib
https://github.com/adafruit/Adafruit_MPR121
https://github.com/adafruit/Adafruit_NeoPixel
