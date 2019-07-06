   /**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <vector>
#include <functional>
#include <bitset>
#if SYSLOG
#include <KFCSyslog.h>
#endif
#include "global.h"
#include "logger.h"
#include "misc.h"
#include "debug_helper.h"
#include "at_mode.h"
#include "reset_detector.h"
#include "dyn_bitset.h"

#define HASH_SIZE               64

#define FLAGS_FACTORY_SETTINGS      0x00000001
#define FLAGS_MODE_STATION          0x00000002
#define FLAGS_MODE_AP               0x00000004
#define FLAGS_MODES                 (FLAGS_MODE_STATION|FLAGS_MODE_AP)
#define FLAGS_AT_MODE               0x00000008
#define FLAGS_SOFTAP_DHCPD          0x00000010
#define FLAGS_STATION_DHCP          0x00000020
#define FLAGS_MQTT_ENABLED          0x00000040
#define FLAGS_NTP_ENABLED           0x00000080
#define FLAGS_FREE2_______          0x00000100
#define FLAGS_SYSLOG_UDP            0x00000200
#define FLAGS_SYSLOG_TCP            0x00000400
#define FLAGS_SYSLOG_TCP_TLS        (FLAGS_SYSLOG_UDP|FLAGS_SYSLOG_TCP)
#define FLAGS_FREE3_____            0x00000800
#define FLAGS_SYSLOG_MASK           (FLAGS_SYSLOG_UDP|FLAGS_SYSLOG_TCP|FLAGS_SYSLOG_TCP_TLS)
#define FLAGS_HTTP_ENABLED          0x00001000
#define FLAGS_HTTP_TLS              0x00002000
#define FLAGS_REST_API_ENABLED      0x00004000
#define FLAGS_SECURE_MQTT           0x00008000
#define FLAGS_LED_SINGLE            0x00010000
#define FLAGS_LED_TWO               0x00020000
#define FLAGS_LED_RGB               (FLAGS_LED_SINGLE|FLAGS_LED_TWO)
#define FLAGS_LED_MODES             (FLAGS_LED_SINGLE|FLAGS_LED_TWO|FLAGS_LED_RGB)
#define FLAGS_SERIAL2TCP_CLIENT     0x00040000
#define FLAGS_SERIAL2TCP_SERVER     0x00080000
#define FLAGS_SERIAL2TCP_TLS        0x00100000
#define FLAGS_HUE_ENABLED           0x00200000
#define FLAGS_SERIAL2TCP_MODES      (FLAGS_SERIAL2TCP_SERVER|FLAGS_SERIAL2TCP_TLS|FLAGS_SERIAL2TCP_CLIENT)
#define FLAGS_MQTT_AUTO_DISCOVERY   0x00400000
#define FLAGS_HIDDEN_SSID           0x00800000
#define FLAGS_HTTP_PERF_160         0x01000000

enum LedMode_t {
    MODE_NO_LED = 0,
    MODE_SINGLE_LED,
    MODE_TWO_LEDS,
    MODE_RGB_LED,
};

enum HttpMode_t {
    HTTP_MODE_DISABLED = 0,
    HTTP_MODE_UNSECURE,
    HTTP_MODE_SECURE,
};

enum MQTTMode_t {
    MQTT_MODE_DISABLED = 0,
    MQTT_MODE_UNSECURE,
    MQTT_MODE_SECURE,
};

enum Serial2Tcp_Mode_t {
    SERIAL2TCP_MODE_DISABLED = 0,
    SERIAL2TCP_MODE_SECURE_SERVER,
    SERIAL2TCP_MODE_UNSECURE_SERVER,
    SERIAL2TCP_MODE_SECURE_CLIENT,
    SERIAL2TCP_MODE_UNSECURE_CLIENT,
};

enum Serial2Tcp_SerialPort_t {
    SERIAL2TCP_HARDWARE_SERIAL = 0,
    SERIAL2TCP_HARDWARE_SERIAL1 = 1,
    SERIAL2TCP_SOFTWARE_CUSTOM = 2,
};

#define BLINK_SLOW              1000
#define BLINK_FAST              250
#define BLINK_FLICKER           50
#define BLINK_OFF               0
#define BLINK_SOLID             1
#define BLINK_SOS               2

void blink_led(int8_t pin, int delay, dynamic_bitset &pattern);

#define MAX_NTP_SERVER          3
#ifndef SNTP_MAX_SERVERS
//#warning SNTP_MAX_SERVERS not defined
#else
#if MAX_NTP_SERVER != SNTP_MAX_SERVERS
#error MAX_NTP_SERVER does not match SNTP_MAX_SERVERS
#endif
#endif

typedef uint32_t ConfigFlags_t;

struct SoftAP {
    IPAddress address;
    IPAddress subnet;
    IPAddress gateway;
    IPAddress dhcp_start;
    IPAddress dhcp_end;
    uint8_t channel;
    uint8_t encryption;
    char wifi_ssid[33];
    char wifi_pass[33];
};

struct HueConfig {
    uint16_t tcp_port;
    char devices[255]; // \n is separator
};

struct NTP {
    char timezone[33];
    char servers[MAX_NTP_SERVER][65];
#if USE_REMOTE_TIMEZONE
    char remote_tz_dst_ofs_url[255];
#endif
};

struct Ping {
    char host1[65];
    char host2[65];
    char host3[65];
    char host4[65];
    uint8_t count;
    uint16_t interval;
    uint16_t timeout;
};

struct Serial2Tcp {
    uint16_t port;
    uint32_t baud_rate;
    char host[65];
    char username[33];
    char password[33];
    uint8_t rx_pin;
    uint8_t tx_pin;
    uint8_t serial_port:3;
    uint8_t auth_mode:1;
    uint8_t auto_connect:1;
    uint8_t auto_reconnect;
    uint8_t keep_alive;
    uint16_t idle_timeout;
};

struct HomeAssistant {
    char api_endpoint[128];
    char token[250];
};

struct Config {
    uint16_t crc;
    uint32_t ident;
    uint16_t version;
    char device_name[17];
    char device_pass[33];
    char wifi_ssid[33];
    char wifi_pass[33];
    ConfigFlags_t flags;
    IPAddress dns1;
    IPAddress dns2;
    IPAddress local_ip;
    IPAddress subnet;
    IPAddress gateway;
    SoftAP soft_ap;
#if WEBSERVER_SUPPORT
    uint16_t http_port;
#endif
#if ASYNC_TCP_SSL_ENABLED
    char cert_passphrase[33];
#endif
#if NTP_CLIENT
    struct NTP ntp;
#endif
    uint8_t led_pin;
#if MQTT_SUPPORT
    char mqtt_host[65];
    uint16_t mqtt_port;
    char mqtt_username[33];
    char mqtt_password[33];
    char mqtt_topic[128];
    char mqtt_fingerprint[20];
    uint16_t mqtt_keepalive:14;
    uint16_t mqtt_qos:2;
#if MQTT_AUTO_DISCOVERY
    char mqtt_discovery_prefix[32];
#endif
#endif
#if HOME_ASSISTANT_INTEGRATION
    HomeAssistant homeassistant;
#endif
#if SYSLOG
    char syslog_host[65];
    uint16_t syslog_port;
#endif
#if SERIAL2TCP
    struct Serial2Tcp serial2tcp;
#endif
#if HUE_EMULATION
    struct HueConfig hue;
#endif
#if PING_MONITOR
    struct Ping ping;
#endif
};

struct SystemStats {
    uint16_t crc;
    uint8_t size;
    time_t runtime;
    uint16_t reboot_counter;
    uint16_t reset_counter;
    uint16_t crash_counter;
    uint16_t ap_clients_connected;
    uint16_t station_reconnects;
    uint32_t ping_success;
    uint32_t ping_timeout;
};

const char *WiFi_encryption_type(uint8_t type);

void config_set_blink(uint16_t milliseconds, int8_t pin = -1);
void config_setup();
void config_loop();
bool config_read(bool init = false, size_t ofs = 0);
void config_write(bool factory_reset = false, size_t ofs = 0);
void config_version();
void config_info();
int config_apply_wifi_settings();
void config_restart();

/**
 * TODO rewrite configuration in progress
 * */

