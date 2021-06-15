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
#include <EventScheduler.h>
#include <vector>
#include <functional>
#include <bitset>
#include <chrono>
#include <Configuration.h>
#include <IOExpander.h>
#if SYSLOG_SUPPORT
#include <Syslog.h>
#endif
#if MDNS_PLUGIN
#include "../src/plugins/mdns/mdns_resolver.h"
#endif
#include <Wire.h>
#include "logger.h"
#include "misc.h"
#include "at_mode.h"
#include "blink_led_timer.h"

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

#include <kfc_fw_ioexpander.h>



// #include "push_pack.h"

// // NOTE: any member of an packed structure (__attribute__packed__ ) cannot be passed to forms as reference, otherwise it might cause an unaligned exception
// // marking integers as bitset prevents using it as reference, i.e. uint8_t value: 8;
// // use _H_STRUCT_VALUE() or suitable macro

// class Config_Button {
// public:
//     typedef struct __attribute__packed__ {
//         uint16_t time;
//         uint16_t actionId;
//     } ButtonAction_t;

//     typedef struct __attribute__packed__ {
//         ButtonAction_t shortpress;
//         ButtonAction_t longpress;
//         ButtonAction_t repeat;
//         ButtonAction_t singleClick;
//         ButtonAction_t doubleClick;
//         uint8_t pin: 8;
//         uint8_t pinMode: 8;
//     } Button_t;

//     typedef std::vector<Button_t> ButtonVector;

//     static void getButtons(ButtonVector &buttons);
//     static void setButtons(ButtonVector &buttons);
// };

// #include "pop_pack.h"


#define _H_IP_VALUE(name, ...) \
    IPAddress(config._H_GET_IP(name)), [__VA_ARGS__](const IPAddress &addr, FormUI::Field::BaseField &, bool store) { \
        if (store) { \
            config._H_SET_IP(name, addr); \
        } \
        return false; \
    }

#define _H_STR_VALUE(name, ...) \
    String(config._H_STR(name)), [__VA_ARGS__](const String &str, FormUI::Field::BaseField &, bool store) { \
        if (store) { \
            config._H_SET_STR(name, str); \
        } \
        return false; \
    }

#define _H_CSTR_FUNC(getter, setter, ...) \
    String(getter()), [__VA_ARGS__](const String &str, FormUI::Field::BaseField &, bool store) { \
        if (store) { \
            setter(str.c_str()); \
        } \
        return false; \
    }

