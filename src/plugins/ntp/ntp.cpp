/**
 * Author: sascha_lammers@gmx.de
 */

#include "ntp_plugin.h"
#include <time.h>
#include <sys/time.h>
#include <KFCTimezone.h>
#include <KFCForms.h>
#include <PrintHtmlEntitiesString.h>
#include <EventScheduler.h>
#include <LoopFunctions.h>
#include <WiFiCallbacks.h>
#include <MicrosTimer.h>
#include "kfc_fw_config.h"
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

typedef struct __attribute__packed__ {
    int32_t offset;
    char abbreviation[4];
    time_t zoneEnd;
#if NTP_RESTORE_SYSTEM_TIME_AFTER_WAKEUP
    time_t currentTime;
    uint32_t sleepTime;
#endif
} NTPClientData_t;

PROGMEM_STRING_DEF(strftime_date_time_zone, "%FT%T %Z");

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
    virtual PluginPriorityEnum_t getSetupPriority() const override {
        return PRIO_NTP;
    }
    virtual uint8_t getRtcMemoryId() const override {
        return NTP_CLIENT_RTC_MEM_ID;
    }

#if NTP_RESTORE_TIMEZONE_AFTER_WAKEUP
    virtual bool autoSetupAfterDeepSleep() const override {
        return true;
    }
    virtual void prepareDeepSleep(uint32_t sleepTimeMillis) override;
#endif
    virtual void setup(PluginSetupMode_t mode) override;
    virtual void reconfigure(PGM_P source) override;
    virtual void restart() override;

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
    static void wifiConnectedCallback(uint8_t event, void *payload);
    static void updateLoop();
    static void configTime();
    static void updateNtpCallback();

private:
    static void _callback(RemoteTimezone::JsonReaderResult *result, const String &error);
    static bool updateRequired();
    static void removeCallbacks();

    void retry(const String &message);

private:
    uint16_t _failureCount;
    EventScheduler::Timer _updateTimer;

private:
    static time_t _zoneEnd;
    static unsigned long _lastNtpUpdate;
    static unsigned long _ntpRefreshTime;
    static uint32_t _lastNtpCallback;
};

time_t NTPPlugin::_zoneEnd = 0;
unsigned long NTPPlugin::_lastNtpUpdate = 0;
unsigned long NTPPlugin::_ntpRefreshTime = 3600000;
uint32_t NTPPlugin::_lastNtpCallback = 0;


static NTPPlugin plugin;

void NTPPlugin::setup(PluginSetupMode_t mode)
{
    if (config._H_GET(Config().flags).ntpClientEnabled) {

        // force SNTP update on WiFi connect
        WiFiCallbacks::add(WiFiCallbacks::EventEnum_t::CONNECTED, wifiConnectedCallback);

#if NTP_RESTORE_TIMEZONE_AFTER_WAKEUP
        if (resetDetector.hasWakeUpDetected()) { // restore timezone from RTC memory
            NTPClientData_t ntp;
            if (RTCMemoryManager::read(NTP_CLIENT_RTC_MEM_ID, &ntp, sizeof(ntp))) {

                auto &timezone = get_default_timezone();
                timezone.setOffset(ntp.offset);
                timezone.setAbbreviation(ntp.abbreviation);
                _zoneEnd = ntp.zoneEnd;
                if (_zoneEnd) {
                    LoopFunctions::add(updateLoop);
                }

// restore time if no RTC is present
#if NTP_RESTORE_SYSTEM_TIME_AFTER_WAKEUP && !RTC_SUPPORT
                time_t msec = ntp.sleepTime + millis();             // add sleep time + time since wake up
                time_t seconds = msec / 1000UL;
                msec -= seconds;                                    // remove full seconds
                struct timeval tv = { (time_t)(ntp.currentTime + seconds), (suseconds_t)(msec * 1000L) };
                settimeofday(&tv, nullptr);
                _debug_printf_P(PSTR("stored time %lu, sleep time %u, millis() %lu, new time sec %lu usec %lu\n"), ntp.currentTime, ntp.sleepTime, millis(), tv.tv_sec, tv.tv_usec);
#endif
                configTime();     // request real time from ntp

                _debug_printf_P(PSTR("restored timezone after wake up. abbreviation=%s, offset=%d, zoneEnd=%lu\n"), ntp.abbreviation, ntp.offset, ntp.zoneEnd);
                return;
            }
        }
#endif

        configTime();

        auto remoteUrl = config._H_STR(Config().ntp.remote_tz_dst_ofs_url);
        if (*remoteUrl) {
            if (WiFi.isConnected()) { // simulate event if WiFi is already connected
                wifiConnectedCallback(WiFiCallbacks::EventEnum_t::CONNECTED, nullptr);
            }
        }

    } else {

		auto str = emptyString.c_str();
		::configTime(0, 0, str, str, str);
        // sntp_set_timezone(0);

        removeCallbacks();
    }
}

