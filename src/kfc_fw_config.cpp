/**
 * Author: sascha_lammers@gmx.de
 */

#include <kfc_fw_config.h>
#include <kfc_fw_config_plugin.h>
#include <stl_ext/array.h>
#include <EEPROM.h>
#include <ReadADC.h>
#include <PrintString.h>
#include <HardwareSerial.h>
#include <EventScheduler.h>
#include <session.h>
#include <misc.h>
#include "SaveCrash.h"
#include "blink_led_timer.h"
#include "fs_mapping.h"
#include "WebUISocket.h"
#include "WebUIAlerts.h"
#include "web_server.h"
#include "build.h"
#include <JsonBaseReader.h>
#include <Form/Types.h>
#if NTP_CLIENT
#include "../src/plugins/ntp/ntp_plugin.h"
#endif
#if MQTT_SUPPORT
#include <AsyncMqttClient.h>
#include "../src/plugins/mqtt/mqtt_client.h"
#endif
#if MDNS_PLUGIN
#include <ESP8266SSDP.h>
#include "../src/plugins/mdns/mdns_plugin.h"
#include "../src/plugins/mdns/mdns_resolver.h"
#endif
#if HTTP2SERIAL_SUPPORT
#include "../src/plugins/http2serial/http2serial.h"
#endif
#if IOT_REMOTE_CONTROL
#include "../src/plugins/remote/remote.h"
#endif
#include "PinMonitor.h"

#if defined(ESP8266)
#include <sntp.h>
#include <sntp-lwip2.h>
#elif defined(ESP32)
#include <lwip/apps/sntp.h>
#endif

#if RTC_SUPPORT
#include <RTClib.h>
#if RTC_DEVICE_DS3231
RTC_DS3231 rtc;
#else
#error RTC device not set
#endif
#endif

#if DEBUG_KFC_CONFIG
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

KFCFWConfiguration config;

using KFCConfigurationClasses::MainConfig;
using KFCConfigurationClasses::Network;
using KFCConfigurationClasses::System;
using KFCConfigurationClasses::Plugins;

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

// Config_Button

void Config_Button::getButtons(ButtonVector &buttons)
{
    uint16_t length;
    auto ptr = config.getBinary(_H(Config().buttons), length);
    if (ptr) {
        auto items = length / sizeof(Button_t);
        buttons.resize(items);
        memcpy(buttons.data(), ptr, items * sizeof(Button_t));
    }
}

void Config_Button::setButtons(ButtonVector &buttons)
{
    config.setBinary(_H(Config().buttons), buttons.data(), buttons.size() * sizeof(Button_t));
}

// KFCFWConfiguration

static_assert(CONFIG_EEPROM_SIZE >= 1024 && (CONFIG_EEPROM_OFFSET + CONFIG_EEPROM_SIZE) <= 4096, "invalid EEPROM size");

KFCFWConfiguration::KFCFWConfiguration() :
    Configuration(CONFIG_EEPROM_OFFSET, CONFIG_EEPROM_SIZE),
    _garbageCollectionCycleDelay(5000),
    _dirty(false),
    _wifiConnected(false),
    _initTwoWire(false),
    _safeMode(false),
    _wifiUp(~0UL),
    _offlineSince(~0UL)
#if ENABLE_DEEP_SLEEP
    , _deepSleepParams()
#endif
{
    _setupWiFiCallbacks();
#if SAVE_CRASH_HAVE_CALLBACKS
    auto nextCallback = EspSaveCrash::getCallback();
    EspSaveCrash::setCallback([this, nextCallback](const EspSaveCrash::ResetInfo_t &info) {
        // release all memory in the event of a crash
        __crashCallback();
        if (nextCallback) {
            nextCallback(info);
        }
    });
#endif
}

KFCFWConfiguration::~KFCFWConfiguration()
{
    LoopFunctions::remove(KFCFWConfiguration::loop);
}

void KFCFWConfiguration::_onWiFiConnectCb(const WiFiEventStationModeConnected &event)
{
    __LDBG_printf("KFCFWConfiguration::_onWiFiConnectCb(%s, %d, %s)", event.ssid.c_str(), (int)event.channel, mac2String(event.bssid).c_str());
    if (!_wifiConnected) {

#if ENABLE_DEEP_SLEEP
        if (resetDetector.hasWakeUpDetected() && ResetDetectorPlugin::_deepSleepWifiTime == ~0U) { // first connection after wakeup
            ResetDetectorPlugin::_deepSleepWifiTime = millis();
            Logger_notice(F("WiFi connected to %s after %u ms"), event.ssid.c_str(), ResetDetectorPlugin::_deepSleepWifiTime);
        }
        else
#endif
        {
            Logger_notice(F("WiFi connected to %s"), event.ssid.c_str());
        }

        __LDBG_printf("Station: WiFi connected to %s, offline for %.3f, millis = %lu", event.ssid.c_str(), _offlineSince == ~0UL ? 0 : ((millis() - _offlineSince) / 1000.0), millis());
        __LDBG_printf("Free heap %s", formatBytes(ESP.getFreeHeap()).c_str());
        _wifiConnected = true;
 #if ENABLE_DEEP_SLEEP
        config.storeQuickConnect(event.bssid, event.channel);
#endif

#if defined(ESP32)
        auto hostname = System::Device::getName();
        __LDBG_printf("WiFi.setHostname(%s)", hostname);
        if (!WiFi.setHostname(hostname)) {
            __LDBG_printf("WiFi.setHostname(%s) failed", hostname);
        }
#endif

    }
}

