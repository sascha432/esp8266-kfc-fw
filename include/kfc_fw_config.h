   /**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#ifndef DEBUG_KFC_CONFIG
#define DEBUG_KFC_CONFIG            0
#endif

// basic load statistics
#ifndef LOAD_STATISTICS
#define LOAD_STATISTICS             0
#endif

#include <Arduino_compat.h>
#include <Wire.h>
#include <vector>
#include <functional>
#include <bitset>
#include <chrono>
#include <Configuration.h>
#if SYSLOG
#include <KFCSyslog.h>
#endif
#include <Timezone.h>
#include <SaveCrash.h>
#include "logger.h"
#include "misc.h"
#include "at_mode.h"
#include "reset_detector.h"
#include "dyn_bitset.h"

#ifdef dhcp_start // defined in framework-arduinoespressif8266@2.20402.4/tools/sdk/lwip2/include/arch/cc.h
#undef dhcp_start
#endif

#if LOAD_STATISTICS
extern unsigned long load_avg_timer;
extern uint32_t load_avg_counter;
extern float load_avg[3]; // 1min, 5min, 15min
#define LOOP_COUNTER_LOAD(avg) (avg ? 45900 / avg : NAN)    // this calculates the load compared to safe mode @ 1.0, no wifi and no load, 80MHz
#endif

#if defined(ESP32)
#include <esp_wifi_types.h>
#define WIFI_ENCRYPTION_ARRAY               array_of<uint8_t>(WIFI_AUTH_OPEN, WIFI_AUTH_WEP, WIFI_AUTH_WPA_PSK, WIFI_AUTH_WPA2_PSK, WIFI_AUTH_WPA_WPA2_PSK, WIFI_AUTH_WPA2_ENTERPRISE)
using WiFiEncryptionTypeArray = std::array<uint8_t, 6>;
using WiFiEncryptionType = wifi_auth_mode_t;
static auto const WiFiEncryptionTypeDefault = WIFI_AUTH_WPA2_PSK;
#elif defined(ESP8266)
#define WIFI_ENCRYPTION_ARRAY               array_of<uint8_t>(ENC_TYPE_NONE, ENC_TYPE_TKIP, ENC_TYPE_WEP, ENC_TYPE_CCMP, ENC_TYPE_AUTO)
using WiFiEncryptionTypeArray = std::array<uint8_t, 5>;
using WiFiEncryptionType = wl_enc_type;
static auto const WiFiEncryptionTypeDefault = ENC_TYPE_CCMP;
#else
#error Platform not supported
#endif


#define HASH_SIZE                   64

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
    ConfigFlags_t ledMode:1;
    ConfigFlags_t wifiMode:2;
    ConfigFlags_t atModeEnabled:1;
    ConfigFlags_t hiddenSSID:1;
    ConfigFlags_t softAPDHCPDEnabled:1;
    ConfigFlags_t stationModeDHCPEnabled:1;
    ConfigFlags_t webServerMode:2;
    ConfigFlags_t ntpClientEnabled:1;
    ConfigFlags_t syslogProtocol:3;
    ConfigFlags_t mqttMode:2;
    ConfigFlags_t mqttAutoDiscoveryEnabled:1;
    ConfigFlags_t restApiEnabled:1;
    ConfigFlags_t serial2TCPMode:3;
    ConfigFlags_t useStaticIPDuringWakeUp:1;
    ConfigFlags_t webServerPerformanceModeEnabled:1;
    ConfigFlags_t apStandByMode: 1;
};

struct HueConfig {
    uint16_t tcp_port;
    char devices[255]; // \n is separator
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

struct DimmerModule {
    float fade_time;
    float on_fade_time;
    float linear_correction;
    uint8_t max_temperature;
    uint8_t metrics_int;
    uint8_t report_temp;
    uint8_t restore_level;
#if IOT_ATOMIC_SUN_V2
    int8_t channel_mapping[4];
#endif
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
#if IOT_DIMMER_MODULE_CHANNELS
    uint8_t pins[IOT_DIMMER_MODULE_CHANNELS * 2];
#endif
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
    bool swap_channels;
    bool channel0_dir;
    bool channel1_dir;
};

#include "push_pack.h"

// NOTE: any member of an packed structure (__attribute__packed__ ) cannot be passed to forms as reference, otherwise it might cause an unaligned exception
// marking integers as bitset prevents using it as reference, i.e. uint8_t value: 8;
// use _H_STRUCT_VALUE() or suitable macro

class Config_HTTP
{
public:
    typedef struct{
        uint16_t port;
    } config_t;
    config_t config;

    Config_HTTP() : config({80}) {
        //TODO
    }
};

class Config_Syslog
{
public:
    typedef struct{
        uint16_t port;
    } config_t;
    config_t config;

    Config_Syslog() : config({514}) {
        //TODO
    }
};

class Config_MQTT
{
public:
    typedef struct {
        uint32_t port: 16;
        uint32_t keepalive: 16;
        uint32_t qos: 8;
    } config_t;
    config_t config;

    Config_MQTT();

    uint16_t getPort() const {
        return config.port;
    }
    uint16_t getKeepAlive() const {
        return config.keepalive;
    }
    uint8_t getQos() const {
        return config.qos;
    }

    static void defaults();

    static Config_MQTT::config_t getConfig();
    static MQTTMode_t getMode();
    static const char *getHost();
    static const char *getUsername();
    static const char *getPassword();
    static const char *getTopic();
    static const char *getDiscoveryPrefix();
    static const uint8_t *getFingerprint();

    char host[65];
    char username[33];
    char password[33];
    char topic[128];
    char fingerprint[20];
    char discovery_prefix[32];
};

class Config_Ping
{
public:
    typedef struct __attribute__packed__ {
        uint8_t count: 8;
        uint16_t interval;
        uint16_t timeout;
    } config_t;
    config_t config;

    Config_Ping() {
        config = { 5, 60, 5000 };
    }

    static const char *getHost(uint8_t num);
    static void defaults();

    char host1[65];
    char host2[65];
    char host3[65];
    char host4[65];
};

class Config_NTP {
public:
    typedef struct __attribute__packed__ {
        int32_t offset;
        char abbreviation[4];
        uint16_t ntpRefresh;
        uint8_t dst;
    } Timezone_t;

    Config_NTP();

    static const char *getTimezone();
    static const char *getServers(uint8_t num);
    static const char *getUrl();
    static Timezone_t getTZ();

    static void defaults();

    char timezone[33];
    char servers[3][65];
#if USE_REMOTE_TIMEZONE
    char remote_tz_dst_ofs_url[255];
#endif
    Timezone_t tz;
};

class Config_Sensor {
public:
#if IOT_SENSOR_HAVE_BATTERY
    typedef struct __attribute__packed__ {
        float calibration;
        uint8_t precision: 8;
        float offset;
    } battery_t;
    battery_t battery;
#endif
#if (IOT_SENSOR_HAVE_HLW8012 || IOT_SENSOR_HAVE_HLW8032)
    typedef struct __attribute__packed__ {
        float calibrationU;
        float calibrationI;
        float calibrationP;
        uint64_t energyCounter;
        uint8_t extraDigits: 8;
    } hlw80xx_t;
    hlw80xx_t hlw80xx;
#endif
    Config_Sensor() {
#if IOT_SENSOR_HAVE_BATTERY
        battery = battery_t({ 1.0f, 2, 0.0f });
#endif
#if (IOT_SENSOR_HAVE_HLW8012 || IOT_SENSOR_HAVE_HLW8032)
        hlw80xx = hlw80xx_t({ 1.0f, 1.0f, 1.0f, 0, 0 });
#endif
    }
};

class Config_Button {
public:
    typedef struct __attribute__packed__ {
        uint16_t time;
        uint16_t actionId;
    } ButtonAction_t;

    typedef struct __attribute__packed__ {
        ButtonAction_t shortpress;
        ButtonAction_t longpress;
        ButtonAction_t repeat;
        ButtonAction_t singleClick;
        ButtonAction_t doubleClick;
        uint8_t pin: 8;
        uint8_t pinMode: 8;
    } Button_t;

    typedef std::vector<Button_t> ButtonVector;

    static void getButtons(ButtonVector &buttons);
    static void setButtons(ButtonVector &buttons);
};

#include "pop_pack.h"

struct Clock {
    uint8_t blink_colon;
    int8_t animation;
    bool time_format_24h;
    uint8_t solid_color[3];
    uint8_t brightness;
    uint8_t temp_75;
    uint8_t temp_50;
    uint8_t temp_prot;
    int16_t auto_brightness;
    // int8_t order[8];
    // uint8_t segmentOrder;
};

namespace Config_QuickConnect
{
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
};

typedef struct {
    uint32_t version;
    ConfigFlags flags;
    char device_name[17];
    char device_title[33];
    char device_pass[33];
    char device_token[128];

    Config_HTTP http;
    Config_Syslog syslog;
    Config_MQTT mqtt;
    Config_NTP ntp;

    uint16_t http_port;
    char cert_passphrase[33];

    char syslog_host[65];
    uint16_t syslog_port;

    DimmerModule dimmer;
    DimmerModuleButtons dimmer_buttons;
    BlindsController blinds_controller;
    struct Serial2Tcp serial2tcp;
    struct HueConfig hue;
    Clock clock;

    Config_Ping ping;
    Config_Sensor sensor;
    Config_Button buttons;
} Config;

#define _H_IP_VALUE(name, ...) \
    IPAddress(config._H_GET_IP(name)), [__VA_ARGS__](const IPAddress &addr, FormField &, bool) { \
        config._H_SET_IP(name, addr); \
        return false; \
    }

#define _H_STR_VALUE(name, ...) \
    String(config._H_STR(name)), [__VA_ARGS__](const String &str, FormField &, bool) { \
        config._H_SET_STR(name, str); \
        return false; \
    }

#define _H_STRUCT_IP_VALUE(name, field, ...) \
    IPAddress(config._H_GET(name).field), [__VA_ARGS__](const IPAddress &value, FormField &, bool) { \
        auto &data = config._H_W_GET(name); \
        data.field = value; \
        return false; \
    }

#define _H_STRUCT_VALUE(name, field, ...) \
    config._H_GET(name).field, [__VA_ARGS__](const decltype(name.field) &value, FormField &, bool) { \
        auto &data = config._H_W_GET(name); \
        data.field = value; \
        return false; \
    }

#define _H_STRUCT_VALUE_TYPE(name, field, type, ...) \
    config._H_GET(name).field, [__VA_ARGS__](const type &value, FormField &, bool) { \
        auto &data = config._H_W_GET(name); \
        data.field = value; \
        return false; \
    }

#define _H_FLAGS_BOOL_VALUE(name, field, ...) \
    config._H_GET(name).field, [__VA_ARGS__](bool &value, FormField &, bool) { \
        auto &data = config._H_W_GET(name); \
        data.field = value; \
        return false; \
    }

#define _H_FLAGS_VALUE(name, field, ...) \
    config._H_GET(name).field, [__VA_ARGS__](uint8_t &value, FormField &, bool) { \
        auto &data = config._H_W_GET(name); \
        data.field = value; \
        return false; \
    }

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
    using milliseconds = std::chrono::duration<uint32_t, std::milli>;
    using seconds = std::chrono::duration<uint32_t, std::ratio<1>>;

    KFCFWConfiguration();
    ~KFCFWConfiguration();

    void setLastError(const String &error);
    const char *getLastError() const;

    void restoreFactorySettings();
    void customSettings();
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
    void enterDeepSleep(milliseconds time, RFMode mode, uint16_t delayAfterPrepare = 0);
    void restartDevice();

    static void printVersion(Print &output);
    void printInfo(Print &output);

    static void loop();
    void gc();
    static uint8_t getMaxWiFiChannels();
    static String getWiFiEncryptionType(uint8_t type);

    bool isSafeMode() const {
        return _safeMode;
    }
    void setSafeMode(bool mode) {
        _safeMode = mode;
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
    bool setRTC(uint32_t unixtime);
    uint32_t getRTC();
    float getRTCTemperature();
    bool rtcLostPower() ;
    void printRTCStatus(Print &output, bool plain = true);

    const char *getDeviceName() const;

public:
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

extern const char *session_get_device_token();

#include "kfc_fw_config_classes.h"
