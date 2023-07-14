/**
 * author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <kfc_fw_config.h>
#include <kfc_fw_config_plugin.h>
#include <stl_ext/array.h>
#include <sys/time.h>
#include <EEPROM.h>
#include <ReadADC.h>
#include <PrintString.h>
#include <HardwareSerial.h>
#include <EventScheduler.h>
#include <BitsToStr.h>
#include <session.h>
#include <misc.h>
#include "blink_led_timer.h"
#include "fs_mapping.h"
#include "WebUISocket.h"
#include "web_server.h"
#include "build.h"
#include "save_crash.h"
#include <JsonBaseReader.h>
#include <Form/Types.h>
#if __LED_BUILTIN_WS2812_NUM_LEDS
#    include <NeoPixelEx.h>
#endif
#include "deep_sleep.h"
#include "PinMonitor.h"
#include "../src/plugins/plugins.h"
#if IOT_SWITCH
#    include "../src/plugins/switch/switch.h"
#endif
#if IOT_WEATHER_STATION
#    include "../src/plugins/weather_station/weather_station.h"
#endif

#if defined(ESP8266)
#    include <core_esp8266_version.h>
#    include <sntp.h>
// #    include <sntp-lwip2.h>
#elif defined(ESP32)
#    include <lwip/apps/sntp.h>
#    include <nvs.h>
#endif

#if RTC_SUPPORT
#    include <RTClib.h>
#    if RTC_DEVICE_DS3231
        RTC_DS3231 rtc;
#    else
#        error RTC device not set
#    endif
#endif

#if DEBUG_KFC_CONFIG
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

bool KFCFWConfiguration::_initTwoWire = false;
KFCFWConfiguration config;

using MainConfig = KFCConfigurationClasses::MainConfig;
using Network = KFCConfigurationClasses::Network;
using System = KFCConfigurationClasses::System;
using Plugins = KFCConfigurationClasses::PluginsType;

#if HAVE_IMPERIAL_MARCH

// source
// https://sourceforge.net/p/reprap/code/HEAD/tree/trunk/users/metalab/python/imperial_gcode.py

const uint8_t imperial_march_notes[IMPERIAL_MARCH_NOTES_COUNT] PROGMEM = {
    12, 12, 12, 8,  15, 12, 8,  15, 12,         // 9
    19, 19, 19, 20, 15, 12, 8,  15, 12,         // 9
    24, 12, 12, 24, 23, 22, 21, 20, 21,         // 9
    13, 18, 17, 16, 15, 14, 15,                 // 7
    8,  11, 8,  11, 15, 12, 15, 19,             // 8
    24, 12, 12, 24, 23, 22, 21, 20, 21,         // 9
    13, 18, 17, 16, 15, 14, 15,                 // 7
    8,  11, 8,  15, 12, 8,  15, 12              // 8
};

const uint8_t imperial_march_lengths[IMPERIAL_MARCH_NOTES_COUNT] PROGMEM = {
    4, 4, 4, 3, 1, 4, 3, 1, 8,
    4, 4, 4, 3, 1, 4, 3, 1, 8,
    4, 3, 1, 4, 3, 1, 1, 1, 4,
    2, 4, 3, 1, 1, 1, 4,
    2, 4, 3, 1, 4, 3, 1, 8,
    4, 3, 1, 4, 3, 1, 1, 1, 4,
    2, 4, 3, 1, 1, 1, 4,
    2, 4, 3, 1, 4, 3, 1, 8
};

// scripts/tools/gen_note_to_freq.py
const uint16_t note_to_frequency[NOTE_TO_FREQUENCY_COUNT] PROGMEM = {
    // fixed point * 32, index, frequency, fixed point >> 5
    2093, // [0] 65.406 65
    2217, // [1] 69.296 69
    2349, // [2] 73.416 73
    2489, // [3] 77.782 77
    2637, // [4] 82.407 82
    2793, // [5] 87.307 87
    2959, // [6] 92.499 92
    3135, // [7] 97.999 97
    3322, // [8] 103.826 103
    3520, // [9] 110.000 110
    3729, // [10] 116.541 116
    3951, // [11] 123.471 123
    4186, // [12] 130.813 130
    4434, // [13] 138.591 138
    4698, // [14] 146.832 146
    4978, // [15] 155.563 155
    5274, // [16] 164.814 164
    5587, // [17] 174.614 174
    5919, // [18] 184.997 184
    6271, // [19] 195.998 195
    6644, // [20] 207.652 207
    7040, // [21] 220.000 220
    7458, // [22] 233.082 233
    7902, // [23] 246.942 246
    8372, // [24] 261.626 261
    8869, // [25] 277.183 277
    9397, // [26] 293.665 293
    9956, // [27] 311.127 311
    10548, // [28] 329.628 329
    11175, // [29] 349.228 349
    11839, // [30] 369.994 369
    12543, // [31] 391.995 391
    13289, // [32] 415.305 415
    14080, // [33] 440.000 440
    14917, // [34] 466.164 466
    15804, // [35] 493.883 493
    16744, // [36] 523.251 523
    17739, // [37] 554.365 554
    18794, // [38] 587.330 587
    19912, // [39] 622.254 622
    21096, // [40] 659.255 659
    22350, // [41] 698.456 698
    23679, // [42] 739.989 739
    25087, // [43] 783.991 783
    26579, // [44] 830.609 830
    28160, // [45] 880.000 880
    29834, // [46] 932.328 932
    31608, // [47] 987.767 987
    33488, // [48] 1046.502 1046
    35479, // [49] 1108.731 1108
    37589, // [50] 1174.659 1174
    39824, // [51] 1244.508 1244
    42192, // [52] 1318.510 1318
    44701, // [53] 1396.913 1396
    47359, // [54] 1479.978 1479
    50175, // [55] 1567.982 1567
};
#endif

// KFCFWConfiguration

static_assert(CONFIG_EEPROM_SIZE >= 1024 && (CONFIG_EEPROM_SIZE) <= 4096, "invalid EEPROM size");

KFCFWConfiguration::KFCFWConfiguration() :
    Configuration(CONFIG_EEPROM_SIZE),
    _wifiConnected(0),
    _wifiUp(0),
    _wifiFirstConnectionTime(0),
    _garbageCollectionCycleDelay(5000),
    _wifiNumActive(0),
    _wifiErrorCount(0),
    _dirty(false),
    _safeMode(false)
{
    _setupWiFiCallbacks();
}

KFCFWConfiguration::~KFCFWConfiguration()
{
    LoopFunctions::remove(KFCFWConfiguration::loop);
}

void KFCFWConfiguration::_onWiFiConnectCb(const WiFiEventStationModeConnected &event)
{
    __LDBG_printf("ssid=%s channel=%u bssid=%s wifi_connected=%u is_connected=%u ip=%u/%s", event.ssid.c_str(), event.channel, mac2String(event.bssid).c_str(), _wifiConnected, WiFi.isConnected(), IPAddress_isValid(WiFi.localIP()), WiFi.localIP().toString().c_str());
    if (!_wifiConnected) {

        _wifiConnected = millis();
        if (_wifiConnected == 0) {
            _wifiConnected++;
        }
        String append;
        if (_wifiFirstConnectionTime == 0) {
            _wifiFirstConnectionTime = _wifiConnected;
            append = PrintString(F(" after %ums"), _wifiConnected);
        }
        Logger_notice(F("WiFi(#%u) connected to %s (%s)%s"), config.getWiFiConfigurationNum() + 1, event.ssid.c_str(), mac2String(event.bssid).c_str(), append.c_str());

        #if ENABLE_DEEP_SLEEP
            config.storeQuickConnect(event.bssid, event.channel);

            // get default WiFi settings
            struct station_config defaultCfg;
            wifi_station_get_config_default(&defaultCfg);

            struct station_config config = {};
            strncpy(reinterpret_cast<char *>(config.ssid), event.ssid.c_str(), sizeof(config.ssid));
            strncpy(reinterpret_cast<char *>(config.password), Network::WiFi::getPassword(_wifiNumActive), sizeof(config.password));
            memmove(config.bssid, event.bssid, sizeof(event.bssid));
            config.bssid_set = *config.bssid != 0;

        #endif

        // #if defined(ESP32)
        //     auto hostname = System::Device::getName();
        //     __LDBG_printf("WiFi.setHostname(%s)", hostname);
        //     if (!WiFi.setHostname(hostname)) {
        //         __LDBG_printf("WiFi.setHostname(%s) failed", hostname);
        //     }
        // #endif

    }

}

void KFCFWConfiguration::_onWiFiDisconnectCb(const WiFiEventStationModeDisconnected &event)
{
    __LDBG_printf("reason=%d error=%s wifi_connected=%u wifi_up=%u is_connected=%u ip=%s", event.reason, WiFi_disconnect_reason(event.reason), _wifiConnected, _wifiUp, WiFi.isConnected(), WiFi.localIP().toString().c_str());

    if (_wifiConnected) {
        #if !IOT_WEATHER_STATION_WS2812_NUM
            BUILTIN_LED_SET(BlinkLEDTimer::BlinkType::FAST);
        #endif

        Logger_notice(F("WiFi disconnected, SSID %s, reason %s, had %sIP address"), event.ssid.c_str(), WiFi_disconnect_reason(event.reason), _wifiUp ? emptyString.c_str() : PSTR("no "));
        _wifiConnected = 0;
        _wifiUp = 0;
        LoopFunctions::callOnce([event]() {
            WiFiCallbacks::callEvent(WiFiCallbacks::EventType::DISCONNECTED, (void *)&event);
        });
        if (event.reason == WIFI_DISCONNECT_REASON_AUTH_EXPIRE) {
            config.reconfigureWiFi(F("Authentication expired, reconfiguring WiFi adapter"));
        }

    }
    #if defined(ESP32)
        else {
            //_onWiFiDisconnectCb 202 = AUTH_FAIL
            if (event.reason == 202) {
                __DBG_printf_E("force WiFi.begin()");
                WiFi.begin();
            }
        }
    #endif
}

void KFCFWConfiguration::_onWiFiGotIPCb(const WiFiEventStationModeGotIP &event)
{
    // auto ip = event.ip.toString();
    // auto mask = event.mask.toString();
    // auto gw = event.gw.toString();
    // auto dns1 = WiFi.dnsIP().toString();
    // auto dns2 = WiFi.dnsIP(1).toString();
    _wifiUp = millis();
    if (_wifiUp == 0) {
        _wifiUp++;
    }
    // __LDBG_printf("%s", ip.c_str(), mask.c_str(), gw.c_str(), dns1.c_str(), dns2.c_str(), _wifiConnected, _wifiUp, WiFi.isConnected(), WiFi.localIP().toString().c_str());

    auto networkCfg = Network::Settings::getConfig().stations[getWiFiConfigurationNum()];

    PrintString msg = networkCfg.isDHCPEnabled() ? F("DHCP") : F("Static configuration");
    msg.print(F(": IP/Net "));
    if (IPAddress_isValid(event.ip)) {
        event.ip.printTo(msg);
        if (IPAddress_isValid(event.mask)) {
            msg.print('/');
            event.mask.printTo(msg);
        }
    }
    if (IPAddress_isValid(event.gw)) {
        msg.print(F(" GW "));
        event.gw.printTo(msg);
    }
    msg.print(F(" DNS: "));
    if (IPAddress_isValid(WiFi.dnsIP(0))) {
        WiFi.dnsIP(0).printTo(msg);
    }
    if (IPAddress_isValid(WiFi.dnsIP(1))) {
        msg.print('/');
        WiFi.dnsIP(1).printTo(msg);
    }
    msg.printf_P(PSTR(" BSSID/Channel: %s/%u"), WiFi.BSSIDstr().c_str(), WiFi.channel());
    Logger_notice(msg);

    setWiFiConnectLedMode();

    #if ENABLE_DEEP_SLEEP
        config.storeStationConfig(event.ip, event.mask, event.gw);
    #endif
    LoopFunctions::callOnce([event]() {
        WiFiCallbacks::callEvent(WiFiCallbacks::EventType::CONNECTED, (void *)&event);
    });
}

void KFCFWConfiguration::setWiFiConnectLedMode()
{
    #if __LED_BUILTIN != IGNORE_BUILTIN_LED_PIN_ID
        if (config.isSafeMode()) {
            BUILTIN_LED_SET(BlinkLEDTimer::BlinkType::SOS);
        }
        else {
            auto mode = System::Device::getConfig().getStatusLedMode();
            if (mode != System::Device::StatusLEDModeType::OFF) {
                if (WiFi.isConnected()) {
                    #if !IOT_WEATHER_STATION_WS2812_NUM
                        BUILTIN_LED_SET(mode == System::Device::StatusLEDModeType::OFF_WHEN_CONNECTED ? BlinkLEDTimer::BlinkType::OFF : BlinkLEDTimer::BlinkType::SOLID);
                    #endif
                }
                else {
                    BUILTIN_LED_SET(BlinkLEDTimer::BlinkType::FAST);
                }
            }
        }
    #endif
}

void KFCFWConfiguration::_onWiFiOnDHCPTimeoutCb()
{
    __LDBG_printf("KFCFWConfiguration::_onWiFiOnDHCPTimeoutCb()");
    #if defined(ESP32)
        Logger_error(F("Lost DHCP IP"));
    #else
        Logger_error(F("DHCP timeout"));
    #endif
    BUILTIN_LED_SET(BlinkLEDTimer::BlinkType::FLICKER);
}

void KFCFWConfiguration::_softAPModeStationConnectedCb(const WiFiEventSoftAPModeStationConnected &event)
{
    __LDBG_printf("KFCFWConfiguration::_softAPModeStationConnectedCb()");
    Logger_notice(F("Station connected [%s]"), mac2String(event.mac).c_str());
}

void KFCFWConfiguration::_softAPModeStationDisconnectedCb(const WiFiEventSoftAPModeStationDisconnected &event)
{
    __LDBG_printf("KFCFWConfiguration::_softAPModeStationDisconnectedCb()");
    Logger_notice(F("Station disconnected [%s]"), mac2String(event.mac).c_str());
}

#if defined(ESP32)

    void KFCFWConfiguration::_onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info)
    {
        switch(event) {
            case WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_CONNECTED: {
                    WiFiEventStationModeConnected dst;
                    dst.ssid = reinterpret_cast<const char *>(info.wifi_sta_connected.ssid);
                    MEMNCPY_S(dst.bssid, info.wifi_sta_connected.bssid);
                    dst.channel = info.wifi_sta_connected.channel;
                    config._onWiFiConnectCb(dst);
                } break;
            case WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED: {
                    WiFiEventStationModeDisconnected dst;
                    dst.ssid = reinterpret_cast<const char *>(info.wifi_sta_disconnected.ssid);
                    MEMNCPY_S(dst.bssid, info.wifi_sta_disconnected.bssid);
                    dst.reason = (WiFiDisconnectReason)info.wifi_sta_disconnected.reason;
                    config._onWiFiDisconnectCb(dst);
                } break;
            #if DEBUG
                case WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP6: {
                        __DBG_printf("ARDUINO_EVENT_WIFI_STA_GOT_IP6 addr=%04X:%04X:%04X:%04X", info.got_ip6.ip6_info.ip.addr[0], info.got_ip6.ip6_info.ip.addr[1], info.got_ip6.ip6_info.ip.addr[2], info.got_ip6.ip6_info.ip.addr[3]);
                    } break;
            #endif
            case WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP: {
                    WiFiEventStationModeGotIP dst;
                    dst.ip = info.got_ip.ip_info.ip.addr;
                    dst.gw = info.got_ip.ip_info.gw.addr;
                    dst.mask = info.got_ip.ip_info.netmask.addr;
                    config._onWiFiGotIPCb(dst);
                } break;
            case WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_LOST_IP: {
                    config._onWiFiOnDHCPTimeoutCb();
                } break;
            case WiFiEvent_t::ARDUINO_EVENT_WIFI_AP_STACONNECTED: {
                    WiFiEventSoftAPModeStationConnected dst;
                    dst.aid = info.wifi_ap_staconnected.aid;
                    MEMNCPY_S(dst.mac, info.wifi_ap_staconnected.mac);
                    config._softAPModeStationConnectedCb(dst);
                } break;
            case WiFiEvent_t::ARDUINO_EVENT_WIFI_AP_STADISCONNECTED: {
                    WiFiEventSoftAPModeStationDisconnected dst;
                    dst.aid = info.wifi_ap_stadisconnected.aid;
                    MEMNCPY_S(dst.mac, info.wifi_ap_stadisconnected.mac);
                    config._softAPModeStationDisconnectedCb(dst);
                } break;
            default:
                break;
        }
    }

#elif USE_WIFI_SET_EVENT_HANDLER_CB

    // converter for System_Event_t

    struct WiFiEventStationModeGotIPEx : WiFiEventStationModeGotIP
    {
        WiFiEventStationModeGotIPEx(const IPAddress &_ip, const IPAddress &_mask, const IPAddress &_gw) : WiFiEventStationModeGotIP({_ip, _mask, _gw}) {}
    };

    struct WiFiEventStationModeConnectedEx : WiFiEventStationModeConnected {
        WiFiEventStationModeConnectedEx(const uint8_t *_ssid, size_t ssidLen, const uint8_t *_bssid, size_t bssidLen, uint8_t channel) : WiFiEventStationModeConnected({String(), {}, channel}) {
            ssid.concat(reinterpret_cast<const char *>(_ssid), ssidLen);
            memcpy_P(bssid, _bssid, std::min<size_t>(sizeof(bssid), bssidLen));
        }
    };

    struct WiFiEventStationModeDisconnectedEx : WiFiEventStationModeDisconnected {
        WiFiEventStationModeDisconnectedEx(const uint8_t *_ssid, size_t ssidLen, const uint8_t *_bssid, size_t bssidLen, uint8_t reason) : WiFiEventStationModeDisconnected({String(), {}, static_cast<WiFiDisconnectReason>(reason)}) {
            ssid.concat(reinterpret_cast<const char *>(_ssid), ssidLen);
            memcpy_P(bssid, _bssid, std::min<size_t>(sizeof(bssid), bssidLen));
        }
    };

    struct WiFiEventSoftAPModeStationDisconnectedEx : WiFiEventSoftAPModeStationDisconnected {
        WiFiEventSoftAPModeStationDisconnectedEx(const uint8_t *_mac, size_t macLen, uint8_t aid) : WiFiEventSoftAPModeStationDisconnected({{}, aid}) {
            memcpy_P(mac, _mac, std::min<size_t>(sizeof(mac), macLen));
        }
    };

    struct WiFiEventSoftAPModeStationConnectedEx : WiFiEventSoftAPModeStationConnected {
        WiFiEventSoftAPModeStationConnectedEx(uint8_t rssi, const uint8_t *_mac, size_t macLen) : WiFiEventSoftAPModeStationConnected({static_cast<uint8_t>(rssi), {}}) {
            memcpy_P(mac, _mac, std::min<size_t>(sizeof(mac), macLen));
        }
    };

    struct WiFiSystemEventEx : System_Event_t {

        operator WiFiEventStationModeConnected() const {
            auto &info = event_info.connected;
            return WiFiEventStationModeConnectedEx(info.ssid, sizeof(info.ssid), info.bssid, sizeof(info.bssid), info.channel);
        }
        operator WiFiEventStationModeDisconnected() const {
            auto &info = event_info.disconnected;
            return WiFiEventStationModeDisconnectedEx(info.ssid, info.ssid_len, info.bssid, sizeof(info.bssid), info.reason);
        }
        operator WiFiEventStationModeGotIP() const {
            auto &info = event_info.got_ip;
            return WiFiEventStationModeGotIPEx(info.ip.addr, info.mask.addr, info.gw.addr);
        }
        operator WiFiEventSoftAPModeStationConnected() const {
            auto &info = event_info.sta_connected;
            return WiFiEventSoftAPModeStationConnectedEx(info.aid, info.mac, sizeof(info.mac));
        }
        operator WiFiEventSoftAPModeStationDisconnected() const {
            auto &info = event_info.sta_disconnected;
            return WiFiEventSoftAPModeStationDisconnectedEx(info.mac, sizeof(info.mac), info.aid);
        }

        const __FlashStringHelper *eventToString() const {
            switch(event) {
                case EVENT_STAMODE_CONNECTED:
                    return F("STAMODE_CONNECTED");
                case EVENT_STAMODE_DISCONNECTED:
                    return F("STAMODE_DISCONNECTED");
                case EVENT_STAMODE_AUTHMODE_CHANGE:
                    return F("STAMODE_AUTHMODE_CHANGE");
                case EVENT_STAMODE_GOT_IP:
                    return F("STAMODE_GOT_IP");
                case EVENT_STAMODE_DHCP_TIMEOUT:
                    return F("STAMODE_DHCP_TIMEOUT");
                case EVENT_SOFTAPMODE_STACONNECTED:
                    return F("SOFTAPMODE_STACONNECTED");
                case EVENT_SOFTAPMODE_STADISCONNECTED:
                    return F("SOFTAPMODE_STADISCONNECTED");
                case EVENT_SOFTAPMODE_PROBEREQRECVED:
                    return F("SOFTAPMODE_PROBEREQRECVED");
                case EVENT_OPMODE_CHANGED:
                    return F("OPMODE_CHANGED");
                case EVENT_SOFTAPMODE_DISTRIBUTE_STA_IP:
                    return F("SOFTAPMODE_DISTRIBUTE_STA_IP");
                default:
                    break;
            }
            return F("UNKNOWN");
        }
    };

    void KFCFWConfiguration::_onWiFiEvent(System_Event_t *orgEventPtr)
    {
        auto &orgEvent = *reinterpret_cast<WiFiSystemEventEx *>(orgEventPtr);
        __LDBG_printf("event=%u(%s)", orgEvent.event, orgEvent.eventToString());
        switch(orgEvent.event) {
            case EVENT_STAMODE_CONNECTED:
                config._onWiFiConnectCb(orgEvent);
                break;
            case EVENT_STAMODE_DISCONNECTED:
                config._onWiFiDisconnectCb(orgEvent);
                break;
            case EVENT_STAMODE_GOT_IP:
                config._onWiFiGotIPCb(orgEvent);
                break;
            case EVENT_STAMODE_DHCP_TIMEOUT:
                config._onWiFiOnDHCPTimeoutCb();
                break;
            case EVENT_SOFTAPMODE_STACONNECTED:
                config._softAPModeStationConnectedCb(orgEvent);
                break;
            case EVENT_SOFTAPMODE_STADISCONNECTED:
                config._softAPModeStationDisconnectedCb(orgEvent);
                break;
        }
    }

#else

    static void __onWiFiConnectCb(const WiFiEventStationModeConnected &event)
    {
        config._onWiFiConnectCb(event);
    }

    static void __onWiFiGotIPCb(const WiFiEventStationModeGotIP &event)
    {
        config._onWiFiGotIPCb(event);
    }

    static void __onWiFiDisconnectCb(const WiFiEventStationModeDisconnected& event)
    {
        config._onWiFiDisconnectCb(event);
    }

    static void __onWiFiOnDHCPTimeoutCb()
    {
        config._onWiFiOnDHCPTimeoutCb();
    }

    static void __softAPModeStationConnectedCb(const WiFiEventSoftAPModeStationConnected &event)
    {
        config._softAPModeStationConnectedCb(event);
    }

    static void __softAPModeStationDisconnectedCb(const WiFiEventSoftAPModeStationDisconnected &event)
    {
        config._softAPModeStationDisconnectedCb(event);
    }

#endif

void KFCFWConfiguration::_setupWiFiCallbacks()
{
    #if defined(ESP32)
        WiFi.onEvent(_onWiFiEvent, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_CONNECTED);
        WiFi.onEvent(_onWiFiEvent, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
        WiFi.onEvent(_onWiFiEvent, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);
        #if DEBUG
            WiFi.onEvent(_onWiFiEvent, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP6);
        #endif
        WiFi.onEvent(_onWiFiEvent, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_LOST_IP);
        WiFi.onEvent(_onWiFiEvent, WiFiEvent_t::ARDUINO_EVENT_WIFI_AP_STACONNECTED);
        WiFi.onEvent(_onWiFiEvent, WiFiEvent_t::ARDUINO_EVENT_WIFI_AP_STADISCONNECTED);

    #elif USE_WIFI_SET_EVENT_HANDLER_CB

        wifi_set_event_handler_cb(&KFCFWConfiguration::_onWiFiEvent);

    #else
        _onWiFiConnect = WiFi.onStationModeConnected(__onWiFiConnectCb); // using a lambda function costs 24 byte RAM per handler
        _onWiFiGotIP = WiFi.onStationModeGotIP(__onWiFiGotIPCb);
        _onWiFiDisconnect = WiFi.onStationModeDisconnected(__onWiFiDisconnectCb);
        _onWiFiOnDHCPTimeout = WiFi.onStationModeDHCPTimeout(__onWiFiOnDHCPTimeoutCb);
        _softAPModeStationConnected = WiFi.onSoftAPModeStationConnected(__softAPModeStationConnectedCb);
        _softAPModeStationDisconnected = WiFi.onSoftAPModeStationDisconnected(__softAPModeStationDisconnectedCb);
    #endif
}

void KFCFWConfiguration::recoveryMode(bool resetPasswords)
{
    if (resetPasswords) {
        System::Device::setPassword(SPGM(defaultPassword));
        Network::WiFi::setSoftApPassword(SPGM(defaultPassword));
    }
    if (!System::Device::getPassword() || !*System::Device::getPassword()) {
        System::Device::setPassword(SPGM(defaultPassword));
    }
    if (!Network::WiFi::getSoftApPassword() || !*Network::WiFi::getSoftApPassword()) {
        Network::WiFi::setSoftApPassword(SPGM(defaultPassword));
    }
    Network::WiFi::setSoftApSSID(defaultDeviceName());
    auto &flags = System::Flags::getWriteableConfig();
    flags.is_softap_ssid_hidden = false;
    flags.is_softap_dhcpd_enabled = true;
    #if IOT_REMOTE_CONTROL
        flags.is_softap_standby_mode_enabled = false;
        flags.is_softap_enabled = false;
    #else
        flags.is_softap_standby_mode_enabled = true;
        flags.is_softap_enabled = false;
        KFC_SAFE_MODE_SERIAL_PORT.printf_P(PSTR("Recovery mode SSID %s\n"), Network::WiFi::getSoftApSSID());
    #endif
    flags.is_web_server_enabled = true;
}

String KFCFWConfiguration::defaultDeviceName()
{
    uint8_t mac[WL_MAC_ADDR_LENGTH];
    WiFi.macAddress(mac);
    return PrintString(F("KFC%02X%02X%02X"), mac[3], mac[4], mac[5]);
}

// TODO add option to keep WiFi SSID, username and password

void KFCFWConfiguration::restoreFactorySettings()
{
    PrintString str;

    Logger_security(F("Restoring factory settings"));

    #undef __DBG_CALL
    #define __DBG_CALL(...) \
        __VA_ARGS__;

    #if ESP8266
        struct station_config config = {};
        wifi_station_set_config(&config);
        wifi_station_set_config_current(&config);
    #endif

    __DBG_CALL(SaveCrash::removeCrashCounterAndSafeMode());
    __DBG_CALL(RTCMemoryManager::clear());

    __DBG_CALL(erase());
    __DBG_CALL(clear());
    __DBG_CALL((SaveCrash::clearStorage(SaveCrash::ClearStorageType::REMOVE_MAGIC));

    __DBG_CALL(auto deviceName = defaultDeviceName()));
    __DBG_CALL(System::Flags::defaults());
    __DBG_CALL(System::Firmware::defaults());
    __DBG_CALL(System::Device::defaults(true));
    __DBG_CALL(System::Device::setName(deviceName));
    // __DBG_CALL(System::Device::setTitle(FSPGM(KFC_Firmware, "KFC Firmware")));
    __DBG_CALL(System::Device::setPassword(FSPGM(defaultPassword, "12345678")));
    __DBG_CALL(System::WebServer::defaults());
    __DBG_CALL(Network::WiFi::setSSID0(deviceName));
    __DBG_CALL(Network::WiFi::setPassword0(FSPGM(defaultPassword)));
    __DBG_CALL(Network::WiFi::setSoftApSSID(deviceName));
    __DBG_CALL(Network::WiFi::setSoftApPassword(FSPGM(defaultPassword)));
    __DBG_CALL(Network::Settings::defaults());
    __DBG_CALL(Network::SoftAP::defaults());

    using Plugins = KFCConfigurationClasses::PluginsType;

    #if MQTT_SUPPORT
        __DBG_CALL(Plugins::MqttClient::defaults());
    #endif
    #if IOT_REMOTE_CONTROL
        __DBG_CALL(Plugins::RemoteControl::defaults());
    #endif
    #if SERIAL2TCP_SUPPORT
        __DBG_CALL(Plugins::Serial2TCP::defaults());
    #endif
    #if SYSLOG_SUPPORT
        __DBG_CALL(Plugins::SyslogClient::defaults());
    #endif
    #if NTP_CLIENT
        __DBG_CALL(Plugins::NTPClient::defaults());
    #endif
    #if IOT_ALARM_PLUGIN_ENABLED
        __DBG_CALL(Plugins::Alarm::defaults());
    #endif
    #if IOT_WEATHER_STATION
        __DBG_CALL(Plugins::WeatherStation::defaults());
    #endif
    #if IOT_SENSOR
        __DBG_CALL(Plugins::Sensor::defaults());
    #endif
    #if IOT_BLINDS_CTRL
        __DBG_CALL(Plugins::Blinds::defaults());
    #endif
    #if PING_MONITOR_SUPPORT
        __DBG_CALL(Plugins::Ping::defaults());
    #endif
    #if IOT_DIMMER_MODULE || IOT_ATOMIC_SUN_V2
        __DBG_CALL(Plugins::Dimmer::defaults());
    #endif
    #if IOT_CLOCK
        __DBG_CALL(Plugins::Clock::defaults());
    #endif

    #if CUSTOM_CONFIG_PRESET
        __DBG_CALL(customSettings());
    #endif
    Logger_security(F("Factory settings restored (flash write pending, see error log for details...)"));

    #if SECURITY_LOGIN_ATTEMPTS
        __DBG_CALL(KFCFS.remove(FSPGM(login_failure_file)));
    #endif
}

#if CUSTOM_CONFIG_PRESET
    // set CUSTOM_CONFIG_PRESET to 0 if no custom config is being used
    #include "retracted/custom_config.h"
#endif

void KFCFWConfiguration::gc()
{
    if (_readAccess && millis() > _readAccess + _garbageCollectionCycleDelay) {
        // _debug_println(F("KFCFWConfiguration::gc(): releasing memory"));
        release();
    }
}

void KFCFWConfiguration::setConfigDirty(bool dirty)
{
    _dirty = dirty;
}

bool KFCFWConfiguration::isConfigDirty() const
{
    return _dirty;
}

const char __compile_date__[] PROGMEM = { __DATE__ " " __TIME__ };

const String KFCFWConfiguration::getFirmwareVersion()
{
    #if DEBUG
        #if ESP32
            return getShortFirmwareVersion() + F("-" ARDUINO_ESP32_RELEASE " " ) + FPSTR(__compile_date__);
        #elif ESP8266
            return getShortFirmwareVersion() + PrintString(F("-" ARDUINO_ESP8266_RELEASE " "), ARDUINO_ESP8266_GIT_VER) + FPSTR(__compile_date__);
        #else
            return getShortFirmwareVersion() + ' ' + FPSTR(__compile_date__);
        #endif
    #else
        return getShortFirmwareVersion() + ' ' + FPSTR(__compile_date__);
    #endif
}

const __FlashStringHelper *KFCFWConfiguration::getShortFirmwareVersion_P()
{
    return F(FIRMWARE_VERSION_STR " Build " __BUILD_NUMBER);
}

const String KFCFWConfiguration::getShortFirmwareVersion()
{
    return getShortFirmwareVersion_P();
}

#if ENABLE_DEEP_SLEEP

    void KFCFWConfiguration::storeQuickConnect(const uint8_t *bssid, int8_t channel)
    {
        __LDBG_printf("bssid=%s channel=%d", mac2String(bssid).c_str(), channel);

        using namespace DeepSleep;

        WiFiQuickConnect quickConnect;
        if (!RTCMemoryManager::read(RTCMemoryManager::RTCMemoryId::QUICK_CONNECT, &quickConnect, sizeof(quickConnect))) {
            quickConnect = WiFiQuickConnect();
        }
        quickConnect.channel = channel;
        quickConnect.bssid = bssid;
        RTCMemoryManager::write(RTCMemoryManager::RTCMemoryId::QUICK_CONNECT, &quickConnect, sizeof(quickConnect));
    }

    void KFCFWConfiguration::storeStationConfig(uint32_t ip, uint32_t netmask, uint32_t gateway)
    {
        __LDBG_printf("ip=%s(%s) netmask=%s gw=%s bssid=%s",
            IPAddress(ip).toString().c_str(),
            WiFi.localIP().toString().c_str(),
            IPAddress(netmask).toString().c_str(),
            IPAddress(gateway).toString().c_str(),
            WiFi.BSSIDstr().c_str()
        );

        using namespace DeepSleep;

        auto quickConnect = WiFiQuickConnect();
        if (RTCMemoryManager::read(RTCMemoryManager::RTCMemoryId::QUICK_CONNECT, &quickConnect, sizeof(quickConnect))) {
            auto tmp = quickConnect;
            quickConnect.local_ip = ip;
            quickConnect.subnet = netmask;
            quickConnect.gateway = gateway;
            quickConnect.dns1 = WiFi.dnsIP();
            quickConnect.dns2 = WiFi.dnsIP(1);
            quickConnect.bssid = WiFi.BSSID();
            auto flags = System::Flags::getConfig();
            quickConnect.use_static_ip = flags.use_static_ip_during_wakeup;
            RTCMemoryManager::write(RTCMemoryManager::RTCMemoryId::QUICK_CONNECT, &quickConnect, sizeof(quickConnect));

            #if ENABLE_DEEP_SLEEP
                // check for changes before storing in EEPROM
                if (memcmp(&tmp, &quickConnect, sizeof(tmp)) != 0) {
                    __LDBG_printf("update persistent config BSSID=%s ip=%s", mac2String(quickConnect.bssid).c_str(), IPAddress(ip).toString().c_str());
                    struct station_config config;
                    wifi_station_get_config(&config);
                    wifi_station_set_config(&config);
                }
            #endif
        }
        else {
            __DBG_print("reading RTC memory failed");
        }
    }

#endif

void KFCFWConfiguration::setup()
{
    String version = KFCFWConfiguration::getFirmwareVersion();
    #if ENABLE_DEEP_SLEEP
        if (!resetDetector.hasWakeUpDetected())
    #endif
    {
        Logger_notice(F("Starting KFCFW %s"), version.c_str());
        #if IOT_WEATHER_STATION_WS2812_NUM
            WeatherStationPlugin::_getInstance()._rainbowStatusLED();
        #else
            BUILTIN_LED_SET(BlinkLEDTimer::BlinkType::FLICKER);
        #endif
    }

    LOOP_FUNCTION_ADD(KFCFWConfiguration::loop);
}

#include "build.h"

// save_crash.h
SaveCrash::Data::FirmwareVersion::FirmwareVersion() :
    major(FIRMWARE_VERSION_MAJOR),
    minor(FIRMWARE_VERSION_MINOR),
    revision(FIRMWARE_VERSION_REVISION),
    build(__BUILD_NUMBER_INT)
{
}

String SaveCrash::Data::FirmwareVersion::toString(const __FlashStringHelper *buildSep) const
{
    return PrintString(F("%u.%u.%u%s%u"), major, minor, revision, buildSep, build);
}

void SaveCrash::Data::FirmwareVersion::printTo(Print &output, const __FlashStringHelper *buildSep) const
{
    output.printf_P(PSTR("%u.%u.%u%s%u"), major, minor, revision, buildSep, build);
}


void KFCFWConfiguration::read(bool wakeup)
{
    if (!Configuration::read()) {
        Logger_error(F("Failed to read configuration, restoring factory settings"));
        config.restoreFactorySettings();
        Configuration::write();
    }
    else if (wakeup == false) {
        auto version = System::Device::getConfig().config_version;
        auto currentVersion = SaveCrash::Data::FirmwareVersion().__version;
        // uint32_t currentVersion = (FIRMWARE_VERSION << 16) | (__BUILD_NUMBER_INT & 0xffff);
        if (currentVersion != version) {

            // auto build = static_cast<uint16_t>(version);
            // version >>= 16;
            // Logger_warning(F("Upgrading EEPROM settings from %d.%d.%d.%u to " FIRMWARE_VERSION_STR "." __BUILD_NUMBER), (version >> 11) & 0x1f, (version >> 6) & 0x1f, (version & 0x3f), build);
            Logger_warning(F("Upgrading EEPROM settings from %s to " FIRMWARE_VERSION_STR "." __BUILD_NUMBER), SaveCrash::Data::FirmwareVersion(version).toString(F(".")).c_str());
            System::Device::getWriteableConfig().config_version = currentVersion;
            config.recoveryMode(false);
            System::Firmware::setMD5(ESP.getSketchMD5());
            Configuration::write();
            // SaveCrash::clearStorage(SaveCrash::ClearStorageType::REMOVE_MAGIC);
        }
    }
}

void KFCFWConfiguration::write()
{
    auto result = Configuration::write();
    if (result != Configuration::WriteResultType::SUCCESS) {
        Logger_error(F("Failed to write settings to EEPROM. %s"), Configuration::getWriteResultTypeStr(result));
    }
}

#if defined(HAVE_NVS_FLASH)

    void KFCFWConfiguration::formatNVS()
    {
        #ifdef SECTION_NVS2_START_ADDRESS
            nvs_flash_deinit_partition("nvs2");
            nvs_flash_erase_partition("nvs2");
        #else
            nvs_flash_deinit();
            nvs_flash_erase();
        #endif
    }

#endif

#if ESP8266

    const __FlashStringHelper *KFCFWConfiguration::getSleepTypeStr(sleep_type_t type)
    {
        switch(type) {
            case sleep_type_t::NONE_SLEEP_T:
                return F("None");
            case sleep_type_t::LIGHT_SLEEP_T:
                return F("Light");
            case sleep_type_t::MODEM_SLEEP_T:
                return F("Modem");
            default:
                break;
        }
        return F("Unknown");
    }

    const __FlashStringHelper *KFCFWConfiguration::getWiFiOpModeStr(uint8_t mode)
    {
        switch(mode) {
            case WiFiMode_t::WIFI_STA:
                return F("Station mode");
            case WiFiMode_t::WIFI_AP:
                return F("AP mode");
            case WiFiMode_t::WIFI_AP_STA:
                return F("AP and station mode");
            case WiFiMode_t::WIFI_OFF:
                return F("Off");
            default:
                break;
        }
        return F("Unknown");
    }

    const __FlashStringHelper *KFCFWConfiguration::getWiFiPhyModeStr(phy_mode_t mode)
    {
        switch(mode) {
            case phy_mode_t::PHY_MODE_11B:
                return F("802.11b");
            case phy_mode_t::PHY_MODE_11G:
                return F("802.11g");
            case phy_mode_t::PHY_MODE_11N:
                return F("802.11n");
            default:
                break;
        }
        return F("");
    }

#endif

void KFCFWConfiguration::printDiag(Print &output, const String &prefix)
{
    #if 1
        WiFi.printDiag(output);
    #elif ESP8266
        station_config wifiConfig;
        wifi_station_get_config_default(&wifiConfig);
        output.print(prefix);
        output.printf_P(PSTR("default cfg ssid=%.32s password=%.64s bssid_set=%u bssid=%s mode=%s\n"), wifiConfig.ssid, wifiConfig.password, wifiConfig.bssid_set, mac2String(wifiConfig.bssid).c_str(), KFCFWConfiguration::getWiFiOpModeStr(wifi_get_opmode_default()));
        wifi_station_get_config(&wifiConfig);
        output.print(prefix);
        output.printf_P(PSTR("config ssid=%.32s password=%.64s bssid_set=%u bssid=%s mode=%s\n"), wifiConfig.ssid, wifiConfig.password, wifiConfig.bssid_set, mac2String(wifiConfig.bssid).c_str(), KFCFWConfiguration::getWiFiOpModeStr(wifi_get_opmode()));
        output.print(prefix);
        output.printf_P(PSTR("sleep=%s phy=%s channel=%u AP_id=%u auto_connect=%u reconnect=%u persistent=%u\n"), KFCFWConfiguration::getSleepTypeStr(wifi_get_sleep_type()), KFCFWConfiguration::getWiFiPhyModeStr(wifi_get_phy_mode()), wifi_get_channel(), wifi_station_get_current_ap_id(), wifi_station_get_auto_connect(), wifi_station_get_reconnect_policy(), WiFi.getPersistent());

        auto lastError = config.getLastErrorStr();
        if (lastError.length()) {
            output.print(prefix);
            output.printf_P(PSTR("last_error=%s\n"), lastError.c_str());
        }

        if ((WiFi.getMode() & WiFiMode_t::WIFI_STA)) {
            if (WiFi.isConnected()) {
                output.print(prefix);
                output.printf_P(PSTR("station_ip=%s gw=%s dns=%s, %s bssid=%s\n"), WiFi.localIP().toString().c_str(), WiFi.gatewayIP().toString().c_str(), WiFi.dnsIP(0).toString().c_str(), WiFi.dnsIP(1).toString().c_str(), WiFi.BSSIDstr().c_str());
            }
            else {
                output.print(prefix);
                output.println(F("station not connected"));
            }
        }

        if ((WiFi.getMode() & WiFiMode_t::WIFI_AP)) {
            output.print(prefix);
            output.printf_P(PSTR("softap_ip=%s ssid=%s clients=%u\n"), WiFi.softAPIP().toString().c_str(), WiFi.softAPSSID().c_str(), WiFi.softAPgetStationNum());
        }
    #elif ESP32
        WiFi.printDiag(output);
    #endif
}

bool KFCFWConfiguration::resolveZeroConf(const String &name, const String &hostname, uint16_t port, MDNSResolver::ResolvedCallback callback) const
{
    __LDBG_printf("resolveZeroConf=%s port=%u", hostname.c_str(), port);
    String prefix, suffix;
    auto start = hostname.indexOf(FSPGM(_var_zeroconf));
    if (start != -1) {
        start += 11;
        auto end = hostname.indexOf('}', start);
        if (end != -1) {
            auto serviceEnd = hostname.indexOf('.');
            auto protoEnd = hostname.indexOf(',');
            auto valuesEnd = hostname.indexOf('|');
            __LDBG_printf("start=%d end=%d service_end=%d proto_end=%d name_end=%d", start, end, serviceEnd, protoEnd, valuesEnd);
            if (serviceEnd != -1 && protoEnd != -1 && serviceEnd < protoEnd && (valuesEnd == -1 || protoEnd < valuesEnd)) {
                auto service = hostname.substring(start, serviceEnd);
                auto proto = hostname.substring(serviceEnd + 1, protoEnd);
                prefix = hostname.substring(0, start - 11);
                suffix = hostname.substring(end + 1);
                String addressValue;
                String portValue;
                String defaultValue;
                int portPos;
                if (valuesEnd == -1) {
                    addressValue = hostname.substring(protoEnd + 1, end);
                }
                else {
                    addressValue = hostname.substring(protoEnd + 1, valuesEnd);
                    defaultValue = hostname.substring(valuesEnd + 1, end);
                    if ((portPos = defaultValue.indexOf(':')) != -1) {
                        port = defaultValue.substring(portPos + 1).toInt();
                        defaultValue.remove(portPos);
                    }
                }
                if ((portPos = addressValue.indexOf(':')) != -1) {
                    portValue = addressValue.substring(portPos + 1);
                    addressValue.remove(portPos);
                }
                else {
                    portValue = FSPGM(port);
                }
                if (service.charAt(0) != '_') {
                    service = String('_') + service;
                }
                if (proto.charAt(0) != '_') {
                    proto = String('_') + proto;
                }

                __LDBG_printf("service=%s proto=%s values=%s:%s default=%s port=%d", service.c_str(), proto.c_str(), addressValue.c_str(), portValue.c_str(), defaultValue.c_str(), port);

                auto mdns = PluginComponent::getPlugin<MDNSPlugin>(F("mdns"), true);
                if (!mdns) {
                    Logger_error(F("Cannot resolve Zeroconf, MDNS plugin not loaded: %s"), hostname.c_str());
                    LoopFunctions::callOnce([defaultValue, port, callback]() {
                        callback(defaultValue, IPAddress(), port, defaultValue, MDNSResolver::ResponseType::TIMEOUT);
                    });
                    return true;
                }

                mdns->resolveZeroConf(new MDNSResolver::Query(name, service, proto, addressValue, portValue, defaultValue, port, prefix, suffix, callback, System::Device::getConfig().zeroconf_timeout));
                return true;

            }
        }
    }
    return false;
}

bool KFCFWConfiguration::hasZeroConf(const String &hostname) const
{
    auto pos = hostname.indexOf(FSPGM(_var_zeroconf));
    if (pos != -1) {
        pos = hostname.indexOf('}', pos + 11);
    }
    __LDBG_printf("has_zero_conf=%s result=%d", hostname.c_str(), pos);
    return pos != -1;
}

#if WIFI_QUICK_CONNECT_MANUAL

    void KFCFWConfiguration::wifiQuickConnect()
    {
        __DBG_printf("quick connect");
        #if ENABLE_DEEP_SLEEP

            #if defined(ESP32)
                WiFi.mode(WIFI_STA); // needs to be called to initialize wifi
                wifi_config_t _config;
                wifi_sta_config_t &config = _config.sta;
                if (esp_wifi_get_config(ESP_IF_WIFI_STA, &_config) == ESP_OK) {
            #elif defined(ESP8266)

                WiFi.persistent(false);
                struct station_config config;
                if (wifi_station_get_config_default(&config)) {
            #endif
                    int32_t channel;
                    uint8_t *bssidPtr;
                    DeepSleep::WiFiQuickConnect quickConnect;

                    // WiFi.enableSTA(true);
                    WiFi.mode(WIFI_STA);

                    if (RTCMemoryManager::read(PluginComponent::RTCMemoryId::QUICK_CONNECT, &quickConnect, sizeof(quickConnect))) {
                        channel = quickConnect.channel;
                        bssidPtr = quickConnect.bssid;
                    }
                    else {
                        quickConnect = {};
                        channel = 0;
                        // bssidPtr = nullptr;
                        __DBG_print("error reading quick connect from rtc memory");
                    }

                    if (channel <= 0 || !bssidPtr) {

                        __DBG_printf("Cannot read quick connect from RTC memory, running WiFi.begin(%s, ***) only", config.ssid);
                        if (WiFi.begin(reinterpret_cast<char *>(config.ssid), reinterpret_cast<char *>(config.password)) != WL_DISCONNECTED) {
                            Logger_error(F("Failed to start WiFi"));
                        }

                    }
                    else {

                        if (quickConnect.use_static_ip && quickConnect.local_ip) {
                            __DBG_printf("configuring static ip");
                            WiFi.config(quickConnect.local_ip, quickConnect.gateway, quickConnect.subnet, quickConnect.dns1, quickConnect.dns2);
                        }

                        wl_status_t result;
                        if ((result = WiFi.begin(reinterpret_cast<char *>(config.ssid), reinterpret_cast<char *>(config.password), channel, bssidPtr, true)) != WL_DISCONNECTED) {
                            __DBG_printf("Failed to start WiFi");
                            Logger_error(F("Failed to start WiFi"));
                        }

                        __DBG_printf("WiFi.begin() = %d, ssid %.32s, channel %d, bssid %s, config: static ip %d, %s/%s gateway %s, dns %s, %s",
                            result,
                            config.ssid,
                            channel,
                            mac2String(bssidPtr).c_str(),
                            quickConnect.use_static_ip ? 1 : 0,
                            __S(IPAddress(quickConnect.local_ip)),
                            inet_nto_cstr(quickConnect.gateway),
                            inet_nto_cstr(quickConnect.subnet),
                            inet_nto_cstr(quickConnect.dns1),
                            inet_nto_cstr(quickConnect.dns2)
                        );

                    }

                }
                else {
                    Logger_error(F("Failed to load WiFi configuration"));
                }
        #endif
    }

#endif

#if ENABLE_DEEP_SLEEP

    void KFCFWConfiguration::enterDeepSleep(milliseconds time, RFMode mode, uint16_t delayAfterPrepare)
    {
        __LDBG_printf("time=%d mode=%d delay_prep=%d", time.count(), mode, delayAfterPrepare);

        // WiFiCallbacks::getVector().clear(); // disable WiFi callbacks to speed up shutdown
        // Scheduler.terminate(); // halt scheduler

        //resetDetector.clearCounter();
        // SaveCrash::removeCrashCounter();

        delay(5);

        for(auto plugin: PluginComponents::Register::getPlugins()) {
            plugin->prepareDeepSleep(time.count());
        }
        if (delayAfterPrepare) {
            delay(delayAfterPrepare);
        }
        __LDBG_printf("Entering deep sleep for %u milliseconds, RF mode %d", time.count(), mode);

        #if __LED_BUILTIN == NEOPIXEL_PIN_ID
            BUILTIN_LED_SET(BlinkLEDTimer::BlinkType::OFF);
        #endif

        DeepSleep::DeepSleepParam::enterDeepSleep(time, mode);
    }

#endif

#if IOT_LED_MATRIX_OUTPUT_PIN
    extern "C" void ClockPluginClearPixels();
#endif

static void invoke_ESP_restart()
{
    #if IOT_LED_MATRIX_OUTPUT_PIN
        #ifndef ESP32
            ClockPluginClearPixels();
        #endif
    #endif

    BUILTIN_LED_SET(BlinkLEDTimer::BlinkType::OFF);
    #if BUILTIN_LED_NEOPIXEL
        WS2812LEDTimer::terminate();
    #endif

    __Scheduler.end();
    ETSTimerEx::end();

    #if IOT_SWITCH && IOT_SWITCH_STORE_STATES_RTC_MEM
        SwitchPlugin::_rtcMemStoreState();
    #endif

    #if RTC_SUPPORT == 0
        RTCMemoryManager::storeTime();
    #endif

    ESP.restart();
}

#if PIN_MONITOR
#    include "pin_monitor.h"
#endif

void KFCFWConfiguration::resetDevice(bool safeMode)
{
    String msg = F("Device is being reset");
    if (safeMode) {
        msg += F(" in SAFE MODE");
    }
    Logger_notice(msg);
    delay(500);

    SaveCrash::removeCrashCounterAndSafeMode();
    resetDetector.setSafeMode(safeMode);
    #if ENABLE_DEEP_SLEEP
        DeepSleep::DeepSleepParam::reset();
    #endif
    invoke_ESP_restart();
}

// enable debugging output (::printf) for shutdown sequence
#define DEBUG_SHUTDOWN_SEQUENCE DEBUG
#if DEBUG_SHUTDOWN_SEQUENCE
#    define _DPRINTF(msg, ...) ::printf(PSTR(msg "\n"), ##__VA_ARGS__)
#else
#    define _DPRINTF(...)
#endif

extern bool is_at_mode_enabled;

void KFCFWConfiguration::restartDevice(bool safeMode)
{
    __LDBG_println();
    is_at_mode_enabled = false;

    String msg = F("Device is being restarted");
    if (safeMode) {
        msg += F(" in SAFE MODE");
    }
    Logger_notice(msg);
    BUILTIN_LED_SET(BlinkLEDTimer::BlinkType::FLICKER);

    delay(500);

    SaveCrash::removeCrashCounterAndSafeMode();
    resetDetector.setSafeMode(safeMode);
    #if ENABLE_DEEP_SLEEP
        DeepSleep::DeepSleepParam::reset();
    #endif
    if (_safeMode) {
        #if DEBUG_SHUTDOWN_SEQUENCE
            _DPRINTF("safe mode: invoking restart");
        #endif
        invoke_ESP_restart();
    }

    // clear queue silently
    ADCManager::terminate(false);

    #if HTTP2SERIAL_SUPPORT
        _DPRINTF("terminating http2serial instance");
        Http2Serial::destroyInstance();
    #endif

    _DPRINTF("scheduled tasks %u, WiFi callbacks %u, Loop Functions %u", _Scheduler.size(), WiFiCallbacks::getVector().size(), LoopFunctions::size());

    auto webUiSocket = WebUISocket::getServerSocket();
    if (webUiSocket) {
        _DPRINTF("terminating webui websocket=%p", webUiSocket);
        webUiSocket->shutdown();
    }

    // execute in reverse order
    auto &plugins = PluginComponents::RegisterEx::getPlugins();
    for(auto iterator = plugins.rbegin(); iterator != plugins.rend(); ++iterator) {
        const auto plugin = *iterator;
        _DPRINTF("shutdown plugin=%s", plugin->getName_P());
        plugin->shutdown();
        plugin->clearSetupTime();
    }

    _DPRINTF("scheduled tasks %u, WiFi callbacks %u, Loop Functions %u", _Scheduler.size(), WiFiCallbacks::getVector().size(), LoopFunctions::size());

    #if PIN_MONITOR
        _DPRINTF("terminating pin monitor");
        PinMonitor::pinMonitor.end();
    #endif

    _DPRINTF("terminating serial handlers");
    serialHandler.end();
    #if DEBUG
        debugStreamWrapper.clear();
    #endif

    #if DEBUG_SHUTDOWN_SEQUENCE
        _DPRINTF("clearing wifi callbacks");
    #endif
    WiFiCallbacks::clear();

    #if DEBUG_SHUTDOWN_SEQUENCE
        _DPRINTF("clearing loop functions");
    #endif
    LoopFunctions::clear();

    #if DEBUG_SHUTDOWN_SEQUENCE
        _DPRINTF("invoking restart");
    #endif
    invoke_ESP_restart();
}

void KFCFWConfiguration::loop()
{
    config.gc();
}

uint8_t KFCFWConfiguration::getMaxWiFiChannels()
{
    wifi_country_t country;
    if (!wifi_get_country(&country)) {
        country.nchan = 255;
    }
    return country.nchan;
}

const __FlashStringHelper *KFCFWConfiguration::getWiFiEncryptionType(uint8_t type)
{
    #if defined(ESP32)
        switch(type) {
            case WIFI_AUTH_OPEN:
                return F("open");
            case WIFI_AUTH_WEP:
                return F("WEP");
            case WIFI_AUTH_WPA_PSK:
                return F("WPA/PSK");
            case WIFI_AUTH_WPA2_PSK:
                return F("WPA2/PSK");
            case WIFI_AUTH_WPA_WPA2_PSK:
                return F("WPA/WPA2/PSK");
            case WIFI_AUTH_WPA2_ENTERPRISE:
                return F("WPA2/ENTERPRISE");
        }
    #elif defined(ESP8266)
        switch(type) {
            case ENC_TYPE_NONE:
                return F("open");
            case ENC_TYPE_WEP:
                return F("WEP");
            case ENC_TYPE_TKIP:
                return F("WPA/PSK");
            case ENC_TYPE_CCMP:
                return F("WPA2/PSK");
            case ENC_TYPE_AUTO:
                return F("auto");
        }
    #endif
    return F("N/A");
}

bool KFCFWConfiguration::reconfigureWiFi(const __FlashStringHelper *msg, uint8_t configNum)
{
    __DBG_printf("re-config=%u", configNum);
    if (msg) {
        Logger_notice(msg);
        #if DEBUG_KFC_CONFIG
            __DBG_printf("WiFi diagnostics");
            WiFi.printDiag(DEBUG_OUTPUT);
        #endif
    }
    WiFi.persistent(false); // disable storing WiFi config
    WiFi.setAutoConnect(false);
    WiFi.setAutoReconnect(false);
    WiFi.disconnect(true);
    WiFi.softAPdisconnect(true);

    return connectWiFi(configNum);
}

bool KFCFWConfiguration::connectWiFi(uint8_t configNum, bool ignoreSoftAP)
{
    __LDBG_printf("config=%u ign_softap=%u", configNum, ignoreSoftAP);
    setLastError(String());
    if (configNum != kKeepWiFiNetwork) {
        _wifiNumActive = configNum;
    }

    bool station_mode_success = false;
    bool ap_mode_success = false;

    #if ESP32
        auto hostname = System::Device::getName();
        WiFi.setHostname(hostname);
        WiFi.softAPsetHostname(hostname);
    #endif

    auto flags = System::Flags::getConfig();
    if (flags.is_station_mode_enabled) {
        __LDBG_printf("init station mode");
        WiFi.setAutoConnect(false); // WiFi callbacks have to be installed first during boot
        WiFi.setAutoReconnect(true);
        WiFi.enableSTA(true);

        auto wifiId = getWiFiConfigurationNum();
        wifiId = scanWifiStrength(wifiId);

        auto network = Network::WiFi::getNetworkConfigOrdered(wifiId);
        bool result;
        if (network.isDHCPEnabled()) {
            result = WiFi.config(0U, 0U, 0U, 0U, 0U);
        }
        else {
            result = WiFi.config(network.getLocalIp(), network.getGateway(), network.getSubnet(), network.getDns1(), network.getDns2());
        }
        if (!result) {
            setLastError(PrintString(F("Failed to configure Station Mode with %s"), network.isDHCPEnabled() ? PSTR("DHCP") : PSTR("static address")));
            Logger_error(F("%s"), getLastError());
        }
        else {
            if (WiFi.begin(Network::WiFi::getSSID(wifiId), Network::WiFi::getPassword(wifiId)) == WL_CONNECT_FAILED) {
                setLastError(F("Failed to start Station Mode"));
                Logger_error(F("%s"), getLastError());
            }
            else {
                __LDBG_printf("Station Mode SSID %s", Network::WiFi::getSSID(wifiId));
                station_mode_success = true;
            }
        }
    }
    else {
        __DBG_printf("disabling station mode");
        WiFi.disconnect(true);
        WiFi.enableSTA(false);
        station_mode_success = true;
    }

    if (!ignoreSoftAP) {

        if (flags.is_softap_enabled) {
            __LDBG_printf("init AP mode");

            WiFi.enableAP(true);

            auto softAp = Network::SoftAP::getConfig();
            if (!WiFi.softAPConfig(softAp.getAddress(), softAp.getGateway(), softAp.getSubnet())) {
                setLastError(F("Cannot configure AP mode"));
                Logger_error(F("%s"), getLastError());
            }
            else {
                #if defined(ESP32)

                    if (tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP) != ESP_OK) {
                        __LDBG_printf("tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP) failed");
                    }

                    dhcps_lease_t lease;
                    lease.enable = flags.is_softap_dhcpd_enabled;
                    lease.start_ip.addr = softAp.dhcp_start;
                    lease.end_ip.addr = softAp.dhcp_end;

                    if (tcpip_adapter_dhcps_option(TCPIP_ADAPTER_OP_SET, TCPIP_ADAPTER_REQUESTED_IP_ADDRESS, &lease, sizeof(lease)) != ESP_OK) {
                        setLastError(F("Failed to configure DHCP server"));
                        Logger_error(F("%s"), getLastError());
                    }
                    else if (flags.is_softap_dhcpd_enabled && tcpip_adapter_dhcps_start(TCPIP_ADAPTER_IF_AP) != ESP_OK) {
                        setLastError(F("Failed to start DHCP server"));
                        Logger_error(F("%s"), getLastError());
                    }

                #elif defined(ESP8266)

                    // setup after WiFi.softAPConfig()
                    struct dhcps_lease dhcp_lease;
                    wifi_softap_dhcps_stop();
                    dhcp_lease.enable = flags.is_softap_dhcpd_enabled;
                    dhcp_lease.start_ip.addr = softAp.dhcp_start;
                    dhcp_lease.end_ip.addr = softAp.dhcp_end;
                    if (!wifi_softap_set_dhcps_lease(&dhcp_lease)) {
                        setLastError(F("Failed to configure DHCP server"));
                        Logger_error(F("%s"), getLastError());
                    }
                    else if (flags.is_softap_dhcpd_enabled && !wifi_softap_dhcps_start()) {
                        setLastError(F("Failed to start DHCP server"));
                        Logger_error(F("%s"), getLastError());
                    }

                #endif

                __LDBG_printf("ap ssid=%s pass=%s channel=%u hidden=%u", Network::WiFi::getSoftApSSID(), Network::WiFi::getSoftApPassword(), softAp.getChannel(), flags.is_softap_ssid_hidden);

                if (!WiFi.softAP(Network::WiFi::getSoftApSSID(), Network::WiFi::getSoftApPassword(), softAp.getChannel(), flags.is_softap_ssid_hidden)) {
                    setLastError(F("Cannot start AP mode"));
                    Logger_error(F("%s"), getLastError());
                }
                else {
                    Logger_notice(F("AP Mode successfully initialized"));
                    ap_mode_success = true;

                    #if ENABLE_ARDUINO_OTA && !ENABLE_ARDUINO_OTA_AUTOSTART
                        if (flags.is_softap_standby_mode_enabled) {
                            auto &plugin = WebServer::Plugin::getInstance();
                            plugin.ArduinoOTAbegin();
                        }
                    #endif
                }
            }
        }
        else {
            __LDBG_print("disabling AP mode");
            WiFi.softAPdisconnect(true);
            WiFi.enableAP(false);
            ap_mode_success = true;
        }

        // install handler to enable AP mode if station mode goes down
        if (flags.is_softap_standby_mode_enabled) {
            WiFiCallbacks::add(WiFiCallbacks::EventType::CONNECTION, KFCFWConfiguration::apStandbyModeHandler);
        }
        else {
            WiFiCallbacks::remove(WiFiCallbacks::EventType::ANY, KFCFWConfiguration::apStandbyModeHandler);
        }

    }
    else {
        __LDBG_printf("skipping soft AP config: IP %s", WiFi.softAPIP().toString().c_str());
    }

    #if __LED_BUILTIN != IGNORE_BUILTIN_LED_PIN_ID || ENABLE_BOOT_LOG
        if (!station_mode_success || !ap_mode_success) {
            #if !IOT_WEATHER_STATION_WS2812_NUM
                BUILTIN_LED_SET(BlinkLEDTimer::BlinkType::FAST);
            #endif
        }
    #endif

    #if ESP8266
        auto hostname = System::Device::getName();
        WiFi.hostname(hostname);
    #endif

    return (station_mode_success && ap_mode_success);
}

const __FlashStringHelper *KFCFWConfiguration::getChipModel()
{
    #if ESP8265
        return F("ESP8265");
    #elif ESP8266
        return F("ESP8266");
    #elif ESP32
        #if CONFIG_IDF_TARGET_ESP32
            return F("ESP32");
        #elif CONFIG_IDF_TARGET_ESP32S2
            return F("ESP32-S2");
        #elif CONFIG_IDF_TARGET_ESP32S3
            return F("ESP32-S3");
        #elif CONFIG_IDF_TARGET_ESP32C3
            return F("ESP32-C3");
        #else
            #error unknown device
        #endif
    #else
        #error unknown device
    #endif
}

void KFCFWConfiguration::printVersion(Print &output)
{
    output.printf_P(PSTR("KFC Firmware %s\nFlash size %s\n"), KFCFWConfiguration::getFirmwareVersion().c_str(), formatBytes(ESP.getFlashChipSize()).c_str());
    if (config._safeMode) {
        Logger_notice(FSPGM(safe_mode_enabled, "Device started in SAFE MODE"));
        output.println(FSPGM(safe_mode_enabled));
    }
}

void KFCFWConfiguration::printInfo(Print &output)
{
    printVersion(output);

    auto flags = System::Flags::getConfig();
    if (flags.is_softap_enabled) {
        output.printf_P(PSTR("AP Mode SSID %s\n"), Network::WiFi::getSoftApSSID());
    }
    if (flags.is_station_mode_enabled) {
        output.printf_P(PSTR("Station Mode SSID %s\n"), Network::WiFi::getSSID(_wifiNumActive));
    }
    if (flags.is_factory_settings) {
        output.println(F("Running on factory settings"));
    }
    if (flags.is_default_password) {
        Logger_security(FSPGM(default_password_warning));
        output.println(FSPGM(default_password_warning, "WARNING! Default password has not been changed"));
    }
    output.printf_P(PSTR("Device %s ready!\n"), System::Device::getName());

    #if AT_MODE_SUPPORTED
        if (at_mode_enabled()) {
            output.println(F("Modified AT instruction set available.\n\nType AT? for help"));
        }
    #endif
}

void KFCFWConfiguration::getStatus(Print &output)
{
    #if defined(HAVE_NVS_FLASH)
        auto stats = config.getNVSStats();
        output.printf_P(PSTR("NVS flash storage max. size %uKB, %.1f%% in use" HTML_S(br)), config.getNVSFlashSize() / 1024, (stats.free_entries * 100) / float(stats.total_entries));
        output.printf_P(PSTR("NVS init partition memory usage %u byte " HTML_S(br)), config.getNVSInitMemoryUsage());
    #else
        output.printf_P(PSTR("EEPROM storage max. size %uKB" HTML_S(br)), SPI_FLASH_SEC_SIZE / 1024);
    #endif
    output.printf_P(PSTR("Stored items %u, size %.2fKB" HTML_S(br)), config.getConfigItemNum(), getConfigItemSize() / 1024.0);
}

uint32_t KFCFWConfiguration::getWiFiUp()
{
    if (!WiFi.isConnected() || !config._wifiUp) {
        return 0;
    }
    return std::max(1U, get_time_diff(config._wifiUp, millis()));
}

#if 0
#    define __DBG_RTC_printf __DBG_printf
#else
#    define __DBG_RTC_printf(...) ;
#endif

#ifndef DISABLE_TWO_WIRE
    TwoWire &KFCFWConfiguration::initTwoWire(bool reset, Print *output)
    {
        if (output) {
            output->printf_P("I2C: SDA=%u, SCL=%u, clock stretch=%u, clock speed=%u, reset=%u\n", KFC_TWOWIRE_SDA, KFC_TWOWIRE_SCL, KFC_TWOWIRE_CLOCK_STRETCH, KFC_TWOWIRE_CLOCK_SPEED, reset);
        }
        if (!_initTwoWire || reset) {
            __DBG_RTC_printf("SDA=%u,SCL=%u,stretch=%u,speed=%u,rst=%u", KFC_TWOWIRE_SDA, KFC_TWOWIRE_SCL, KFC_TWOWIRE_CLOCK_STRETCH, KFC_TWOWIRE_CLOCK_SPEED, reset);
            _initTwoWire = true;
            Wire.begin(KFC_TWOWIRE_SDA, KFC_TWOWIRE_SCL);
            Wire.setClockStretchLimit(KFC_TWOWIRE_CLOCK_STRETCH);
            Wire.setClock(KFC_TWOWIRE_CLOCK_SPEED);
        }
        return Wire;
    }
#endif

bool KFCFWConfiguration::setRTC(uint32_t unixtime)
{
    __DBG_RTC_printf("time=%u", unixtime);
    #if RTC_SUPPORT
        initTwoWire();
        if (!rtc.begin()) {
            __DBG_RTC_printf("rtc.begin() failed #1");
            delay(5);
            if (!rtc.begin()) {
                __DBG_RTC_printf("rtc.begin() failed #2");
                return false;
            }
        }
        rtc.adjust(DateTime(unixtime));
        return true;
    #endif
    return false;
}

void KFCFWConfiguration::setupRTC()
{
    #if RTC_SUPPORT
        // support for external RTCs
        initTwoWire();
        auto unixtime = config.getRTC();
        __DBG_RTC_printf("unixtime=%u", unixtime);
        if (unixtime != 0) {
            struct timeval tv = { static_cast<time_t>(unixtime), 0 };
            settimeofday(&tv, nullptr);
            RTCMemoryManager::setTime(unixtime, RTCMemoryManager::SyncStatus::YES);
        }
        else {
            RTCMemoryManager::setSyncStatus(false);
        }
    #else
        // support for the internal RTC
        // NTP should be used to synchronize the RTC after each restart, if possible after deep sleep as well
        // issues:
        // - inaccurate when using deep sleep
        // - no battery backup
        //
        // after a reset, the time is marked as not synchronized (rtcLostPower() == false) until set by NTP or manually
        // while it does not work if a device gets turned off, it is still ok when rebooting or recovering from a crash
        // time won't start at 1970 and the timezone is also set...
        RTCMemoryManager::setSyncStatus(false);
        auto rtc = RTCMemoryManager::readTime();
        __DBG_RTC_printf("unixtime=%u", rtc.time);
        struct timeval tv = { static_cast<time_t>(rtc.time), 0 };
        settimeofday(&tv, nullptr);
        RTCMemoryManager::setupRTC();
    #endif
}

bool KFCFWConfiguration::rtcLostPower()
{
    #if RTC_SUPPORT
        auto status = RTCMemoryManager::getSyncStatus();
        if (status == RTCMemoryManager::SyncStatus::NO) {
            return false;
        }
        initTwoWire();
        uint32_t unixtime = 0;
        if (!rtc.begin()) {
            __DBG_RTC_printf("rtc.begin() failed #1");
            delay(5);
            if (!rtc.begin()) {
                __DBG_RTC_printf("rtc.begin() failed #2");
                return false;
            }
        }
        unixtime = rtc.now().unixtime();
        if (unixtime == 0) {
            RTCMemoryManager::setSyncStatus(false);
        }
        return unixtime != 0;
    #else
        return RTCMemoryManager::getSyncStatus() != RTCMemoryManager::SyncStatus::YES;
    #endif
}

KFCFWConfiguration::RtcStatus KFCFWConfiguration::getRTCStatus()
{
    RtcStatus data;
    #if RTC_SUPPORT
        initTwoWire();
        if (!rtc.begin()) {
            __DBG_RTC_printf("rtc.begin() failed #1");
            delay(5);
            if (!rtc.begin()) {
                __DBG_RTC_printf("rtc.begin() failed #2");
                return data;
            }
        }
        data.time = rtc.now().unixtime();
        if (data.time == 0) {
            RTCMemoryManager::setSyncStatus(false);
        }
        data.temperature = rtc.getTemperature();
        data.lostPower = rtc.lostPower();
    #else
        data.time = time(nullptr);
        data.lostPower = RTCMemoryManager::getSyncStatus() != RTCMemoryManager::SyncStatus::NO;
    #endif
    return data;
}

uint32_t KFCFWConfiguration::getRTC()
{
    #if RTC_SUPPORT
        initTwoWire();
        if (!rtc.begin()) {
            __DBG_RTC_printf("rtc.begin() failed #1");
            delay(5);
            if (!rtc.begin()) {
                __DBG_RTC_printf("rtc.begin() failed #2");
                return 0;
            }
        }
        uint32_t unixtime = rtc.now().unixtime();
        __DBG_RTC_printf("time=%u", unixtime);
        if (rtc.lostPower()) {
            __DBG_RTC_printf("time=0, lostPower=true");
            return 0;
        }
        return unixtime;
    #endif
    __LDBG_printf("time=error");
    return 0;
}

float KFCFWConfiguration::getRTCTemperature()
{
    #if RTC_SUPPORT
        initTwoWire();
        if (!rtc.begin()) {
            __DBG_RTC_printf("rtc.begin() failed #1");
            delay(5);
            if (!rtc.begin()) {
                __DBG_RTC_printf("rtc.begin() failed #2");
                return NAN;
            }
        }
        return rtc.getTemperature();
    #endif
    return NAN;
}

void KFCFWConfiguration::printRTCStatus(Print &output, bool plain)
{
    RtcStatus data;
    #if RTC_SUPPORT
        initTwoWire();
    #endif

    data = getRTCStatus();
    PrintString nowStr;
    nowStr.strftime(FSPGM(strftime_date_time_zone), data.time);
    PGM_P nl = plain ? PSTR(", ") : PSTR(HTML_S(br));
    output.print(F("Timestamp: "));
    output.print(nowStr);
    __DBG_RTC_printf("temp=%.3f lost_power=%u", data.temperature, data.lostPower);
    if (!isnan(data.temperature)) {
        output.printf_P(PSTR("%sTemperature: %.2f%s"), nl, data.temperature, plain ? PSTR("C") : SPGM(UTF8_degreeC));
    }
    output.printf_P(PSTR("%sLost Power: %s"), nl, data.lostPower ? SPGM(Yes) : SPGM(No));

    #if RTC_SUPPORT
        if (data.time == 0) {
            output.print(F("Failed to initialize RTC"));
        }
    #endif
}

KFCFWConfiguration::StationConfigType KFCFWConfiguration::scanWifiStrength(StationConfigType configNum)
{
    auto list = Network::WiFi::getStations(nullptr);
    // check if there is any station with priority 0 / auto detect strength
    auto iter = std::find_if(list.begin(), list.end(), [](const auto &station) {
        return station._SSID.length() && (station._priority == 0);
    });
    if (iter == list.end()) {
        __LDBG_printf("no stations in auto mode");
        return configNum;
    }

    // scan wifi networks and display wifi strength
    __LDBG_printf("scan wifi");
    auto num = WiFi.scanNetworks(false, true);
    __LDBG_printf("num=%u", num);
    if (num > 0) {
        #if DEBUG_KFC_CONFIG
            // display all scanned wifi networks
            for(uint8_t i = 0; i < num; i++) {
                String ssid;
                uint8_t encType = 0;
                int32_t rssi = 0;
                uint8_t *bssid = nullptr;
                int32_t channel = 0;
                bool hidden = false;
                #if ESP8266
                    auto res = WiFi.getNetworkInfo(i, ssid, encType, rssi, bssid, channel, hidden);
                #else
                    auto res = WiFi.getNetworkInfo(i, ssid, encType, rssi, bssid, channel);
                #endif
                if (res) {
                    __DBG_printf("wifi=%s rssi=%d hidden=%d bssid=%s channel=%d enc=%d", ssid.c_str(), rssi, hidden, mac2String(bssid).c_str(), channel, encType);
                }
                else {
                    __DBG_printf("wifi=%d error", i);
                }
            }
            __DBG_printf("---");
        #endif
        for(auto &station: list) {
            for(uint8_t i = 0; i < num; i++) {
                if (station._SSID.length()) {
                    auto ssid = WiFi.SSID(i);
                    if (ssid == station._SSID) {
                        if (station._priority == 0) {
                            station._priority = -WiFi.RSSI(i);
                            if (WiFi.BSSID(i)) {
                                // update BSSID for network
                                memmove(station._bssid, WiFi.BSSID(i), sizeof(station._bssid));
                            }
                        }
                        __LDBG_printf("ssid=%s prio=%d bssid=%s rssi=%d", ssid.c_str(), station._priority, mac2String(WiFi.BSSID(i)).c_str(), WiFi.RSSI(i));
                        break;
                    }
                }
            }
        }
    }
    __LDBG_printf("scan wifi done");
    return configNum;
}

static KFCConfigurationPlugin plugin;

PROGMEM_DEFINE_PLUGIN_OPTIONS(
    KFCConfigurationPlugin,
    "cfg",              // name
    "cfg",              // friendly name
    // web_templates (class StatusTemplate)
    "index,status",
    // config_forms
    "wifi,network,device,password",
    // reconfigure_dependencies
    "wifi,network",
    PluginComponent::PriorityType::CONFIG,
    PluginComponent::RTCMemoryId::CONFIG,
    static_cast<uint8_t>(PluginComponent::MenuType::NONE),
    true,               // allow_safe_mode
    true,               // setup_after_deep_sleep
    false,              // has_get_status
    true,               // has_config_forms
    false,              // has_web_ui
    true,               // has_web_templates
    false,              // has_at_mode
    0                   // __reserved
);

KFCConfigurationPlugin::KFCConfigurationPlugin() : PluginComponent(PROGMEM_GET_PLUGIN_OPTIONS(KFCConfigurationPlugin))
{
    REGISTER_PLUGIN(this, "KFCConfigurationPlugin");
}

void KFCConfigurationPlugin::setup(SetupModeType mode, const PluginComponents::DependenciesPtr &dependencies)
{
    __LDBG_printf("safe mode %d, wake up %d", (mode == SetupModeType::SAFE_MODE), resetDetector.hasWakeUpDetected());
    config.setup();

    #if NTP_LOG_TIME_UPDATE
        PrintString nowStr;
        nowStr.strftime(FSPGM(strftime_date_time_zone), time(nullptr));
        Logger_notice(F("RTC time: %s"), nowStr.c_str());
    #endif

    #if DEBUG
        if (!resetDetector.hasWakeUpDetected()) {
            // WiFi should not be connected unless coming back from deep sleep
            if (WiFi.isConnected()) {
                __LDBG_print("WiFi up, skipping init");
                Logger_error(F("WiFi already up"));
                BUILTIN_LED_SET(BlinkLEDTimer::BlinkType::SOS);
                return;
            }
        }
    #endif

    #if ENABLE_DEEP_SLEEP
        if (!resetDetector.hasWakeUpDetected())
    #endif
    {
        config.printInfo(Serial);

        if (!config.connectWiFi()) {
            Serial.printf_P(PSTR("Configuring WiFi failed: %s\n"), config.getLastError());
            #if DEBUG
                config.dump(Serial);
            #endif
        }
    }
}

void KFCConfigurationPlugin::reconfigure(const String &source)
{
    if (source == FSPGM(network) || source == FSPGM(wifi)) {
        config.reconfigureWiFi();
    }
}

WebTemplate *KFCConfigurationPlugin::getWebTemplate(const String &name)
{
    return new StatusTemplate();
}
