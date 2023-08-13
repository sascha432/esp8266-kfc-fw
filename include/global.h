/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#define FIRMWARE_IDENT            0x72a82168
#define FIRMWARE_VERSION_MAJOR    0
#define FIRMWARE_VERSION_MINOR    0
#define FIRMWARE_VERSION_REVISION 8
#define ________STR(str)          #str
#define _______STR(str)           ________STR(str)
#define FIRMWARE_VERSION          (((FIRMWARE_VERSION_MAJOR & ((1U << 5) - 1)) << 11) | ((FIRMWARE_VERSION_MINOR & ((1U << 5) - 1)) << 6) | (FIRMWARE_VERSION_REVISION & ((1U << 6) - 1)))
#define FIRMWARE_VERSION_STR      _______STR(FIRMWARE_VERSION_MAJOR) \
                                  "." _______STR(FIRMWARE_VERSION_MINOR) "." _______STR(FIRMWARE_VERSION_REVISION)

#if defined(ESP8266)

#   include <core_version.h>

#    if ARDUINO_ESP8266_MAJOR == 0
#        error Invalid coe config
#   endif

#elif defined(ESP32)

// #   include <core_version.h>
#    include <esp_arduino_version.h>

#endif

#ifndef ASYNC_TCP_SSL_ENABLED
#    define ASYNC_TCP_SSL_ENABLED 0
#endif

#ifndef KFC_SERIAL_PORT
#    define KFC_SERIAL_PORT Serial
#endif

// set to baud rate to use Serial1 for all debug output
#ifndef KFC_DEBUG_USE_SERIAL1
#    define KFC_DEBUG_USE_SERIAL1 0
// #define KFC_DEBUG_USE_SERIAL1                        115200
#endif

// this port gets initialized during boot and all output is displayed there until
// the system has been initialized
// NOTE: Serial0 = Serial
#ifndef KFC_SAFE_MODE_SERIAL_PORT
#    define KFC_SAFE_MODE_SERIAL_PORT Serial0
#endif

#ifndef KFC_SERIAL_RATE
#    define KFC_SERIAL_RATE 115200
#endif

#ifndef DEBUG
// Enable debug mode
#    define DEBUG 1
#endif

#ifndef DEBUG_OUTPUT
// Output stream for debugging messages
#    define DEBUG_OUTPUT DebugSerial
#endif

#ifndef KFC_TWOWIRE_SDA
#    define KFC_TWOWIRE_SDA 4
#endif

#ifndef KFC_TWOWIRE_SCL
#    define KFC_TWOWIRE_SCL 5
#endif

#ifndef KFC_TWOWIRE_CLOCK_STRETCH
#    define KFC_TWOWIRE_CLOCK_STRETCH 45000
#endif

#ifndef KFC_TWOWIRE_CLOCK_SPEED
#    define KFC_TWOWIRE_CLOCK_SPEED 100000
#endif

#ifndef ENABLE_DEEP_SLEEP
#    define ENABLE_DEEP_SLEEP 0
#endif

#ifndef DEBUG_INCLUDE_SOURCE_INFO
// Add source file and line number to the debug output
#    define DEBUG_INCLUDE_SOURCE_INFO 0
#endif

#ifndef LOGGER
// Logging to files on FS
#    define LOGGER 1
#endif

#ifndef LOGGER_MAX_FILESIZE
// max. size of the log file. if the size is exceeded, a backup is created
// and a new log started. 0 = no size limit (NOT recommended)
#    define LOGGER_MAX_FILESIZE (4096 * 16)      // 64kb
#endif

#ifndef LOGGER_MAX_BACKUP_FILES
// max. number of backup files to keep
// make sure that the file system has enough free space at all times
#    if DEBUG
#        define LOGGER_MAX_BACKUP_FILES 3
#    else
#        define LOGGER_MAX_BACKUP_FILES 1
#    endif
#endif

#ifndef AT_MODE_SUPPORTED
#    define AT_MODE_SUPPORTED 1 // AT commands over serial
#endif

#ifndef AT_MODE_HELP_SUPPORTED
#    define AT_MODE_HELP_SUPPORTED 0
#endif

#ifndef HTTP2SERIAL_SUPPORT
#    define HTTP2SERIAL_SUPPORT 1 // HTTP2Serial bridge (requires ATMODE support)
#endif

#ifndef IOT_SSDP_SUPPORT
#    define IOT_SSDP_SUPPORT 1
#endif

#ifndef WEBSERVER_SPEED_TEST
#    define WEBSERVER_SPEED_TEST 0
#endif

