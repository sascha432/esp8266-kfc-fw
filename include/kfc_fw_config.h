   /**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#ifndef DEBUG_KFC_CONFIG
#define DEBUG_KFC_CONFIG        0
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
#include "../.pio/libdeps/remote/EspSaveCrash/src/EspSaveCrash.h"
// #include <EspSaveCrash.h>
#endif

#ifdef dhcp_start // defined in framework-arduinoespressif8266@2.20402.4/tools/sdk/lwip2/include/arch/cc.h
#undef dhcp_start
#endif

#if defined(ESP32)
#define WIFI_DEFAULT_ENCRYPTION             WIFI_AUTH_WPA2_PSK
#define WIFI_ENCRYPTION_ARRAY               array_of<uint8_t>(WIFI_AUTH_OPEN, WIFI_AUTH_WEP, WIFI_AUTH_WPA_PSK, WIFI_AUTH_WPA2_PSK, WIFI_AUTH_WPA_WPA2_PSK, WIFI_AUTH_WPA2_ENTERPRISE)
#define WIFI_ENCRYPTION_ARRAY_SIZE          6
#elif defined(ESP8266)
#define WIFI_DEFAULT_ENCRYPTION             ENC_TYPE_CCMP
#define WIFI_ENCRYPTION_ARRAY               array_of<uint8_t>(ENC_TYPE_NONE, ENC_TYPE_TKIP, ENC_TYPE_WEP, ENC_TYPE_CCMP, ENC_TYPE_AUTO)
#define WIFI_ENCRYPTION_ARRAY_SIZE          5
#else
#error Platform not supported
#endif


#define HASH_SIZE               64

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
#if defined(ESP8266)
    ConfigFlags_t webServerPerformanceModeEnabled:1;
#endif
    ConfigFlags_t ntpClientEnabled:1;
    ConfigFlags_t syslogProtocol:3;
    ConfigFlags_t mqttMode:2;
    ConfigFlags_t mqttAutoDiscoveryEnabled:1;
    ConfigFlags_t restApiEnabled:1;
    ConfigFlags_t serial2TCPMode:3;
    ConfigFlags_t hueEnabled:1;
    ConfigFlags_t useStaticIPDuringWakeUp:1;
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

class Config_Network
{
public:
    typedef struct {
        uint32_t dns1;
        uint32_t dns2;
        uint32_t local_ip;
        uint32_t subnet;
        uint32_t gateway;
    } config_t;
    config_t config;

    Config_Network();

    static const char *getSSID();
    static const char *getPassword();

    char wifi_ssid[33];
    char wifi_pass[33];
};

class Config_SoftAP
{
public:
    typedef struct {
        uint32_t address;
        uint32_t subnet;
        uint32_t gateway;
        uint32_t dhcp_start;
        uint32_t dhcp_end;
        uint16_t channel: 8;
        uint16_t encryption: 8;
    } config_t;
    config_t config;

    Config_SoftAP();

    IPAddress getAddress() const {
        return IPAddress(config.address);
    }

    IPAddress getSubnet() const {
        return IPAddress(config.subnet);
    }

    IPAddress getGateway() const {
        return IPAddress(config.gateway);
    }

    IPAddress getDHCPStart() const {
        return IPAddress(config.dhcp_start);
    }

    IPAddress getDHCPEnd() const {
        return IPAddress(config.dhcp_end);
    }

    uint8_t getChannel() const {
        return config.channel;
    }

    uint8_t getEncryption() const {
        return config.encryption;
    }

    static const char *getAPSSID();
    static const char *getPassword();

    char wifi_ssid[33];
    char wifi_pass[33];
};


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

    static void defaults(MQTTMode_t mode = MQTT_MODE_UNSECURE);

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
        int32_t offset: 32;
        char abbreviation[4];
        bool dst;
        uint16_t ntpRefresh;
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


class Config_HomeAssistant {
public:
    typedef enum : uint8_t {
        NONE = 0,
        TURN_ON,
        TURN_OFF,
        SET_BRIGHTNESS,
        CHANGE_BRIGHTNESS,  // values: min brightness(0), max brightness(255), turn on/off if brightness=0(1)/change brightness only(0)
    } ActionEnum_t;
    typedef struct __attribute__packed__ {
        uint16_t id: 16;
        ActionEnum_t action;
        uint8_t valuesLen;
        uint8_t entityLen;
    } ActionHeader_t;

    class Action {
    public:
        typedef std::vector<int32_t> ValuesVector;

        Action() = default;
        Action(uint16_t id, ActionEnum_t action, const ValuesVector &values, const String &entityId) : _id(id), _action(action), _values(values), _entityId(entityId) {
        }
        ActionEnum_t getAction() const {
            return _action;
        }
        const __FlashStringHelper *getActionFStr() const {
            return Config_HomeAssistant::getActionStr(_action);
        }
        void setAction(ActionEnum_t action) {
            _action = action;
        }
        uint16_t getId() const {
            return _id;
        }
        void setId(uint16_t id) {
            _id = id;
        }
        int32_t getValue(uint8_t num) const {
            if (num < _values.size()) {
                return _values.at(num);
            }
            return 0;
        }
        void setValues(const ValuesVector &values) {
            _values = values;
        }
        ValuesVector &getValues() {
            return _values;
        }
        const String getValuesStr() const {
            String str;
            int n = 0;
            for(auto value: _values) {
                if (n++ > 0) {
                    str += ',';
                }
                str += String(value);
            }
            return str;
        }
        uint8_t getNumValues() const {
            return _values.size();
        }
        const String &getEntityId() const {
            return _entityId;
        }
        void setEntityId(const String &entityId) {
            _entityId = entityId;
        }
    private:
        uint16_t _id;
        ActionEnum_t _action;
        ValuesVector _values;
        String _entityId;
    };

    typedef std::vector<Action> ActionVector;

    Config_HomeAssistant() {
    }

    static const char *getApiEndpoint();
    static const char *getApiToken();
    static void getActions(ActionVector &actions);
    static void setActions(ActionVector &actions);
    static Action getAction(uint16_t id);
    static const __FlashStringHelper *getActionStr(ActionEnum_t action);

    char api_endpoint[128];
    char token[250];
    uint8_t *actions;
};

class Config_RemoteControl {
public:
    typedef struct __attribute__packed__ {
        uint16_t shortpress;
        uint16_t longpress;
        uint16_t repeat;
    } Action_t;
    typedef struct __attribute__packed__ {
        uint8_t autoSleepTime: 8;
        uint16_t deepSleepTime: 16;       // ESP8266 ~14500 seconds, 0 = indefinitely
        uint16_t shortPressTime;
        uint16_t longpressTime;
        uint16_t repeatTime;
        Action_t actions[4];
    } config_t;
    config_t config;

    Config_RemoteControl() : config() {
        config.autoSleepTime = 5;
        config.shortPressTime = 300;
        config.longpressTime = 750;
        config.repeatTime = 250;
    }

    void validate() {
        if (!config.shortPressTime) {
            config = config_t();
            config.shortPressTime = 300;
            config.longpressTime = 750;
            config.repeatTime = 250;
        }
        if (!config.autoSleepTime) {
            config.autoSleepTime = 5;
        }
    }
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

class Config_WeatherStation
{
public:
    typedef struct __attribute__packed__ {
        uint8_t is_metric: 1;
        uint8_t time_format_24h: 1;
        uint16_t weather_poll_interval;
        uint16_t api_timeout;
        uint8_t backlight_level;
        uint8_t touch_threshold;
        uint8_t released_threshold;
        float temp_offset;

        void reset() {
            *this = {false, false, 15, 30, 100, 5, 8, 0.0};
        }

        void validate() {
            if (weather_poll_interval == 0 || api_timeout == 0 || touch_threshold == 0 || released_threshold == 0) {
                reset();
            }
            if (backlight_level < 10) {
                backlight_level = 10;
            }
        }
        uint32_t getPollIntervalMillis() {
            return weather_poll_interval * 60000UL;
        }
    } WeatherStationConfig_t;

    Config_WeatherStation() {
        config.reset();
    }

    static void defaults();
    static const char *getApiKey();
    static const char *getQueryString();
    static WeatherStationConfig_t &getWriteableConfig();
    static WeatherStationConfig_t getConfig();

    char openweather_api_key[65];
    char openweather_api_query[65];
    WeatherStationConfig_t config;
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
    char device_pass[33];

    Config_Network network;
    Config_SoftAP soft_ap;

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

    Config_HomeAssistant homeassistant;
    Config_Ping ping;
    Config_WeatherStation weather_station;
    Config_Sensor sensor;
    Config_RemoteControl remote_control;
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
    KFCFWConfiguration();
    ~KFCFWConfiguration();

    void setLastError(const String &error);
    const char *getLastError() const;

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
