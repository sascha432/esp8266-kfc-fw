/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#define FIRMWARE_IDENT                  0x72a82165
#define FIRMWARE_VERSION_MAJOR          0
#define FIRMWARE_VERSION_MINOR          0
#define FIRMWARE_VERSION_REVISION       5
#define ________STR(str)                      #str
#define _______STR(str)                 ________STR(str)
#define FIRMWARE_VERSION                (((FIRMWARE_VERSION_MAJOR & ((1U << 5) - 1)) << 11) | ((FIRMWARE_VERSION_MINOR & ((1U << 5) - 1)) << 6) | (FIRMWARE_VERSION_REVISION & ((1U << 6) - 1)))
#define FIRMWARE_VERSION_STR            _______STR(FIRMWARE_VERSION_MAJOR) "." _______STR(FIRMWARE_VERSION_MINOR) "." _______STR(FIRMWARE_VERSION_REVISION)

#if defined(ESP8266)

#include <core_version.h>

// supported releases
#if defined(ARDUINO_ESP8266_RELEASE_2_5_2)
#define ARDUINO_ESP8266_VERSION_COMBINED 0x020502
#elif defined(ARDUINO_ESP8266_RELEASE_2_6_3)
#define ARDUINO_ESP8266_VERSION_COMBINED 0x020603
#elif defined(ARDUINO_ESP8266_RELEASE_2_7_1)
#define ARDUINO_ESP8266_VERSION_COMBINED 0x020701
#elif defined(ARDUINO_ESP8266_RELEASE_2_7_2)
#define ARDUINO_ESP8266_VERSION_COMBINED 0x020702
#elif defined(ARDUINO_ESP8266_RELEASE_2_7_4)
#define ARDUINO_ESP8266_VERSION_COMBINED 0x020704
#elif defined(ARDUINO_ESP8266_RELEASE_3_0_0)
#define ARDUINO_ESP8266_VERSION_COMBINED 0x030000
#elif defined(ARDUINO_ESP8266_RELEASE_3_0_1)
#define ARDUINO_ESP8266_VERSION_COMBINED 0x030001
#elif ARDUINO_ESP8266_DEV
#define ARDUINO_ESP8266_VERSION_COMBINED 0x030001
#else
#error set ARDUINO_ESP8266_VERSION_COMBINED
#endif

#endif

#ifndef ASYNC_TCP_SSL_ENABLED
#define ASYNC_TCP_SSL_ENABLED                       0
#endif

#ifndef KFC_SERIAL_PORT
#define KFC_SERIAL_PORT                             Serial
#endif

// set to baud rate to use Serial1 for all debug output
#ifndef KFC_DEBUG_USE_SERIAL1
#define KFC_DEBUG_USE_SERIAL1                       0
// #define KFC_DEBUG_USE_SERIAL1                        115200
#endif

// this port gets initialized during boot and all output is displayed there until
// the system has been initialized
// NOTE: Serial0 = Serial
#ifndef KFC_SAFE_MODE_SERIAL_PORT
#define KFC_SAFE_MODE_SERIAL_PORT                   Serial0
#endif

#ifndef KFC_SERIAL_RATE
#define KFC_SERIAL_RATE                             115200
#endif

#ifndef DEBUG
// Enable debug mode
#define DEBUG                                       1
#endif

#ifndef DEBUG_OUTPUT
// Output stream for debugging messages
#define DEBUG_OUTPUT                                DebugSerial
#endif

#ifndef KFC_TWOWIRE_SDA
#define KFC_TWOWIRE_SDA                             4
#endif

#ifndef KFC_TWOWIRE_SCL
#define KFC_TWOWIRE_SCL                             5
#endif

#ifndef KFC_TWOWIRE_CLOCK_STRETCH
#define KFC_TWOWIRE_CLOCK_STRETCH                   45000
#endif

#ifndef KFC_TWOWIRE_CLOCK_SPEED
#define KFC_TWOWIRE_CLOCK_SPEED                     100000
#endif

#ifndef ENABLE_DEEP_SLEEP
#define ENABLE_DEEP_SLEEP                           0
#endif

#ifndef DEBUG_INCLUDE_SOURCE_INFO
// Add source file and line number to the debug output
#define DEBUG_INCLUDE_SOURCE_INFO                   0
#endif

#ifndef LOGGER
// Logging to files on FS
#define LOGGER                                      1
#endif

