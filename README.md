# esp8266-kfc-fw
Firmware for ESP8266/ESP32 and IoT devices

The firmware offers a plugin interface to easily extend its functionality. The plugins can be configured via web interface and have access to the network, file system, logging, serial console, I2C bus, GPIO, EEPROM, RTC memory etc... via API functions. Support for deep sleep and WiFi quick connect (~250-300ms) after wake up.

## Libraries

### KFCSyslog

Send messages to a syslog server via UDP (or TCP not implemented yet)

### KFCTimezone

Platform independent timezone implementation with remote API support (Self hosted PHP script or https://timezonedb.com/register)

### KFCWebBuilder

Framework to build Web UIs with bootstrap and store them mostly compressed in a virtual file system

### KFCVirtualFileSystem

Read only file system with long filename support, optimized for low memory environments

### KFCJson

Library to read streamed JSON documents

### KFCEventScheduler

Platform indepentend Timer, Scheduler, WiFi Callbacks and loop function callbacks

### KFCResetDetector

Detect crashes and offer safe mode by pressing reset button during boot, count number of resets for extended functionality (reset configuration, open public hotspot etc...)
It also offers a RTC memory mananger to store data identified by an unqiue id

### KFCConfiguration

Library to handle configurations stored in the EEPROM, read on demand to save memory, manage changes in structure automatically

## Plugins

### AT Mode

Configure and control the device with AT commands via serial interface or web UI

### MQTT Client

MQTT Client with API to easily add components that work with Home Assistant, as well as auto discovery

### Http2Serial

Serial console access over the web UI using web sockets

### NTP Client

NTP Client with timezone support via REST API

### Syslog

Send log messages to a syslog server

### Ping Monitor

Ping remote hosts over the webui and run background ping statistics

### I2C Scanner

Scan all GPIO pins for I2C devices

### File Manager

Web UI to explore and modify SPIFFS and KFCVirtualFileSystem

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