class ConfigOptions {
public:
    ConfigOptions(Config &config) : _config(config) {
    }

    bool isAtMode() {
        return _isFlags(FLAGS_AT_MODE);
    }
    void setAtMode(bool enabled) {
        _setFlags(FLAGS_AT_MODE, enabled);
    }

    bool setWiFiMode(WiFiMode_t mode) {
        switch(mode) {
            case WIFI_AP_STA:
                _setFlags(FLAGS_MODE_STATION|FLAGS_MODE_AP, true, FLAGS_MODES);
                break;
            case WIFI_AP:
                _setFlags(FLAGS_MODE_AP, true, FLAGS_MODES);
                break;
            case WIFI_STA:
                _setFlags(FLAGS_MODE_STATION, true, FLAGS_MODES);
                break;
            case WIFI_OFF:
                _setFlags(FLAGS_MODES, false, FLAGS_MODES);
                break;
            default:
                return false;
        }
        return true;
    }
    WiFiMode_t getWiFiMode() {
        uint8_t mode = WIFI_OFF;
        if (_isFlags(FLAGS_MODE_AP)) {
            mode += WIFI_AP;
        }
        if (_isFlags(FLAGS_MODE_STATION)) {
            mode += WIFI_STA;
        }
        return (WiFiMode_t)mode;
    }
    inline void setWiFiOff() {
        setWiFiMode(WIFI_OFF);
    }
    void setWiFiStationMode(bool enabled) {
        _setFlags(FLAGS_MODE_STATION, enabled);
    }
    inline void setWiFiSoftAPMode(bool enabled) {
        _setFlags(FLAGS_MODE_AP, enabled);
    }
    inline void setWiFiSoftAPAndStationMode() {
        setWiFiMode(WIFI_AP_STA);
    }
    bool isWiFiStationMode() {
        return _isFlags(FLAGS_MODE_STATION);
    }
    bool isWiFiSoftApMode() {
        return _isFlags(FLAGS_MODE_AP);
    }
    bool isWiFiSoftApAndStationMode() {
        return _isFlags(FLAGS_MODE_AP|FLAGS_MODE_STATION);
    }