#ifndef LOGGER_MAX_FILESIZE
// max. size of the log file. if the size is exceeded, a backup is created
// and a new log started. 0 = no size limit
#    if DEBUG
#        define LOGGER_MAX_FILESIZE                 4096
#    else
#        define LOGGER_MAX_FILESIZE                 65535
#    endif
#endif

#ifndef LOGGER_MAX_BACKUP_FILES
// max. number of backup files to keep
#    if DEBUG
#        define LOGGER_MAX_BACKUP_FILES             32
#    else
#        define LOGGER_MAX_BACKUP_FILES             4
#    endif
#endif

#ifndef AT_MODE_SUPPORTED
#define AT_MODE_SUPPORTED                           1               // AT commands over serial
#endif

#ifndef HTTP2SERIAL_SUPPORT
#define HTTP2SERIAL_SUPPORT                         1               // HTTP2Serial bridge (requires ATMODE support)
#endif

#ifndef IOT_SSDP_SUPPORT
#define IOT_SSDP_SUPPORT                            1
#endif

#if HTTP2SERIAL_SUPPORT && !WEBSERVER_SUPPORT
#pragma message("HTTP2SERIAL_SUPPORT requires WEBSERVER_SUPPORT. Automatically disabled!")
#undef HTTP2SERIAL_SUPPORT
#define HTTP2SERIAL_SUPPORT                                 0
#endif

// check http2serial.h for a description
#ifndef HTTP2SERIAL_SERIAL_BUFFER_FLUSH_DELAY
#define HTTP2SERIAL_SERIAL_BUFFER_FLUSH_DELAY               75
#endif
#ifndef HTTP2SERIAL_SERIAL_BUFFER_MAX_LEN
#define HTTP2SERIAL_SERIAL_BUFFER_MAX_LEN                   512
#endif
#ifndef HTTP2SERIAL_SERIAL_BUFFER_MIN_LEN
#define HTTP2SERIAL_SERIAL_BUFFER_MIN_LEN                   128
#endif

#ifndef KFC_SERIAL_PORT
#define KFC_SERIAL_PORT Serial
#endif

// disable at mode when a client connects to the serial console via web socket
#ifndef HTTP2SERIAL_DISABLE_AT_MODE
#define HTTP2SERIAL_DISABLE_AT_MODE                         0
#endif

#ifndef NTP_CLIENT
// NTP client support
#define NTP_CLIENT                                          1
#endif

#ifndef MQTT_SUPPORT
// Support for a MQTT broker
#define MQTT_SUPPORT                                        1
#endif

#ifndef MQTT_USE_PACKET_CALLBACKS
#define MQTT_USE_PACKET_CALLBACKS                           0
#endif

#ifndef MQTT_AUTO_DISCOVERY
#define MQTT_AUTO_DISCOVERY                                 1
#endif

#ifndef WEBSERVER_SUPPORT
// enable embded web server
#define WEBSERVER_SUPPORT                                   1
#endif

#ifndef SECURITY_LOGIN_ATTEMPTS
#define SECURITY_LOGIN_ATTEMPTS                             1
#endif

// time in seconds before the system is in a stable state
// if it keeps crashing within this timeframe, safe mode is activated
#ifndef KFC_CRASH_RECOVERY_TIME
#define KFC_CRASH_RECOVERY_TIME                             300
#endif

// if the device crashes more than 3 times within KFC_CRASH_RECOVERY_TIME, start safe mode automatically
// 0 = disable
#ifndef KFC_AUTO_SAFE_MODE_CRASH_COUNT
#define KFC_AUTO_SAFE_MODE_CRASH_COUNT                      3
#endif

// number of crashes within KFC_CRASH_RECOVERY_TIME to activate safe mode
#ifndef KFC_CRASH_SAFE_MODE_COUNT
#define KFC_CRASH_SAFE_MODE_COUNT                           3
#endif

// resets are counted with RESET_DETECTOR_TIMEOUT (default=5sec). if the device is running
// for a longer period of time, the counter is set to 0. the counter starts with 1 after
// the first reset

// resetting the device 4 times in a row restores factory setttings
// this is indicated by flashing the LED for RESET_DETECTOR_TIMEOUT
// 0 = disable
#ifndef KFC_RESTORE_FACTORY_SETTINGS_RESET_COUNT
#define KFC_RESTORE_FACTORY_SETTINGS_RESET_COUNT            4
#endif

