/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#define FIRMWARE_IDENT                  0x72a82165
#define FIRMWARE_VERSION_MAJOR          0
#define FIRMWARE_VERSION_MINOR          0
#define FIRMWARE_VERSION_REVISION       4
#define ________STR(str)                      #str
#define _______STR(str)                 ________STR(str)
#define FIRMWARE_VERSION                ((FIRMWARE_VERSION_MAJOR<<16)|(FIRMWARE_VERSION_MINOR<<8)|FIRMWARE_VERSION_REVISION)
#define FIRMWARE_VERSION_STR            _______STR(FIRMWARE_VERSION_MAJOR) "." _______STR(FIRMWARE_VERSION_MINOR) "." _______STR(FIRMWARE_VERSION_REVISION)

#if ESP8266
#include <core_version.h>

// #define ARDUINO_ESP8266_GIT_DESC_STR            _______STR(ARDUINO_ESP8266_GIT_DESC) "."
// #define ARDUINO_ESP8266_VERSION_GET_INT(i)      (ARDUINO_ESP8266_GIT_DESC_STR[i + 1] == '.' ? (ARDUINO_ESP8266_GIT_DESC_STR[i] - '0') : (ARDUINO_ESP8266_GIT_DESC_STR[i + 2] == '.' ? (((ARDUINO_ESP8266_GIT_DESC_STR[i] - '0') * 10) + (ARDUINO_ESP8266_GIT_DESC_STR[i + 1] - '0')) : (((ARDUINO_ESP8266_GIT_DESC_STR[i] - '0') * 100) + ((ARDUINO_ESP8266_GIT_DESC_STR[i + 1] - '0') * 10) + (ARDUINO_ESP8266_GIT_DESC_STR[i + 2] - '0'))))

// #define ARDUINO_ESP8266_VERSION_GET_INT_OFS(n)  (n > 99 ? 4 : (n > 9 ? 3 : 2))

// #define ARDUINO_ESP8266_VERSION_MAJOR           ARDUINO_ESP8266_VERSION_GET_INT(0)
// #define ARDUINO_ESP8266_VERSION_MINOR           ARDUINO_ESP8266_VERSION_GET_INT(ARDUINO_ESP8266_VERSION_GET_INT_OFS(ARDUINO_ESP8266_VERSION_MAJOR))
// #define ARDUINO_ESP8266_VERSION_REVISION        ARDUINO_ESP8266_VERSION_GET_INT(ARDUINO_ESP8266_VERSION_GET_INT_OFS(ARDUINO_ESP8266_VERSION_MAJOR) + ARDUINO_ESP8266_VERSION_GET_INT_OFS(ARDUINO_ESP8266_VERSION_MINOR))
// #define ARDUINO_ESP8266_VERSION_COMBINED        ((ARDUINO_ESP8266_VERSION_MAJOR << 16) | (ARDUINO_ESP8266_VERSION_MINOR << 8) | ARDUINO_ESP8266_VERSION_REVISION)

#if defined(ARDUINO_ESP8266_RELEASE_2_7_2)
#define ARDUINO_ESP8266_VERSION_COMBINED 0x020702
#elif defined(ARDUINO_ESP8266_RELEASE_2_7_1)
#define ARDUINO_ESP8266_VERSION_COMBINED 0x020701
#elif defined(ARDUINO_ESP8266_RELEASE_2_6_3)
#define ARDUINO_ESP8266_VERSION_COMBINED 0x020603
#elif defined(ARDUINO_ESP8266_RELEASE_2_5_2)
#define ARDUINO_ESP8266_VERSION_COMBINED 0x020502
#else
#error
#endif
#endif

#ifndef ASYNC_TCP_SSL_ENABLED
#define ASYNC_TCP_SSL_ENABLED           0
#endif

#ifndef SPIFFS_SUPPORT
#define SPIFFS_SUPPORT                  1               // SPIFFS support
#endif

