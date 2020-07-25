/**
 * Author: sascha_lammers@gmx.de
 */

#include "kfc_fw_config.h"
#include <EEPROM.h>
#include <PrintString.h>
#include <LoopFunctions.h>
#include <WiFiCallbacks.h>
#include <EventScheduler.h>
#include <EventTimer.h>
#include <session.h>
#include <misc.h>
#include "blink_led_timer.h"
#include "fs_mapping.h"
#include "WebUISocket.h"
#include "WebUIAlerts.h"
#include "web_server.h"
#include "build.h"
#if NTP_CLIENT
#include "../src/plugins/ntp/ntp_plugin.h"
#endif
#if MQTT_SUPPORT
#include "../src/plugins/mqtt/mqtt_client.h"
#include "../src/plugins/mqtt/mqtt_persistant_storage.h"
#endif

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

PROGMEM_STRING_DEF(safe_mode_enabled, "Device started in SAFE MODE");

KFCFWConfiguration config;

// Config_NTP

Config_NTP::Config_NTP() {
}

const char *Config_NTP::getTimezone()
{
    return config._H_STR(Config().ntp.timezone);
}

const char *Config_NTP::getPosixTZ()
{
    return config._H_STR(Config().ntp.posix_tz);
}

const char *Config_NTP::getServers(uint8_t num)
{
    switch(num) {
        case 0:
            return config._H_STR(Config().ntp.servers[0]);
        case 1:
            return config._H_STR(Config().ntp.servers[1]);
        case 2:
            return config._H_STR(Config().ntp.servers[2]);
    }
    return nullptr;
}

const char *Config_NTP::getUrl()
{
    return config._H_STR(Config().ntp.remote_tz_dst_ofs_url);
}

uint16_t Config_NTP::getNtpRfresh()
{
    return config._H_GET(Config().ntp.ntpRefresh);
}

void Config_NTP::defaults()
{
    ::config._H_SET(Config().ntp.ntpRefresh, 12 * 60);
    ::config._H_SET_STR(Config().ntp.timezone, F("Etc/Universal"));
    ::config._H_SET_STR(Config().ntp.posix_tz, F("UTC0"));
    ::config._H_SET_STR(Config().ntp.servers[0], F("pool.ntp.org"));
    ::config._H_SET_STR(Config().ntp.servers[1], F("time.nist.gov"));
    ::config._H_SET_STR(Config().ntp.servers[2], F("time.windows.com"));
}

// Config_MQTT

Config_MQTT::Config_MQTT()
{
    using KFCConfigurationClasses::System;

    config.port = (System::Flags::read()->mqttMode == MQTT_MODE_SECURE) ? 8883 : 1883;
    config.keepalive = 15;
    config.qos = 2;
}

void Config_MQTT::defaults()
{
    ::config._H_SET(Config().mqtt.config, Config_MQTT().config);
    ::config._H_SET_STR(Config().mqtt.username, emptyString);
    ::config._H_SET_STR(Config().mqtt.password, emptyString);
    ::config._H_SET_STR(Config().mqtt.topic, F("home/${device_name}"));
    ::config._H_SET_STR(Config().mqtt.discovery_prefix, F("homeassistant"));
}

Config_MQTT::config_t Config_MQTT::getConfig()
{
    return ::config._H_GET(Config().mqtt.config);
}

MQTTMode_t Config_MQTT::getMode()
{
    return static_cast<MQTTMode_t>(::config._H_GET(Config().flags).mqttMode);
}

const char *Config_MQTT::getHost()
{
    return ::config._H_STR(Config().mqtt.host);
}

const char *Config_MQTT::getUsername()
{
    return ::config._H_STR(Config().mqtt.username);
}

const char *Config_MQTT::Config_MQTT::getPassword()
{
    return ::config._H_STR(Config().mqtt.password);
}

const char *Config_MQTT::getTopic()
{
    return ::config._H_STR(Config().mqtt.topic);
}

const char *Config_MQTT::getDiscoveryPrefix()
{
    return ::config._H_STR(Config().mqtt.discovery_prefix);
}

const uint8_t *Config_MQTT::getFingerprint()
{
    return reinterpret_cast<const uint8_t *>(::config._H_STR(Config().mqtt.fingerprint));
}


// Config_Ping

const char *Config_Ping::getHost(uint8_t num)
{
    switch(num) {
        case 0:
            return ::config._H_STR(Config().ping.host1);
        case 1:
            return ::config._H_STR(Config().ping.host2);
        case 2:
            return ::config._H_STR(Config().ping.host3);
        case 3:
            return ::config._H_STR(Config().ping.host4);
        default:
            break;
    }
    return nullptr;
}

