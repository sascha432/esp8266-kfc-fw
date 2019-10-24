/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#define FIRMWARE_IDENT                  0x72a82165
#define FIRMWARE_VERSION_MAJOR          0
#define FIRMWARE_VERSION_MINOR          0
#define FIRMWARE_VERSION_REVISION       1
#define FIRMWARE_VERSION                ((FIRMWARE_VERSION_MAJOR<<16)|(FIRMWARE_VERSION_MINOR<<8)|FIRMWARE_VERSION_REVISION)
#define ________STR(str)                      #str
#define _______STR(str)                 ________STR(str)
#define FIRMWARE_VERSION_STR            _______STR(FIRMWARE_VERSION_MAJOR) "." _______STR(FIRMWARE_VERSION_MINOR) "." _______STR(FIRMWARE_VERSION_REVISION)

#ifndef ASYNC_TCP_SSL_ENABLED
#define ASYNC_TCP_SSL_ENABLED           0
#endif

#ifdef __LED_BUILTIN
#ifdef LED_BUILTIN
#undef LED_BUILTIN
#endif
#define LED_BUILTIN                     __LED_BUILTIN
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
#define KFC_TWOWIRE_SDA                 D3
#endif

#ifndef KFC_TWOWIRE_SCL
#define KFC_TWOWIRE_SCL                 D5
#endif

#ifndef KFC_TWOWIRE_CLOCK_STRECH
#define KFC_TWOWIRE_CLOCK_STRECH        45000
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

#ifndef FS_MAPPING_SORTED
#define FS_MAPPING_SORTED               1               // Mappings are sorted by path
#endif

#ifndef FS_MAPPINGS_COUNTER_TYPE                        // type of the counter of .mappings
#define FS_MAPPINGS_COUNTER_TYPE         uint8_t
#endif

#ifndef FS_MAPPINGS_FLAGS_GZIPPED                       // flags bitset of .mappings
#define FS_MAPPINGS_FLAGS_GZIPPED       0x01
#endif

#ifndef FS_MAPPINGS_HASH_ALGO                            // hash function to generate file hashes for .mappings
// #define FS_MAPPINGS_PHP_HASH_FUNCTION   "sha256"
// #define FS_MAPPINGS_HASH_LENGTH         32           // length of the binary hash
#define FS_MAPPINGS_HASH_ALGO           "sha1"
#define FS_MAPPINGS_HASH_LENGTH         20              // length of the binary hash
#endif


#ifndef LOGGER
#define LOGGER                          1               // Logging to files on SPIFFS
#endif

#if LOGGER
#ifndef LOGGER_MAX_FILESIZE
#define LOGGER_MAX_FILESIZE             32768           // Max. size of the log file. if it exceeds the size, a copy is created and the
                                                        // logfile truncated. The copy gets overwritten every time the file gets rotated.
                                                        // 0 disables rotation
#endif
#if !SPIFFS_SUPPORT
#error SPIFFS support required
#endif
#endif

#ifndef SYSLOG
#define SYSLOG                          1
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
#if !SPIFFS_SUPPORT && SYSLOG && SYSLOG_SPIFF_QUEUE_SIZE
#error SPIFFS support required
#endif

#if defined(DEBUG_USE_SYSLOG_HOST) && !defined(DEBUG_USE_SYSLOG)
#define DEBUG_USE_SYSLOG                1
#endif

#if DEBUG_USE_SYSLOG
#  if !SYSLOG
#    error DEBUG_USE_SYSLOG requires SYSLOG
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

#ifndef WEB_SOCKET_ENCRYPTION
#define WEB_SOCKET_ENCRYPTION           WEBSERVER_SUPPORT   // AES 128, 192 and 256 encryption for web sockets
#endif

#ifndef HTTP2SERIAL
#define HTTP2SERIAL                     1               // HTTP2Serial bridge (requires ATMODE support)
#endif

#if HTTP2SERIAL && !WEBSERVER_SUPPORT
#pragma message("HTTP2SERIAL requires WEBSERVER_SUPPORT. Automatically disabled!")
#undef HTTP2SERIAL
#define HTTP2SERIAL 0
#endif

// The serial buffer is to increase the size of the web socket packets. The delay allows to collect more in the buffer
// before it is sent to the client. There is a certain threshold where the small packets cause more lagg than having a
// delay, but that depends on different factors like wifi, web browser...
#if HTTP2SERIAL
#define SERIAL_BUFFER_FLUSH_DELAY       60
#define SERIAL_BUFFER_MAX_LEN           512             // maximum size of output buffer
#endif

