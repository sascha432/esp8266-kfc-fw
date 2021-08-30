/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#ifndef DEBUG_KFC_CONFIG
#    define DEBUG_KFC_CONFIG (0 || defined(DEBUG_ALL))
#endif

#include <Arduino_compat.h>
#include <EventScheduler.h>
#include <vector>
#include <functional>
#include <bitset>
#include <chrono>
#include <Configuration.hpp>
#include <IOExpander.h>
#if SYSLOG_SUPPORT
#include <Syslog.h>
#endif
#if MDNS_PLUGIN
#include "../src/plugins/mdns/mdns_resolver.h"
#endif
#include <Wire.h>
#include <dyn_bitset.h>
#include "logger.h"
#include "misc.h"
#include "at_mode.h"
#include "blink_led_timer.h"
#include "reset_detector.h"

#ifdef dhcp_start // defined in framework-arduinoespressif8266@2.20402.4/tools/sdk/lwip2/include/arch/cc.h
#    undef dhcp_start
#endif

#define HASH_SIZE 64

#include "kfc_fw_config_types.h"
#include <IOExpander.h>

// NOTE using the new handlers (USE_WIFI_SET_EVENT_HANDLER_CB=0) costs 896 byte RAM with 5 handlers
#ifndef USE_WIFI_SET_EVENT_HANDLER_CB
#    define USE_WIFI_SET_EVENT_HANDLER_CB 1
#endif
#if defined(ESP32)
#    if USE_WIFI_SET_EVENT_HANDLER_CB == 0
#        error ESP32 requires USE_WIFI_SET_EVENT_HANDLER_CB=1
#    endif
#endif

#include "kfc_fw_config_classes.h"

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
    bool reconfigureWiFi(const __FlashStringHelper *msg = nullptr);
    bool connectWiFi();
    void read(bool wakeup = false);
    void write();

    #if ESP8266
        static const __FlashStringHelper *getSleepTypeStr(sleep_type_t type);
        static const __FlashStringHelper *getWiFiOpModeStr(uint8_t mode);
        static const __FlashStringHelper *getWiFiPhyModeStr(phy_mode_t mode);
    #endif

    static void printDiag(Print &output, const String &prefix);

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

    static void apStandbyModehandler(WiFiCallbacks::EventType event, void *payload);

private:
    void _setupWiFiCallbacks();
    void _apStandbyModehandler(WiFiCallbacks::EventType event);
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
    bool _dirty;
    bool _safeMode;

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

extern KFCFWConfiguration config;

#include "kfc_fw_config_classes.hpp"

inline bool KFCFWConfiguration::isSafeMode() const
{
    return _safeMode;
}

inline void KFCFWConfiguration::setSafeMode(bool mode)
{
    _safeMode = mode;
}

inline void KFCFWConfiguration::_apStandbyModehandler(WiFiCallbacks::EventType event)
{
    __LDBG_printf("event=%u", event);
    if (event == WiFiCallbacks::EventType::CONNECTED) {
        WiFi.enableAP(false);
        Logger_notice(F("AP Mode disabled"));
    }
    else if (event == WiFiCallbacks::EventType::DISCONNECTED) {
        WiFi.enableAP(true);
        Logger_notice(F("AP Mode enabled"));
    }
}

inline void KFCFWConfiguration::apStandbyModehandler(WiFiCallbacks::EventType event, void *payload)
{
    config._apStandbyModehandler(event);
}

inline void KFCFWConfiguration::setLastError(const String &error)
{
    _lastError = error;
}

inline const char *KFCFWConfiguration::getLastError() const
{
    return _lastError.c_str();
}

#ifndef HAVE_IMPERIAL_MARCH
#    define HAVE_IMPERIAL_MARCH 1
#endif

#if HAVE_IMPERIAL_MARCH

#    define IMPERIAL_MARCH_NOTES_COUNT 66
#    define MPERIAL_MARCH_NOTE_OFFSET  19
#    define NOTE_TO_FREQUENCY_COUNT    56
#    define NOTE_FP_TO_INT(freq)       ((uint16_t)(freq >> 5))
#    define NOTE_FP_TO_FLOAT(freq)     ((float)(freq / 32.0f))

extern const uint16_t note_to_frequency[NOTE_TO_FREQUENCY_COUNT] PROGMEM;
extern const uint8_t imperial_march_notes[IMPERIAL_MARCH_NOTES_COUNT] PROGMEM;
extern const uint8_t imperial_march_lengths[IMPERIAL_MARCH_NOTES_COUNT] PROGMEM;

#endif
