/**
 * Author: sascha_lammers@gmx.de
 */

#include "ntp_plugin.h"
#include <time.h>
#include <sys/time.h>
#include <KFCForms.h>
#include <PrintHtmlEntitiesString.h>
#include <EventScheduler.h>
#include <EventTimer.h>
#include <LoopFunctions.h>
#include <WiFiCallbacks.h>
#include <MicrosTimer.h>
#include "kfc_fw_config.h"
#include "kfc_fw_config_classes.h"
#include "../include/templates.h"
#include "logger.h"
#include "plugins.h"

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

#include <push_pack.h>

PROGMEM_STRING_DEF(strftime_date_time_zone, "%FT%T %Z");

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

class NTPPlugin : public PluginComponent {
// PluginComponent
public:
    NTPPlugin() {
        REGISTER_PLUGIN(this);
    }

    virtual PGM_P getName() const {
        return PSTR("ntp");
    }
    virtual const __FlashStringHelper *getFriendlyName() const {
        return F("NTP Client");
    }
    virtual PriorityType getSetupPriority() const override {
        return PriorityType::NTP;
    }

#if ENABLE_DEEP_SLEEP
    virtual bool autoSetupAfterDeepSleep() const override {
        return true;
    }
#endif

    virtual void setup(SetupModeType mode) override;
    virtual void reconfigure(PGM_P source) override;
    virtual void shutdown() override;

    virtual bool hasStatus() const override {
        return true;
    }
    virtual void getStatus(Print &output) override;

    virtual PGM_P getConfigureForm() const override {
        return getName();
    }
    virtual void createConfigureForm(AsyncWebServerRequest *request, Form &form) override;

#if AT_MODE_SUPPORTED
    virtual bool hasAtMode() const override {
        return true;
    }
    virtual void atModeHelpGenerator() override;
    virtual bool atModeHandler(AtModeArgs &args) override;
#endif

public:
    // sets the callback and executes configTime()
    static void execConfigTime();
    static void updateNtpCallback();
    static void _checkTimerCallback(EventScheduler::TimerPtr);

public:
    static uint32_t _ntpRefreshTimeMillis;

private:
    EventScheduler::Timer _checkTimer;
};

uint32_t NTPPlugin::_ntpRefreshTimeMillis = 15000;


static NTPPlugin plugin;

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

void NTPPlugin::createConfigureForm(AsyncWebServerRequest *request, Form &form)
{
    form.add<bool>(F("ntp_enabled"), _H_FLAGS_BOOL_VALUE(Config().flags, ntpClientEnabled));

    form.add(F("ntp_server1"), _H_STR_VALUE(Config().ntp.servers[0]));
    form.addValidator(new FormValidHostOrIpValidator(true));

    form.add(F("ntp_server2"), _H_STR_VALUE(Config().ntp.servers[1]));
    form.addValidator(new FormValidHostOrIpValidator(true));

    form.add(F("ntp_server3"), _H_STR_VALUE(Config().ntp.servers[2]));
    form.addValidator(new FormValidHostOrIpValidator(true));

    form.add(F("ntp_timezone"), _H_STR_VALUE(Config().ntp.timezone));
    form.add(F("ntp_posix_tz"), _H_STR_VALUE(Config().ntp.posix_tz));

    form.add<uint16_t>(F("ntp_refresh"), config._H_GET(Config().ntp.ntpRefresh), [](uint16_t value, FormField &, bool) {
        config._H_SET(Config().ntp.ntpRefresh, value);
        return false;
    });
    form.addValidator(new FormRangeValidator(F("Invalid refresh interval: %min%-%max% minutes"), 60, 43200));

    debug_printf_P(PSTR("ntp_timezone=%s ntp_posix_tz=%s\n"), config._H_STR(Config().ntp.timezone), config._H_STR(Config().ntp.posix_tz));

    form.finalize();
}

#if AT_MODE_SUPPORTED

#include "at_mode.h"

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(NOW, "NOW", "<update>", "Display current time or update NTP");
PROGMEM_AT_MODE_HELP_COMMAND_DEF(TZ, "TZ", "<timezone>", "Set timezone", "Show timezone information");

void NTPPlugin::atModeHelpGenerator()
{
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(NOW), getName());
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(TZ), getName());
}

