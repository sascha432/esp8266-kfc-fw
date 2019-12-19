   /**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#ifndef DEBUG_KFC_CONFIG
#define DEBUG_KFC_CONFIG 0
#endif

#include <Arduino_compat.h>
#include <Wire.h>
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
#if DEBUG_HAVE_SAVECRASH
#include "EspSaveCrash.h"
#endif

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
    char servers[3][65];
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
    uint8_t serial_port: 3;
    uint8_t auth_mode: 1;
    uint8_t auto_connect: 1;
    uint8_t auto_reconnect;
    uint8_t keep_alive;
    uint16_t idle_timeout;
};

struct HomeAssistant {
    char api_endpoint[128];
    char token[250];
};

struct DimmerModule {
    float fade_time;
    float on_fade_time;
    float linear_correction;
    uint8_t max_temperature;
    uint8_t metrics_int;
    uint8_t report_temp;
    uint8_t restore_level;
};

struct BlindsControllerChannel {
    uint16_t pwmValue;
    uint16_t currentLimit;
    uint16_t currentLimitTime;
    uint16_t openTime;
    uint16_t closeTime;
};

struct BlindsController {
    struct BlindsControllerChannel channels[2];
    uint8_t swap_channels: 1;
    uint8_t channel0_dir: 1;
    uint8_t channel1_dir: 1;
};

typedef struct  {
    uint8_t is_metric: 1;
    uint8_t time_format_24h: 1;
    uint16_t weather_poll_interval;
    uint16_t api_timeout;
    uint8_t backlight_level;
} WeatherStationConfig_t;

struct WeatherStation {
    char openweather_api_key[65];
    char openweather_api_query[65];
    WeatherStationConfig_t config;
};

struct Sensor {
#if IOT_SENSOR_HAVE_BATTERY
    struct {
        float calibration;
    } battery;
#endif
#if (IOT_SENSOR_HAVE_HLW8012 || IOT_SENSOR_HAVE_HLW8032)
    struct {
        float calibrationU;
        float calibrationI;
        float calibrationP;
    } hlw80xx;
#endif
};

struct Clock {
    uint8_t blink_colon;
    int8_t animation;
    bool time_format_24h;
    uint8_t solid_color[3];
    int8_t order[8];
};

struct DimmerModuleButtons {
    uint16_t shortpress_time;
    uint16_t longpress_time;
    uint16_t repeat_time;
    uint16_t shortpress_no_repeat_time;
    uint8_t min_brightness;
    uint8_t shortpress_step;
    uint8_t longpress_max_brightness;
    uint8_t longpress_min_brightness;
    float shortpress_fadetime;
    float longpress_fadetime;
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

    uint16_t mqtt_port;
    uint16_t mqtt_keepalive;
    uint8_t mqtt_qos;

    char syslog_host[65];
    uint16_t syslog_port;

    struct NTP ntp;
    HomeAssistant homeassistant;
    DimmerModule dimmer;
    DimmerModuleButtons dimmer_buttons;
    BlindsController blinds_controller;
    SoftAP soft_ap;
    struct Serial2Tcp serial2tcp;
    struct HueConfig hue;
    struct Ping ping;
    WeatherStation weather_station;
    Sensor sensor;
    Clock clock;
};

#define _H_IP_FORM_OBJECT(name)                     config._H_GET_IP(name), [](const IPAddress &addr, FormField &) { config._H_SET_IP(name, addr); }
#define _H_STRUCT_FORMVALUE(name, type, field)      config._H_GET(name).field, [](type value, FormField &) { auto &data = config._H_W_GET(name); data.field = value; }

// NOTE using the new handlers (USE_WIFI_SET_EVENT_HANDLER_CB=0) costs 896 byte RAM with 5 handlers
#ifndef USE_WIFI_SET_EVENT_HANDLER_CB
#define USE_WIFI_SET_EVENT_HANDLER_CB           1
#endif
#if defined(ESP32)
#if USE_WIFI_SET_EVENT_HANDLER_CB == 0
#error ESP32 requires USE_WIFI_SET_EVENT_HANDLER_CB=1
#endif
#endif

class KFCFWConfiguration : public Configuration {
public:
    KFCFWConfiguration();
    ~KFCFWConfiguration();

    void setLastError(const String &error) override;
    const char *getLastError() const override;

    void restoreFactorySettings();
    static const String getFirmwareVersion();
    static const String getShortFirmwareVersion();

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

    bool isSafeMode() const {
        return _safeMode;
    }

private:
    void _setupWiFiCallbacks();
#if defined(ESP32)
    static void _onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info);
#elif USE_WIFI_SET_EVENT_HANDLER_CB
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

    TwoWire &initTwoWire(bool reset = false, Print *output = nullptr);

private:
    friend class KFCConfigurationPlugin;

    String _lastError;
    int16_t _garbageCollectionCycleDelay;
    uint8_t _dirty : 1;
    uint8_t _wifiConnected : 1;
    uint8_t _initTwoWire : 1;
    uint8_t _safeMode : 1;
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

#if DEBUG_HAVE_SAVECRASH
extern EspSaveCrash SaveCrash;
#endif
