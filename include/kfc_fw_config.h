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
#include <WiFiCallbacks.h>
#include <Wire.h>
#include <vector>
#include <functional>
#include <bitset>
#include <chrono>
#include <Configuration.h>
#if SYSLOG_SUPPORT
#include <KFCSyslog.h>
#endif
#include "../src/plugins/mdns/mdns_resolver.h"
#include <SaveCrash.h>
#include <StringKeyValueStore.h>
#include "../src/generated/FlashStringGeneratorAuto.h"
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

#include "kfc_fw_config_types.h"

#define HASH_SIZE                   64

#include "../src/plugins/dimmer_module/firmware_protocol.h"

struct DimmerModule {
    register_mem_cfg_t cfg;
    float on_off_fade_time;
    float fade_time;
    bool config_valid;
    // float fade_time;
    // float on_fade_time;
    // float linear_correction;
    // uint8_t max_temperature;
    // uint8_t metrics_int;
    // uint8_t report_temp;
    // uint8_t restore_level;
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
    Config_NTP();

    static const char *getTimezone();
    static const char *getPosixTZ();
    static const char *getServers(uint8_t num);
    static const char *getUrl();
    static uint16_t getNtpRfresh();

    static void defaults();

    char timezone[33];
    char servers[3][65];
    char posix_tz[33];
    uint16_t ntpRefresh;
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

typedef union __attribute__packed__ {
    uint32_t value: 24;
    uint8_t bgr[3];
    struct __attribute__packed__ {
        uint8_t blue;
        uint8_t green;
        uint8_t red;
    };
} ClockColor_t;

typedef struct __attribute__packed__ {
    ClockColor_t solid_color;
    int8_t animation: 7;
    int8_t time_format_24h: 1;
    uint8_t brightness;
    int16_t auto_brightness;
    uint16_t blink_colon_speed;
    uint16_t flashing_speed;
    struct {
        uint8_t temperature_50;
        uint8_t temperature_75;
        uint8_t max_temperature;
    } protection;
    struct {
        float multiplier;
        uint16_t speed;
        ClockColor_t factor;
        ClockColor_t minimum;
    } rainbow;
    struct {
        ClockColor_t color;
        uint16_t speed;
    } alarm;
    struct {
        float speed;
        uint16_t delay;
        ClockColor_t factor;
    } fading;
    // int8_t order[8];
    // uint8_t segmentOrder;
} Clock_t;

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
    ConfigFlags flags;

    Config_NTP ntp;

    uint16_t http_port;
    char cert_passphrase[33];

    DimmerModule dimmer;
    DimmerModuleButtons dimmer_buttons;
    BlindsController blinds_controller;
    Clock_t clock;

    Config_Ping ping;
    Config_Sensor sensor;
    Config_Button buttons;
} Config;

#define _H_IP_VALUE(name, ...) \
    IPAddress(config._H_GET_IP(name)), [__VA_ARGS__](const IPAddress &addr, FormField &, bool store) { \
        if (store) { \
            config._H_SET_IP(name, addr); \
        } \
        return false; \
    }

#define _H_STR_VALUE(name, ...) \
    String(config._H_STR(name)), [__VA_ARGS__](const String &str, FormField &, bool store) { \
        if (store) { \
            config._H_SET_STR(name, str); \
        } \
        return false; \
    }

#define _H_CSTR_FUNC(getter, setter, ...) \
    String(getter()), [__VA_ARGS__](const String &str, FormField &, bool store) { \
        if (store) { \
            setter(str.c_str()); \
        } \
        return false; \
    }