    bool isSoftApDhcpd() {
        return _isFlags(FLAGS_SOFTAP_DHCPD);
    }
    void setSoftApDhcpd(bool enabled) {
        _setFlags(FLAGS_SOFTAP_DHCPD, enabled);
    }

    bool isStationDhcp() {
        return _isFlags(FLAGS_STATION_DHCP);
    }
    void setStationDhcp(bool enabled) {
        _setFlags(FLAGS_STATION_DHCP, enabled);
    }

    bool isHiddenSSID() {
        return _isFlags(FLAGS_HIDDEN_SSID);
    }
    void setHiddenSSID(bool enabled) {
        _setFlags(FLAGS_HIDDEN_SSID, enabled);
    }

#if MQTT_SUPPORT
    MQTTMode_t getMQTTMode() {
#  if ASYNC_TCP_SSL_ENABLED
        if (isSecureMQTT()) {
            return MQTT_MODE_SECURE;
        } else
#  endif
        if (isMQTT()) {
            return MQTT_MODE_UNSECURE;
        }
        return MQTT_MODE_DISABLED;
    }
    void setMQTTMode(MQTTMode_t mode) {
        switch(mode) {
            case MQTT_MODE_UNSECURE:
                _setFlags(FLAGS_MQTT_ENABLED, true, FLAGS_SECURE_MQTT);
                break;
            case MQTT_MODE_SECURE:
                _setFlags(FLAGS_MQTT_ENABLED|FLAGS_SECURE_MQTT, true);
                break;
            case MQTT_MODE_DISABLED:
                _setFlags(FLAGS_MQTT_ENABLED|FLAGS_SECURE_MQTT, false);
                break;
        }
    }
    bool isMQTT() {
        return _getFlags(FLAGS_MQTT_ENABLED|FLAGS_SECURE_MQTT);
    }
    // void setMQTT(bool enabled) {
    //     _setFlags(FLAGS_MQTT_ENABLED, enabled, FLAGS_SECURE_MQTT);
    // }
#if MQTT_AUTO_DISCOVERY
    void setMQTTAutoDiscovery(bool enabled) {
        _setFlags(FLAGS_MQTT_AUTO_DISCOVERY, enabled);
    }
    bool isMQTTAutoDiscovery() {
        return _getFlags(FLAGS_MQTT_AUTO_DISCOVERY);
    }
#endif
    // // use setMQTTMode()
#  if  ASYNC_TCP_SSL_ENABLED
    bool isSecureMQTT() {
        return _isFlags(FLAGS_SECURE_MQTT|FLAGS_MQTT_ENABLED);
    }
    // // use getMQTTMode()
    // void setSecureMQTT(bool enabled) {
    //     _setFlags(FLAGS_SECURE_MQTT|FLAGS_MQTT_ENABLED, enabled);
    // }
    uint8_t *mqttFingerPrint() {
        return (uint8_t *)_config.mqtt_fingerprint;
    }
#  endif
#endif

#if NTP_CLIENT
    bool isNTPClient() {
        return _isFlags(FLAGS_NTP_ENABLED);
    }
    void setNTPClient(bool enabled) {
        _setFlags(FLAGS_NTP_ENABLED, enabled);
    }
#endif

