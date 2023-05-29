/**
 * Author: sascha_lammers@gmx.de
 */

#include <../include/time.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>
#include <EventScheduler.h>
#include <stdlib.h>
#include <PrintHtmlEntitiesString.h>
#include <MicrosTimer.h>
#include "../include/templates.h"
#include "logger.h"
#include "plugins.h"
#include "ntp_plugin_class.h"
#include "ntp_plugin.h"

#if defined(ESP8266)
#include <sntp.h>
#include <coredecls.h>
#elif defined(ESP32)
#include <lwip/apps/sntp.h>
#include <esp_sntp.h>
#endif

#if DEBUG_NTP_CLIENT
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using KFCConfigurationClasses::System;
using Plugins = KFCConfigurationClasses::PluginsType;

#if NTP_HAVE_CALLBACKS

static std::vector<TimeUpdatedCallback_t> _callbacks;

void addTimeUpdatedCallback(TimeUpdatedCallback_t callback)
{
    __LDBG_printf("func=%p", &callback);
    _callbacks.push_back(callback);
}

#endif

static NTPPlugin plugin;

PROGMEM_DEFINE_PLUGIN_OPTIONS(
    NTPPlugin,
    "ntp",              // name
    "NTP Client",       // friendly name
    "",                 // web_templates
    "ntp",              // config_forms
    "",                 // reconfigure_dependencies
    PluginComponent::PriorityType::NTP,
    PluginComponent::RTCMemoryId::NONE,
    static_cast<uint8_t>(PluginComponent::MenuType::AUTO),
    false,              // allow_safe_mode
    true,               // setup_after_deep_sleep
    true,               // has_get_status
    true,               // has_config_forms
    false,              // has_web_ui
    false,              // has_web_templates
    true,               // has_at_mode
    0                   // __reserved
);

NTPPlugin::NTPPlugin() :
    PluginComponent(PROGMEM_GET_PLUGIN_OPTIONS(NTPPlugin)),
    _callbackState(CallbackState::STARTUP),
    _lastUpdateSeconds(0)
{
    REGISTER_PLUGIN(this, "NTPPlugin");
}

#if SNTP_STARTUP_DELAY

#ifndef SNTP_STARTUP_DELAY_MAX_TIME
#define SNTP_STARTUP_DELAY_MAX_TIME         15000
#endif

#ifndef SNTP_STARTUP_DELAY_MIN_TIME
#define SNTP_STARTUP_DELAY_MIN_TIME         10000
#endif

#if (SNTP_STARTUP_DELAY_MAX_TIME - SNTP_STARTUP_DELAY_MIN_TIME) < 0
#error invalid range SNTP_STARTUP_DELAY_MIN_TIME - SNTP_STARTUP_DELAY_MAX_TIME
#endif

#if SNTP_STARTUP_DELAY_MIN_TIME < 500
#error invalid SNTP_STARTUP_DELAY_MIN_TIME
#endif

inline static uint32_t plusMinusPercent(uint32_t value, uint8_t pct)
{
    int32_t rnd = value / (100 / pct);
    rnd = rand() % rnd;
    return rand() % 2 == 0 ? value + rnd : value - rnd;
}

extern "C" uint32_t sntp_startup_delay_MS_rfc_not_less_than_60000();

static uint32_t _startUpDelay = SNTP_STARTUP_DELAY_MAX_TIME;

uint32_t sntp_startup_delay_MS_rfc_not_less_than_60000()
{
    _startUpDelay = (rand() % (SNTP_STARTUP_DELAY_MAX_TIME - SNTP_STARTUP_DELAY_MIN_TIME)) + SNTP_STARTUP_DELAY_MIN_TIME;
    if (plugin._callbackState == NTPPlugin::CallbackState::STARTUP) {
        if (_startUpDelay - (int32_t)millis() > 1000) {
            _startUpDelay -= millis();
        }
        else {
            _startUpDelay = 1000;
        }
    }
    __LDBG_printf("sntp_startup_delay_MS_rfc_not_less_than_60000=%u", _startUpDelay);
    return _startUpDelay;
}

#endif

// sntp_get_sync_interval
extern "C" uint32_t sntp_update_delay_MS_rfc_not_less_than_15000();

static uint32_t _updateDelay = 3600000;

uint32_t sntp_update_delay_MS_rfc_not_less_than_15000()
{
    __LDBG_printf("sntp_update_delay_MS_rfc_not_less_than_15000=%u", _updateDelay);
    return _updateDelay;
}

void NTPPlugin::setup(SetupModeType mode, const DependenciesPtr &dependencies)
{
    __LDBG_printf("setup");
    _updateDelay = plusMinusPercent(Plugins::NTPClient::getConfig().getRefreshIntervalMillis(), 5);
    if (System::Flags::getConfig().is_ntp_client_enabled) {
        _callbackState = CallbackState::STARTUP;
        _execConfigTime();
    }
    else {
        safeSetTZ(F("GMT"));
        shutdown();
    }
}

void NTPPlugin::reconfigure(const String &source)
{
    shutdown();
    setup(SetupModeType::DEFAULT, nullptr);
}

void NTPPlugin::shutdown()
{
    _callbackState = CallbackState::SHUTDOWN;
    _Timer(_checkTimer).remove();
    settimeofday_cb((settimeofday_cb_t)nullptr);
    sntp_stop();
    for (uint8_t i = 0; i < Plugins::NTPClient::kServersMax; i++) {
        // remove pointer to server before releasing memory
        sntp_setservername(i, nullptr);
        if (_servers[i]) {
            free(_servers[i]);
            _servers[i] = nullptr;
        }
    }
}

