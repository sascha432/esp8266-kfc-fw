# Changelog

## Version 0.0.3

- Device configuration page for WebUI
- Option to turn status LED off when connected to WiFi
- Deep sleep support can be disabled
- Arduino ESP8266 2.6.3 release changes
- Support for MQTT component names instead of enumeration
- Conditional attributes for HTML forms
- Form validator for double
- Advanced firmware configuration for dimmer plugin
- Option to enable/disable mDNS
- Arduino ESP8266 2.7.1 release changes/fixes
- NTP startup delay reduced to 1 second (from up to 5 minutes)
- MQTT auto discovery queue to reduce memory usage and network load
- Send calibration/config. to MQTT for recovery after firmware updates
- MQTT auto discovery abbreviations
- MQTT auto discovery aggregates all entities as single device
- Replaced remote timezone with native POSIX timezone support

## Version 0.0.2

- Option to enable SoftAP only if WiFi station mode is not connected
- Option to disable the "WebUI" page
- Reboot after certain time if running in Safe Mode
- Restart WiFi subsystem once a minute if the connection failed
- Persistant message popups for the WebUI (WebAlerts)
- Multiple instances of Home Assistant supported
- Set mireds for Hass plugin
- Support for DHT11/22 sensor
- Store crash dumps in flash memory instead of EEPROM
- RestAPI class
- Replaced manual PROGMEM strings with ArduinoFlashStringGenerator
- Ambient light sensor for clock plugin
- WebUI/MMQT update rates seperated
- Support for INA219 sensor
- Improved MMQT TCP buffer handling
- MDNS service discovery for WebUI
- MDNS service queries
- MDNS version info and device name/title
- Improved WiFi quick connect performance by ~20%
- Disabled sending MQTT auto discovery after wake up
- Added device title
- Added bearer token for authentication (WebUI, web sockets, TCP2Serial...)
- Server side includes and extended HTML templates
- HLW8012 sensor update rate and precision improved
- Live view of HLW8012 sensor data via kfcfw_tool
- Cli OTA update tool for Flash, SPIFFS and ATmega flash images
- Dynamic menu for WebUI
- Support for RTC DS3231
- Support for espressif8266 2.3.2/arduino 2.6.3
- WiFi download speed test page for WebUI
- Export configuration as JSON
- Button groups for WebUI
- KFCGFx library with compressed canvas class to fit the contents of big TFTs in memory
- STK500 programmer for AVR over serial port via WiFi or from files stored on SPIFFS
- Support for CCS811 sensor
- Support for BME680 sensor
- Class to create JSON objects with streaming support with small buffer size
- Syslog over TCP support
- New virtual file system with reduced memory usage and long filenames
- WebUI "Reboot Device" checkbox "Safe Mode" fixed
- Fixed login issues with expired session cookie
- Fixed double menu entries after wake up from deep sleep
- Fixed invalid timezone/abbreviatation
- Fixed MDNS interface issues with SoftAP and station mode enabled
- Fixed missing SSID for WebUI WiFi scan
- Started to update change log

## Version 0.0.1

- Started to commit files
