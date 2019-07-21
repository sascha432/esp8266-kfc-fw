   /**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#ifndef DEBUG_KFC_CONFIG
#define DEBUG_KFC_CONFIG 1
#endif

#include <Arduino_compat.h>
#include <vector>
#include <functional>
#include <bitset>
#include <Configuration.h>
#if SYSLOG
#include <KFCSyslog.h>
#endif
#include "logger.h"
#include "misc.h"
#include "at_mode.h"
#include "reset_detector.h"
#include "dyn_bitset.h"

#ifdef dhcp_start // defined in framework-arduinoespressif8266@2.20402.4/tools/sdk/lwip2/include/arch/cc.h
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

#define CONFIG_RTC_MEM_ID 2

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
    ConfigFlags_t useStaticIPDuringWakeUp:1;
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

typedef struct  {
    int16_t channel: 15;                    //  0
    int16_t use_static_ip: 1;               // +2 byte
    uint8_t bssid[WL_MAC_ADDR_LENGTH];      // +6 byte
    uint32_t local_ip;
    uint32_t dns1;
    uint32_t dns2;
    uint32_t subnet;
    uint32_t gateway;
} WiFiQuickConnect_t;

struct Config {
    uint32_t version;
    ConfigFlags flags;
    char device_name[17];
    char device_pass[33];
    char wifi_ssid[33];
    char wifi_pass[33];
    IPAddress dns1;
    IPAddress dns2;
    IPAddress local_ip;
    IPAddress subnet;
    IPAddress gateway;

    uint16_t http_port;
    char cert_passphrase[33];

    uint8_t led_pin;

    char mqtt_host[65];
    char mqtt_username[33];
    char mqtt_password[33];
    char mqtt_topic[128];
    char mqtt_fingerprint[20];
    char mqtt_discovery_prefix[32];

    // comparing memory usage
    uint16_t mqtt_port;
    uint16_t mqtt_keepalive;
    uint8_t mqtt_qos;

    // comparing memory usage
    // struct {
    //     uint16_t port;
    //     uint16_t keepalive;
    //     uint8_t qos;
    // } mqtt_options;

    char syslog_host[65];
    uint16_t syslog_port;

    struct NTP ntp;
    HomeAssistant homeassistant;
    SoftAP soft_ap;
    struct Serial2Tcp serial2tcp;
    struct HueConfig hue;
    struct Ping ping;
};

void config_set_blink(uint16_t milliseconds, int8_t pin = -1);
void config_deep_sleep(uint32_t time, RFMode mode);

uint8_t WiFi_mode_connected(uint8_t mode = WIFI_AP_STA, uint32_t *station_ip = nullptr, uint32_t *ap_ip = nullptr);

#define _H_IP_FORM_OBJECT(name)                     config._H_GET_IP(name), [](const IPAddress &addr, FormField *) { config._H_SET_IP(name, addr); }
#define _H_STRUCT_FORMVALUE(name, type, field)      config._H_GET(name).field, [](type value, FormField *) { auto &data = config._H_W_GET(name); data.field = value; }

// NOTE using the new handlers (USE_WIFI_SET_EVENT_HANDLER_CB=0) costs 896 byte RAM with 5 handlers, by using lambda functions even 1016 byte
#ifndef USE_WIFI_SET_EVENT_HANDLER_CB
#define USE_WIFI_SET_EVENT_HANDLER_CB           1
#endif

class KFCFWConfiguration : public Configuration {
public:
    KFCFWConfiguration();
    ~KFCFWConfiguration();

    void setLastError(const String &error) override;
    const char *getLastError() const override;

    void restoreFactorySettings();
    static const String getFirmwareVersion();

    // flag to tell if the device has to be rebooted to apply all configuration changes
    void setConfigDirty(bool dirty);
    bool isConfigDirty() const;

    void storeQuickConnect(const uint8_t *bssid, int8_t channel);
    void storeStationConfig(uint32_t ip, uint32_t netmask, uint32_t gateway);

    void setup();
    bool reconfigureWiFi();
    bool connectWiFi();
    void read();
    void write();

    void wakeUpFromDeepSleep();
    void enterDeepSleep(uint32_t time_in_ms, RFMode mode, uint16_t delayAfterPrepare = 0);
    void restartDevice();

    static void printVersion(Print &output);
    void printInfo(Print &output);

    static void loop();
    void garbageCollector();
    static uint8_t getMaxWiFiChannels();
    static String getWiFiEncryptionType(uint8_t type);

private:
    void _setupWiFiCallbacks();
#if USE_WIFI_SET_EVENT_HANDLER_CB
    static void _onWiFiEvent(System_Event_t *orgEvent);
#endif

public:
    void _onWiFiConnectCb(const WiFiEventStationModeConnected &);
    void _onWiFiDisconnectCb(const WiFiEventStationModeDisconnected &);
    void _onWiFiGotIPCb(const WiFiEventStationModeGotIP&);
    void _onWiFiOnDHCPTimeoutCb();
    void _softAPModeStationConnectedCb(const WiFiEventSoftAPModeStationConnected &);
    void _softAPModeStationDisconnectedCb(const WiFiEventSoftAPModeStationDisconnected &);

    static bool isWiFiUp();
    static unsigned long getWiFiUp();

private:
    String _lastError;
    int16_t _garbageCollectionCycleDelay;
    bool _dirty;
    bool _wifiConnected;
    unsigned long _wifiUp;
    unsigned long _offlineSince;

#if USE_WIFI_SET_EVENT_HANDLER_CB == 0
    WiFiEventHandler _onWiFiConnect;
    WiFiEventHandler _onWiFiDisconnect;
    WiFiEventHandler _onWiFiGotIP;
    WiFiEventHandler _onWiFiOnDHCPTimeout;
    WiFiEventHandler _softAPModeStationConnected;
    WiFiEventHandler _softAPModeStationDisconnected;
#endif
};

extern KFCFWConfiguration config;
