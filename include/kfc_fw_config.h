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
#include <SaveCrash.h>
#include <Wire.h>
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

#if HAVE_PCF8574
#include <IOExpander.h>
using _PCF8574Range = IOExpander::PCF8574_Range<PCF8574_PORT_RANGE_START, PCF8574_PORT_RANGE_END>;
extern IOExpander::PCF8574 _PCF8574;
// these functions must be implemented and exported
extern void initialize_pcf8574();
extern void print_status_pcf8574(Print &output);
#endif

#if HAVE_PCF8575
#include <PCF8575.h>
extern PCF8575 _PCF8575;
extern void initialize_pcf8575();
extern void print_status_pcf8575(Print &output);
#endif

#if HAVE_PCA9685
extern void initialize_pca9785();
extern void print_status_pca9785(Print &output);
#endif
#if HAVE_MCP23017
extern void initialize_mcp23017();
extern void print_status_mcp23017(Print &output);
#endif

static inline void _digitalWrite(uint8_t pin, uint8_t value) {
#if HAVE_PCF8574

    if (_PCF8574Range::inRange(pin)) {
        _PCF8574.digitalWrite(_PCF8574Range::digitalPin2Pin(pin), value);
    }
    else
#endif
    {
        digitalWrite(pin, value);
    }
}

static inline uint8_t _digitalRead(uint8_t pin) {
#if HAVE_PCF8574
    if (_PCF8574Range::inRange(pin)) {
        return _PCF8574.digitalRead(_PCF8574Range::digitalPin2Pin(pin));
    }
    else
#endif
    {
        return digitalRead(pin);
    }
}

static inline void _analogWrite(uint8_t pin, uint16_t value) {
#if HAVE_PCF8574
    if (_PCF8574Range::inRange(pin)) {
        _PCF8574.digitalWrite(_PCF8574Range::digitalPin2Pin(pin), value ? HIGH : LOW);
    }
    else
#endif
    {
        analogWrite(pin, value);
    }
}

static inline uint16_t _analogRead(uint8_t pin) {
#if HAVE_PCF8574
    if (_PCF8574Range::inRange(pin)) {
        return _PCF8574.digitalRead(_PCF8574Range::digitalPin2Pin(pin)) ? 1023 : 0;
    }
    else
#endif
    {
        return analogRead(pin);
    }

}

static inline void _pinMode(uint8_t pin, uint8_t mode) {
#if HAVE_PCF8574
    if (_PCF8574Range::inRange(pin)) {
        _PCF8574.pinMode(_PCF8574Range::digitalPin2Pin(pin), mode);
    }
    else
#endif
    {
        pinMode(pin, mode);
    }
}

static inline size_t _pinName(uint8_t pin, char *buf, size_t size) {
#if HAVE_PCF8574
    if (_PCF8574Range::inRange(pin)) {
        return snprintf_P(buf, size - 1, PSTR("%u (PCF8574 pin %u)"), pin, _PCF8574Range::digitalPin2Pin(pin));
    }
    else
#endif
    {
        return snprintf_P(buf, size - 1, PSTR("%u"), pin);
    }
}

static inline bool _pinHasAnalogRead(uint8_t pin) {
#if HAVE_PCF8574
    if (_PCF8574Range::inRange(pin)) {
        return false;
    }
    else
#endif
    {
#if ESP8266
        return (pin == A0);
#else
#error missing
#endif
    }

}

static inline bool _pinHasAnalogWrite(uint8_t pin) {
#if HAVE_PCF8574
    if (_PCF8574Range::inRange(pin)) {
        return false;
    }
    else
#endif
    {
#if ESP8266
        return pin < A0;
#else
#error missing
#endif
    }

}


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

namespace DeepSleep
{
    struct __attribute__packed__ WiFiQuickConnect_t {
        int16_t channel: 15;                    //  0
        int16_t use_static_ip: 1;               // +2 byte
        uint8_t bssid[WL_MAC_ADDR_LENGTH];      // +6 byte
        uint32_t local_ip;
        uint32_t dns1;
        uint32_t dns2;
        uint32_t subnet;
        uint32_t gateway;
    };

