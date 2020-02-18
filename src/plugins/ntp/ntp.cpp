/**
 * Author: sascha_lammers@gmx.de
 */

#if NTP_CLIENT

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
#include "progmem_data.h"
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

class TimezoneData;

static TimezoneData *timezoneData = nullptr;

class TimezoneData {
public:
    TimezoneData();
    virtual ~TimezoneData();

    RemoteTimezone *createRemoteTimezone();
    void deleteRemoteTimezone();

    bool updateRequired();
    void setZoneEnd(time_t zoneEnd);
    const time_t *getZoneEndPtr() const;
    void retry(const String &message);

    static void wifiConnectedCallback(uint8_t event, void *payload);
    static void getStatus(Print &output);
    static void _setZoneEnd(time_t zoneEnd);

public:
    static const time_t getZoneEnd();
    static void configTime();

public:
    static void updateLoop();
    static void updateNtpCallback();

private:
    static time_t _zoneEnd;
    static unsigned long _lastNtpUpdate;
    static unsigned long _ntpRefreshTime;
    static uint32_t _lastNtpCallback;

    static void _callback(bool status, const String message, time_t zoneEnd);

    RemoteTimezone *_remoteTimezone;
    uint16_t _failureCount;
    EventScheduler::Timer _updateTimer;
};

time_t TimezoneData::_zoneEnd = 0;
unsigned long TimezoneData::_lastNtpUpdate = 0;
unsigned long TimezoneData::_ntpRefreshTime = 3600000;
uint32_t TimezoneData::_lastNtpCallback = 0;

TimezoneData::TimezoneData()
{
    _debug_println(emptyString);
    _zoneEnd = 0;
    _lastNtpUpdate = 0;
    _failureCount = 0;
    _remoteTimezone = nullptr;
}

TimezoneData::~TimezoneData()
{
    _debug_println(emptyString);
    _updateTimer.remove();
    deleteRemoteTimezone();
}

RemoteTimezone *TimezoneData::createRemoteTimezone()
{
    _debug_printf_P(PSTR("creating RemoteTimezone object %p\n"), _remoteTimezone);
    return (_remoteTimezone = new RemoteTimezone());
}

void TimezoneData::deleteRemoteTimezone()
{
    _debug_printf_P(PSTR("deleting RemoteTimezone object %p\n"), _remoteTimezone);
    if (_remoteTimezone) {
        delete _remoteTimezone;
        _remoteTimezone = nullptr;
    }
}

bool TimezoneData::updateRequired()
{
    return (!_remoteTimezone && (_zoneEnd == 0 || time(nullptr) >= _zoneEnd));
}

void TimezoneData::setZoneEnd(time_t zoneEnd)
{
    _zoneEnd = zoneEnd;
    _failureCount = 0;
}

// static
void TimezoneData::_setZoneEnd(time_t zoneEnd)
{
    _zoneEnd = zoneEnd;
    if (_zoneEnd) {
        LoopFunctions::add(TimezoneData::updateLoop);
    }
}

const time_t *TimezoneData::getZoneEndPtr() const
{
    return &_zoneEnd;
}

const time_t TimezoneData::getZoneEnd()
{
    return _zoneEnd;
}


void TimezoneData::retry(const String &message)
{
    _updateTimer.remove();
    deleteRemoteTimezone();

    // echo '<?php for($i = 0; $i < 30; $i++) { echo floor(10 * (1 + ($i * $i / 3)).", "; } echo "\n";' |php
    // 10, 13, 23, 40, 63, 93, 130, 173, 223, 280, 343, 413, 490, 573, 663, 760, 863, 973, 1090, 1213, 1343, 1480, 1623, 1773, 1930, 2093, 2263, 2440, 2623, 2813, ...
    uint32_t next_check = 10 * (1 + (_failureCount * _failureCount / 3));

    _updateTimer.add((int64_t)next_check * (int64_t)1000, false, [](EventScheduler::TimerPtr timer) {
        if (WiFi.isConnected()) { // retry if WiFi is connected, otherwise wait for connected event
            wifiConnectedCallback(WiFiCallbacks::EventEnum_t::CONNECTED, nullptr);
        }
    });
    _failureCount++;

    Logger_notice(F("NTP: Update failed. Retrying (#%d) in %d second(s). %s"), _failureCount, next_check, message.c_str());
}