// check http2serial.h for a description
#ifndef HTTP2SERIAL_SERIAL_BUFFER_FLUSH_DELAY
#    define HTTP2SERIAL_SERIAL_BUFFER_FLUSH_DELAY 75
#endif
#ifndef HTTP2SERIAL_SERIAL_BUFFER_MAX_LEN
#    define HTTP2SERIAL_SERIAL_BUFFER_MAX_LEN 512
#endif
#ifndef HTTP2SERIAL_SERIAL_BUFFER_MIN_LEN
#    define HTTP2SERIAL_SERIAL_BUFFER_MIN_LEN 128
#endif

#ifndef KFC_SERIAL_PORT
#    define KFC_SERIAL_PORT Serial
#endif

// disable at mode when a client connects to the serial console via web socket
#ifndef HTTP2SERIAL_DISABLE_AT_MODE
#    define HTTP2SERIAL_DISABLE_AT_MODE 0
#endif

#ifndef NTP_CLIENT
// NTP client support
#    define NTP_CLIENT 1
#endif

#ifndef MQTT_SUPPORT
// Support for a MQTT broker
#    define MQTT_SUPPORT 1
#endif

#ifndef MQTT_USE_PACKET_CALLBACKS
#    define MQTT_USE_PACKET_CALLBACKS 0
#endif

#ifndef MQTT_AUTO_DISCOVERY
#    define MQTT_AUTO_DISCOVERY 1
#endif

// record login failures and block IP if there are too many failed attempts
#ifndef SECURITY_LOGIN_ATTEMPTS
#    define SECURITY_LOGIN_ATTEMPTS 1
#endif

// time in seconds before the system is in a stable state
// if it keeps crashing within this time frame, safe mode is activated
#ifndef KFC_CRASH_RECOVERY_TIME
#    define KFC_CRASH_RECOVERY_TIME 300
#endif

// if the device crashes more than 3 times within KFC_CRASH_RECOVERY_TIME, start safe mode automatically
// 0 = disable
#ifndef KFC_AUTO_SAFE_MODE_CRASH_COUNT
#    define KFC_AUTO_SAFE_MODE_CRASH_COUNT 3
#endif

// number of crashes within KFC_CRASH_RECOVERY_TIME to activate safe mode
#ifndef KFC_CRASH_SAFE_MODE_COUNT
#    define KFC_CRASH_SAFE_MODE_COUNT 3
#endif

// resets are counted with RESET_DETECTOR_TIMEOUT (default=5sec). if the device is running
// for a longer period of time, the counter is set to 0. the counter starts with 1 after
// the first reset

// resetting the device 4 times in a row restores factory settings
// this is indicated by flashing the LED for RESET_DETECTOR_TIMEOUT
// 0 = disable
#ifndef KFC_RESTORE_FACTORY_SETTINGS_RESET_COUNT
#    define KFC_RESTORE_FACTORY_SETTINGS_RESET_COUNT 4
#endif

// if the device is reset more than 1 time in row, the boot menu is displayed on KFC_SAFE_MODE_SERIAL_PORT
// 0 = disable boot menu
#ifndef KFC_SHOW_BOOT_MENU_RESET_COUNT
#    define KFC_SHOW_BOOT_MENU_RESET_COUNT 1
#endif

// continue normal boot after displaying the boot menu
// if a timeout occurs, the reset is counted as system crash and safe mode
// gets automatically activated (see KFC_AUTO_SAFE_MODE_CRASH_COUNT)
#ifndef KFC_BOOT_MENU_TIMEOUT
#    define KFC_BOOT_MENU_TIMEOUT 15
#endif

// option for KFCWebBuilder
// Minify html, css and js
#ifndef MINIFY_WEBUI
#    define MINIFY_WEBUI 1
#endif

// TLS support for the web server
#if !defined(WEBSERVER_TLS_SUPPORT)
#    if defined(ESP32)
#        define WEBSERVER_TLS_SUPPORT 0
#    elif defined(ESP8266)
#        define WEBSERVER_TLS_SUPPORT 0
#    endif
#endif

// Simple web file manager for FS
#ifndef FILE_MANAGER
#    define FILE_MANAGER 1
#endif

#if defined(ESP32)
#    if ASYNC_TCP_SSL_ENABLED
#        error ASYNC_TCP_SSL_ENABLED not supported
#    endif
#elif defined(ESP8266)
#    if ASYNC_TCP_SSL_ENABLED && WEBSERVER_TLS_SUPPORT
#        warning Using SSL requires a lot RAM and processing power. It is not recommended to run the web server with TLS. This might lead to poor performance and stability.
#    endif
#    if WEBSERVER_TLS_SUPPORT && !ASYNC_TCP_SSL_ENABLED
#        error TLS support for ESPAsyncTCP is not enabled
#    endif
#endif

// offset of the configuration in the EEPROM
// length of the data stored in EEPROM, actual size required would be CONFIG_EEPROM_SIZELENGTH
#ifndef CONFIG_EEPROM_SIZE
#    define CONFIG_EEPROM_SIZE 4096
#endif