#if SPIFFS_SUPPORT
#if DEBUG
#ifndef SPIFFS_CLEANUP_TMP_DURING_BOOT
#define SPIFFS_CLEANUP_TMP_DURING_BOOT  1               // Remove all files in /tmp during boot
#endif
#ifndef SPIFFS_TMP_FILES_TTL
#define SPIFFS_TMP_FILES_TTL            3600            // Default TTL for temporary files
#endif
#endif
#if SPIFFS_TMP_FILES_TTL
#ifndef SPIFFS_TMP_CLEAUP_INTERVAL
#define SPIFFS_TMP_CLEAUP_INTERVAL      300             // Cleanup interval for temporary files
#endif
#else
#define SPIFFS_CLEANUP_TMP_DURING_BOOT  0
#define SPIFFS_TMP_FILES_TTL            0
#define SPIFFS_TMP_CLEAUP_INTERVAL      0
#endif
#endif

#ifndef KFC_SERIAL_PORT
#define KFC_SERIAL_PORT                 Serial
#endif

#ifndef KFC_SERIAL_RATE
#define KFC_SERIAL_RATE                 115200
#endif

#ifndef KFC_SAFE_MODE_SERIAL_PORT
#define KFC_SAFE_MODE_SERIAL_PORT       KFC_SERIAL_PORT
#endif

#ifndef KFC_TWOWIRE_SDA
#define KFC_TWOWIRE_SDA                 SDA
#endif

#ifndef KFC_TWOWIRE_SCL
#define KFC_TWOWIRE_SCL                 SCL
#endif

#ifndef KFC_TWOWIRE_CLOCK_STRETCH
#define KFC_TWOWIRE_CLOCK_STRETCH       45000
#endif

#ifndef KFC_TWOWIRE_CLOCK_SPEED
#define KFC_TWOWIRE_CLOCK_SPEED         100000
#endif

#ifndef ENABLE_DEEP_SLEEP
#define ENABLE_DEEP_SLEEP                   0
#endif

#ifndef SPEED_BOOSTER_ENABLED
#if defined(ESP8266)
#define SPEED_BOOSTER_ENABLED               1
#else
#define SPEED_BOOSTER_ENABLED               0
#endif
#endif

#ifndef DEBUG
#define DEBUG                           1                // Enable debug mode
#endif

#ifndef DEBUG_OUTPUT
#define DEBUG_OUTPUT                    DebugSerial     // Output device for debugging messages
#endif

#ifndef DEBUG_INCLUDE_SOURCE_INFO
#define DEBUG_INCLUDE_SOURCE_INFO       0               // Add source file and line number to the debug output
#endif

#if !SPIFFS_SUPPORT
#error SPIFFS support required
#endif

#ifndef LOGGER
#define LOGGER                          1               // Logging to files on SPIFFS
#endif

#if LOGGER
#ifndef LOGGER_MAX_FILESIZE
#define LOGGER_MAX_FILESIZE             0xffff          // Max. size of the log file. if it exceeds the size, a copy is created and the
                                                        // logfile truncated. The copy gets overwritten every time the file gets rotated.
                                                        // 0 disables rotation
#endif
#if !SPIFFS_SUPPORT
#error SPIFFS support required
#endif
#endif

#ifndef SYSLOG_SUPPORT
#define SYSLOG_SUPPORT                  1
#endif
#ifndef SYSLOG_SPIFF_QUEUE_SIZE
#define SYSLOG_SPIFF_QUEUE_SIZE        (1024 * 32)
#endif
#ifndef SYSLOG_RETRANSMIT_RETRY_DELAY
#define SYSLOG_RETRANSMIT_RETRY_DELAY   5000           // Retry to retransmit syslog messages in case of a failure
#endif
#ifndef SYSLOG_QUEUE_DELAY
#define SYSLOG_QUEUE_DELAY              10000             // Delay before sending a queued message or transmitting the next one
#endif
#if !SPIFFS_SUPPORT && SYSLOG_SUPPORT && SYSLOG_SPIFF_QUEUE_SIZE
#error SPIFFS support required
#endif

#if defined(DEBUG_USE_SYSLOG_HOST) && !defined(DEBUG_USE_SYSLOG)
#define DEBUG_USE_SYSLOG                1
#endif