    struct __attribute__packed__ DeepSleepParams_t {
        uint32_t unixtime;                  // unixtime when activating deep sleep. should be set to zero if no real time is available
        uint32_t lastWakeUpUnixTime;        // unixtime of the last wake up cycle. requires an RTC
        uint32_t deepSleepTime;             // total sleep time in millis
        uint32_t currentSleepTime;          // current cycle in millis
        uint32_t remainingSleepTime;        // remaining time in millis
        uint32_t cycleRuntime;              // time spent cycling in micros
        RFMode mode;
        bool abortOnKeyPress: 1;            // abort deepsleep on keypress

        struct __attribute__packed__ {
            bool connectWiFi: 1;            // use quick connect to get wifi access
            bool refreshDHCP: 1;            // refresh WiFi IP address if DHCP is enabled
            bool waitForNTP: 1;             // wait for NTP
            bool connectMQTT: 1;            // connect to MQTT and ppublish sensor data

            uint16_t timeLimit;             // time limit before going back to deep sleep even if tasks have not been completed
            struct __attribute__packed__ {
                uint8_t count;              // limit the use of errorDeepSleepTime
                uint8_t reset;              // reset count on success
                uint32_t time;              // time to sleep before retrying. if 0, deepSleepTime will be used
            } errorRetry;

            // max. 1048560 millis, ~1048 seconds
            void setTimeLimit(uint32_t millis) {
                timeLimit = millis >> 4U;
            }
            uint32_t getTimeLimit() const {
                return timeLimit << 4U;
            }

        } tasks;

        uint64_t getSleepTimeMicros() const {
            return currentSleepTime * 1000ULL;
        }

        void calcSleepTime();

        bool isUnlimited() const {
            return deepSleepTime == 0;
        }

        void abortCycles() {
            currentSleepTime = 0;
            if (!remainingSleepTime) {
                remainingSleepTime++;
            }
        }

        bool hasBeenAborted() const {
            return remainingSleepTime != 0 && currentSleepTime == 0;
        }

        bool cyclesCompeleted() const {
            return deepSleepTime != 0 && remainingSleepTime == 0 && currentSleepTime == 0;
        }

        DeepSleepParams_t() = default;
        DeepSleepParams_t(uint32_t sleepTimeMillis, RFMode rfMode);

        // return max. deep sleep length (80% of the max. value to be on the safe side)
        // or the value that has been set with setDeepSleepMaxTime()
        static uint32_t getDeepSleepMaxMillis();
        // set millis = 0 to get 80% of ESP.deepSleepMax() from getDeepSleepMaxMillis()
        static void setDeepSleepMaxTime(uint32_t millis = 0);
    private:
        static uint32_t _deepSleepMaxTime;
    };

    inline DeepSleepParams_t::DeepSleepParams_t(uint32_t sleepTimeMillis, RFMode rfMode) :
        unixtime(0),
        lastWakeUpUnixTime(0),
        deepSleepTime(sleepTimeMillis),
        remainingSleepTime(sleepTimeMillis),
        cycleRuntime(0),
        mode(rfMode),
        abortOnKeyPress(true),
        tasks({true, true, true, true, (60 * 1000U) >> 4, 5, 5, 3600 * 1000U})
    {
        calcSleepTime();
        if (deepSleepTime < tasks.errorRetry.time) {
            tasks.errorRetry.time = deepSleepTime;
        }
        ::printf(PSTR("deep sleep cycle started: time=%ums rest=%ums total=%ums\n"), currentSleepTime, remainingSleepTime, deepSleepTime);
    }

};

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

#if ENABLE_DEEP_SLEEP
    DeepSleep::DeepSleepParams_t _deepSleepParams;
#endif

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