void KFCFWConfiguration::_onWiFiDisconnectCb(const WiFiEventStationModeDisconnected &event)
{
    __LDBG_printf("KFCFWConfiguration::_onWiFiDisconnectCb(%d = %s)", (int)event.reason, WiFi_disconnect_reason(event.reason).c_str());
    if (_wifiConnected) {
        BlinkLEDTimer::setBlink(__LED_BUILTIN, BlinkLEDTimer::FAST);
        __LDBG_printf("WiFi disconnected after %.3f seconds, millis = %lu", ((_wifiUp == ~0UL) ? -1.0 : ((millis() - _wifiUp) / 1000.0)), millis());

        Logger_notice(F("WiFi disconnected, SSID %s, reason %s"), event.ssid.c_str(), WiFi_disconnect_reason(event.reason).c_str());
        _offlineSince = millis();
        _wifiConnected = false;
        _wifiUp = ~0UL;
        LoopFunctions::callOnce([event]() {
            WiFiCallbacks::callEvent(WiFiCallbacks::EventType::DISCONNECTED, (void *)&event);
        });

    }
#if defined(ESP32)
    else {
        //_onWiFiDisconnectCb 202 = AUTH_FAIL
        if (event.reason == 202) {
            _debug_println(F("force WiFi.begin()"));
            WiFi.begin();
        }
    }

    // work around for ESP32 losing the connection and not reconnecting automatically
    static Event::Timer _reconnectTimer;
    if (!_reconnectTimer.active()) {
        _Timer(_reconnectTimer).add(60000, true, [this](Event::CallbackTimerPtr timer) {
            if (_wifiConnected) {
                timer->detach();
            }
            else {
                BlinkLEDTimer::setBlink(__LED_BUILTIN, BlinkLEDTimer::FAST);
                _debug_println(F("force WiFi reconnect"));
                reconfigureWiFi(); // reconfigure wifi, WiFi.begin() does not seem to work
            }
        });
    }
#endif
}

void KFCFWConfiguration::_onWiFiGotIPCb(const WiFiEventStationModeGotIP &event)
{
    auto ip = event.ip.toString();
    auto mask = event.mask.toString();
    auto gw = event.gw.toString();
    auto dns1 = WiFi.dnsIP().toString();
    auto dns2 = WiFi.dnsIP(1).toString();
    __LDBG_printf("KFCFWConfiguration::_onWiFiGotIPCb(%s, %s, %s DNS %s, %s)", ip.c_str(), mask.c_str(), gw.c_str(), dns1.c_str(), dns2.c_str());

    Logger_notice(F("%s: IP/Net %s/%s GW %s DNS: %s, %s"), System::Flags::getConfig().is_station_mode_dhcp_enabled ? PSTR("DHCP") : PSTR("Static configuration"), ip.c_str(), mask.c_str(), gw.c_str(), dns1.c_str(), dns2.c_str());

    using Device = KFCConfigurationClasses::System::Device;

    BlinkLEDTimer::setBlink(__LED_BUILTIN, (Device::getConfig().getStatusLedMode() == Device::StatusLEDModeType::SOLID_WHEN_CONNECTED) ? BlinkLEDTimer::SOLID : BlinkLEDTimer::OFF);
    _wifiUp = millis();
#if ENABLE_DEEP_SLEEP
    config.storeStationConfig(event.ip, event.mask, event.gw);
#endif

    LoopFunctions::callOnce([event]() {
        WiFiCallbacks::callEvent(WiFiCallbacks::EventType::CONNECTED, (void *)&event);
    });
}

void KFCFWConfiguration::_onWiFiOnDHCPTimeoutCb()
{
    __LDBG_printf("KFCFWConfiguration::_onWiFiOnDHCPTimeoutCb()");
#if defined(ESP32)
    Logger_error(F("Lost DHCP IP"));
#else
    Logger_error(F("DHCP timeout"));
#endif
    BlinkLEDTimer::setBlink(__LED_BUILTIN, BlinkLEDTimer::FLICKER);
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
        case WiFiEvent_t::SYSTEM_EVENT_STA_CONNECTED: {
                WiFiEventStationModeConnected dst;
                dst.ssid = reinterpret_cast<const char *>(info.connected.ssid);
                MEMNCPY_S(dst.bssid, info.connected.bssid);
                dst.channel = info.connected.channel;
                config._onWiFiConnectCb(dst);
            } break;
        case WiFiEvent_t::SYSTEM_EVENT_STA_DISCONNECTED: {
                WiFiEventStationModeDisconnected dst;
                dst.ssid = reinterpret_cast<const char *>(info.disconnected.ssid);
                MEMNCPY_S(dst.bssid, info.disconnected.bssid);
                dst.reason = (WiFiDisconnectReason)info.disconnected.reason;
                config._onWiFiDisconnectCb(dst);
            } break;
#if DEBUG
        case WiFiEvent_t::SYSTEM_EVENT_GOT_IP6: {
                debug_printf_P(PSTR("_nWiFiEvent(SYSTEM_EVENT_GOT_IP6, addr=%04X:%04X:%04X:%04X)\n"), info.got_ip6.ip6_info.ip.addr[0], info.got_ip6.ip6_info.ip.addr[1], info.got_ip6.ip6_info.ip.addr[2], info.got_ip6.ip6_info.ip.addr[3]);
            } break;
#endif
        case WiFiEvent_t::SYSTEM_EVENT_STA_GOT_IP: {
                WiFiEventStationModeGotIP dst;
                dst.ip = info.got_ip.ip_info.ip.addr;
                dst.gw = info.got_ip.ip_info.gw.addr;
                dst.mask = info.got_ip.ip_info.netmask.addr;
                config._onWiFiGotIPCb(dst);
            } break;
        case WiFiEvent_t::SYSTEM_EVENT_STA_LOST_IP: {
                config._onWiFiOnDHCPTimeoutCb();
            } break;
        case WiFiEvent_t::SYSTEM_EVENT_AP_STACONNECTED: {
                WiFiEventSoftAPModeStationConnected dst;
                dst.aid = info.sta_connected.aid;
                MEMNCPY_S(dst.mac, info.sta_connected.mac);
                config._softAPModeStationConnectedCb(dst);
            } break;
        case WiFiEvent_t::SYSTEM_EVENT_AP_STADISCONNECTED: {
                WiFiEventSoftAPModeStationDisconnected dst;
                dst.aid = info.sta_connected.aid;
                MEMNCPY_S(dst.mac, info.sta_connected.mac);
                config._softAPModeStationDisconnectedCb(dst);
            } break;
        default:
            break;
    }
}

#elif USE_WIFI_SET_EVENT_HANDLER_CB