void TimezoneData::wifiConnectedCallback(uint8_t event, void *payload)
{
    _debug_printf_P(PSTR("event=%u\n"), event);
    if (!IS_TIME_VALID(time(nullptr))) {
        _debug_printf_P(PSTR("wifiConnectedCallback(%d): time not valid, updating SNTP\n"), event);
        TimezoneData::configTime();
    }

    _debug_printf_P(PSTR("wifiConnectedCallback(%d): updateRequired() = %d\n"), event, timezoneData->updateRequired());

    if (timezoneData->updateRequired()) {

        auto rtz = timezoneData->createRemoteTimezone();
        rtz->setUrl(config._H_STR(Config().ntp.remote_tz_dst_ofs_url));
        rtz->setTimezone(config._H_STR(Config().ntp.timezone));
        rtz->setStatusCallback(_callback);

        rtz->get();
    }
}

void TimezoneData::_callback(bool status, const String message, time_t zoneEnd)
{
    _debug_printf_P(PSTR("status=%d, message=%s, zoneEnd=%ld\n"), status, message.c_str(), (long)zoneEnd);
    if (status) {
        auto &timezone = get_default_timezone();
        auto offset = timezone.getOffset();
        // do not set sntp_set_timezone()
        // sint8_t tzOfs = timezone.isValid() ? offset / 3600 : 0;
        // sntp_set_timezone(tzOfs);
        // sntp_set_daylight(0);

        timezoneData->setZoneEnd(zoneEnd);
        LoopFunctions::add(TimezoneData::updateLoop);
        auto tmp = timezoneData;
        Scheduler.addTimer(1000, false, [tmp](EventScheduler::TimerPtr timer) { // delete outside the callback or deleting the async http client causes a crash in ~RemoteTimezone()
            delete tmp;
        });
        timezoneData = nullptr;

        char buf[32];
        timezone_strftime_P(buf, sizeof(buf), PSTR("%a, %d %b %Y %T %Z"), timezone_localtime(timezoneData->getZoneEndPtr()));
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

    } else {
        timezoneData->retry(message);
    }
}

void TimezoneData::getStatus(Print &out)
{
    if (config._H_GET(Config().flags).ntpClientEnabled) {
        auto firstServer = true;
        auto &timezone = get_default_timezone();
        out.print(F("Timezone "));
        if (timezone.isValid()) {
            out.printf_P(PSTR("%s, %02d:%02u %s"), timezone.getTimezone().c_str(), timezone.getOffset() / 3600, timezone.getOffset() % 60, timezone.getAbbreviation().c_str());
            if (_zoneEnd) {
                auto tm = timezone_localtime(&_zoneEnd);
                char buf[32];
                timezone_strftime_P(buf, sizeof(buf), SPGM(strftime_date_time_zone), tm);
                out.printf_P(PSTR(", next check on %s"), buf);
            }
        } else {
            out.printf_P(PSTR("%s, status invalid"), config._H_STR(Config().ntp.timezone));
        }
        static const ConfigurationParameter::Handle_t handles[] = { _H(Config().ntp.servers[0]), _H(Config().ntp.servers[1]), _H(Config().ntp.servers[2]) };
        for (int i = 0; i < 2; i++) {
            auto server = config.getString(handles[i]);
            if (*server) {
                if (firstServer) {
                    firstServer = false;
                    out.print(F(HTML_S(br) "Servers "));
                } else {
                    out.print(FSPGM(comma_));
                }
                out.print(server);
            }
        }
        if (_lastNtpCallback) {
            float seconds = get_time_diff(_lastNtpCallback, millis()) / 1000.0;
            unsigned int nextSeconds = (_ntpRefreshTime - get_time_diff(_lastNtpUpdate, millis())) / 1000;
            out.printf_P(PSTR(HTML_S(br) "Last update %.2f seconds ago, next update in %u seconds"), seconds, nextSeconds);
        }
    }
    else {
        out.print(FSPGM(Disabled));
    }
}

void TimezoneData::updateLoop()
{
    if (get_time_diff(_lastNtpUpdate, millis()) > _ntpRefreshTime) { // refresh NTP time manually
        _debug_println(F("refreshing NTP time manually"));
        configTime();
    }
    if (_zoneEnd != 0 && time(nullptr) >= _zoneEnd) {
        _debug_printf_P(PSTR("triggered\n"));
        LoopFunctions::remove(TimezoneData::updateLoop); // remove once triggered
        timezoneData = new TimezoneData();
        if (WiFi.isConnected()) { // simulate event if WiFi is already connected
            timezoneData->wifiConnectedCallback(WiFiCallbacks::EventEnum_t::CONNECTED, nullptr);
        }
    }
}

void TimezoneData::updateNtpCallback()
{
    debug_printf_P(PSTR("new time=%u, tz=%d\n"), (uint32_t)time(nullptr), sntp_get_timezone());

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

#if RTC_SUPPORT
    // update RTC
    config.setRTC(time(nullptr));
#endif
#if NTP_HAVE_CALLBACKS
    for(auto callback: _callbacks) {
        callback(time(nullptr));
    }
#endif
}

void TimezoneData::configTime()
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