void Config_Ping::defaults()
{
    ::config._H_SET(Config().ping.config, Config_Ping().config);
    ::config._H_SET_STR(Config().ping.host1, F("${gateway}"));
    ::config._H_SET_STR(Config().ping.host2, F("8.8.8.8"));
    ::config._H_SET_STR(Config().ping.host3, F("www.google.com"));
}

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
{
    _setupWiFiCallbacks();
#if SAVE_CRASH_HAVE_CALLBACKS
    auto nextCallback = EspSaveCrash::getCallback();
    EspSaveCrash::setCallback([this, nextCallback](const EspSaveCrash::ResetInfo_t &info) {
        // release all memory in the event of a crash
        discard();
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
    _debug_printf_P(PSTR("KFCFWConfiguration::_onWiFiConnectCb(%s, %d, %s)\n"), event.ssid.c_str(), (int)event.channel, mac2String(event.bssid).c_str());
    if (!_wifiConnected) {

#if ENABLE_DEEP_SLEEP
        if (resetDetector.hasWakeUpDetected() && ResetDetectorPlugin::_deepSleepWifiTime == ~0) { // first connection after wakeup
            ResetDetectorPlugin::_deepSleepWifiTime = millis();
            Logger_notice(F("WiFi connected to %s after %u ms"), event.ssid.c_str(), ResetDetectorPlugin::_deepSleepWifiTime);
        }
        else
#endif
        {
            Logger_notice(F("WiFi connected to %s"), event.ssid.c_str());
        }

        _debug_printf_P(PSTR("Station: WiFi connected to %s, offline for %.3f, millis = %lu\n"), event.ssid.c_str(), _offlineSince == ~0UL ? 0 : ((millis() - _offlineSince) / 1000.0), millis());
        _debug_printf_P(PSTR("Free heap %s\n"), formatBytes(ESP.getFreeHeap()).c_str());
        _wifiConnected = true;
 #if ENABLE_DEEP_SLEEP
        config.storeQuickConnect(event.bssid, event.channel);
#endif

#if defined(ESP32)
        auto hostname = config.getDeviceName();
        _debug_printf_P(PSTR("WiFi.setHostname(%s)\n"), hostname);
        if (!WiFi.setHostname(hostname)) {
            _debug_printf_P(PSTR("WiFi.setHostname(%s) failed\n"), hostname);
        }
#endif

    }
}

void KFCFWConfiguration::_onWiFiDisconnectCb(const WiFiEventStationModeDisconnected &event)
{
    _debug_printf_P(PSTR("KFCFWConfiguration::_onWiFiDisconnectCb(%d = %s)\n"), (int)event.reason, WiFi_disconnect_reason(event.reason).c_str());
    if (_wifiConnected) {
        BlinkLEDTimer::setBlink(__LED_BUILTIN, BlinkLEDTimer::FAST);
        _debug_printf_P(PSTR("WiFi disconnected after %.3f seconds, millis = %lu\n"), ((_wifiUp == ~0UL) ? -1.0 : ((millis() - _wifiUp) / 1000.0)), millis());

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
    static EventScheduler::Timer _reconnectTimer;
    if (!_reconnectTimer.active()) {
        _reconnectTimer.add(60000, true, [this](EventScheduler::TimerPtr timer) {
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
    _debug_printf_P(PSTR("KFCFWConfiguration::_onWiFiGotIPCb(%s, %s, %s DNS %s, %s)\n"), ip.c_str(), mask.c_str(), gw.c_str(), dns1.c_str(), dns2.c_str());

    Logger_notice(F("%s: IP/Net %s/%s GW %s DNS: %s, %s"), config._H_GET(Config().flags).stationModeDHCPEnabled ? PSTR("DHCP") : PSTR("Static configuration"), ip.c_str(), mask.c_str(), gw.c_str(), dns1.c_str(), dns2.c_str());

    using KFCConfigurationClasses::System;

    BlinkLEDTimer::setBlink(__LED_BUILTIN, (System::Device::getStatusLedMode() == System::Device::StatusLEDModeEnum::SOLID_WHEN_CONNECTED) ? BlinkLEDTimer::SOLID : BlinkLEDTimer::OFF);
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
    _debug_printf_P(PSTR("KFCFWConfiguration::_onWiFiOnDHCPTimeoutCb()\n"));
#if defined(ESP32)
    Logger_error(F("Lost DHCP IP"));
#else
    Logger_error(F("DHCP timeout"));
#endif
    BlinkLEDTimer::setBlink(__LED_BUILTIN, BlinkLEDTimer::FLICKER);
}

void KFCFWConfiguration::_softAPModeStationConnectedCb(const WiFiEventSoftAPModeStationConnected &event)
{
    _debug_printf_P(PSTR("KFCFWConfiguration::_softAPModeStationConnectedCb()\n"));
    Logger_notice(F("Station connected [%s]"), mac2String(event.mac).c_str());
}

void KFCFWConfiguration::_softAPModeStationDisconnectedCb(const WiFiEventSoftAPModeStationDisconnected &event)
{
    _debug_printf_P(PSTR("KFCFWConfiguration::_softAPModeStationDisconnectedCb()\n"));
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
    _debug_printf_P(PSTR("event=%u\n"), event);
    if (event == WiFiCallbacks::EventType::CONNECTED) {
        WiFi.enableAP(false);
        Logger_notice(F("AP Mode disabled"));
    }
    else if (event == WiFiCallbacks::EventType::DISCONNECTED) {
        WiFi.enableAP(true);
        Logger_notice(F("AP Mode enabled"));
    }
}

using KFCConfigurationClasses::MainConfig;
using KFCConfigurationClasses::Network;
using KFCConfigurationClasses::System;
using KFCConfigurationClasses::Plugins;

void KFCFWConfiguration::recoveryMode()
{
    System::Device::setPassword(FSPGM(defaultPassword));
    Network::WiFiConfig::setSoftApPassword(FSPGM(defaultPassword));
    auto flags = System::Flags();
    flags->hiddenSSID = false;
    flags->softAPDHCPDEnabled = true;
    flags->wifiMode |= WIFI_AP;
    flags->webServerMode = HTTP_MODE_UNSECURE;
    flags->apStandByMode = false;
    config._H_SET(Config().http_port, 80);
    flags.write();
}

// TODO add option to keep WiFi SSID, username and password

void KFCFWConfiguration::restoreFactorySettings()
{
    _debug_println();
    PrintString str;

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
    _H_SET(Config().version, FIRMWARE_VERSION);
#if DEBUG
    if (length) {
        System::Firmware::setElfHash(hashCopy);
    }
#endif

    auto flags = System::Flags();
    flags.write();

    uint8_t mac[6];
    WiFi.macAddress(mac);
    auto deviceName = PrintString(F("KFC%02X%02X%02X"), mac[3], mac[4], mac[5]);
    System::Device::defaults();
    System::Device::setName(deviceName);
    System::Device::setTitle(FSPGM(KFC_Firmware, "KFC Firmware"));
    System::Device::setPassword(FSPGM(defaultPassword, "12345678"));

    Network::WiFiConfig::setSSID(deviceName);
    Network::WiFiConfig::setPassword(FSPGM(defaultPassword));
    Network::WiFiConfig::setSoftApSSID(deviceName);
    Network::WiFiConfig::setSoftApPassword(FSPGM(defaultPassword));
    Network::Settings::defaults();
    Network::SoftAP::defaults();

#if WEBSERVER_TLS_SUPPORT
    _H_SET(Config().http_port, flags.webServerMode == HTTP_MODE_SECURE ? 443 : 80);
#elif WEBSERVER_SUPPORT
    _H_SET(Config().http_port, 80);
#endif
#if NTP_CLIENT
    Config_NTP::defaults();
#endif
#if MQTT_SUPPORT
    Config_MQTT::defaults();
#endif
#if SYSLOG_SUPPORT
    _H_SET(Config().syslog_port, 514);
#endif

#if HOME_ASSISTANT_INTEGRATION
    Plugins::HomeAssistant::setApiEndpoint(F("http://<CHANGE_ME>:8123/api/"));
#endif
#if IOT_REMOTE_CONTROL
    Plugins::RemoteControl::defaults();
#endif

#if PING_MONITOR_SUPPORT
    Config_Ping::defaults();
#endif
#if SERIAL2TCP_SUPPORT
    Plugins::Serial2TCP::defaults();
#endif
#if IOT_DIMMER_MODULE || IOT_ATOMIC_SUN_V2
    DimmerModule dimmer;
    dimmer.config_valid = false;
    dimmer.on_off_fade_time = 7.5;
    dimmer.fade_time = 5.0;
#if IOT_ATOMIC_SUN_V2
#ifdef IOT_ATOMIC_SUN_CHANNEL_WW1
    dimmer.channel_mapping[0] = IOT_ATOMIC_SUN_CHANNEL_WW1;
    dimmer.channel_mapping[1] = IOT_ATOMIC_SUN_CHANNEL_WW2;
    dimmer.channel_mapping[2] = IOT_ATOMIC_SUN_CHANNEL_CW1;
    dimmer.channel_mapping[3] = IOT_ATOMIC_SUN_CHANNEL_CW2;
#else
    dimmer.channel_mapping[0] = 0;
    dimmer.channel_mapping[1] = 1;
    dimmer.channel_mapping[2] = 2;
    dimmer.channel_mapping[3] = 3;
#endif
#endif
    _H_SET(Config().dimmer, dimmer);
#if IOT_DIMMER_MODULE_HAS_BUTTONS
    DimmerModuleButtons dimmer_buttons;
    dimmer_buttons.shortpress_time = 250;
    dimmer_buttons.longpress_time = 600;
    dimmer_buttons.repeat_time = 75;
    dimmer_buttons.shortpress_no_repeat_time = 650;
    dimmer_buttons.min_brightness = 25;
    dimmer_buttons.shortpress_step = 2;
    dimmer_buttons.longpress_max_brightness = 100;
    dimmer_buttons.longpress_min_brightness = 45;
    dimmer_buttons.shortpress_fadetime = 3.0;
    dimmer_buttons.longpress_fadetime = 4.5;
    memset(&dimmer_buttons.pins, 0, sizeof(dimmer_buttons.pins));
#if IOT_SENSOR_HAVE_HLW8012
    dimmer_buttons.pins[0] = D2;
    dimmer_buttons.pins[1] = D7;
#else
    dimmer_buttons.pins[0] = D6;
    dimmer_buttons.pins[1] = D7;
#endif
    _H_SET(Config().dimmer_buttons, dimmer_buttons);
#endif
#endif

#if IOT_BLINDS_CTRL
    BlindsController blinds;
    blinds.swap_channels = 1;
    blinds.channel0_dir = 0;
    blinds.channel1_dir = 0;
    blinds.channels[0].pwmValue = 600;
    blinds.channels[0].currentLimit = 110;
    blinds.channels[0].currentLimitTime = 50;
    blinds.channels[0].openTime = 2000;
    blinds.channels[0].closeTime = 3100;
    blinds.channels[1].pwmValue = 900;
    blinds.channels[1].currentLimit = 140;
    blinds.channels[1].currentLimitTime = 50;
    blinds.channels[1].openTime = 7500;
    blinds.channels[1].closeTime = 7500;
    _H_SET(Config().blinds_controller, blinds);
#endif

#if IOT_ALARM_PLUGIN_ENABLED
    Plugins::Alarm::defaults();
#endif

#if IOT_WEATHER_STATION
    Plugins::WeatherStation::defaults();
#endif

#if defined(IOT_SENSOR_HLW8012_U)
    {
        auto sensor = Config_Sensor();
        sensor.hlw80xx.calibrationU = IOT_SENSOR_HLW8012_U;
        sensor.hlw80xx.calibrationI = IOT_SENSOR_HLW8012_I;
        sensor.hlw80xx.calibrationP = IOT_SENSOR_HLW8012_P;
        config._H_SET(Config().sensor, sensor);

    }
#elif IOT_SENSOR
    _H_SET(Config().sensor, Config_Sensor());
#endif

#if IOT_CLOCK
    {
        auto cfg = _H_GET(Config().clock);
        cfg = {};
        cfg.animation = 0;
        cfg.time_format_24h = 1;
        cfg.solid_color.value = 0x00ff00;
        cfg.auto_brightness = -1;
        cfg.brightness = 128;
        cfg.protection = { 65, 55, 75 };
        cfg.rainbow = { 5.23, 30, 0xffffff, 0 };
        cfg.alarm = { 0xff0000, 250 };
        cfg.blink_colon_speed = 1000;
        cfg.flashing_speed = 150;
        cfg.fading = { 7.5, 2, 0xffffff };
        // cfg.segmentOrder = 0;
        // static const int8_t order[8] PROGMEM = { 0, 1, -1, 2, 3, -2, 4, 5 }; // <1st digit> <2nd digit> <1st colon> <3rd digit> <4th digit> <2nd colon> <5th digit> <6th digit>
        // memcpy_P(cfg.order, order, sizeof(cfg.order));
        _H_SET(Config().clock, cfg);
    }
#endif

#if CUSTOM_CONFIG_PRESET
    customSettings();
#endif
    WebUIAlerts_notice(F("Factory settings restored"));
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
    _debug_printf_P(PSTR("bssid=%s channel=%d\n"), mac2String(bssid).c_str(), channel);

    Config_QuickConnect::WiFiQuickConnect_t quickConnect;
    RTCMemoryManager::read(CONFIG_RTC_MEM_ID, &quickConnect, sizeof(quickConnect));
    quickConnect.channel = channel;
    memcpy(quickConnect.bssid, bssid, WL_MAC_ADDR_LENGTH);
    RTCMemoryManager::write(CONFIG_RTC_MEM_ID, &quickConnect, sizeof(quickConnect));
}

void KFCFWConfiguration::storeStationConfig(uint32_t ip, uint32_t netmask, uint32_t gateway)
{
    _debug_printf_P(PSTR("ip=%s netmask=%s gw=%s dhcp=%u\n"),
        IPAddress(ip).toString().c_str(),
        IPAddress(netmask).toString().c_str(),
        IPAddress(gateway).toString().c_str(),
        (wifi_station_dhcpc_status() == DHCP_STARTED)
    );

    Config_QuickConnect::WiFiQuickConnect_t quickConnect;
    if (RTCMemoryManager::read(CONFIG_RTC_MEM_ID, &quickConnect, sizeof(quickConnect))) {
        quickConnect.local_ip = ip;
        quickConnect.subnet = netmask;
        quickConnect.gateway = gateway;
        quickConnect.dns1 = (uint32_t)WiFi.dnsIP();
        quickConnect.dns2 = (uint32_t)WiFi.dnsIP(1);
        auto flags = config._H_GET(Config().flags);
        quickConnect.use_static_ip = flags.useStaticIPDuringWakeUp || !flags.stationModeDHCPEnabled;
        RTCMemoryManager::write(CONFIG_RTC_MEM_ID, &quickConnect, sizeof(quickConnect));
    } else {
        _debug_println(F("reading RTC memory failed"));
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

void KFCFWConfiguration::read()
{
    _debug_println();

    if (!Configuration::read()) {
        Logger_error(F("Failed to read configuration, restoring factory settings"));
        config.restoreFactorySettings();
        Configuration::write();
    } else {
        auto version = config._H_GET(Config().version);
        if (FIRMWARE_VERSION > version) {
            Logger_warning(F("Upgrading EEPROM settings from %d.%d.%d to " FIRMWARE_VERSION_STR), (version >> 16), (version >> 8) & 0xff, (version & 0xff));
            config._H_SET(Config().version, FIRMWARE_VERSION);
            Configuration::write();
        }
    }
}

void KFCFWConfiguration::write()
{
    _debug_println(F("KFCFWConfiguration::write()"));

    auto flags = config._H_GET(Config().flags);
    if (flags.isFactorySettings) {
        flags.isFactorySettings = false;
        config._H_SET(Config().flags, flags);
    }

    if (!Configuration::write()) {
        Logger_error(F("Failure to write settings to EEPROM"));
    }
}

void KFCFWConfiguration::wifiQuickConnect()
{
    _debug_println();

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
        Config_QuickConnect::WiFiQuickConnect_t quickConnect;

        if (RTCMemoryManager::read(PluginComponent::RTCMemoryId::CONFIG, &quickConnect, sizeof(quickConnect))) {
            channel = quickConnect.channel;
            bssidPtr = quickConnect.bssid;
        } else {
            channel = 0;
            // bssidPtr = nullptr;
        }

        if (channel <= 0 || !bssidPtr) {

            _debug_printf_P(PSTR("Cannot read quick connect from RTC memory, running WiFi.begin(%s, ***) only\n"), config.ssid);
            if (WiFi.begin(reinterpret_cast<char *>(config.ssid), reinterpret_cast<char *>(config.password)) != WL_DISCONNECTED) {
                Logger_error(F("Failed to start WiFi"));
            }

        } else {

            if (quickConnect.use_static_ip && quickConnect.local_ip) {
                _debug_printf_P(PSTR("configuring static ip\n"));
                WiFi.config(quickConnect.local_ip, quickConnect.gateway, quickConnect.subnet, quickConnect.dns1, quickConnect.dns2);
            }

            wl_status_t result;
            if ((result = WiFi.begin(reinterpret_cast<char *>(config.ssid), reinterpret_cast<char *>(config.password), channel, bssidPtr, true)) != WL_DISCONNECTED) {
                Logger_error(F("Failed to start WiFi"));
            }

            _debug_printf_P(PSTR("WiFi.begin() = %d, channel %d, bssid %s, config: static ip %d, %s/%s gateway %s, dns %s, %s\n"),
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

void KFCFWConfiguration::wakeUpFromDeepSleep()
{
    wifiQuickConnect();
}

void KFCFWConfiguration::enterDeepSleep(milliseconds time, RFMode mode, uint16_t delayAfterPrepare)
{
    _debug_printf_P(PSTR("time=%d mode=%d delay_prep=%d\n"), time.count(), mode, delayAfterPrepare);

    // WiFiCallbacks::getVector().clear(); // disable WiFi callbacks to speed up shutdown
    // Scheduler.terminate(); // halt scheduler

    resetDetector.clearCounter();
    SaveCrash::removeCrashCounter();

    delay(1);

    for(auto plugin: plugins) {
        plugin->prepareDeepSleep(time.count());
    }
    if (delayAfterPrepare) {
        delay(delayAfterPrepare);
    }
    _debug_printf_P(PSTR("Entering deep sleep for %d milliseconds, RF mode %d\n"), time.count(), mode);

#if __LED_BUILTIN == -3
    BlinkLEDTimer::setBlink(__LED_BUILTIN, BlinkLEDTimer::OFF);
#endif

#if defined(ESP8266)
    ESP.deepSleep(time.count() * 1000ULL, mode);
    ESP.deepSleep(0, mode); // if the first attempt fails try with 0
#else
    ESP.deepSleep(time.count() * 1000UL);
    ESP.deepSleep(0);
#endif
}

#endif

static uint32_t restart_device_timeout;

static void invoke_ESP_restart()
{
#if __LED_BUILTIN == -3
    BlinkLEDTimer::setBlink(__LED_BUILTIN, BlinkLEDTimer::OFF);
#endif
    ESP.restart();
}

static void restart_device()
{
    if (millis() > restart_device_timeout) {
        _debug_println();
        invoke_ESP_restart();
    }
}

#if PIN_MONITOR
#include "pin_monitor.h"
#endif

void KFCFWConfiguration::restartDevice(bool safeMode)
{
    _debug_println();

    String msg = F("Device is being restarted");
    if (safeMode) {
        msg += F(" in SAFE MODE");
    }
    Logger_notice(msg);
    BlinkLEDTimer::setBlink(__LED_BUILTIN, BlinkLEDTimer::FLICKER);

    SaveCrash::removeCrashCounterAndSafeMode();
    resetDetector.setSafeMode(safeMode);
    if (_safeMode) {
        invoke_ESP_restart();
    }

    _debug_printf_P(PSTR("Scheduled tasks %u, WiFi callbacks %u, Loop Functions %u\n"), Scheduler.size(), WiFiCallbacks::getVector().size(), LoopFunctions::size());

    auto webUiSocket = WsWebUISocket::getWsWebUI();
    if (webUiSocket) {
        debug_printf_P(PSTR("websocket %p\n"), webUiSocket);
        webUiSocket->shutdown();
    }

    // execute in reverse order
    for(auto iterator = plugins.rbegin(); iterator != plugins.rend(); ++iterator) {
        const auto plugin = *iterator;
        debug_printf("plugin=%s\n", plugin->getName_P());
        plugin->shutdown();
        plugin->clearSetupTime();
    }

    _debug_printf_P(PSTR("After plugins: Scheduled tasks %u, WiFi callbacks %u, Loop Functions %u\n"), Scheduler.size(), WiFiCallbacks::getVector().size(), LoopFunctions::size());

    WiFiCallbacks::clear();
    LoopFunctions::clear();
    Scheduler.end();

#if PIN_MONITOR
    _debug_printf_P(PSTR("Pin monitor has %u pins attached\n"), PinMonitor::getInstance() ? PinMonitor::getInstance()->size() : 0);
    PinMonitor::deleteInstance();
#endif

    // give system time to finish all tasks
    restart_device_timeout = millis() + 250;
    LoopFunctions::add(restart_device);
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
    setLastError(String());

    bool station_mode_success = false;
    bool ap_mode_success = false;

    auto flags = config._H_GET(Config().flags);
    if (flags.wifiMode & WIFI_STA) {
        _debug_println(F("init station mode"));
        WiFi.setAutoConnect(false); // WiFi callbacks have to be installed first during boot
        WiFi.setAutoReconnect(true);

        auto network = config._H_GET(MainConfig().network.settings);

        bool result;
        if (flags.stationModeDHCPEnabled) {
            result = WiFi.config((uint32_t)0, (uint32_t)0, (uint32_t)0, (uint32_t)0, (uint32_t)0);
        } else {
            result = WiFi.config(network.localIp(), network.gateway(), network.subnet(), network.dns1(), network.dns2());
        }
        if (!result) {
            PrintString message;
            message.printf_P(PSTR("Failed to configure Station Mode with %s"), flags.stationModeDHCPEnabled ? PSTR("DHCP") : PSTR("static address"));
            setLastError(message);
            Logger_error(message);
        } else {
            if (WiFi.begin(config._H_STR(MainConfig().network.WiFiConfig._ssid), config._H_STR(MainConfig().network.WiFiConfig._password)) == WL_CONNECT_FAILED) {
                String message = F("Failed to start Station Mode");
                setLastError(message);
                Logger_error(message);
            } else {
                _debug_printf_P(PSTR("Station Mode SSID %s\n"), config._H_STR(MainConfig().network.WiFiConfig._ssid));
                station_mode_success = true;
            }
        }
    }
    else {
        _debug_println(F("disabling station mode"));
        WiFi.disconnect(true);
        station_mode_success = true;
    }

    if (flags.wifiMode & WIFI_AP) {
        _debug_println(F("init AP mode"));

        // config._H_GET(Config().soft_ap.encryption not used

        auto softAp = config._H_GET(MainConfig().network.softAp);

        if (!WiFi.softAPConfig(softAp.address(), softAp.gateway(), softAp.subnet())) {
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
            wifi_softap_dhcps_stop();
            dhcp_lease.enable = flags.softAPDHCPDEnabled;
            dhcp_lease.start_ip.addr = softAp._dhcpStart;
            dhcp_lease.end_ip.addr = softAp._dhcpEnd;
            if (!wifi_softap_set_dhcps_lease(&dhcp_lease)) {
                String message = F("Failed to configure DHCP server");
                setLastError(message);
                Logger_error(message);
            } else {
                if (flags.softAPDHCPDEnabled) {
                    if (!wifi_softap_dhcps_start()) {
                        String message = F("Failed to start DHCP server");
                        setLastError(message);
                        Logger_error(message);
                    }
                }
            }

#endif

            if (!WiFi.softAP(Network::WiFiConfig::getSoftApSSID(), Network::WiFiConfig::getSoftApPassword(), softAp.channel(), flags.hiddenSSID)) {
                String message = F("Cannot start AP mode");
                setLastError(message);
                Logger_error(message);
            } else {
                Logger_notice(F("AP Mode sucessfully initialized"));
                ap_mode_success = true;
            }
        }

        // install hnalder to enable AP mode if station mode goes down
        if (flags.apStandByMode) {
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
    }

    auto hostname = config.getDeviceName();
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
        Logger_notice(FSPGM(safe_mode_enabled));
        output.println(FSPGM(safe_mode_enabled));
    }
}

void KFCFWConfiguration::printInfo(Print &output)
{
    printVersion(output);

    auto flags = config._H_GET(Config().flags);
    if (flags.wifiMode & WIFI_AP) {
        output.printf_P(PSTR("AP Mode SSID %s\n"), Network::WiFiConfig::getSoftApSSID());
    }
    if (flags.wifiMode & WIFI_STA) {
        output.printf_P(PSTR("Station Mode SSID %s\n"), Network::WiFiConfig::getSSID());
    }
    if (flags.isFactorySettings) {
        output.println(F("Running on factory settings"));
    }
    if (flags.isDefaultPassword) {
        Logger_security(FSPGM(default_password_warning));
        output.println(FSPGM(default_password_warning, "WARNING! Default password has not been changed"));
    }
    output.printf_P(PSTR("Device %s ready!\n"), config.getDeviceName());

#if AT_MODE_SUPPORTED
    if (flags.atModeEnabled) {
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
        _debug_printf_P("SDA=%u,SCL=%u,stretch=%u,speed=%u,rst=%u\n", KFC_TWOWIRE_SDA, KFC_TWOWIRE_SCL, KFC_TWOWIRE_CLOCK_STRETCH, KFC_TWOWIRE_CLOCK_SPEED, reset);
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
    _debug_printf_P(PSTR("time=%u\n"), unixtime);
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
        _debug_printf_P(PSTR("time=%u\n"), unixtime);
        if (rtc.lostPower()) {
            _debug_printf_P(PSTR("time=0, lostPower=true\n"));
            return 0;
        }
        return unixtime;
    }
#endif
    _debug_printf_P(PSTR("time=error\n"));
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
            output.printf_P(PSTR(", temperature: %.2f&deg;C, lost power: %s"), rtc.getTemperature(), rtc.lostPower() ? SPGM(yes, "yes") : SPGM(no, "no"));
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

const char *KFCFWConfiguration::getDeviceName() const
{
    using KFCConfigurationClasses::System;
    return System::Device::getName();
}

const char *KFCFWConfiguration::getDeviceTitle() const
{
    using KFCConfigurationClasses::System;
    return System::Device::getTitle();
}

bool KFCFWConfiguration::callPersistantConfig(ContainerPtr data, PersistantConfigCallback callback)
{
#if MQTT_SUPPORT
    bool result = false;
    auto client = MQTTClient::getClient();
    if (client && client->isConnected()) {
        result = MQTTPersistantStorageComponent::create(client, data, callback);
    }
#endif
    return _debug_print_result(result);
}

class KFCConfigurationPlugin : public PluginComponent {
public:
    KFCConfigurationPlugin();

    virtual void setup(SetupModeType mode) override;
    virtual void reconfigure(const String &source) override;
    virtual void createConfigureForm(FormCallbackType type, const String &formName, Form &form, AsyncWebServerRequest *request) override;
    virtual WebTemplate *getWebTemplate(const String &templateName) override;
};

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
    _debug_printf_P(PSTR("safe mode %d, wake up %d\n"), (mode == SetupModeType::SAFE_MODE), resetDetector.hasWakeUpDetected());

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
        debug_println(F("WiFi up, skipping init."));
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
    config.reconfigureWiFi();
}

void KFCConfigurationPlugin::createConfigureForm(FormCallbackType type, const String &formName, Form &form, AsyncWebServerRequest *request)
{
    using KFCConfigurationClasses::MainConfig;
    using KFCConfigurationClasses::System;

    if (type == FormCallbackType::SAVE) {
        if (String_equals(formName, F("password"))) {
            auto &flags = System::Flags::getWriteable();
            flags.isDefaultPassword = false;
        }
        else if (String_equals(formName, F("device"))) {
            config.setConfigDirty(true);
        }
    }
    else if (isCreateFormCallbackType(type)) {

        if (String_equals(formName, F("wifi"))) {

            form.add<uint8_t>(F("mode"), _H_FLAGS_VALUE(Config().flags, wifiMode));
            form.addValidator(new FormRangeValidator(F("Invalid mode"), WIFI_OFF, WIFI_AP_STA));

            form.add(F("wifi_ssid"), _H_STR_VALUE(MainConfig().network.WiFiConfig._ssid));
            form.addValidator(new FormLengthValidator(1, sizeof(MainConfig().network.WiFiConfig._ssid) - 1));

            form.add(F("wifi_password"), _H_STR_VALUE(MainConfig().network.WiFiConfig._password));
            form.addValidator(new FormLengthValidator(8, sizeof(MainConfig().network.WiFiConfig._password) - 1));

            form.add(F("ap_wifi_ssid"), _H_STR_VALUE(MainConfig().network.WiFiConfig._softApSsid));
            form.addValidator(new FormLengthValidator(1, sizeof(MainConfig().network.WiFiConfig._softApSsid) - 1));

            form.add(F("ap_wifi_password"), _H_STR_VALUE(MainConfig().network.WiFiConfig._softApPassword));
            form.addValidator(new FormLengthValidator(8, sizeof(MainConfig().network.WiFiConfig._softApPassword) - 1));

            form.add<uint8_t>(F("channel"), _H_STRUCT_VALUE(MainConfig().network.softAp, _channel));
            form.addValidator(new FormRangeValidator(1, config.getMaxWiFiChannels()));

            form.add<uint8_t>(F("encryption"), _H_STRUCT_VALUE(MainConfig().network.softAp, _encryption));

            form.addValidator(new FormEnumValidator<uint8_t, WiFiEncryptionTypeArray().size()>(F("Invalid encryption"), createWiFiEncryptionTypeArray()));

            form.add<bool>(F("ap_hidden"), _H_FLAGS_BOOL_VALUE(Config().flags, hiddenSSID), FormField::InputFieldType::CHECK);
            form.add<bool>(F("ap_standby_mode"), _H_FLAGS_BOOL_VALUE(Config().flags, apStandByMode), FormField::InputFieldType::SELECT);

        }
        else if (String_equals(formName, F("network"))) {

            form.add(F("hostname"), _H_STR_VALUE(Config().device_name));
            form.addValidator(new FormLengthValidator(4, sizeof(Config().device_name) - 1));
            form.add(FSPGM(title), _H_STR_VALUE(Config().device_title));
            form.addValidator(new FormLengthValidator(1, sizeof(Config().device_title) - 1));

            form.add<bool>(F("dhcp_client"), _H_FLAGS_BOOL_VALUE(Config().flags, stationModeDHCPEnabled));

            form.add(F("ip_address"), _H_STRUCT_IP_VALUE(MainConfig().network.settings, _localIp));
            form.add(F("subnet"), _H_STRUCT_IP_VALUE(MainConfig().network.settings, _subnet));
            form.add(F("gateway"), _H_STRUCT_IP_VALUE(MainConfig().network.settings, _gateway));
            form.add(F("dns1"), _H_STRUCT_IP_VALUE(MainConfig().network.settings, _dns1));
            form.add(F("dns2"), _H_STRUCT_IP_VALUE(MainConfig().network.settings, _dns2));

            form.add<bool>(F("softap_dhcpd"), _H_FLAGS_BOOL_VALUE(Config().flags, softAPDHCPDEnabled));

            form.add(F("dhcp_start"), _H_STRUCT_IP_VALUE(MainConfig().network.softAp, _dhcpStart));
            form.add(F("dhcp_end"), _H_STRUCT_IP_VALUE(MainConfig().network.softAp, _dhcpEnd));
            form.add(F("ap_ip_address"), _H_STRUCT_IP_VALUE(MainConfig().network.softAp, _address));
            form.add(F("ap_subnet"), _H_STRUCT_IP_VALUE(MainConfig().network.softAp, _subnet));

        }
        else if (String_equals(formName, F("device"))) {

            form.add(FSPGM(title), _H_STR_VALUE(Config().device_title));
            form.addValidator(new FormLengthValidator(1, sizeof(Config().device_title) - 1));

            form.add<uint16_t>(F("safe_mode_reboot_time"), _H_STRUCT_VALUE(MainConfig().system.device.settings, _safeModeRebootTime));
            form.addValidator(new FormRangeValidator(5, 1440, true));

            form.add<bool>(F("enable_mdns"), _H_FLAGS_BOOL_VALUE(Config().flags, enableMDNS));

            form.add<uint16_t>(F("webui_keep_logged_in_days"), _H_STRUCT_VALUE(MainConfig().system.device.settings, _webUIKeepLoggedInDays));
            form.addValidator(new FormRangeValidator(3, 180, true));

            form.add<bool>(F("disable_webalerts"), _H_FLAGS_BOOL_VALUE(Config().flags, disableWebAlerts));
            form.add<bool>(F("disable_webui"), _H_FLAGS_BOOL_VALUE(Config().flags, disableWebUI));
            form.add<uint8_t>(F("status_led_mode"), _H_STRUCT_VALUE(MainConfig().system.device.settings, _statusLedMode));

        }
        else if (String_equals(formName, F("password"))) {

            form.add(new FormField(FSPGM(password), System::Device::getPassword()));
            form.addValidator(new FormMatchValidator(F("The entered password is not correct"), [](FormField &field) {
                return field.getValue().equals(System::Device::getPassword());
            }));

            form.add(F("password2"), _H_STR_VALUE(Config().device_pass))
                ->setValue(emptyString);

            form.addValidator(new FormRangeValidator(F("The password has to be at least %min% characters long"), 6, 0));
            form.addValidator(new FormRangeValidator(6, sizeof(Config().device_pass) - 1))
                ->setValidateIfValid(false);

            form.add(F("password3"), emptyString, FormField::InputFieldType::TEXT);
            form.addValidator(new FormMatchValidator(F("The password confirmation does not match"), [](FormField &field) {
                    return field.equals(field.getForm().getField(F("password2")));
                }))
                ->setValidateIfValid(false);

        }
        else {
            return;
        }
        form.finalize();
    }
}

WebTemplate *KFCConfigurationPlugin::getWebTemplate(const String &name)
{
    return new StatusTemplate();
}