void NTPPlugin::reconfigure(PGM_P source)
{
    setup(PLUGIN_SETUP_DEFAULT);
}

void NTPPlugin::restart()
{
    settimeofday_cb(nullptr);
    removeCallbacks();
}

void NTPPlugin::getStatus(Print &output)
{
    if (config._H_GET(Config().flags).ntpClientEnabled) {
        auto firstServer = true;
        auto &timezone = get_default_timezone();
        output.print(F("Timezone "));
        if (timezone.isValid()) {
            output.printf_P(PSTR("%s, %02d:%02u %s"), timezone.getTimezone().c_str(), timezone.getOffset() / 3600, timezone.getOffset() % 60, timezone.getAbbreviation().c_str());
            if (_zoneEnd) {
                auto tm = timezone_localtime(&_zoneEnd);
                char buf[32];
                timezone_strftime_P(buf, sizeof(buf), SPGM(strftime_date_time_zone), tm);
                output.printf_P(PSTR(", next check on %s"), buf);
            }
        } else {
            output.printf_P(PSTR("%s, status invalid"), config._H_STR(Config().ntp.timezone));
        }
        static const ConfigurationParameter::Handle_t handles[] = { _H(Config().ntp.servers[0]), _H(Config().ntp.servers[1]), _H(Config().ntp.servers[2]) };
        for (int i = 0; i < 2; i++) {
            auto server = config.getString(handles[i]);
            if (*server) {
                if (firstServer) {
                    firstServer = false;
                    output.print(F(HTML_S(br) "Servers "));
                } else {
                    output.print(FSPGM(comma_));
                }
                output.print(server);
            }
        }
        if (_lastNtpCallback) {
            float seconds = get_time_diff(_lastNtpCallback, millis()) / 1000.0;
            unsigned int nextSeconds = (_ntpRefreshTime - get_time_diff(_lastNtpUpdate, millis())) / 1000;
            output.printf_P(PSTR(HTML_S(br) "Last update %.2f seconds ago, next update in %u seconds"), seconds, nextSeconds);
        }
    }
    else {
        output.print(FSPGM(Disabled));
    }
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

    form.add<uint16_t>(F("ntp_refresh"), _H_STRUCT_VALUE(Config().ntp.tz, ntpRefresh));
    form.addValidator(new FormRangeValidator(F("Invalid refresh interval: %min%-%max% minutes"), 60, 43200));

#if USE_REMOTE_TIMEZONE
    form.add(F("ntp_remote_tz_url"), _H_STR_VALUE(Config().ntp.remote_tz_dst_ofs_url));
#endif

    form.finalize();
}

#if NTP_RESTORE_SYSTEM_TIME_AFTER_WAKEUP

void NTPPlugin::prepareDeepSleep(uint32_t sleepTimeMillis)
{
    NTPClientData_t ntp;
    if (RTCMemoryManager::read(NTP_CLIENT_RTC_MEM_ID, &ntp, sizeof(ntp))) {
        ntp.currentTime = time(nullptr);
        ntp.sleepTime = sleepTimeMillis;
        RTCMemoryManager::write(NTP_CLIENT_RTC_MEM_ID, &ntp, sizeof(ntp));
    }
}

#endif

#if AT_MODE_SUPPORTED

#include "at_mode.h"

#if DEBUG
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(SNTPFU, "SNTPFU", "Force SNTP to update time");
#endif
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(NOW, "NOW", "Display current time");
PROGMEM_AT_MODE_HELP_COMMAND_DEF(TZ, "TZ", "<timezone>", "Set timezone", "Show timezone information");

void NTPPlugin::atModeHelpGenerator()
{
#if DEBUG
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(SNTPFU), getName());
#endif
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(NOW), getName());
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(TZ), getName());
}

bool NTPPlugin::atModeHandler(AtModeArgs &args)
{
#if DEBUG
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(SNTPFU))) {
        configTime();
        args.print(F("Waiting up to 5 seconds for a valid time..."));
        ulong end = millis() + 5000;
        while(millis() < end && !IS_TIME_VALID(time(nullptr))) {
            delay(10);
        }
        goto commandNow;
    }
    else
