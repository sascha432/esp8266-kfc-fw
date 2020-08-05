/**
 * Author: sascha_lammers@gmx.de
 */

#include <time.h>
#include <sys/time.h>
#include <EventScheduler.h>
#include <EventTimer.h>
#include <PrintHtmlEntitiesString.h>
#include <LoopFunctions.h>
#include <WiFiCallbacks.h>
#include <MicrosTimer.h>
#include "../include/templates.h"
#include "logger.h"
#include "plugins.h"
#include "ntp_plugin_class.h"
#include "ntp_plugin.h"
#include "kfc_fw_config.h"

#if defined(ESP8266)
#include <sntp.h>
#include <sntp-lwip2.h>
#elif defined(ESP32)
#include <lwip/apps/sntp.h>
#endif

#if DEBUG_NTP_CLIENT
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using KFCConfigurationClasses::System;
using KFCConfigurationClasses::Plugins;

#if ESP8266
static char *_GMT = _tzname[0]; // copy pointer, points to static char gmt[] = "GMT";
#else
static const char _GMT[] = "GMT";
#endif


#if NTP_HAVE_CALLBACKS

static std::vector<TimeUpdatedCallback_t> _callbacks;

void addTimeUpdatedCallback(TimeUpdatedCallback_t callback)
{
    _debug_printf_P(PSTR("func=%p\n"), &callback);
    _callbacks.push_back(callback);
}

#endif

uint32_t NTPPlugin::_ntpRefreshTimeMillis = 15000;

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

NTPPlugin::NTPPlugin() : PluginComponent(PROGMEM_GET_PLUGIN_OPTIONS(NTPPlugin))
{
    REGISTER_PLUGIN(this, "NTPPlugin");
}

#if SNTP_STARTUP_DELAY

#ifndef SNTP_STARTUP_DELAY_MAX_TIME
#define SNTP_STARTUP_DELAY_MAX_TIME         1000UL
#endif

uint32_t sntp_startup_delay_MS_rfc_not_less_than_60000()
{
    return rand() % SNTP_STARTUP_DELAY_MAX_TIME;
}

#endif

uint32_t sntp_update_delay_MS_rfc_not_less_than_15000()
{
    return NTPPlugin::_ntpRefreshTimeMillis;
}

void NTPPlugin::setup(SetupModeType mode)
{
    if (System::Flags::getConfig().is_ntp_client_enabled) {
        execConfigTime();
    } else {
		auto str = emptyString.c_str();
		configTime(_GMT, str, str, str);
    }
}

void NTPPlugin::reconfigure(const String & source)
{
    _checkTimer.remove();
    setup(SetupModeType::DEFAULT);
}

void NTPPlugin::shutdown()
{
    _checkTimer.remove();
    settimeofday_cb(nullptr);
}

void NTPPlugin::getStatus(Print &output)
{
    if (System::Flags::getConfig().is_ntp_client_enabled) {

        output.printf_P(PSTR("Timezone %s, "), Plugins::NTPClient::getTimezoneName());
        static_cast<PrintString &>(output).strftime_P(PSTR("%z %Z"), time(nullptr));

        auto firstServer = true;
        const char *server;
        for (uint8_t i = 0; (server = Plugins::NTPClient::getServer(i)) != nullptr; i++) {
            String serverStr = server;
            serverStr.trim();
            if (serverStr.length()) {
                if (firstServer) {
                    firstServer = false;
                    output.print(F(HTML_S(br) "Servers "));
                } else {
                    output.print(FSPGM(comma_));
                }
                output.print(serverStr);
            }
        }
    }
    else {
        output.print(FSPGM(Disabled));
    }
}


void NTPPlugin::execConfigTime()
{
    settimeofday_cb(updateNtpCallback);
     // set refresh time to a minimum for the update. calling configTime() seems to ignore this value. once
     // the time is valid, it is reset to the regular NTP refresh interval
    _ntpRefreshTimeMillis = 15000;
    _debug_printf_P(PSTR("server1=%s,server2=%s,server3=%s, refresh in %u seconds\n"), Plugins::NTPClient::getServer1(), Plugins::NTPClient::getServer2(), Plugins::NTPClient::getServer3(), (unsigned)(_ntpRefreshTimeMillis / 1000));
    configTime(Plugins::NTPClient::getPosixTimezone(), Plugins::NTPClient::getServer1(), Plugins::NTPClient::getServer2(), Plugins::NTPClient::getServer3());

    // check time every minute
    // for some reason, NTP does not always update the time even with _ntpRefreshTimeMillis = 15seconds
    plugin._checkTimer.add(15000, true, _checkTimerCallback);
}

void NTPPlugin::updateNtpCallback()
{
    auto now = time(nullptr);
    _debug_printf_P(PSTR("new time=%u\n"), (int)now);

    if (IS_TIME_VALID(now)) {
        NTPPlugin::_ntpRefreshTimeMillis = Plugins::NTPClient::getConfig().getRefreshIntervalMillis();
        plugin._checkTimer.remove();
    }

#if RTC_SUPPORT
    // update RTC
    config.setRTC(now);
#endif

#if NTP_LOG_TIME_UPDATE
    char buf[32];
    strftime_P(buf, sizeof(buf), SPGM(strftime_date_time_zone, "%FT%T %Z"), localtime(&now));
    Logger_notice(F("NTP time: %s"), buf);
#endif

#if NTP_HAVE_CALLBACKS
    for(auto callback: _callbacks) {
        callback(time(nullptr));
    }
#endif
}

void NTPPlugin::_checkTimerCallback(EventScheduler::TimerPtr timer)
{
    if (IS_TIME_VALID(time(nullptr))) {
        _debug_printf_P(PSTR("detaching NTP check timer\n"));
        timer->detach();
    }
    else {
        _debug_printf_P(PSTR("NTP did not update, calling configTime() again, delay=%u\n"), timer->getDelayUInt32());
        execConfigTime();
        if (timer->getDelayUInt32() < 60000U) { // change to 60 seconds after first attempt
            timer->rearm(60000);
        }
    }
}
