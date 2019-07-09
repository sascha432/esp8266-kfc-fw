   /**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <vector>
#include <functional>
#include <bitset>
#include "Configuration.h"
#if SYSLOG
#include <KFCSyslog.h>
#endif
#include "logger.h"
#include "misc.h"
#include "at_mode.h"
#include "reset_detector.h"
#include "dyn_bitset.h"

#ifdef dhcp_start
#undef dhcp_start
#endif

#define HASH_SIZE               64

enum LedMode_t : uint8_t {
    MODE_NO_LED = 0,
    MODE_SINGLE_LED,
    MODE_TWO_LEDS,
    MODE_RGB_LED,
};

enum HttpMode_t : uint8_t {
    HTTP_MODE_DISABLED = 0,
    HTTP_MODE_UNSECURE,
    HTTP_MODE_SECURE,
};

enum MQTTMode_t : uint8_t {
    MQTT_MODE_DISABLED = 0,
    MQTT_MODE_UNSECURE,
    MQTT_MODE_SECURE,
};

enum Serial2Tcp_Mode_t : uint8_t {
    SERIAL2TCP_MODE_DISABLED = 0,
    SERIAL2TCP_MODE_SECURE_SERVER,
    SERIAL2TCP_MODE_UNSECURE_SERVER,
    SERIAL2TCP_MODE_SECURE_CLIENT,
    SERIAL2TCP_MODE_UNSECURE_CLIENT,
};

enum Serial2Tcp_SerialPort_t : uint8_t {
    SERIAL2TCP_HARDWARE_SERIAL = 0,
    SERIAL2TCP_HARDWARE_SERIAL1,
    SERIAL2TCP_SOFTWARE_CUSTOM,
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

struct ConfigFlags {
    ConfigFlags_t isFactorySettings:1;
    ConfigFlags_t isDefaultPassword:1; //TODO disable password after 5min if it has not been changed
    ConfigFlags_t ledMode:2;
    ConfigFlags_t wifiMode:2;
    ConfigFlags_t atModeEnabled:1;
    ConfigFlags_t hiddenSSID:1;
    ConfigFlags_t softAPDHCPDEnabled:1;
    ConfigFlags_t stationModeDHCPEnabled:1;
    ConfigFlags_t webServerMode:2;
    ConfigFlags_t webServerPerformanceModeEnabled:1;
    ConfigFlags_t ntpClientEnabled:1;
    ConfigFlags_t syslogProtocol:3;
    ConfigFlags_t mqttMode:2;
    ConfigFlags_t mqttAutoDiscoveryEnabled:1;
    ConfigFlags_t restApiEnabled:1;
    ConfigFlags_t serial2TCPMode:3;
    ConfigFlags_t hueEnabled:1;
};

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
    uint16_t version;
    char device_name[17];
    char device_pass[33];
    char wifi_ssid[33];
    char wifi_pass[33];
    ConfigFlags flags;
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
    uint16_t mqtt_keepalive;
    uint8_t mqtt_qos;
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

ulong system_stats_get_runtime(ulong *runtime_since_reboot = NULL);
void system_stats_display(String prefix, Stream &output);
void system_stats_update(bool open_eeprom = true);
void system_stats_read();

extern SystemStats _systemStats;
extern bool safe_mode;

uint8_t WiFi_mode_connected(uint8_t mode = WIFI_AP_STA, uint32_t *station_ip = nullptr, uint32_t *ap_ip = nullptr);


#define _H_IP_FORM_OBJECT(name)                     config._H_GET_IP(name), [](const IPAddress &addr, FormField *) { config._H_SET_IP(name, addr); }
#define _H_STRUCT_FORMVALUE(name, type, field)      config._H_GET(name).field, [](type value, FormField *) { auto &data = config._H_W_GET(name); data.field = value; }

class KFCFWConfiguration : public Configuration {
public:
    KFCFWConfiguration();
    ~KFCFWConfiguration();

    void restoreFactorySettings();
    static const String getFirmwareVersion();

    void setLastError(const String &error);
    const char *getLastError() const;

    uint8_t getMaxChannels();

    void garbageCollector();

    // flag to tell if the device has to be rebooted to apply all configuration changes
    void setConfigDirty(bool dirty);
    bool isConfigDirty() const;

private:
    String _lastError;
    int16_t _garbageCollectionCycleDelay;
    bool _dirty;
};

extern KFCFWConfiguration config;