// if the device is reset more than 1 time in row, the boot menu is displayed on KFC_SAFE_MODE_SERIAL_PORT
// 0 = disable boot menu
#ifndef KFC_SHOW_BOOT_MENU_RESET_COUNT
#define KFC_SHOW_BOOT_MENU_RESET_COUNT                      1
#endif

// continue normal boot after displaying the boot menu
// if a timeout occurs, the reset is counted as system crash and safe mode
// gets automatically activated (see KFC_AUTO_SAFE_MODE_CRASH_COUNT)
#ifndef KFC_BOOT_MENU_TIMEOUT
#define KFC_BOOT_MENU_TIMEOUT                               15
#endif

// option for KFCWebBuilder
// Minify html, css and js
#ifndef MINIFY_WEBUI
#define MINIFY_WEBUI                                        1
#endif

// TLS support for the web server
#if WEBSERVER_SUPPORT && !defined(WEBSERVER_TLS_SUPPORT)
#if defined(ESP32)
#define WEBSERVER_TLS_SUPPORT                               0
#elif defined(ESP8266)
#define WEBSERVER_TLS_SUPPORT                               0
#endif
#endif

// Simple web file manager for FS
#ifndef FILE_MANAGER
#define FILE_MANAGER                                        1
#endif
#if FILE_MANAGER && !WEBSERVER_SUPPORT
#error WEBSERVER_SUPPORT=1 required
#endif

#if defined(ESP32)
    #if ASYNC_TCP_SSL_ENABLED
        #error ASYNC_TCP_SSL_ENABLED not supported
    #endif
#elif defined(ESP8266)
    #if ASYNC_TCP_SSL_ENABLED && WEBSERVER_TLS_SUPPORT
        #warning Using SSL requires a lot RAM and processing power. It is not recommended to run the web server with TLS. This might lead to poor performance and stability.
    #endif
    #if WEBSERVER_TLS_SUPPORT && !ASYNC_TCP_SSL_ENABLED
        #error TLS support for ESPAsyncTCP is not enabled
    #endif
#endif

// offset of the configuration in the EEPROM
// length of the data stored in EEPROM, actual size required would be CONFIG_EEPROM_SIZELENGTH
#ifndef CONFIG_EEPROM_SIZE
#define CONFIG_EEPROM_SIZE                                  4096
#endif

// MDNS support
#ifndef MDNS_PLUGIN
#define MDNS_PLUGIN                                         1
#endif

// NETBIOS support for MDNS plugin
#ifndef MDNS_NETBIOS_SUPPORT
#define MDNS_NETBIOS_SUPPORT                                1
#endif

#if !MDNS_PLUGIN && MDNS_NETBIOS_SUPPORT
#error MDNS_PLUGIN=1 required
#endif

#ifndef HAS_STRFTIME_P
#define HAS_STRFTIME_P                                      1
#endif

#ifndef __ICACHE_FLASH_ATTR
#define __ICACHE_FLASH_ATTR                                 ICACHE_FLASH_ATTR
#endif

// default "0" defines for plugins

#ifndef IOT_SWITCH
#define IOT_SWITCH                                          0
#endif

// 0 for internal RTC, 1 for external
#ifndef RTC_SUPPORT
#define RTC_SUPPORT                                         0
#endif

#ifndef RTC_DEVICE_DS3231
#define RTC_DEVICE_DS3231                                   0
#endif

#ifndef IOT_BLINDS_CTRL
#define IOT_BLINDS_CTRL                                     0
#endif

#ifndef PING_MONITOR_SUPPORT
#define PING_MONITOR_SUPPORT                                0
#endif

#ifndef REST_API_SUPPORT
#define REST_API_SUPPORT                                    0
#endif

#ifndef I2CSCANNER_PLUGIN
#define I2CSCANNER_PLUGIN                                   0
#endif

#ifndef IOT_CLOCK
#define IOT_CLOCK                                           0
#endif

#ifndef IOT_ALARM_PLUGIN_ENABLED
#define IOT_ALARM_PLUGIN_ENABLED                            0
#endif

#ifndef IOT_RF24_MASTER
#define IOT_RF24_MASTER                                     0
#endif

#ifndef IOT_WEATHER_STATION
#define IOT_WEATHER_STATION                                 0
#endif