 #if SYSLOG
    SyslogProtocol getSyslogProtocol();
    bool isSyslogProtocol(SyslogProtocol protocol);
    void setSyslogProtocol(SyslogProtocol protocol);
#endif

#if WEBSERVER_SUPPORT
    bool isHttpPerformanceMode() {
        return _isFlags(FLAGS_HTTP_PERF_160);
    }
    HttpMode_t getHttpMode() {
#  if WEBSERVER_TLS_SUPPORT
        if (isHttpServerTLS()) {
            return HTTP_MODE_SECURE;
        }
#  endif
        return isHttpServer() ? HTTP_MODE_UNSECURE : HTTP_MODE_DISABLED;
    }
    void setHttpMode(HttpMode_t mode) {
        switch(mode) {
            case HTTP_MODE_UNSECURE:
                _setFlags(FLAGS_HTTP_ENABLED, true, FLAGS_HTTP_TLS);
                break;
            case HTTP_MODE_SECURE:
                _setFlags(FLAGS_HTTP_ENABLED|FLAGS_HTTP_TLS, true);
                break;
            case HTTP_MODE_DISABLED:
                _setFlags(FLAGS_HTTP_ENABLED|FLAGS_HTTP_TLS, false);
                break;
        }
    }
    bool isHttpServer() {
        return _getFlags(FLAGS_HTTP_ENABLED|FLAGS_HTTP_TLS);
    }
#  if WEBSERVER_TLS_SUPPORT
    bool isHttpServerTLS() {
        return _isFlags((FLAGS_HTTP_TLS|FLAGS_HTTP_ENABLED));
    }
#  endif
#endif

#if HUE_EMULATION
    bool isHueEmulation() {
        return _isFlags(FLAGS_HUE_ENABLED);
    }
    void setHueEmulation(bool enabled) {
        _setFlags(FLAGS_HUE_ENABLED, enabled);
    }
#endif

#if REST_API_SUPPORT
    bool isRestApi() {
        return _isFlags(FLAGS_REST_API_ENABLED);
    }
    void setRestApi(bool enabled) {
        _setFlags(FLAGS_REST_API_ENABLED, enabled);
    }
#endif

#if SERIAL2TCP
    Serial2Tcp_Mode_t getSerial2TcpMode() {
        if (_isFlags(FLAGS_SERIAL2TCP_SERVER)) {
            return _isFlags(FLAGS_SERIAL2TCP_TLS) ? SERIAL2TCP_MODE_SECURE_SERVER : SERIAL2TCP_MODE_UNSECURE_SERVER;
        } else if (_isFlags(FLAGS_SERIAL2TCP_CLIENT)) {
            return _isFlags(FLAGS_SERIAL2TCP_TLS) ? SERIAL2TCP_MODE_SECURE_CLIENT : SERIAL2TCP_MODE_UNSECURE_CLIENT;
        }
        return SERIAL2TCP_MODE_DISABLED;
    }