void NTPPlugin::getStatus(Print &output)
{
    if (System::Flags::getConfig().is_ntp_client_enabled) {

        output.printf_P(PSTR("Timezone %s, "), Plugins::NTPClient::getTimezoneName());
        static_cast<PrintString &>(output).strftime_P(PSTR("%z %Z"), time(nullptr));

        auto firstServer = true;
        for (uint8_t i = 0; i < Plugins::NTPClient::kServersMax; i++) {
            auto server = Plugins::NTPClient::getServer(i);
            if (server.length()) {
                if (firstServer) {
                    firstServer = false;
                    output.print(F(HTML_S(br) "Servers "));
                }
                else {
                    output.print(FSPGM(comma_));
                }
                output.print(server);
            }
        }

        output.printf_P(PSTR(HTML_S(br) "RTC status: %s" HTML_S(br) "NTP Status: %s"), RTCMemoryManager::RtcTime::getStatus(RTCMemoryManager::getSyncStatus()), getCallbackState());
        if (_lastUpdateSeconds) {
            output.print(F(", last update "));
            auto time = (millis64() / 1000) - _lastUpdateSeconds;
            output.print(formatTimeShort(F(", "), F(" and "), false, time % 60, (time / 60) % 60, (time / 3600) % 24, time / 86400));
            output.print(F(" ago"));
        }

    }
    else {
        output.print(FSPGM(Disabled));
    }
}

void NTPPlugin::_execConfigTime()
{
    // set to out of sync while updating
    RTCMemoryManager::setSyncStatus(false);
    settimeofday_cb(updateNtpCallback);

    sntp_stop();
    sntp_setoperatingmode(SNTP_OPMODE_POLL);

    for(uint8_t i = 0; i < Plugins::NTPClient::kServersMax; i++) {
        auto server = Plugins::NTPClient::getServer(i);
        sntp_setservername(i, nullptr);
        // release memory after setting a new server
        if (_servers[i]) {
            free(_servers[i]);
        }
        _servers[i] = strdup(server.c_str());
        sntp_setservername(i, _servers[i]);
    }

    auto timezone = Plugins::NTPClient::getPosixTimezone();
	safeSetTZ(FPSTR(timezone ? timezone : PSTR("GMT")));
    sntp_init();

    auto interval = kCheckInterval + _startUpDelay;
    __LDBG_printf("timer=%u callback_state=%s timer=%u sntp_enabled=%u", interval, getCallbackState(), (bool)_checkTimer, sntp_enabled());
    _callbackState = CallbackState::WAITING_FOR_CALLBACK;
    _Timer(_checkTimer).add(interval, true, checkTimerCallback);
}

void NTPPlugin::updateNtpCallback(settimeofday_cb_args_t)
{
    #if ESP32
        LoopFunctions::callOnce([]() {
            plugin._updateNtpCallback();
        });
    #else
        plugin._updateNtpCallback();
    #endif
}

void NTPPlugin::_updateNtpCallback()
{
    auto now = time(nullptr);
    __LDBG_printf("new time=" TIME_T_FMT " @ %.3fs valid=%u lk_time=" TIME_T_FMT " timer=%u callback_state=%s",now, millis() / 1000.0, isTimeValid(now), getLastKnownTimeOfDay(), (bool)_checkTimer, getCallbackState());

    _callbackState = CallbackState::WAITING_FOR_REFRESH;
    _Timer(_checkTimer).remove();

    PrintString timeStr;
    timeStr.strftime(FSPGM(strftime_date_time_zone, "%FT%T %Z"), now);

    if (isTimeValid(now + 60)) { // allow to change time back up to 60 seconds
        setLastKnownTimeOfDay(now);
        RTCMemoryManager::setTime(time(nullptr), RTCMemoryManager::SyncStatus::YES);
        #if NTP_LOG_TIME_UPDATE
            Logger_notice(F("NTP time: %s"), timeStr.c_str());
        #endif
    }
    else {
        __LDBG_printf("invalid SNTP time=" TIME_T_FMT "+60 lk_time=" TIME_T_FMT, now, getLastKnownTimeOfDay());
        Logger_error(F("NTP time is in the past: %s"), timeStr.c_str());
    }

    #if NTP_HAVE_CALLBACKS
        for(const auto &callback: _callbacks) {
            callback(time(nullptr));
        }
    #endif

    #if DEBUG_NTP_CLIENT
        for(uint8_t i = 0; i < Plugins::NTPClient::kServersMax; i++) {
            __DBG_printf("reachability server %s=%u", __S(sntp_getservername(i)), sntp_getreachability(i));
        }
    #endif

    _lastUpdateSeconds = millis64() / 1000;

}

void NTPPlugin::checkTimerCallback(Event::CallbackTimerPtr timer)
{
    plugin._checkTimerCallback(timer);
}

void NTPPlugin::_checkTimerCallback(Event::CallbackTimerPtr timer)
{
    __LDBG_printf("interval=%u armed=%u callback_state=%s startup_delay=%u sntp_enabled=%u wifi=%u", timer->getShortInterval(), timer->isArmed(), getCallbackState(), _startUpDelay, sntp_enabled(), WiFi.isConnected());

    if (WiFi.isConnected()) {

        #if DEBUG_NTP_CLIENT
            __LDBG_printf("checktimer retrying update interval=%u", kCheckInterval);
        #endif
        timer->updateInterval(Event::milliseconds(kCheckInterval));
        _execConfigTime();
    }
    else {
        // retry quickly until WiFi is up
        timer->updateInterval(Event::seconds(15));
    }
}