bool NTPPlugin::atModeHandler(AtModeArgs &args)
{
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(NOW))) {
        if (args.size()) {
            execConfigTime();
            args.print(F("Waiting up to 5 seconds for a valid time..."));
            ulong end = millis() + 5000;
            while(millis() < end && !IS_TIME_VALID(time(nullptr))) {
                delay(10);
            }
        }
        time_t now = time(nullptr);
        char timestamp[64];
        if (!IS_TIME_VALID(now)) {
            args.printf_P(PSTR("Time is currently not set (%lu). NTP is %s"), now, (config._H_GET(Config().flags).ntpClientEnabled ? FSPGM(enabled) : FSPGM(disabled)));
        }
        else {
            strftime_P(timestamp, sizeof(timestamp), SPGM(strftime_date_time_zone), gmtime(&now));
            args.printf_P(PSTR("gmtime=%s, unixtime=%u"), timestamp, now);

            strftime_P(timestamp, sizeof(timestamp), SPGM(strftime_date_time_zone), localtime(&now));
            args.printf_P(PSTR("localtime=%s"), timestamp);
        }
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(TZ))) {
        if (args.isQueryMode()) {
            auto fmt = PSTR("%FT%T %Z %z");
            String fmtStr = fmt;
            auto &tz = *__gettzinfo();
            Serial.printf_P(PSTR("_tzname[0]=%s,_tzname[1]=%s\n"), _tzname[0], _tzname[1]);
            Serial.printf_P(PSTR("__tznorth=%u,__tzyear=%u\n"), tz.__tznorth, tz.__tzyear);
            for(int i = 0; i < 2; i++) {
                Serial.printf_P(PSTR("ch=%c,m=%d,n=%d,d=%d,s=%d,change=%ld,offset=%ld\n"), tz.__tzrule[i].ch, tz.__tzrule[i].m, tz.__tzrule[i].n, tz.__tzrule[i].d, tz.__tzrule[i].s, tz.__tzrule[i].change, tz.__tzrule[i].offset);
            }
        } else if (args.requireArgs(2, 2)) {
            auto arg = args.get(1);
            if (args.isAnyMatchIgnoreCase(0, F("tz"))) {
                setTZ(arg);
                auto now = time(nullptr);
                localtime(&now); // update __gettzinfo()
                args.printf_P(PSTR("TZ set to '%s'"), arg);
            }
            else if (args.isAnyMatchIgnoreCase(0, F("ntp"))) {
                _ntpRefreshTimeMillis = 15000;
                configTime(arg, "pool.ntp.org");
                args.printf_P(PSTR("configTime called with '%s'"), arg);
            }
        }
        return true;
    }
    return false;
}

#endif

void NTPPlugin::setup(SetupModeType mode)
{
    if (config._H_GET(Config().flags).ntpClientEnabled) {
        execConfigTime();
    } else {
		auto str = emptyString.c_str();
		configTime(_GMT, str, str, str);
    }
}

void NTPPlugin::reconfigure(PGM_P source)
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
    if (config._H_GET(Config().flags).ntpClientEnabled) {

        char buf[16];
        auto now = time(nullptr);
        auto tm = localtime(&now);
        strftime_P(buf, sizeof(buf), PSTR("%z %Z"), tm);
        output.printf_P(PSTR("Timezone %s, %s"), Config_NTP::getTimezone(), buf);

        auto firstServer = true;
        const char *server;
        for (int i = 0; (server = Config_NTP::getServers(i)) != nullptr; i++) {
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
    _debug_printf_P(PSTR("server1=%s,server2=%s,server3=%s, refresh in %u seconds\n"), Config_NTP::getServers(0), Config_NTP::getServers(1), Config_NTP::getServers(2), (unsigned)(_ntpRefreshTimeMillis / 1000));
    configTime(Config_NTP::getPosixTZ(), Config_NTP::getServers(0), Config_NTP::getServers(1), Config_NTP::getServers(2));

    // check time every minute
    // for some reason, NTP does not always update the time even with _ntpRefreshTimeMillis = 15seconds
    plugin._checkTimer.add(15000, true, _checkTimerCallback);
}

void NTPPlugin::updateNtpCallback()
{
    _debug_printf_P(PSTR("new time=%u\n"), (uint32_t)time(nullptr));

    if (IS_TIME_VALID(time(nullptr))) {
        NTPPlugin::_ntpRefreshTimeMillis = Config_NTP::getNtpRfresh() * 60 * 1000UL;
        plugin._checkTimer.remove();
    }

#if RTC_SUPPORT
    // update RTC
    config.setRTC(time(nullptr));
#endif

#if NTP_LOG_TIME_UPDATE
    char buf[32];
    auto now = time(nullptr);
    strftime_P(buf, sizeof(buf), SPGM(strftime_date_time_zone), localtime(&now));
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

#include <pop_pack.h>