#define _H_FUNC(getter, setter, ...) \
    _H_FUNC_TYPE(getter, setter, decltype(getter()), ##__VA_ARGS__)

#define _H_FUNC_TYPE(getter, setter, type, ...) \
    static_cast<type>(getter()), [__VA_ARGS__](const type value, FormField &, bool store) { \
        if (store) { \
            setter(static_cast<decltype(getter())>(value)); \
        } \
        return false; \
    }

#define _H_STRUCT_IP_VALUE(name, field, ...) \
    IPAddress(config._H_GET(name).field), [__VA_ARGS__](const IPAddress &value, FormField &, bool store) { \
        if (store) { \
            auto &data = config._H_W_GET(name); \
            data.field = value; \
        } \
        return false; \
    }

#define _H_W_STRUCT_IP_VALUE(name, field, ...) \
    IPAddress(name.field), [&name, ##__VA_ARGS__](const IPAddress &value, FormField &, bool store) { \
        if (store) { \
            name.field = value; \
        } \
        return false; \
    }

#define _H_STRUCT_VALUE(name, field, ...) \
    config._H_GET(name).field, [__VA_ARGS__](const decltype(name.field) &value, FormField &, bool store) { \
        if (store) { \
            auto &data = config._H_W_GET(name); \
            data.field = value; \
        } \
        return false; \
    }

#define _H_STRUCT_VALUE_TYPE(name, field, type, ...) \
    config._H_GET(name).field, [__VA_ARGS__](const type &value, FormField &, bool store) { \
        if (store) { \
            auto &data = config._H_W_GET(name); \
            data.field = value; \
        } \
        return false; \
    }

#define _H_W_STRUCT_VALUE(name, field, ...) \
    name.field, [&name, ##__VA_ARGS__](const decltype(name.field) &value, FormField &, bool store) { \
        if (store) { \
            name.field = value; \
        } \
        return false; \
    }

#define _H_W_STRUCT_VALUE_TYPE(name, field, type, ...) \
    name.field, [&name, ##__VA_ARGS__](const type &value, FormField &, bool store) { \
        if (store) { \
            name.field = value; \
        } \
        return false; \
    }

#define _H_FLAGS_BOOL_VALUE(name, field, ...) \
    config._H_GET(name).field, [__VA_ARGS__](bool &value, FormField &, bool store) { \
        if (store) { \
            auto &data = config._H_W_GET(name); \
            data.field = value; \
        } \
        return false; \
    }

#define _H_FLAGS_VALUE(name, field, ...) \
    config._H_GET(name).field, [__VA_ARGS__](uint8_t &value, FormField &, bool store) { \
        if (store) { \
            auto &data = config._H_W_GET(name); \
            data.field = value; \
        } \
        return false; \
    }

#define addWriteableStruct(struct, member, ...) \
    add<decltype(struct.member)>(Form::normalizeName(F(_STRINGIFY(member))), _H_W_STRUCT_VALUE(struct, member, ##__VA_ARGS__)

#define addWriteableStructIPAddress(struct, member, ...) \
    add(Form::normalizeName(F(_STRINGIFY(member))), _H_W_STRUCT_IP_VALUE(struct, member, ##__VA_ARGS__)

#define addCStrGetterSetter(getter, setter, ...) \
    add(Form::normalizeName(F(_STRINGIFY(member))), _H_CSTR_FUNC(getter, setter, ##__VA_ARGS__)

#define addGetterSetter(getter, setter, ...) \
    add(Form::normalizeName(F(_STRINGIFY(member))), _H_FUNC(getter, setter, ##__VA_ARGS__)

#define addGetterSetterType(getter, setter, type, ...) \
    add(Form::normalizeName(F(_STRINGIFY(member))), _H_FUNC_TYPE(getter, setter, type, ##__VA_ARGS__)


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

    // restores defaults
    void restoreFactorySettings();
    // enables AP mode and sets passwords to default
    void recoveryMode();

    void customSettings();
    static const String getFirmwareVersion();
    static const String getShortFirmwareVersion();
    static const __FlashStringHelper *getShortFirmwareVersion_P();

    // flag to tell if the device has to be rebooted to apply all configuration changes
    void setConfigDirty(bool dirty);
    bool isConfigDirty() const;

    void setup();
    bool reconfigureWiFi();
    bool connectWiFi();
    void read();
    void write();

    // support for zeroconf
    // ${zeroconf:<service>.<proto>:<address|value[:port value]>|<fallback[:port]>}

    // resolve zeroconf, optional port to use as default or 0
    bool resolveZeroConf(const String &name, const String &hostname, uint16_t port, MDNSResolver::ResolvedCallback callback) const;

    // check if the hostname contains zeroconf
    bool hasZeroConf(const String &hostname) const;

#if ENABLE_DEEP_SLEEP

    void storeQuickConnect(const uint8_t *bssid, int8_t channel);
    void storeStationConfig(uint32_t ip, uint32_t netmask, uint32_t gateway);

    void wakeUpFromDeepSleep();
    void enterDeepSleep(milliseconds time, RFMode mode, uint16_t delayAfterPrepare = 0);

#endif
    void wifiQuickConnect();

    void restartDevice(bool safeMode = false);

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

    static void apStandModehandler(WiFiCallbacks::EventType event, void *payload);

private:
    void _setupWiFiCallbacks();
    void _apStandModehandler(WiFiCallbacks::EventType event);
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

public:
    using Container = KeyValueStorage::Container;
    using ContainerPtr = KeyValueStorage::ContainerPtr;
    using PersistantConfigCallback = std::function<void(Container &data)>;

    bool callPersistantConfig(ContainerPtr data, PersistantConfigCallback callback = nullptr);

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

#include "kfc_fw_config_classes.h"
