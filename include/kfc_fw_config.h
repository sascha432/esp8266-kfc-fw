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

#include <kfc_fw_config_types.h>

#define HASH_SIZE                   64

#include "push_pack.h"

// NOTE: any member of an packed structure (__attribute__packed__ ) cannot be passed to forms as reference, otherwise it might cause an unaligned exception
// marking integers as bitset prevents using it as reference, i.e. uint8_t value: 8;
// use _H_STRUCT_VALUE() or suitable macro

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

#define addWriteableStruct(form_name, struct, member, ...) \
    add<decltype(struct.member)>(F(form_name), _H_W_STRUCT_VALUE(struct, member, ##__VA_ARGS__)

#define addWriteableStructIPAddress(form_name, struct, member, ...) \
    add(F(form_name), _H_W_STRUCT_IP_VALUE(struct, member, ##__VA_ARGS__)

#define addCStrGetterSetter(form_name, getter, setter, ...) \
    add(F(form_name), _H_CSTR_FUNC(getter, setter, ##__VA_ARGS__)

#define addGetterSetter(form_name, getter, setter, ...) \
    add(F(form_name), _H_FUNC(getter, setter, ##__VA_ARGS__)

#define addGetterSetterType(form_name, getter, setter, type, ...) \
    add(F(form_name), _H_FUNC_TYPE(getter, setter, type, ##__VA_ARGS__)

#define addGetterSetterType_P(form_name, getter, setter, type, ...) \
    add(FPSTR(form_name), _H_FUNC_TYPE(getter, setter, type, ##__VA_ARGS__)


// NOTE using the new handlers (USE_WIFI_SET_EVENT_HANDLER_CB=0) costs 896 byte RAM with 5 handlers
#ifndef USE_WIFI_SET_EVENT_HANDLER_CB
#define USE_WIFI_SET_EVENT_HANDLER_CB           1
#endif
#if defined(ESP32)
#if USE_WIFI_SET_EVENT_HANDLER_CB == 0
#error ESP32 requires USE_WIFI_SET_EVENT_HANDLER_CB=1
#endif
#endif

#include <kfc_fw_config_classes.h>

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