#endif
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(NOW))) {
#if DEBUG
commandNow:
#endif
        time_t now = time(nullptr);
        char timestamp[64];
        if (!IS_TIME_VALID(now)) {
            args.printf_P(PSTR("Time is currently not set (%lu). NTP is %s"), now, (config._H_GET(Config().flags).ntpClientEnabled ? FSPGM(enabled) : FSPGM(disabled)));
#if DEBUG && defined(ESP32)
            args.printf_P(PSTR("sntp_enabled() = %d"), sntp_enabled());
#endif
        }
        else {
            auto &timezone = get_default_timezone();
            strftime_P(timestamp, sizeof(timestamp), SPGM(strftime_date_time_zone), gmtime(&now));
            args.printf_P(PSTR("%s, unixtime=%u, valid=%u, dst=%u"), timestamp, now, timezone.isValid(), timezone.isDst());
            timezone_strftime_P(timestamp, sizeof(timestamp), SPGM(strftime_date_time_zone), timezone_localtime(&now));
            args.print(timestamp);
        }
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(TZ))) {
        auto &timezone = get_default_timezone();
        if (args.isQueryMode()) { // TZ?
            if (timezone.isValid()) {
                char buf[32];
                if (!_zoneEnd) {
                    strcpy_P(buf, PSTR("not scheduled"));
                }
                else {
                    timezone_strftime_P(buf, sizeof(buf), SPGM(strftime_date_time_zone), timezone_localtime(&_zoneEnd));
                }
                args.printf_P(PSTR("Timezone %s abbreviation %s offset %02d:%02u next update %s"), timezone.getTimezone().c_str(), timezone.getAbbreviation().c_str(), timezone.getOffset() / 3600, timezone.getOffset() % 60, buf);
            } else {
                args.print(F("No valid timezone set"));
            }
        }
        else if (args.requireArgs(1, 1)) {
            if (config._H_GET(Config().flags).ntpClientEnabled) {
                config._H_SET_STR(Config().ntp.timezone, args.get(0));
                args.printf_P(PSTR("Timezone set to %s"), config._H_STR(_Config.ntp.timezone));
                setup(PLUGIN_SETUP_DEFAULT);
            }
            else {
                args.printf_P(PSTR("NTP %s"), FSPGM(disabled));
            }
        }
        return true;
    }
    return false;
}

#endif

void NTPPlugin::configTime()
{
    if (get_time_diff(_lastNtpUpdate, millis()) < 1000) {
        _debug_printf_P(PSTR("skipped multiple calls within 1000ms\n"));
        return;
    }
    _lastNtpUpdate = millis();

    auto refresh = config._H_GET(Config().ntp.tz).ntpRefresh;
    if (refresh < 60) {
        refresh = 60;
    }
    _ntpRefreshTime = (refresh * 60) * (950 + (rand() % 100)); // +-5%

    settimeofday_cb(updateNtpCallback);

    _debug_printf_P(PSTR("server1=%s,server2=%s,server3=%s, refresh in %.0f seconds\n"), config._H_STR(Config().ntp.servers[0]), config._H_STR(Config().ntp.servers[1]), config._H_STR(Config().ntp.servers[2]), _ntpRefreshTime / 1000.0);
    // force SNTP to update the time
    ::configTime(0, 0, config._H_STR(Config().ntp.servers[0]), config._H_STR(Config().ntp.servers[1]), config._H_STR(Config().ntp.servers[2]));
}

void NTPPlugin::updateNtpCallback()
{
    _debug_printf_P(PSTR("new time=%u\n"), (uint32_t)time(nullptr));
    // _debug_printf_P(PSTR("new time=%u, tz=%d\n"), (uint32_t)time(nullptr), sntp_get_timezone());

#if RTC_SUPPORT
    // update RTC
    config.setRTC(time(nullptr));
#endif

    if (get_time_diff(_lastNtpCallback, millis()) < 1000) {
        _debug_printf_P(PSTR("called twice within 1000ms (%u), ignored multiple calls\n"), get_time_diff(_lastNtpCallback, millis()));
        return;
    }

#if NTP_LOG_TIME_UPDATE
    char buf[32];
    auto now = time(nullptr);
    auto tm = timezone_localtime(&now);
    timezone_strftime_P(buf, sizeof(buf), SPGM(strftime_date_time_zone), tm);
    Logger_notice(F("NTP: new time: %s"), buf);
#endif

    _lastNtpCallback = millis();
    _lastNtpUpdate = _lastNtpCallback;

#if NTP_HAVE_CALLBACKS
    for(auto callback: _callbacks) {
        callback(time(nullptr));
    }
#endif
}

void NTPPlugin::updateLoop()
{
    if (get_time_diff(_lastNtpUpdate, millis()) > _ntpRefreshTime) { // refresh NTP time manually
        _debug_println(F("refreshing NTP time manually"));
        configTime();
    }
    if (_zoneEnd != 0 && time(nullptr) >= _zoneEnd) {
        _debug_printf_P(PSTR("triggered\n"));
        LoopFunctions::remove(updateLoop); // remove once triggered
        _zoneEnd = 0;
        _lastNtpUpdate = 0;
        plugin._failureCount = 0;
        if (WiFi.isConnected()) { // simulate event if WiFi is already connected
            wifiConnectedCallback(WiFiCallbacks::EventEnum_t::CONNECTED, nullptr);
        }
    }
}