    void setSerial2TcpMode(Serial2Tcp_Mode_t mode) {
        switch(mode) {
            case SERIAL2TCP_MODE_DISABLED:
                _setFlags(FLAGS_SERIAL2TCP_SERVER|FLAGS_SERIAL2TCP_CLIENT, false);
                break;
            case SERIAL2TCP_MODE_SECURE_SERVER:
                _setFlags(FLAGS_SERIAL2TCP_SERVER|FLAGS_SERIAL2TCP_TLS, true);
                _setFlags(FLAGS_SERIAL2TCP_CLIENT, false);
                break;
            case SERIAL2TCP_MODE_UNSECURE_SERVER:
                _setFlags(FLAGS_SERIAL2TCP_SERVER, true);
                _setFlags(FLAGS_SERIAL2TCP_CLIENT|FLAGS_SERIAL2TCP_TLS, false);
                break;
            case SERIAL2TCP_MODE_SECURE_CLIENT:
                _setFlags(FLAGS_SERIAL2TCP_CLIENT|FLAGS_SERIAL2TCP_TLS, true);
                _setFlags(FLAGS_SERIAL2TCP_SERVER, false);
                break;
            case SERIAL2TCP_MODE_UNSECURE_CLIENT:
                _setFlags(FLAGS_SERIAL2TCP_CLIENT, true);
                _setFlags(FLAGS_SERIAL2TCP_SERVER|FLAGS_SERIAL2TCP_TLS, false);
                break;
        }
    }

    bool isSerial2TcpClient() {
        return _isFlags(FLAGS_SERIAL2TCP_CLIENT);
    }

    bool isSerial2TcpServer() {
        return _isFlags(FLAGS_SERIAL2TCP_SERVER);
    }

    bool isSerial2TcpTLS() {
        return _isFlags(FLAGS_SERIAL2TCP_TLS);
    }

    void setSerial2TcpServer(bool enabled) {
        _setFlags(FLAGS_SERIAL2TCP_SERVER, enabled, FLAGS_SERIAL2TCP_MODES);
        if (enabled) {
            _setFlags(FLAGS_SERIAL2TCP_TLS, false, FLAGS_SERIAL2TCP_MODES);
        }
    }

#endif

    void setLedMode(LedMode_t mode) {
        switch(mode) {
            case MODE_NO_LED:
                _setFlags(FLAGS_LED_MODES, false);
                break;
            case MODE_SINGLE_LED:
                _setFlags(FLAGS_LED_SINGLE, true, FLAGS_LED_MODES);
                break;
            case MODE_TWO_LEDS:
                _setFlags(FLAGS_LED_TWO, true, FLAGS_LED_MODES);
                break;
            case MODE_RGB_LED:
                _setFlags(FLAGS_LED_RGB, true, FLAGS_LED_MODES);
                break;
        }
    }

    LedMode_t getLedMode() {
        if (_isFlags(FLAGS_LED_RGB)) {
            return MODE_RGB_LED;
        } else if (_isFlags(FLAGS_LED_SINGLE)) {
            return MODE_SINGLE_LED;
        } else if (_isFlags(FLAGS_LED_TWO)) {
            return MODE_TWO_LEDS;
        }
        return MODE_NO_LED;
    }

private:
    void _setFlags(ConfigFlags_t bitset, bool enabled, ConfigFlags_t mask);
    void _setFlags(ConfigFlags_t bitset, bool enabled);
    ConfigFlags_t _getFlags(ConfigFlags_t bitset);
    bool _isFlags(ConfigFlags_t bitset);

private:
    Config &_config;
};


class Configuration {
public:
    Configuration(Config &config);
    virtual ~Configuration() {
    }

    ConfigOptions &getOptions() {
        return _options;
    }
    Config &get() {
        return _config;
    }
    const char *getLastError();

    bool isFactorySettings() {
        return _config.flags & FLAGS_FACTORY_SETTINGS;
    }
    void restoreFactorySettings();

    uint8_t getMaxChannels() {
        wifi_country_t country;
        if (!wifi_get_country(&country)) {
            country.nchan = 255;
        }
        return country.nchan;
    }

    String getServerCert(const String &name) {
        return "";
    }
    String getServerKey(const String &name) {
        return "";
    }

    const char *getCertPassphrase() {
        return "";
    }

    void setDirty(bool dirty);
    bool isDirty();

private:
    ConfigOptions _options;
    Config &_config;
    bool _dirty;
};

ulong system_stats_get_runtime(ulong *runtime_since_reboot = NULL);
void system_stats_display(String prefix, Stream &output);
void system_stats_update(bool open_eeprom = true);
void system_stats_read();

extern Configuration _Config;
extern SystemStats _systemStats;
extern bool safe_mode;

uint8_t WiFi_mode_connected(uint8_t mode = WIFI_AP_STA, uint32_t *station_ip = nullptr, uint32_t *ap_ip = nullptr);
void timezone_setup();

const String config_firmware_version();

#if DEBUG
void config_dump(Stream &stream);
#endif