#if DEBUG_USE_SYSLOG
#  if !SYSLOG_SUPPORT
#    error DEBUG_USE_SYSLOG requires SYSLOG_SUPPORT
#  endif
#ifndef DEBUG_USE_SYSLOG_PORT
#define DEBUG_USE_SYSLOG_PORT           514
#endif
#ifndef DEBUG_USE_SYSLOG_UDP
#define DEBUG_USE_SYSLOG_UDP            false
#endif
#endif

#ifndef AT_MODE_SUPPORTED
#define AT_MODE_SUPPORTED               1               // AT commands over serial
#endif

#ifndef HTTP2SERIAL_SUPPORT
#define HTTP2SERIAL_SUPPORT                     1               // HTTP2Serial bridge (requires ATMODE support)
#endif

#if HTTP2SERIAL_SUPPORT && !WEBSERVER_SUPPORT
#pragma message("HTTP2SERIAL_SUPPORT requires WEBSERVER_SUPPORT. Automatically disabled!")
#undef HTTP2SERIAL_SUPPORT
#define HTTP2SERIAL_SUPPORT 0
#endif

// The serial buffer is to increase the size of the web socket packets. The delay allows to collect more in the buffer
// before it is sent to the client. There is a certain threshold where the small packets cause more lagg than having a
// delay, but that depends on different factors like wifi, web browser...
#if HTTP2SERIAL_SUPPORT
#define SERIAL_BUFFER_FLUSH_DELAY       60
#define SERIAL_BUFFER_MAX_LEN           512             // maximum size of output buffer
#endif

#ifndef KFC_SERIAL_PORT
#define KFC_SERIAL_PORT Serial
#endif

extern class Stream &MySerial;
extern class Stream &DebugSerial;

#ifndef HTTP2SERIAL_DISABLE_AT_MODE
#define HTTP2SERIAL_DISABLE_AT_MODE     0
#endif

#ifndef NTP_CLIENT
#define NTP_CLIENT                      1               // NTP client support
#endif

#ifndef SNTP_STARTUP_DELAY
#if ARDUINO_ESP8266_VERSION_COMBINED < 0x020603
#define SNTP_STARTUP_DELAY              0
#endif
#endif

#ifndef MQTT_SUPPORT
#define MQTT_SUPPORT                    1               // Support for a MQTT broker
#endif

#ifndef MQTT_USE_PACKET_CALLBACKS
#define MQTT_USE_PACKET_CALLBACKS       0
#endif

#ifndef MQTT_FILE_MANAGER
#define MQTT_FILE_MANAGER               1               // SPIFFS list files/upload over MQTT
#endif

#ifndef MQTT_AUTO_DISCOVERY
#define MQTT_AUTO_DISCOVERY             1               // enable auto discovery over MQTT for other KFCFW devices
#endif

#ifndef MQTT_REMOTE_CONFIG
#define MQTT_REMOTE_CONFIG              1               // enable remote configuration via MQTT
#endif

#ifndef WEBSERVER_SUPPORT
#define WEBSERVER_SUPPORT               1               // enable embded web server
#endif

#if WEBSERVER_SUPPORT
#define SECURITY_LOGIN_ATTEMPTS         5               // maxmimum failed logins within timeframe
#define SECURITY_LOGIN_TIMEFRAME        300
#if SPIFFS_SUPPORT                                      // store failed logins on SPIFFS
#define SECURITY_LOGIN_REWRITE_INTERVAL 3600            // rewrite file every hour to remove old records
#define SECURITY_LOGIN_STORE_TIMEFRAME  86400           // keep failed attempts stored on SPIFFS
#define SECURITY_LOGIN_FILE             "/login_failures"
#endif
#endif

#ifndef MINIFY_WEBUI
#define MINIFY_WEBUI                    1               // Compress html, css and js
#endif

#if WEBSERVER_SUPPORT && !defined(WEBSERVER_TLS_SUPPORT)
#if defined(ESP32)
#define WEBSERVER_TLS_SUPPORT           0               // TLS support for the web server
#elif defined(ESP8266)
#define WEBSERVER_TLS_SUPPORT           0               // TLS support for the web server
#endif
#endif