#ifndef SSD1306_PLUGIN
#define SSD1306_PLUGIN                                      0
#endif

#ifndef IOT_ATOMIC_SUN_V2
#define IOT_ATOMIC_SUN_V2                                   0
#endif

#ifndef IOT_DIMMER_MODULE
#define IOT_DIMMER_MODULE                                   0
#endif

#ifndef PIN_MONITOR
#define PIN_MONITOR                                         0
#endif

#ifndef IOT_SENSOR
#define IOT_SENSOR                                          0
#endif
#define IOT_SENSOR_HAS(name)                                defined(IOT_SENSOR_##name) && (IOT_SENSOR_##name)

#ifndef IOT_SENSOR_HAVE_SYSTEM_METRICS
#define IOT_SENSOR_HAVE_SYSTEM_METRICS                      1
#endif

#ifndef SERIAL2TCP_SUPPORT
#define SERIAL2TCP_SUPPORT                                  0
#endif

#ifndef STK500V1
#define STK500V1                                            0
#endif

#ifndef IOT_ALARM_PLUGIN_ENABLED
#define IOT_ALARM_PLUGIN_ENABLED                            0
#endif

#if IOT_ALARM_PLUGIN_ENABLED
#define IF_IOT_ALARM_PLUGIN_ENABLED(...) __VA_ARGS__
#else
#define IF_IOT_ALARM_PLUGIN_ENABLED(...)
#endif

#ifndef ENABLE_ARDUINO_OTA
#define ENABLE_ARDUINO_OTA                                  0
#endif

#ifndef ENABLE_ARDUINO_OTA_AUTOSTART
#define ENABLE_ARDUINO_OTA_AUTOSTART                        0
#endif


#ifndef HAVE_PCF8574
#define HAVE_PCF8574                                        0
#endif

#ifndef PCF8574_I2C_ADDRESS
#define PCF8574_I2C_ADDRESS                                 0x20
#endif

#ifndef PCF8574_PORT_RANGE_START
#define PCF8574_PORT_RANGE_START                            128
#define PCF8574_PORT_RANGE_END                              (PCF8574_PORT_RANGE_START + 8)
#endif

#ifndef TINYPWM_PORT_RANGE_START
#define TINYPWM_PORT_RANGE_START                            140
#define TINYPWM_PORT_RANGE_END                              (TINYPWM_PORT_RANGE_START + 2)
#endif

#ifndef HAVE_I2CSCANNER
#define HAVE_I2CSCANNER                                     1
#endif

// disable crash counter on FS
#ifndef KFC_DISABLE_CRASHCOUNTER
#define KFC_DISABLE_CRASHCOUNTER                            0
#endif

// delay after writing "Starting in safemode..."
#ifndef KFC_SAFEMODE_BOOT_DELAY
#define KFC_SAFEMODE_BOOT_DELAY                             2000
#endif

// enable safemode by having one or multiple pins high/low during boot
// all pins are read 4 times in a 50ms interval and if any of those
// tests fails, safemode will not be activated
//
#ifndef KFC_SAFEMODE_GPIO_COMBO
#define KFC_SAFEMODE_GPIO_COMBO                             0
#endif

// mask all GPIOs set in this mask
#ifndef KFC_SAFEMODE_GPIO_MASK
#define KFC_SAFEMODE_GPIO_MASK                              0
#endif

// compare the masked result with this value
// every bit set will represent GPIO active high, every bit not set GPIO active low
// bits that are masked need to be 0
#ifndef KFC_SAFEMODE_GPIO_RESULT
#define KFC_SAFEMODE_GPIO_RESULT                            0
#endif

#if (KFC_SAFEMODE_GPIO_RESULT & KFC_SAFEMODE_GPIO_MASK) != KFC_SAFEMODE_GPIO_RESULT
#error invalid KFC_SAFEMODE_GPIO_RESULT for KFC_SAFEMODE_GPIO_MASK
#endif

#ifndef KFC_HAVCE_NEOPIXEL_EX
#define KFC_HAVCE_NEOPIXEL_EX                               0
#endif

#ifndef HAVE_GDBSTUB
#define HAVE_GDBSTUB                                        0
#endif

class Stream;
class HardwareSerial;

extern Stream &Serial;
extern Stream &DebugSerial;
extern HardwareSerial Serial0;