#define _H_FUNC(getter, setter, ...) \
    _H_FUNC_TYPE(getter, setter, decltype(getter()), ##__VA_ARGS__)

#define _H_FUNC_TYPE(getter, setter, type, ...) \
    static_cast<type>(getter()), [__VA_ARGS__](const type value, FormUI::Field::BaseField &, bool store) { \
        if (store) { \
            setter(static_cast<decltype(getter())>(value)); \
        } \
        return false; \
    }

#define _H_STRUCT_IP_VALUE(name, field, ...) \
    IPAddress(config._H_GET(name).field), [__VA_ARGS__](const IPAddress &value, FormUI::Field::BaseField &, bool store) { \
        if (store) { \
            auto &data = config._H_W_GET(name); \
            data.field = value; \
        } \
        return false; \
    }

#define _H_W_STRUCT_IP_VALUE(name, field, ...) \
    IPAddress(name.field), [&name, ##__VA_ARGS__](const IPAddress &value, FormUI::Field::BaseField &, bool store) { \
        if (store) { \
            name.field = value; \
        } \
        return false; \
    }

#define _H_STRUCT_VALUE(name, field, ...) \
    config._H_GET(name).field, [__VA_ARGS__](const decltype(name.field) &value, FormUI::Field::BaseField &, bool store) { \
        if (store) { \
            auto &data = config._H_W_GET(name); \
            data.field = value; \
        } \
        return false; \
    }


#define _H_STRUCT_VALUE_TYPE(name, field, type, ...) \
    config._H_GET(name).field, [__VA_ARGS__](const type &value, FormUI::Field::BaseField &, bool store) { \
        if (store) { \
            auto &data = config._H_W_GET(name); \
            data.field = value; \
        } \
        return false; \
    }

#define _H_W_STRUCT_VALUE(name, field, ...) \
    name.field, [&name, ##__VA_ARGS__](const decltype(name.field) &value, FormUI::Field::BaseField &, bool store) { \
        if (store) { \
            name.field = value; \
        } \
        return false; \
    }

#define _H_W_STRUCT_VALUE_TYPE(name, field, type, ...) \
    name.field, [&name, ##__VA_ARGS__](const type &value, FormUI::Field::BaseField &, bool store) { \
        if (store) { \
            name.field = value; \
        } \
        return false; \
    }

#define _H_FLAGS_BOOL_VALUE(name, field, ...) \
    config._H_GET(name).field, [__VA_ARGS__](bool &value, FormUI::Field::BaseField &, bool store) { \
        if (store) { \
            auto &data = config._H_W_GET(name); \
            data.field = value; \
        } \
        return false; \
    }

#define _H_FLAGS_VALUE(name, field, ...) \
    config._H_GET(name).field, [__VA_ARGS__](uint8_t &value, FormUI::Field::BaseField &, bool store) { \
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
    using minutes = std::chrono::duration<uint32_t, std::ratio<60>>;

    // static constexpr auto test1 = std::chrono::duration_cast<seconds>(KFCFWConfiguration::minutes(300)).count();

    KFCFWConfiguration();
    ~KFCFWConfiguration();

    void setLastError(const String &error);
    const char *getLastError() const;

    // get default device name based on its MAC address
    static String defaultDeviceName();
    // restores defaults
    void restoreFactorySettings();
    // enables AP mode and sets passwords to default
    void recoveryMode(bool resetPasswords = true);

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
    void read(bool wakeup = false);
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

    inline static void wakeUpFromDeepSleep() {
        BUILDIN_LED_SET(BlinkLEDTimer::BlinkType::FLICKER);
        wifiQuickConnect();
    }
    void enterDeepSleep(milliseconds time, RFMode mode, uint16_t delayAfterPrepare = 0);

#endif
public:
    static void wifiQuickConnect();

    // shutdown plugins gracefully and reboot device
    void restartDevice(bool safeMode = false);
    // clear RTC memory and reset device
    void resetDevice(bool safeMode = false);

    static void printVersion(Print &output);
    void printInfo(Print &output);

    static void loop();
    void gc();
    static uint8_t getMaxWiFiChannels();
    static const __FlashStringHelper *getWiFiEncryptionType(uint8_t type);

    bool isSafeMode() const;
    void setSafeMode(bool mode);

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

    // if the LED is not disabled, set LED to solid or off when connected and to fast blink if not
    static void setWiFiConnectLedMode();

    // return seconds since WiFi has been connected
    // 0 = not connected
    static uint32_t getWiFiUp();

    static TwoWire &initTwoWire(bool reset = false, Print *output = nullptr);

    static void setupRTC();
    static bool setRTC(uint32_t unixtime);
    static uint32_t getRTC();
    float getRTCTemperature();
    bool rtcLostPower() ;
    void printRTCStatus(Print &output, bool plain = true);

private:
    friend class KFCConfigurationPlugin;
    friend void WiFi_get_status(Print &out);

    String _lastError;
    uint32_t _wifiConnected;        // time of connection
    uint32_t _wifiUp;               // time of receiving IP address
    uint32_t _wifiFirstConnectionTime;
    int16_t _garbageCollectionCycleDelay;
    uint8_t _dirty : 1;
    uint8_t _safeMode : 1;

    static bool _initTwoWire;

#if USE_WIFI_SET_EVENT_HANDLER_CB == 0
    WiFiEventHandler _onWiFiConnect;
    WiFiEventHandler _onWiFiDisconnect;
    WiFiEventHandler _onWiFiGotIP;
    WiFiEventHandler _onWiFiOnDHCPTimeout;
    WiFiEventHandler _softAPModeStationConnected;
    WiFiEventHandler _softAPModeStationDisconnected;
#endif
};

inline bool KFCFWConfiguration::isSafeMode() const
{
    return _safeMode;
}

inline void KFCFWConfiguration::setSafeMode(bool mode)
{
    _safeMode = mode;
}


extern KFCFWConfiguration config;

#ifndef HAVE_IMPERIAL_MARCH
#define HAVE_IMPERIAL_MARCH 1
#endif

#if HAVE_IMPERIAL_MARCH

#define IMPERIAL_MARCH_NOTES_COUNT          66
#define MPERIAL_MARCH_NOTE_OFFSET           19
#define NOTE_TO_FREQUENCY_COUNT             56
#define NOTE_FP_TO_INT(freq)                ((uint16_t)(freq >> 5))
#define NOTE_FP_TO_FLOAT(freq)              ((float)(freq / 32.0f))

extern const uint16_t note_to_frequency[NOTE_TO_FREQUENCY_COUNT] PROGMEM;
extern const uint8_t imperial_march_notes[IMPERIAL_MARCH_NOTES_COUNT] PROGMEM;
extern const uint8_t imperial_march_lengths[IMPERIAL_MARCH_NOTES_COUNT] PROGMEM;

#endif