void NTPPlugin::wifiConnectedCallback(uint8_t event, void *payload)
{
    _debug_printf_P(PSTR("event=%u payload=%p\n"), event, payload);
    if (!IS_TIME_VALID(time(nullptr))) {
        _debug_printf_P(PSTR("time not valid, updating SNTP\n"));
        configTime();
    }

    _debug_printf_P(PSTR("updateRequired=%d\n"), updateRequired());
    if (updateRequired()) {
        auto rtz = new RemoteTimezone::RestApi();
        rtz->setUrl(config._H_STR(Config().ntp.remote_tz_dst_ofs_url));
        rtz->setZoneName(config._H_STR(Config().ntp.timezone));
        rtz->setAutoDelete(true);
        rtz->call(_callback);
    }
}

void NTPPlugin::_callback(RemoteTimezone::JsonReaderResult *result, const String &error)
{
    if (result && result->getStatus()) {
        _debug_printf_P(PSTR("status=%d, message=%s, zoneEnd=%ld\n"), result->getStatus(), result->getMessage().c_str(), (long)result->getZoneEnd());

        auto &timezone = get_default_timezone();
        timezone.invalidate();
        timezone.setAbbreviation(result->getAbbreviation());
        timezone.setOffset(result->getOffset());
        timezone.setTimezone(0, result->getZoneName());
        timezone.setDst(result->getDst());
        timezone.save();

        auto offset = timezone.getOffset();
        // do not set sntp_set_timezone()
        // sint8_t tzOfs = timezone.isValid() ? offset / 3600 : 0;
        // sntp_set_timezone(tzOfs);
        // sntp_set_daylight(0);

        _zoneEnd = result->getZoneEnd();
        if (_zoneEnd <= 0) {
            _zoneEnd = time(nullptr) + 86400; // retry in 24 hours if zoneEnd is not available
        }
        LoopFunctions::add(updateLoop);

        char buf[32];
        timezone_strftime_P(buf, sizeof(buf), PSTR("%a, %d %b %Y %T %Z"), timezone_localtime(&_zoneEnd));
        Logger_notice(F("NTP: %s offset %02d:%02u. Next check on %s"), timezone.getAbbreviation().c_str(), offset / 3600, offset % 60, buf);

#if NTP_RESTORE_TIMEZONE_AFTER_WAKEUP
        // store timezone in RTC memory that it is available after wake up
        NTPClientData_t ntp;
        ntp.offset = offset;
        strncpy(ntp.abbreviation, timezone.getAbbreviation().c_str(), sizeof(ntp.abbreviation) - 1)[sizeof(ntp.abbreviation) - 1] = 0;
        ntp.zoneEnd = _zoneEnd;
        RTCMemoryManager::write(NTP_CLIENT_RTC_MEM_ID, &ntp, sizeof(ntp));
#endif

#if NTP_HAVE_CALLBACKS
        for(auto callback: _callbacks) {
            callback(-1);
        }
#endif

    }
    else {
        plugin.retry(result ? result->getMessage() : error);
    }
}

void NTPPlugin::retry(const String &message)
{
    _zoneEnd = 0;
    _updateTimer.remove();

    // echo '<?php for($i = 0; $i < 30; $i++) { echo floor(10 * (1 + ($i * $i / 3)).", "; } echo "\n";' |php
    // 10, 13, 23, 40, 63, 93, 130, 173, 223, 280, 343, 413, 490, 573, 663, 760, 863, 973, 1090, 1213, 1343, 1480, 1623, 1773, 1930, 2093, 2263, 2440, 2623, 2813, ...
    uint32_t next_check = 10 * (1 + (_failureCount * _failureCount / 3));
    _failureCount++;

    // schedule next retry
    _updateTimer.add((int64_t)next_check * (int64_t)1000, false, [](EventScheduler::TimerPtr timer) {
        if (WiFi.isConnected()) { // retry if WiFi is connected, otherwise wait for connected event
            wifiConnectedCallback(WiFiCallbacks::EventEnum_t::CONNECTED, nullptr);
        }
    });

    Logger_notice(F("NTP: Update failed. Retrying (#%d) in %d second(s). %s"), _failureCount, next_check, message.c_str());
}

bool NTPPlugin::updateRequired()
{
    return (_zoneEnd == 0 || time(nullptr) >= _zoneEnd);
}

void NTPPlugin::removeCallbacks()
{
    plugin._updateTimer.remove();
    WiFiCallbacks::remove(WiFiCallbacks::EventEnum_t::ANY, wifiConnectedCallback);
    LoopFunctions::remove(updateLoop);
}

#include <pop_pack.h>