/*
 * Enable SNTP and remote timezone check if configured
 */
void timezone_setup()
{
    if (config._H_GET(Config().flags).ntpClientEnabled) {

        // force SNTP update on WiFi connect
        WiFiCallbacks::add(WiFiCallbacks::EventEnum_t::CONNECTED, TimezoneData::wifiConnectedCallback);

#if NTP_RESTORE_TIMEZONE_AFTER_WAKEUP
        if (resetDetector.hasWakeUpDetected()) { // restore timezone from RTC memory
            NTPClientData_t ntp;
            if (RTCMemoryManager::read(NTP_CLIENT_RTC_MEM_ID, &ntp, sizeof(ntp))) {

                auto &timezone = get_default_timezone();
                timezone.setOffset(ntp.offset);
                timezone.setAbbreviation(ntp.abbreviation);
                TimezoneData::_setZoneEnd(ntp.zoneEnd);

// restore time if no RTC is present
#if NTP_RESTORE_SYSTEM_TIME_AFTER_WAKEUP && !RTC_SUPPORT
                time_t msec = ntp.sleepTime + millis();             // add sleep time + time since wake up
                time_t seconds = msec / 1000UL;
                msec -= seconds;                                    // remove full seconds
                struct timeval tv = { (time_t)(ntp.currentTime + seconds), (suseconds_t)(msec * 1000L) };
                settimeofday(&tv, nullptr);
                _debug_printf_P(PSTR("stored time %lu, sleep time %u, millis() %lu, new time sec %lu usec %lu\n"), ntp.currentTime, ntp.sleepTime, millis(), tv.tv_sec, tv.tv_usec);
#endif
                TimezoneData::configTime();     // request real time from ntp

                _debug_printf_P(PSTR("restored timezone after wake up. abbreviation=%s, offset=%d, zoneEnd=%lu\n"), ntp.abbreviation, ntp.offset, ntp.zoneEnd);
                return;
            }
        }
#endif

        TimezoneData::configTime();

        auto remoteUrl = config._H_STR(Config().ntp.remote_tz_dst_ofs_url);
        if (*remoteUrl) {
            if (!timezoneData) {
                timezoneData = new TimezoneData();
            }
            if (WiFi.isConnected()) { // simulate event if WiFi is already connected
                TimezoneData::wifiConnectedCallback(WiFiCallbacks::EventEnum_t::CONNECTED, nullptr);
            }
        }

    } else {

		auto str = emptyString.c_str();
		configTime(0, 0, str, str, str);
        // sntp_set_timezone(0);

        LoopFunctions::remove(TimezoneData::updateLoop);

        if (timezoneData) {
            delete timezoneData;
            timezoneData = nullptr;
        }
    }
}

inline void ntp_client_reconfigure_plugin(PGM_P source)
{
    timezone_setup();
}

class NTPPlugin : public PluginComponent, public KFCRestAPI {
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

// KFCRestAPI
public:
    virtual void getRestUrl(String &url) const {
        url = FPSTR(config._H_STR(Config().ntp.remote_tz_dst_ofs_url));
    }
    virtual void getBearerToken(String &token) const {
    }
};


static NTPPlugin plugin;

void NTPPlugin::setup(PluginSetupMode_t mode)
{
    timezone_setup();
}

void NTPPlugin::reconfigure(PGM_P source)
{
    ntp_client_reconfigure_plugin(source);
}

void NTPPlugin::restart()
{
    settimeofday_cb(nullptr);
    WiFiCallbacks::remove(WiFiCallbacks::EventEnum_t::ANY, TimezoneData::wifiConnectedCallback);
    LoopFunctions::remove(TimezoneData::updateLoop);

    if (timezoneData) {
        delete timezoneData;
        timezoneData = nullptr;
    }
}

void NTPPlugin::getStatus(Print &output)
{
    TimezoneData::getStatus(output);
    output.printf_P(PSTR(", %u request(s) pending"), _requests.size());
}

void NTPPlugin::createConfigureForm(AsyncWebServerRequest *request, Form &form) {

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
        ntp.sleepTime = time;
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
        TimezoneData::configTime();
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
commandNow:
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
            args.printf_P(PSTR("%s, unixtime=%u, valid=%u, dst=%u, SNTP tz=%d"), timestamp, now, timezone.isValid(), timezone.isDst(), sntp_get_timezone());
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
                time_t zoneEnd = TimezoneData::getZoneEnd();
                if (!zoneEnd) {
                    strcpy_P(buf, PSTR("not scheduled"));
                }
                else {
                    timezone_strftime_P(buf, sizeof(buf), SPGM(strftime_date_time_zone), timezone_localtime(&zoneEnd));
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
                timezone_setup();
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

#include <pop_pack.h>

#endif