#ifndef FILE_MANAGER
#define FILE_MANAGER                    1               // Simple file manager for SPIFFS
#endif

#if FILE_MANAGER && !WEBSERVER_SUPPORT
#undef FILE_MANAGER
#define FILE_MANAGER 0
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

#if DEBUG_HAVE_SAVECRASH
#if defined(ESP32)
#undef DEBUG_HAVE_SAVECRASH
#define DEBUG_HAVE_SAVECRASH 0
#endif
#endif

// offset of the configuration in the EEPROM
#ifndef CONFIG_EEPROM_OFFSET
#define CONFIG_EEPROM_OFFSET            0
#endif

// writing data to the EEPROM requires CONFIG_EEPROM_OFFSET + CONFIG_EEPROM_SIZE memory
#ifndef CONFIG_EEPROM_SIZE
#define CONFIG_EEPROM_SIZE              2048
#endif

#ifndef MDNS_PLUGIN
#define MDNS_PLUGIN                     1
#endif

#ifndef SERIAL_HANDLER
#if SERIAL2TCP || HTTP2SERIAL_SUPPORT || AT_MODE_SUPPORTED
#define SERIAL_HANDLER                  1
#else
#define SERIAL_HANDLER                  0
#endif
#endif

#ifndef SERIAL_HANDLER_INPUT_BUFFER_MAX
#define SERIAL_HANDLER_INPUT_BUFFER_MAX 512
#endif

#ifndef HAS_STRFTIME_P
#define HAS_STRFTIME_P                  1
#endif

#ifndef __ICACHE_FLASH_ATTR
#define __ICACHE_FLASH_ATTR             ICACHE_FLASH_ATTR
#endif

// default "0" defines for plugins

#ifndef IOT_SWITCH
#define IOT_SWITCH 0
#endif

#ifndef RTC_SUPPORT
#define RTC_SUPPORT 0
#endif

#ifndef RTC_DEVICE_DS3231
#define RTC_DEVICE_DS3231 0
#endif

#ifndef IOT_BLINDS_CTRL
#define IOT_BLINDS_CTRL 0
#endif

#ifndef PING_MONITOR_SUPPORT
#define PING_MONITOR_SUPPORT 0
#endif

#ifndef REST_API_SUPPORT
#define REST_API_SUPPORT 0
#endif

#ifndef HOME_ASSISTANT_INTEGRATION
#define HOME_ASSISTANT_INTEGRATION 0
#endif

#ifndef I2CSCANNER_PLUGIN
#define I2CSCANNER_PLUGIN 0
#endif

#ifndef IOT_CLOCK
#define IOT_CLOCK 0
#endif

#ifndef IOT_ALARM_PLUGIN_ENABLED
#define IOT_ALARM_PLUGIN_ENABLED 0
#endif

#ifndef IOT_RF24_MASTER
#define IOT_RF24_MASTER 0
#endif

#ifndef IOT_WEATHER_STATION
#define IOT_WEATHER_STATION 0
#endif

#ifndef SSD1306_PLUGIN
#define SSD1306_PLUGIN 0
#endif

#ifndef IOT_ATOMIC_SUN_V2
#define IOT_ATOMIC_SUN_V2 0
#endif

#ifndef IOT_DIMMER_MODULE
#define IOT_DIMMER_MODULE 0
#endif

#ifndef PIN_MONITOR
#define PIN_MONITOR 0
#endif

#ifndef IOT_SENSOR
#define IOT_SENSOR 0
#endif
#define IOT_SENSOR_HAS(name)    defined(IOT_SENSOR_##name) && (IOT_SENSOR_##name)

#ifndef IOT_SENSOR_HAVE_SYSTEM_METRICS
#define IOT_SENSOR_HAVE_SYSTEM_METRICS 1
#endif

#ifndef SERIAL2TCP
#define SERIAL2TCP 0
#endif

#ifndef STK500V1
#define STK500V1 0
#endif