#ifndef KFC_SERIAL_PORT
#define KFC_SERIAL_PORT Serial
#endif

extern class Stream &MySerial;
extern class Stream &DebugSerial;

#ifndef SERIAL2TCP
#define SERIAL2TCP                      0               // connect to com port over TCP
#endif
#ifndef HTTP2SERIAL_DISABLE_AT_MODE
#define HTTP2SERIAL_DISABLE_AT_MODE     1
#endif

#ifndef NTP_CLIENT
#define NTP_CLIENT                      1               // NTP client support
#endif

#ifndef USE_REMOTE_TIMEZONE
#define USE_REMOTE_TIMEZONE             1               // Use remote server to determine timezone offset and daylight saving offset
                                                        // Currently https://timezonedb.com/register is supported as well as any similar API
                                                        // TODO add php API for self hosting
#endif
#if !USE_REMOTE_TIMEZONE
#define INCLUDE_TZ_AND_DST_OFS          1               // Include timezone and dst offsets
#endif

#ifndef MQTT_SUPPORT
#define MQTT_SUPPORT                    1               // Support for a MQTT broker
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

#ifndef HUE_EMULATION
#define HUE_EMULATION                   0               // HUE emulation for ALEXA
#endif

#ifndef REST_API_SUPPORT
#define REST_API_SUPPORT                1               // rest API support
#endif

#ifndef HOME_ASSISTANT_INTEGRATION
#define HOME_ASSISTANT_INTEGRATION      0               // home assistant integration https://www.home-assistant.io/
#endif

#ifndef WEBSERVER_SUPPORT
#define WEBSERVER_SUPPORT               1               // enable embded web server
#endif

#if WEBSERVER_SUPPORT
#define SECURITY_LOGIN_ATTEMPTS         5               // maxmimum failed logins within timeframe
#define SECURITY_LOGIN_TIMEFRAME        300
#if SPIFFS_SUPPORT                                      // store failed logins on SPIFFS
#define SECURITY_LOGIN_STORE_TIMEFRAME  86400           // keep failed attempts stored on SPIFFS
#define SECURITY_LOGIN_FILE             "/login_failures"
#endif
#endif

#ifndef MINIFY_WEBUI
#define MINIFY_WEBUI                    1               // Compress html, css and js
#endif

#if WEBSERVER_SUPPORT && !defined(WEBSERVER_TLS_SUPPORT)
#if defined(ESP32)
#define WEBSERVER_TLS_SUPPORT           1               // TLS support for the web server
#elif defined(ESP8266)
#define WEBSERVER_TLS_SUPPORT           0               // TLS support for the web server
#endif
#endif

#ifndef PING_MONITOR
#define PING_MONITOR                    0               // Automated pings with statistics
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

#ifndef CONFIG_EEPROM_OFFSET
#if DEBUG_HAVE_SAVECRASH
#define CONFIG_EEPROM_OFFSET             (DEBUG_SAVECRASH_OFS + DEBUG_SAVECRASH_SIZE)
#else
#define CONFIG_EEPROM_OFFSET             0
#endif
#endif
#ifndef CONFIG_EEPROM_MAX_LENGTH
#define CONFIG_EEPROM_MAX_LENGTH        4096
#endif

#ifndef MDNS_SUPPORT
#define MDNS_SUPPORT                    1               // support for MDNS_SUPPORT
#endif

// #if MDNS_SUPPORT
// #ifndef NO_GLOBAL_MDNS
// #error requires NO_GLOBAL_MDNS to be defined
// #endif
// #endif

#ifndef SERIAL_HANDLER
#if SERIAL2TCP || HTTP2SERIAL || AT_MODE_SUPPORTED
#define SERIAL_HANDLER                  1
#else
#define SERIAL_HANDLER                  0
#endif
#endif

#ifndef IOT_SWITCH
#define IOT_SWITCH                      0
#endif

#ifndef SERIAL_HANDLER_INPUT_BUFFER_MAX
#define SERIAL_HANDLER_INPUT_BUFFER_MAX 512
#endif

#ifndef RTC_SUPPORT
#define RTC_SUPPORT                     0               // support for RTC
#endif

#ifndef HAS_STRFTIME_P
#define HAS_STRFTIME_P                  1
#endif

#ifndef __ICACHE_FLASH_ATTR
#define __ICACHE_FLASH_ATTR             ICACHE_FLASH_ATTR
#endif