void KFCFWConfiguration::_onWiFiEvent(System_Event_t *orgEvent) {
    //event->event_info.connected
    switch(orgEvent->event) {
        case EVENT_STAMODE_CONNECTED: {
                WiFiEventStationModeConnected dst;
                auto &src = orgEvent->event_info.connected;
                dst.ssid = reinterpret_cast<const char *>(src.ssid);
                MEMNCPY_S(dst.bssid, src.bssid);
                dst.channel = src.channel;
                config._onWiFiConnectCb(dst);
            } break;
        case EVENT_STAMODE_DISCONNECTED: {
                WiFiEventStationModeDisconnected dst;
                auto &src = orgEvent->event_info.disconnected;
                dst.ssid = reinterpret_cast<const char *>(src.ssid);
                MEMNCPY_S(dst.bssid, src.bssid);
                dst.reason = (WiFiDisconnectReason)src.reason;
                config._onWiFiDisconnectCb(dst);
            } break;
        case EVENT_STAMODE_GOT_IP: {
                WiFiEventStationModeGotIP dst;
                auto &src = orgEvent->event_info.got_ip;
                dst.gw = src.gw.addr;
                dst.ip = src.ip.addr;
                dst.mask = src.mask.addr;
                config._onWiFiGotIPCb(dst);
            } break;
        case EVENT_STAMODE_DHCP_TIMEOUT: {
                config._onWiFiOnDHCPTimeoutCb();
            } break;
        case EVENT_SOFTAPMODE_STACONNECTED: {
                WiFiEventSoftAPModeStationConnected dst;
                auto &src = orgEvent->event_info.sta_connected;
                dst.aid = src.aid;
                MEMNCPY_S(dst.mac, src.mac);
                config._softAPModeStationConnectedCb(dst);
            } break;
        case EVENT_SOFTAPMODE_STADISCONNECTED: {
                WiFiEventSoftAPModeStationDisconnected dst;
                auto &src = orgEvent->event_info.sta_disconnected;
                dst.aid = src.aid;
                MEMNCPY_S(dst.mac, src.mac);
                config._softAPModeStationDisconnectedCb(dst);
            } break;
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

void KFCFWConfiguration::apStandModehandler(WiFiCallbacks::EventType event, void *payload)
{
    config._apStandModehandler(event);
}

void KFCFWConfiguration::_setupWiFiCallbacks()
{
    _debug_println();

#if defined(ESP32)

    WiFi.onEvent(_onWiFiEvent, WiFiEvent_t::SYSTEM_EVENT_STA_CONNECTED);
    WiFi.onEvent(_onWiFiEvent, WiFiEvent_t::SYSTEM_EVENT_STA_DISCONNECTED);
    WiFi.onEvent(_onWiFiEvent, WiFiEvent_t::SYSTEM_EVENT_STA_GOT_IP);
#if DEBUG
    WiFi.onEvent(_onWiFiEvent, WiFiEvent_t::SYSTEM_EVENT_GOT_IP6);
#endif
    WiFi.onEvent(_onWiFiEvent, WiFiEvent_t::SYSTEM_EVENT_STA_LOST_IP);
    WiFi.onEvent(_onWiFiEvent, WiFiEvent_t::SYSTEM_EVENT_AP_STACONNECTED);
    WiFi.onEvent(_onWiFiEvent, WiFiEvent_t::SYSTEM_EVENT_AP_STADISCONNECTED);

#elif USE_WIFI_SET_EVENT_HANDLER_CB

    wifi_set_event_handler_cb((wifi_event_handler_cb_t)&KFCFWConfiguration::_onWiFiEvent);

#else
    _onWiFiConnect = WiFi.onStationModeConnected(__onWiFiConnectCb); // using a lambda function costs 24 byte RAM per handler
    _onWiFiGotIP = WiFi.onStationModeGotIP(__onWiFiGotIPCb);
    _onWiFiDisconnect = WiFi.onStationModeDisconnected(__onWiFiDisconnectCb);
    _onWiFiOnDHCPTimeout = WiFi.onStationModeDHCPTimeout(__onWiFiOnDHCPTimeoutCb);
    _softAPModeStationConnected = WiFi.onSoftAPModeStationConnected(__softAPModeStationConnectedCb);
    _softAPModeStationDisconnected = WiFi.onSoftAPModeStationDisconnected(__softAPModeStationDisconnectedCb);
#endif
}

void KFCFWConfiguration::_apStandModehandler(WiFiCallbacks::EventType event)
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
    flags.is_softap_standby_mode_enabled = false;
    flags.is_softap_enabled = true;
    flags.is_web_server_enabled = true;
    KFC_SAFE_MODE_SERIAL_PORT.printf_P(PSTR("Recovery mode SSID %s\n"), Network::WiFi::getSoftApSSID());
}

String KFCFWConfiguration::defaultDeviceName()
{
    uint8_t mac[6];
    WiFi.macAddress(mac);
    return PrintString(F("KFC%02X%02X%02X"), mac[3], mac[4], mac[5]);
}

// TODO add option to keep WiFi SSID, username and password

void KFCFWConfiguration::restoreFactorySettings()
{
    __LDBG_println();
    PrintString str;

    SaveCrash::clearEEPROM();

#if DEBUG
    uint16_t length = 0;
    auto hash = System::Firmware::getElfHash(length);
    uint8_t hashCopy[System::Firmware::getElfHashSize()];
    if (hash && length == sizeof(hashCopy)) {
        memcpy(hashCopy, hash, sizeof(hashCopy));
    } else {
        length = 0;
    }
#endif

    clear();

#if DEBUG
    if (length) {
        System::Firmware::setElfHash(hashCopy);
    }
#endif

    auto deviceName = defaultDeviceName();

    System::Flags::defaults();
    System::Firmware::defaults();
    System::Device::defaults();
    System::Device::setName(deviceName);
    System::Device::setTitle(FSPGM(KFC_Firmware, "KFC Firmware"));
    System::Device::setPassword(FSPGM(defaultPassword, "12345678"));
    System::WebServer::defaults();
    Network::WiFi::setSSID(deviceName);
    Network::WiFi::setPassword(FSPGM(defaultPassword));
    Network::WiFi::setSoftApSSID(deviceName);
    Network::WiFi::setSoftApPassword(FSPGM(defaultPassword));
    Network::Settings::defaults();
    Network::SoftAP::defaults();

#if MQTT_SUPPORT
    Plugins::MQTTClient::defaults();
#endif
#if HOME_ASSISTANT_INTEGRATION
    Plugins::HomeAssistant::defaults();
#endif
#if IOT_REMOTE_CONTROL
    Plugins::RemoteControl::defaults();
#endif
#if SERIAL2TCP_SUPPORT
    Plugins::Serial2TCP::defaults();
#endif
#if SYSLOG_SUPPORT
    Plugins::SyslogClient::defaults();
#endif
#if NTP_CLIENT
    Plugins::NTPClient::defaults();
#endif
#if IOT_ALARM_PLUGIN_ENABLED
    Plugins::Alarm::defaults();
#endif
#if IOT_WEATHER_STATION
    Plugins::WeatherStation::defaults();
#endif
#if IOT_SENSOR
    Plugins::Sensor::defaults();
#endif
#if IOT_BLINDS_CTRL
    Plugins::Blinds::defaults();
#endif
#if PING_MONITOR_SUPPORT
    Plugins::Ping::defaults();
#endif
#if IOT_DIMMER_MODULE || IOT_ATOMIC_SUN_V2
    Plugins::Dimmer::defaults();
#endif
#if IOT_CLOCK
    Plugins::Clock::defaults();
#endif

#if CUSTOM_CONFIG_PRESET
    customSettings();
#endif
    WebAlerts::Alert::warning(F("Factory settings restored"));
}

#if CUSTOM_CONFIG_PRESET
    // set CUSTOM_CONFIG_PRESET to 0 if no custom config is being used
    #include "retracted/custom_config.h"
#endif

void KFCFWConfiguration::setLastError(const String &error)
{
    _lastError = error;
}

const char *KFCFWConfiguration::getLastError() const
{
    return _lastError.c_str();
}

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

const char __compile_date__[] PROGMEM = { __DATE__ };

const String KFCFWConfiguration::getFirmwareVersion()
{
#if DEBUG
#if ESP8266
    return getShortFirmwareVersion() + F("." ARDUINO_ESP8266_RELEASE " " ) + FPSTR(__compile_date__);
#else
    return getShortFirmwareVersion() + ' ' + FPSTR(__compile_date__);
#endif

#else // #if DEBUG
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

    DeepSleep::WiFiQuickConnect_t quickConnect;
    if (!RTCMemoryManager::read(RTCMemoryManager::RTCMemoryId::CONFIG, &quickConnect, sizeof(quickConnect))) {
        quickConnect = {};
    }
    quickConnect.channel = channel;
    memcpy(quickConnect.bssid, bssid, WL_MAC_ADDR_LENGTH);
    RTCMemoryManager::write(RTCMemoryManager::RTCMemoryId::CONFIG, &quickConnect, sizeof(quickConnect));
}

void KFCFWConfiguration::storeStationConfig(uint32_t ip, uint32_t netmask, uint32_t gateway)
{
    __LDBG_printf("ip=%s netmask=%s gw=%s dhcp=%u",
        IPAddress(ip).toString().c_str(),
        IPAddress(netmask).toString().c_str(),
        IPAddress(gateway).toString().c_str(),
        (wifi_station_dhcpc_status() == DHCP_STARTED)
    );

    DeepSleep::WiFiQuickConnect_t quickConnect = {};
    if (RTCMemoryManager::read(RTCMemoryManager::RTCMemoryId::CONFIG, &quickConnect, sizeof(quickConnect))) {
        quickConnect.local_ip = ip;
        quickConnect.subnet = netmask;
        quickConnect.gateway = gateway;
        quickConnect.dns1 = (uint32_t)WiFi.dnsIP();
        quickConnect.dns2 = (uint32_t)WiFi.dnsIP(1);
        auto flags = System::Flags::getConfig();
        quickConnect.use_static_ip = flags.use_static_ip_during_wakeup || !flags.is_station_mode_dhcp_enabled;
        RTCMemoryManager::write(RTCMemoryManager::RTCMemoryId::CONFIG, &quickConnect, sizeof(quickConnect));
    } else {
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
        BlinkLEDTimer::setBlink(__LED_BUILTIN, BlinkLEDTimer::FLICKER);
    }

    // ~5ms
    rng.begin(version.c_str());
    uint8_t mac[WL_MAC_ADDR_LENGTH];
    rng.stir(WiFi.macAddress(mac), WL_MAC_ADDR_LENGTH);
    auto channel = WiFi.channel();
    rng.stir((const uint8_t *)&channel, sizeof(channel));
    rng.stir((const uint8_t *)&config, sizeof(config));

    LoopFunctions::add(KFCFWConfiguration::loop);
}

#include "build.h"

void KFCFWConfiguration::read()
{
    __LDBG_println();

    BOOTLOG_PRINTF("read config");

    if (!Configuration::read()) {
        BOOTLOG_PRINTF("failed");

        Logger_error(F("Failed to read configuration, restoring factory settings"));
        config.restoreFactorySettings();
        Configuration::write();
    } else {
        auto version = System::Device::getConfig().config_version;
        BOOTLOG_PRINTF("version %08x", version);
        uint32_t currentVersion = (FIRMWARE_VERSION << 16) | (uint16_t)String(F(__BUILD_NUMBER)).toInt();
        if (currentVersion != version) {
            uint16_t build = version;
            version >>= 16;
            Logger_warning(F("Upgrading EEPROM settings from %d.%d.%d.%u to " FIRMWARE_VERSION_STR "." __BUILD_NUMBER), (version >> 16), (version >> 8) & 0xff, (version & 0xff), build);
            System::Device::getWriteableConfig().config_version = currentVersion;
            config.recoveryMode(false);
            Configuration::write();
            SaveCrash::clearEEPROM();
        }
    }
}

void KFCFWConfiguration::write()
{
    __LDBG_println();

    System::Flags::getWriteableConfig().is_factory_settings = false;

    if (!Configuration::write()) {
        Logger_error(F("Failure to write settings to EEPROM"));
    }
}

bool KFCFWConfiguration::resolveZeroConf(const String &name, const String &hostname, uint16_t port, MDNSResolver::ResolvedCallback callback) const
{
    auto mdns = PluginComponent::getPlugin<MDNSPlugin>(F("mdns"), true);
    if (!mdns) {
        Logger_error(F("Cannot resolve Zeroconf, MDNS plugin not loaded: %s"), hostname.c_str());
        return false;
    }

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

void KFCFWConfiguration::wifiQuickConnect()
{
#if defined(ESP32)
    WiFi.mode(WIFI_STA); // needs to be called to initialize wifi
    wifi_config_t _config;
    wifi_sta_config_t &config = _config.sta;
    if (esp_wifi_get_config(ESP_IF_WIFI_STA, &_config) == ESP_OK) {
#elif defined(ESP8266)
    struct station_config config;
    if (wifi_station_get_config_default(&config)) {
#endif
        int32_t channel;
        uint8_t *bssidPtr;
        DeepSleep::WiFiQuickConnect_t quickConnect;

        if (RTCMemoryManager::read(PluginComponent::RTCMemoryId::CONFIG, &quickConnect, sizeof(quickConnect))) {
            channel = quickConnect.channel;
            bssidPtr = quickConnect.bssid;
        } else {
            quickConnect = {};
            channel = 0;
            // bssidPtr = nullptr;
        }

        if (channel <= 0 || !bssidPtr) {

            __LDBG_printf("Cannot read quick connect from RTC memory, running WiFi.begin(%s, ***) only", config.ssid);
            if (WiFi.begin(reinterpret_cast<char *>(config.ssid), reinterpret_cast<char *>(config.password)) != WL_DISCONNECTED) {
                Logger_error(F("Failed to start WiFi"));
            }

        } else {

            if (quickConnect.use_static_ip && quickConnect.local_ip) {
                __LDBG_printf("configuring static ip");
                WiFi.config(quickConnect.local_ip, quickConnect.gateway, quickConnect.subnet, quickConnect.dns1, quickConnect.dns2);
            }

            wl_status_t result;
            if ((result = WiFi.begin(reinterpret_cast<char *>(config.ssid), reinterpret_cast<char *>(config.password), channel, bssidPtr, true)) != WL_DISCONNECTED) {
                Logger_error(F("Failed to start WiFi"));
            }

            __LDBG_printf("WiFi.begin() = %d, channel %d, bssid %s, config: static ip %d, %s/%s gateway %s, dns %s, %s",
                result,
                channel,
                mac2String(bssidPtr).c_str(),
                quickConnect.use_static_ip ? 1 : 0,
                inet_ntoString(quickConnect.local_ip).c_str(),
                inet_ntoString(quickConnect.gateway).c_str(),
                inet_ntoString(quickConnect.subnet).c_str(),
                inet_ntoString(quickConnect.dns1).c_str(),
                inet_ntoString(quickConnect.dns2).c_str()
            );

        }

    } else {
        Logger_error(F("Failed to load WiFi configuration"));
    }

}

#if ENABLE_DEEP_SLEEP


uint32_t DeepSleep::DeepSleepParams_t::getDeepSleepMaxMillis()
{
    return (_deepSleepMaxTime == 0) ? (ESP.deepSleepMax() * 9ULL / 10000ULL) : _deepSleepMaxTime;
}

void DeepSleep::DeepSleepParams_t::setDeepSleepMaxTime(uint32_t millis)
{
    _deepSleepMaxTime = millis;
}

uint32_t DeepSleep::DeepSleepParams_t::_deepSleepMaxTime = 0;


// update remainingSleepTime and currentSleepTime
// both will be 0 when the last cycle has completed
void DeepSleep::DeepSleepParams_t::calcSleepTime()
{
    if (isUnlimited()) {
        remainingSleepTime = 0;
        currentSleepTime = 0;
        return;
    }
    // split sleep time up into ~3h cycles
    auto maxSleepTime = getDeepSleepMaxMillis();

    if (remainingSleepTime) {
        if (remainingSleepTime > maxSleepTime * 2) {
            // use max. sleep time
            remainingSleepTime -= maxSleepTime;
            currentSleepTime = maxSleepTime;
        }
        else if (remainingSleepTime < maxSleepTime) {
            // wait for the remaining time
            currentSleepTime = remainingSleepTime;
            remainingSleepTime = 0;
        }
        else {
            // split 50:50
            remainingSleepTime = maxSleepTime / 2;
            currentSleepTime = remainingSleepTime;
        }
    }
    else {
        currentSleepTime = 0;
    }
    ::printf(PSTR("deep sleep cycle: time=%ums rest=%ums total=%ums max=%u/%.0f\n"), currentSleepTime, remainingSleepTime, deepSleepTime, maxSleepTime, ESP.deepSleepMax() / 1000.0);

}

void KFCFWConfiguration::wakeUpFromDeepSleep()
{
    ::printf(PSTR("wakeUpFromDeepSleep\n"));

#if IOT_REMOTE_CONTROL
    auto &remote = RemoteControlPlugin::getInstance();
    if (remote.detectKeyPress() != 0) {
        ::printf(PSTR("deep sleep: key press bitset: %s\n"), String(remote.getKeysPressed(), 2).c_str());
    }
#endif

    if (RTCMemoryManager::read(RTCMemoryManager::RTCMemoryId::DEEP_SLEEP, &_deepSleepParams, sizeof(_deepSleepParams)) && _deepSleepParams.deepSleepTime) {
        if (_deepSleepParams.remainingSleepTime) {
            _deepSleepParams.calcSleepTime();

#if RTC_SUPPORT
            _deepSleepParams.lastWakeUpUnixTime = getRTC();
#endif

#if IOT_REMOTE_CONTROL
            if (remote.getKeysPressed() && _deepSleepParams.abortOnKeyPress) {
                _deepSleepParams.abortCycles();
            }
#endif

            uint32_t ms = micros();
            _deepSleepParams.cycleRuntime += ms + 3500;
            ::printf(PSTR("cycle_runtime=%uus last=%uus\n"), _deepSleepParams.cycleRuntime, ms);

            RTCMemoryManager::write(RTCMemoryManager::RTCMemoryId::DEEP_SLEEP, &_deepSleepParams, sizeof(_deepSleepParams));

            auto dur = micros() - ms;
            ::printf(PSTR("additional time: %u\n"), (unsigned)dur);

            if (_deepSleepParams.hasBeenAborted()) {
                // deep sleep aborted by a keypress
            }
            else if (_deepSleepParams.isUnlimited()) {
                ::printf(PSTR("entering deep sleep: %u\n"), 0);
                ESP.deepSleepInstant(0, _deepSleepParams.mode);
            }
            else if (!_deepSleepParams.cyclesCompeleted()) {

                if (_deepSleepParams.remainingSleepTime < 1000 && _deepSleepParams.mode != RFMode::RF_CAL) {
                    _deepSleepParams.mode = RFMode::RF_CAL;
                    ::printf(PSTR("running RF_CAL\n"));
                } else {
                    _deepSleepParams.mode = RFMode::RF_DISABLED;
                    ::printf(PSTR("running RF_DISABLED\n"));
                }

                ::printf(PSTR("entering deep sleep: %u\n"), _deepSleepParams.currentSleepTime);

                ESP.deepSleepInstant(_deepSleepParams.currentSleepTime * 1000ULL, _deepSleepParams.mode);
                ::printf(PSTR("entering deep sleep: %u\n"), 0);
                ESP.deepSleepInstant(0, _deepSleepParams.mode); // go to deep sleep indefinitely if the first call fails
            }
            else {
                ::printf(PSTR("executing full wakeup\n"));
            }
        }
        if (_deepSleepParams.tasks.connectWiFi == false) {
            ::printf(PSTR("wifi quick connect disabled\n"));
            return;
        }
    }
    else {
        _deepSleepParams = {};
    }

    wifiQuickConnect();
}

void KFCFWConfiguration::enterDeepSleep(milliseconds time, RFMode mode, uint16_t delayAfterPrepare)
{
    __DBG_printf("time=%d mode=%d delay_prep=%d", time.count(), mode, delayAfterPrepare);

#if RTC_SUPPORT
    _deepSleepParams.unixtime = getRTC();
#else
    time_t now = ::time(nullptr);
    if (IS_TIME_VALID(now)) {
        _deepSleepParams.unixtime = now;
    }
#endif

    // WiFiCallbacks::getVector().clear(); // disable WiFi callbacks to speed up shutdown
    // Scheduler.terminate(); // halt scheduler

    resetDetector.clearCounter();
    SaveCrash::removeCrashCounter();

    delay(1);
    uint32_t milliseconds = time.count();

    for(auto plugin: plugins) {
        plugin->prepareDeepSleep(milliseconds);
    }
    if (delayAfterPrepare) {
        delay(delayAfterPrepare);
    }
    __LDBG_printf("Entering deep sleep for %u milliseconds, RF mode %d", milliseconds, mode);

#if __LED_BUILTIN == -3
    BlinkLEDTimer::setBlink(__LED_BUILTIN, BlinkLEDTimer::OFF);
#endif

#if defined(ESP8266)
    _deepSleepParams = DeepSleep::DeepSleepParams_t(milliseconds, mode);
    RTCMemoryManager::write(RTCMemoryManager::RTCMemoryId::DEEP_SLEEP, &_deepSleepParams, sizeof(_deepSleepParams));

    ESP.deepSleep(_deepSleepParams.getSleepTimeMicros(), mode);
    // try again with half of the max. time if first call fails
    PrintString tmp = F("Deep sleep failed with time_us=");
    tmp.print(_deepSleepParams.getSleepTimeMicros());
    Logger_error(tmp);
    ESP.deepSleep(ESP.deepSleepMax() / 2, mode);
    // go to deep sleep indefinitely
    ESP.deepSleep(0, mode);
#else
    ESP.deepSleep(time.count() * 1000UL);
    ESP.deepSleep(0);
#endif
}

#endif

static void invoke_ESP_restart()
{
#if __LED_BUILTIN == -3
    BlinkLEDTimer::setBlink(__LED_BUILTIN, BlinkLEDTimer::OFF);
#endif
    ESP.restart();
}

#if PIN_MONITOR
#include "pin_monitor.h"
#endif

void KFCFWConfiguration::restartDevice(bool safeMode)
{
    __LDBG_println();

// enable debugging output (::printf) for shutdown sequence
#define DEBUG_SHUTDOWN_SEQUENCE DEBUG

    String msg = F("Device is being restarted");
    if (safeMode) {
        msg += F(" in SAFE MODE");
    }
    Logger_notice(msg);
    BlinkLEDTimer::setBlink(__LED_BUILTIN, BlinkLEDTimer::FLICKER);

    delay(500);

    SaveCrash::removeCrashCounterAndSafeMode();
    resetDetector.setSafeMode(safeMode);
    if (_safeMode) {
#if DEBUG_SHUTDOWN_SEQUENCE
    ::printf(PSTR("safe mode: invoking restart\n"));
#endif
        invoke_ESP_restart();
    }

    // clear queue silently
    ADCManager::terminate(false);

#if HTTP2SERIAL_SUPPORT
#if DEBUG_SHUTDOWN_SEQUENCE
    ::printf(PSTR("terminating http2serial instance\n"));
#endif
    Http2Serial::destroyInstance();
#endif

#if DEBUG_SHUTDOWN_SEQUENCE
    ::printf(PSTR("scheduled tasks %u, WiFi callbacks %u, Loop Functions %u\n"), _Scheduler.size(), WiFiCallbacks::getVector().size(), LoopFunctions::size());
#endif

    auto webUiSocket = WsWebUISocket::getWsWebUI();
    if (webUiSocket) {
#if DEBUG_SHUTDOWN_SEQUENCE
        ::printf(PSTR("terminating webui websocket=%p\n"), webUiSocket);
#endif
        webUiSocket->shutdown();
    }

    // execute in reverse order
    for(auto iterator = plugins.rbegin(); iterator != plugins.rend(); ++iterator) {
        const auto plugin = *iterator;
#if DEBUG_SHUTDOWN_SEQUENCE
        ::printf(PSTR("shutdown plugin=%s\n"), plugin->getName_P());
#endif
        plugin->shutdown();
        plugin->clearSetupTime();
    }

#if DEBUG_SHUTDOWN_SEQUENCE
    ::printf(PSTR("scheduled tasks %u, WiFi callbacks %u, Loop Functions %u\n"), _Scheduler.size(), WiFiCallbacks::getVector().size(), LoopFunctions::size());
#endif

#if DEBUG_SHUTDOWN_SEQUENCE
    ::printf(PSTR("terminating pin monitor\n"));
#endif
#if PIN_MONITOR
    pinMonitor.end();
#endif

#if DEBUG_SHUTDOWN_SEQUENCE
    ::printf(PSTR("terminating serial handlers\n"));
#endif
    serialHandler.end();
#if DEBUG
    debugStreamWrapper.clear();
#endif

#if DEBUG_SHUTDOWN_SEQUENCE
    ::printf(PSTR("clearing wifi callbacks\n"));
#endif
    WiFiCallbacks::clear();

#if DEBUG_SHUTDOWN_SEQUENCE
    ::printf(PSTR("clearing loop functions\n"));
#endif
    LoopFunctions::clear();

#if DEBUG_SHUTDOWN_SEQUENCE
    ::printf(PSTR("terminating event scheduler\n"));
#endif
    __Scheduler.end();

#if DEBUG_SHUTDOWN_SEQUENCE
    ::printf(PSTR("invoking restart\n"));
#endif
    invoke_ESP_restart();
}

void KFCFWConfiguration::loop()
{
    rng.loop();
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

String KFCFWConfiguration::getWiFiEncryptionType(uint8_t type)
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

bool KFCFWConfiguration::reconfigureWiFi()
{
    _debug_println();
    BOOTLOG_PRINTF("reconfigure WiFi");

    WiFi.persistent(false); // disable during disconnects since it saves the configuration
    WiFi.setAutoConnect(false);
    WiFi.setAutoReconnect(false);
    WiFi.disconnect(true);
    WiFi.softAPdisconnect(true);
    WiFi.persistent(true);

    return connectWiFi();
}

bool KFCFWConfiguration::connectWiFi()
{
    __LDBG_println();
    BOOTLOG_PRINTF("connecting to WiFi");
    setLastError(String());

    bool station_mode_success = false;
    bool ap_mode_success = false;

    auto flags = System::Flags::getConfig();
    if (flags.is_station_mode_enabled) {
        BOOTLOG_PRINTF("init station mode");
        __LDBG_print("init station mode");
        WiFi.setAutoConnect(false); // WiFi callbacks have to be installed first during boot
        WiFi.setAutoReconnect(true);

        auto network = Network::Settings::getConfig();

        bool result;
        if (flags.is_station_mode_dhcp_enabled) {
            result = WiFi.config((uint32_t)0, (uint32_t)0, (uint32_t)0, (uint32_t)0, (uint32_t)0);
        } else {
            result = WiFi.config(network.getLocalIp(), network.getGateway(), network.getSubnet(), network.getDns1(), network.getDns2());
        }
        if (!result) {
            PrintString message;
            message.printf_P(PSTR("Failed to configure Station Mode with %s"), flags.is_station_mode_dhcp_enabled ? PSTR("DHCP") : PSTR("static address"));
            setLastError(message);
            Logger_error(message);
        } else {

            if (WiFi.begin(Network::WiFi::getSSID(), Network::WiFi::getPassword()) == WL_CONNECT_FAILED) {
                String message = F("Failed to start Station Mode");
                setLastError(message);
                Logger_error(message);
            } else {
                __LDBG_printf("Station Mode SSID %s", Network::WiFi::getSSID());
                station_mode_success = true;
            }
        }
    }
    else {
        __LDBG_print("disabling station mode");
        WiFi.disconnect(true);
        station_mode_success = true;
    }

    if (flags.is_softap_enabled) {
        BOOTLOG_PRINTF("init AP mode");
        __LDBG_print("init AP mode");

        auto softAp = Network::SoftAP::getConfig();
        BOOTLOG_PRINTF("config=%p size=%u", &softAp, sizeof(softAp));

        BOOTLOG_PRINTF("WiFi.softAPConfig");
        if (!WiFi.softAPConfig(softAp.getAddress(), softAp.getGateway(), softAp.getSubnet())) {
            BOOTLOG_PRINTF("failed");
            String message = F("Cannot configure AP mode");
            setLastError(message);
            Logger_error(message);
        } else {

#if defined(ESP32)

            if (tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP) != ESP_OK) {
                _debug_println(F("tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP) failed"));
            }

            dhcps_lease_t lease;
            lease.enable = flags.softAPDHCPDEnabled;
            lease.start_ip.addr = softAp._dhcpStart;
            lease.end_ip.addr = softAp._dhcpEnd;

            if (tcpip_adapter_dhcps_option(TCPIP_ADAPTER_OP_SET, TCPIP_ADAPTER_REQUESTED_IP_ADDRESS, &lease, sizeof(lease)) != ESP_OK) {
                String message = F("Failed to configure DHCP server");
                setLastError(message);
                Logger_error(message);
            }
            else if (flags.softAPDHCPDEnabled && tcpip_adapter_dhcps_start(TCPIP_ADAPTER_IF_AP) != ESP_OK) {
                String message = F("Failed to start DHCP server");
                setLastError(message);
                Logger_error(message);
            }

#elif defined(ESP8266)

            // setup after WiFi.softAPConfig()
            struct dhcps_lease dhcp_lease;
            BOOTLOG_PRINTF("wifi_softap_dhcps_stop");
            wifi_softap_dhcps_stop();
            dhcp_lease.enable = flags.is_softap_dhcpd_enabled;
            dhcp_lease.start_ip.addr = softAp.dhcp_start;
            dhcp_lease.end_ip.addr = softAp.dhcp_end;
            BOOTLOG_PRINTF("wifi_softap_set_dhcps_lease");
            if (!wifi_softap_set_dhcps_lease(&dhcp_lease)) {
                String message = F("Failed to configure DHCP server");
                setLastError(message);
                Logger_error(message);
            } else {
                BOOTLOG_PRINTF("wifi_softap_dhcps_start");
                if (flags.is_softap_dhcpd_enabled) {
                    if (!wifi_softap_dhcps_start()) {
                        String message = F("Failed to start DHCP server");
                        setLastError(message);
                        Logger_error(message);
                    }
                }
            }

#endif

            BOOTLOG_PRINTF("WiFi.softAP");
            if (!WiFi.softAP(Network::WiFi::getSoftApSSID(), Network::WiFi::getSoftApPassword(), softAp.getChannel(), flags.is_softap_ssid_hidden)) {
                BOOTLOG_PRINTF("failed");
                String message = F("Cannot start AP mode");
                setLastError(message);
                Logger_error(message);
            } else {
                Logger_notice(F("AP Mode sucessfully initialized"));
                ap_mode_success = true;
            }
        }

        // install hnalder to enable AP mode if station mode goes down
        if (flags.is_softap_standby_mode_enabled) {
            WiFiCallbacks::add(WiFiCallbacks::EventType::CONNECTION, KFCFWConfiguration::apStandModehandler);
        } else {
            WiFiCallbacks::remove(WiFiCallbacks::EventType::ANY, KFCFWConfiguration::apStandModehandler);
        }

    }
    else {
        _debug_println(F("disabling AP mode"));
        WiFi.softAPdisconnect(true);
        ap_mode_success = true;
    }

    if (!station_mode_success || !ap_mode_success) {
        BlinkLEDTimer::setBlink(__LED_BUILTIN, BlinkLEDTimer::FAST);
        BOOTLOG_PRINTF("WiFi error");
    }

    auto hostname = System::Device::getName();
    BOOTLOG_PRINTF("hostname %p", hostname);
#if defined(ESP32)
    WiFi.setHostname(hostname);
    WiFi.softAPsetHostname(hostname);
#elif defined(ESP8266)
    WiFi.hostname(hostname);
#endif

    return (station_mode_success && ap_mode_success);
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
        output.printf_P(PSTR("Station Mode SSID %s\n"), Network::WiFi::getSSID());
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
    if (flags.is_at_mode_enabled) {
        output.println(F("Modified AT instruction set available.\n\nType AT? for help"));
    }
#endif
}

bool KFCFWConfiguration::isWiFiUp()
{
    return config._wifiUp != ~0UL;
}

unsigned long KFCFWConfiguration::getWiFiUp()
{
    return config._wifiUp;
}

TwoWire &KFCFWConfiguration::initTwoWire(bool reset, Print *output)
{
    if (output) {
        output->printf_P("I2C: SDA=%u, SCL=%u, clock stretch=%u, clock speed=%u, reset=%u\n", KFC_TWOWIRE_SDA, KFC_TWOWIRE_SCL, KFC_TWOWIRE_CLOCK_STRETCH, KFC_TWOWIRE_CLOCK_SPEED, reset);
    }
    if (!_initTwoWire || reset) {
        __LDBG_printf("SDA=%u,SCL=%u,stretch=%u,speed=%u,rst=%u", KFC_TWOWIRE_SDA, KFC_TWOWIRE_SCL, KFC_TWOWIRE_CLOCK_STRETCH, KFC_TWOWIRE_CLOCK_SPEED, reset);
        _initTwoWire = true;
        Wire.begin(KFC_TWOWIRE_SDA, KFC_TWOWIRE_SCL);
#if ESP8266
        Wire.setClockStretchLimit(KFC_TWOWIRE_CLOCK_STRETCH);
#endif
        Wire.setClock(KFC_TWOWIRE_CLOCK_SPEED);
    }
    return Wire;
}

bool KFCFWConfiguration::setRTC(uint32_t unixtime)
{
    __LDBG_printf("time=%u", unixtime);
#if RTC_SUPPORT
    initTwoWire();
    if (rtc.begin()) {
        rtc.adjust(DateTime(unixtime));
    }
#endif
    return false;
}

bool KFCFWConfiguration::rtcLostPower() {
#if RTC_SUPPORT
    initTwoWire();
    if (rtc.begin()) {
        return rtc.lostPower();
    }
#endif
    return true;
}

uint32_t KFCFWConfiguration::getRTC()
{
#if RTC_SUPPORT
    initTwoWire();
    if (rtc.begin()) {
        auto unixtime = rtc.now().unixtime();
        __LDBG_printf("time=%u", unixtime);
        if (rtc.lostPower()) {
            __LDBG_printf("time=0, lostPower=true");
            return 0;
        }
        return unixtime;
    }
#endif
    __LDBG_printf("time=error");
    return 0;
}

float KFCFWConfiguration::getRTCTemperature()
{
#if RTC_SUPPORT
    initTwoWire();
    if (rtc.begin()) {
        return rtc.getTemperature();
    }
#endif
    return NAN;
}

void KFCFWConfiguration::printRTCStatus(Print &output, bool plain)
{
#if RTC_SUPPORT
    initTwoWire();
    if (rtc.begin()) {
        auto now = rtc.now();
        if (plain) {
            output.print(F("timestamp="));
            output.print(now.timestamp());
#if RTC_DEVICE_DS3231
            output.printf_P(PSTR(" temperature=%.3f lost_power=%u"), rtc.getTemperature(), rtc.lostPower());
#endif
        }
        else {
            output.print(F("Timestamp: "));
            output.print(now.timestamp());
#if RTC_DEVICE_DS3231
            output.printf_P(PSTR(", temperature: %.2f" "\xb0" "C, lost power: %s"), rtc.getTemperature(), rtc.lostPower() ? SPGM(yes, "yes") : SPGM(no, "no"));
#endif
        }
    }
    else {
        output.print(F("Failed to initialize RTC"));
    }
#else
    output.print(F("RTC not supported"));
#endif
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

void KFCConfigurationPlugin::setup(SetupModeType mode)
{
    __DBG_printf("safe mode %d, wake up %d", (mode == SetupModeType::SAFE_MODE), resetDetector.hasWakeUpDetected());

    config.setup();

#if RTC_SUPPORT
    auto rtc = config.getRTC();
    if (rtc != 0) {
        struct timeval tv = { (time_t)rtc, 0 };
        settimeofday(&tv, nullptr);

#if NTP_LOG_TIME_UPDATE
        char buf[32];
        auto now = time(nullptr);
        strftime_P(buf, sizeof(buf), SPGM(strftime_date_time_zone), localtime(&now));
        Logger_notice(F("RTC time: %s"), buf);
#endif
    }
#endif

    if (WiFi.isConnected()) {
        __DBG_print("WiFi up, skipping init.");
        BlinkLEDTimer::setBlink(__LED_BUILTIN, BlinkLEDTimer::OFF);
        return;
    }

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
    if (String_equals(source, SPGM(network)) || String_equals(source, SPGM(wifi))) {
        config.reconfigureWiFi();
    }
}

WebTemplate *KFCConfigurationPlugin::getWebTemplate(const String &name)
{
    return __LDBG_new(StatusTemplate);
}

extern const char *session_get_token();
extern size_t session_get_token_min_size();

const char *session_get_token()
{
    return System::Device::getToken();
}

size_t session_get_token_min_size()
{
    return System::Device::kTokenMinSize;
}