// MDNS support
#ifndef MDNS_PLUGIN
#    define MDNS_PLUGIN 1
#endif

// NETBIOS support for MDNS plugin
#ifndef MDNS_NETBIOS_SUPPORT
#    define MDNS_NETBIOS_SUPPORT 1
#endif

#if !MDNS_PLUGIN && MDNS_NETBIOS_SUPPORT
#    error MDNS_PLUGIN=1 required
#endif

#ifndef HAS_STRFTIME_P
#    define HAS_STRFTIME_P 1
#endif

#ifndef __ICACHE_FLASH_ATTR
#    define __ICACHE_FLASH_ATTR ICACHE_FLASH_ATTR
#endif

// time in minutes before the softAP is shutdown in safe mode
#ifndef SOFTAP_SHUTDOWN_TIME
#    define SOFTAP_SHUTDOWN_TIME 15
#endif

// default "0" defines for plugins

#ifndef IOT_SWITCH
#    define IOT_SWITCH 0
#endif

// 0 for internal RTC, 1 for external
#ifndef RTC_SUPPORT
#    define RTC_SUPPORT 0
#endif

#ifndef RTC_DEVICE_DS3231
#    define RTC_DEVICE_DS3231 0
#endif

#ifndef IOT_BLINDS_CTRL
#    define IOT_BLINDS_CTRL 0
#endif

#ifndef PING_MONITOR_SUPPORT
#    define PING_MONITOR_SUPPORT 0
#endif

#ifndef REST_API_SUPPORT
#    define REST_API_SUPPORT 0
#endif

#ifndef I2CSCANNER_PLUGIN
#    define I2CSCANNER_PLUGIN 0
#endif

#ifndef IOT_CLOCK
#    define IOT_CLOCK 0
#endif

#ifndef IOT_LED_MATRIX
#    define IOT_LED_MATRIX 0
#endif

#if defined(IOT_LED_MATRIX_OUTPUT_PIN4)
#    error more than 4 channels not supported
#elseif defined(IOT_LED_MATRIX_OUTPUT_PIN3) && IOT_LED_MATRIX_OUTPUT_PIN3 != -1
#    define IOT_LED_MATRIX_CHANNELS 4
#elif defined(IOT_LED_MATRIX_OUTPUT_PIN2) && IOT_LED_MATRIX_OUTPUT_PIN2 != -1
#    define IOT_LED_MATRIX_CHANNELS 3
#elif defined(IOT_LED_MATRIX_OUTPUT_PIN1) && IOT_LED_MATRIX_OUTPUT_PIN1 != -1
#    define IOT_LED_MATRIX_CHANNELS 2
#else
#    define IOT_LED_MATRIX_CHANNELS 1
#endif

#if CONFIG_HEAP_POISONING_COMPREHENSIVE
#    ifndef FASTLED_RMT_MAX_CHANNELS
#        define FASTLED_RMT_MAX_CHANNELS 1
#    endif
#    if IOT_LED_MATRIX_CHANNELS > 1
#        warning this might lead to 'Interrupt wdt timeout on CPUx'. set FASTLED_RMT_MAX_CHANNELS to 1 during debugging
#    endif
#endif


#if IOT_LED_MATRIX && !IOT_CLOCK
#    error IOT_LED_MATRIX requires IOT_CLOCK
#endif

#ifndef IOT_ALARM_PLUGIN_ENABLED
#    define IOT_ALARM_PLUGIN_ENABLED 0
#endif

#ifndef IOT_RF24_MASTER
#    define IOT_RF24_MASTER 0
#endif

#ifndef IOT_WEATHER_STATION
#    define IOT_WEATHER_STATION 0
#endif

#ifndef SSD1306_PLUGIN
#    define SSD1306_PLUGIN 0
#endif

#ifndef IOT_ATOMIC_SUN_V2
#    define IOT_ATOMIC_SUN_V2 0
#endif

#ifndef IOT_DIMMER_MODULE
#    define IOT_DIMMER_MODULE 0
#endif

#ifndef PIN_MONITOR
#    define PIN_MONITOR 0
#endif

#ifndef IOT_SENSOR
#    define IOT_SENSOR 0
#endif
#define IOT_SENSOR_HAS(name) defined(IOT_SENSOR_##name) && (IOT_SENSOR_##name)

#ifndef IOT_SENSOR_HAVE_SYSTEM_METRICS
#    define IOT_SENSOR_HAVE_SYSTEM_METRICS 1
#endif

#ifndef SERIAL2TCP_SUPPORT
#    define SERIAL2TCP_SUPPORT 0
#endif

#ifndef STK500V1
#    define STK500V1 0
#endif

#ifndef IOT_ALARM_PLUGIN_ENABLED
#    define IOT_ALARM_PLUGIN_ENABLED 0
#endif

#if IOT_ALARM_PLUGIN_ENABLED
#    define IF_IOT_ALARM_PLUGIN_ENABLED(...) __VA_ARGS__
#else
#    define IF_IOT_ALARM_PLUGIN_ENABLED(...)
#endif

#ifndef ENABLE_ARDUINO_OTA
#    define ENABLE_ARDUINO_OTA 1
#endif

#ifndef ENABLE_ARDUINO_OTA_AUTOSTART
#    define ENABLE_ARDUINO_OTA_AUTOSTART 0
#endif

#ifndef WEBSERVER_KFC_OTA
#    error WEBSERVER_KFC_OTA is not defined
#endif

// disable I2C scanner if I2C is not used
#if DISABLE_TWO_WIRE
#    undef HAVE_I2CSCANNER
#    define HAVE_I2CSCANNER 0
#else
#    ifndef HAVE_I2CSCANNER
#        define HAVE_I2CSCANNER 1
#    endif
#endif

// disable crash counter on FS
#ifndef KFC_DISABLE_CRASH_COUNTER
#    define KFC_DISABLE_CRASH_COUNTER 0
#endif

// delay after writing "Starting in safemode..."
#ifndef KFC_SAFEMODE_BOOT_DELAY
#    define KFC_SAFEMODE_BOOT_DELAY 2000
#endif

// enable safemode by having one or multiple pins high/low during boot
// all pins are read 4 times in a 50ms interval and if any of those
// tests fails, safemode will not be activated
//
#ifndef KFC_SAFEMODE_GPIO_COMBO
#    define KFC_SAFEMODE_GPIO_COMBO 0
#endif

// mask all GPIOs set in this mask
#ifndef KFC_SAFEMODE_GPIO_MASK
#    define KFC_SAFEMODE_GPIO_MASK 0
#endif

// compare the masked result with this value
// every bit set will represent GPIO active high, every bit not set GPIO active low
// bits that are masked need to be 0
#ifndef KFC_SAFEMODE_GPIO_RESULT
#    define KFC_SAFEMODE_GPIO_RESULT 0
#endif

#if (KFC_SAFEMODE_GPIO_RESULT & KFC_SAFEMODE_GPIO_MASK) != KFC_SAFEMODE_GPIO_RESULT
#    error invalid KFC_SAFEMODE_GPIO_RESULT for KFC_SAFEMODE_GPIO_MASK
#endif

#ifndef KFC_HAVE_NEOPIXEL_EX
#    define KFC_HAVE_NEOPIXEL_EX 0
#endif

#ifndef HAVE_GDBSTUB
#    define HAVE_GDBSTUB 0
#endif

#if defined(HAVE_IOEXPANDER)
#    if !defined(IOEXPANDER_DEVICE_CONFIG)
#        error HAVE_IOEXPANDER=1 but IOEXPANDER_DEVICE_CONFIG not defined
#    endif
#else
#    if defined(IOEXPANDER_DEVICE_CONFIG)
#        error IOEXPANDER_DEVICE_CONFIG defined but HAVE_IOEXPANDER missing
#    endif
#endif

// max. number for the word clock screen (1-4)
#ifndef WEATHER_STATION_MAX_CLOCKS
#    define WEATHER_STATION_MAX_CLOCKS 4
#endif

// enable screen to display an analog clock
#ifndef HAVE_WEATHER_STATION_ANALOG_CLOCK
#    define HAVE_WEATHER_STATION_ANALOG_CLOCK 1
#endif

// enable screen to display system/wifi information
#ifndef HAVE_WEATHER_STATION_INFO_SCREEN
#    define HAVE_WEATHER_STATION_INFO_SCREEN 1
#endif

// enable displaying jpeg images from /CuratedArt/*.jpg in a random order
#ifndef HAVE_WEATHER_STATION_CURATED_ART
#    define HAVE_WEATHER_STATION_CURATED_ART 1
#endif

// option to download BMP screenshots of the TFT display
#ifndef WEATHER_STATION_HAVE_BMP_SCREENSHOT
#    define WEATHER_STATION_HAVE_BMP_SCREENSHOT IOT_WEATHER_STATION
// #    define WEATHER_STATION_HAVE_BMP_SCREENSHOT 0
#endif

// check if pin 3 is used as output
// all possible PINs can be added here
#if ESP8266
#    if IOT_LED_MATRIX_STANDBY_PIN == 3 || IOT_LED_MATRIX_OUTPUT_PIN == 3
#        if !ESP8266_USE_UART_RX_AS_OUTPUT
#            error ESP8266_USE_UART_RX_AS_OUTPUT must be set to 1
#        endif
#    endif
#endif

class Stream;
class HardwareSerial;

extern Stream &Serial;
extern Stream &DebugSerial;
extern HardwareSerial Serial0;
