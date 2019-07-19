/**
 * Author: sascha_lammers@gmx.de
 */

#if NTP_CLIENT

#ifndef DEBUG_NTP_CLIENT
#define DEBUG_NTP_CLIENT 0
#endif

#include <sntp.h>
#include <KFCTimezone.h>
#include <KFCForms.h>
#include <PrintHtmlEntitiesString.h>
#include <EventScheduler.h>
#include <LoopFunctions.h>
#include <WiFiCallbacks.h>
#include "templates.h"
#include "progmem_data.h"
#include "logger.h"
#include "plugins.h"

#if DEBUG_NTP_CLIENT
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#include <push_pack.h>

typedef struct __attribute__packed__ {
    int32_t offset;
    char abbreviation[4];
} NTPClientData_t;

PROGMEM_STRING_DEF(strftime_date_time_zone, "%FT%T %Z");

#define NTP_CLIENT_RTC_MEM_ID 3

class TimezoneData;

static TimezoneData *timezoneData = nullptr;

class TimezoneData {
public:
    TimezoneData();
    virtual ~TimezoneData();

    RemoteTimezone *createRemoteTimezone();
    void deleteRemoteTimezone();

    void removeUpdateTimer();
    bool updateRequired();
    void setZoneEnd(time_t zoneEnd);
    const time_t *getZoneEndPtr() const;
    void retry(const String &message);

    static void wifiConnectedCallback(uint8_t event, void *payload);
    static const String getStatus();
    static const time_t getZoneEnd();

    static void updateLoop();

private:
    static time_t _zoneEnd;

    RemoteTimezone *_remoteTimezone;
    uint16_t _failureCount;
    EventScheduler::TimerPtr _updateTimer;
};

time_t TimezoneData::_zoneEnd = 0;

TimezoneData::TimezoneData() {
    _zoneEnd = 0;
    _failureCount = 0;
    _remoteTimezone = nullptr;
    _updateTimer = nullptr;
    WiFiCallbacks::add(WiFiCallbacks::EventEnum_t::CONNECTED, TimezoneData::wifiConnectedCallback);
}

TimezoneData::~TimezoneData() {
    WiFiCallbacks::remove(WiFiCallbacks::EventEnum_t::ANY, TimezoneData::wifiConnectedCallback);
    removeUpdateTimer();
    deleteRemoteTimezone();
}

RemoteTimezone *TimezoneData::createRemoteTimezone() {
    _debug_printf_P(PSTR("creating RemoteTimezone object %p\n"), _remoteTimezone);
    return (_remoteTimezone = _debug_new RemoteTimezone());
}

void TimezoneData::deleteRemoteTimezone() {
    _debug_printf_P(PSTR("deleting RemoteTimezone object %p\n"), _remoteTimezone);
    if (_remoteTimezone) {
        delete _remoteTimezone;
        _remoteTimezone = nullptr;
    }
}

void TimezoneData::removeUpdateTimer() {
    if (_updateTimer && Scheduler.hasTimer(_updateTimer)) {
        Scheduler.removeTimer(_updateTimer);
    }
    _updateTimer = nullptr;
}

bool TimezoneData::updateRequired() {
    return (!_remoteTimezone && (_zoneEnd == 0 || time(nullptr) >= _zoneEnd));
}

void TimezoneData::setZoneEnd(time_t zoneEnd) {
    _zoneEnd = zoneEnd;
    _failureCount = 0;
}

const time_t *TimezoneData::getZoneEndPtr() const {
    return &_zoneEnd;
}

const time_t TimezoneData::getZoneEnd() {
    return _zoneEnd;
}


void TimezoneData::retry(const String &message) {

    removeUpdateTimer();
    deleteRemoteTimezone();

    // echo '<?php for($i = 0; $i < 30; $i++) { echo floor(10 * (1 + ($i * $i / 3)).", "; } echo "\n";' |php
    // 10, 13, 23, 40, 63, 93, 130, 173, 223, 280, 343, 413, 490, 573, 663, 760, 863, 973, 1090, 1213, 1343, 1480, 1623, 1773, 1930, 2093, 2263, 2440, 2623, 2813, ...
    uint32_t next_check = 10 * (1 + (_failureCount * _failureCount / 3));

    _updateTimer = Scheduler.addTimer((int64_t)next_check * (int64_t)1000, false, [](EventScheduler::TimerPtr timer) {
        if (WiFi.isConnected()) { // retry if WiFi is connected, otherwise wait for connected event
            wifiConnectedCallback(WiFiCallbacks::EventEnum_t::CONNECTED, nullptr);
        }
    });
    _failureCount++;

    Logger_notice(F("Remote timezone: Update failed. Retrying (#%d) in %d second(s). %s"), _failureCount, next_check, message.c_str());
}

void TimezoneData::wifiConnectedCallback(uint8_t event, void *payload) {

#if DEBUG
    if (!timezoneData) {
        _debug_printf_P(PSTR("Remote timezone: wifiConnectedCallback(%d): timezoneData = nullptr\n"), event);
        return;
    }
#endif

    _debug_printf_P(PSTR("Remote timezone: wifiConnectedCallback(%d): updateRequired() = %d\n"), event, timezoneData->updateRequired());

    if (timezoneData->updateRequired()) {

        auto rtz = timezoneData->createRemoteTimezone();
        rtz->setUrl(config.getString(_H(Config().ntp.remote_tz_dst_ofs_url)));
        rtz->setTimezone(config.getString(_H(Config().ntp.timezone)));
        rtz->setStatusCallback([](bool status, const String message, time_t zoneEnd) {
            _debug_printf_P(PSTR("remote timezone callback: status %d message %s zoneEnd %ld\n"), status, message.c_str(), (long)zoneEnd);
            if (status) {
                auto &timezone = get_default_timezone();
                auto offset = timezone.getOffset();
                sint8_t tzOfs = timezone.isValid() ? offset / 3600 : 0;
                sntp_set_timezone(tzOfs);
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
                Logger_notice(F("Remote timezone: %s offset %0.2d:%0.2u. Next check at %s"), timezone.getAbbreviation().c_str(), offset / 3600, offset % 60, buf);

                // store timezone in RTC memory that it is available after wake up
                NTPClientData_t ntp;
                ntp.offset = offset;
                strncpy(ntp.abbreviation, timezone.getAbbreviation().c_str(), sizeof(ntp.abbreviation) - 1)[sizeof(ntp.abbreviation) - 1] = 0;
                RTCMemoryManager::write(NTP_CLIENT_RTC_MEM_ID, &ntp, sizeof(ntp));

            } else {
                timezoneData->retry(message);
            }
        });
        rtz->get();
    }
}


const String TimezoneData::getStatus() {
    if (config.get<ConfigFlags>(_H(Config().flags)).ntpClientEnabled) {
        PrintHtmlEntitiesString out;
        auto firstServer = true;
        auto &timezone = get_default_timezone();
        out.print(F("Timezone "));
        if (timezone.isValid()) {
            out.printf_P(PSTR("%s, %0.2d:%0.2u %s"), timezone.getTimezone().c_str(), timezone.getOffset() / 3600, timezone.getOffset() % 60, timezone.getAbbreviation().c_str());
        } else {
            out.printf_P(PSTR("%s, status invalid"), config._H_STR(ntp.timezone));
        }
        for (int i = 0; i < 3; i++) {
            const char *server;
            switch(i) { // _H_STR() = constexpr
                case 0:
                    server = config._H_STR(ntp.servers[0]);
                    break;
                case 1:
                    server = config._H_STR(ntp.servers[1]);
                    break;
                case 2:
                    server = config._H_STR(ntp.servers[2]);
                    break;
                case 3:
                    server = config._H_STR(ntp.servers[3]);
                    break;
            }
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
        return out;
    } else {
        return SPGM(Disabled);
    }
}

void TimezoneData::updateLoop() {
    if (_zoneEnd != 0 && time(nullptr) >= _zoneEnd) {
        _debug_printf_P(PSTR("Remote timezone: updateLoop triggered\n"));
        LoopFunctions::remove(TimezoneData::updateLoop); // remove once triggered
        timezoneData = _debug_new TimezoneData();
        if (WiFi.isConnected()) { // simulate event if WiFi is already connected
            timezoneData->wifiConnectedCallback(WiFiCallbacks::EventEnum_t::CONNECTED, nullptr);
        }
    }
}

/*
 * Enable SNTP and remote timezone check if configured
 */
void timezone_setup() {

    if (config.get<ConfigFlags>(_H(Config().flags)).ntpClientEnabled) {

        configTime(0, 0, config.getString(_H(Config().ntp.servers[0])), config.getString(_H(Config().ntp.servers[1])), config.getString(_H(Config().ntp.servers[2])));

        auto remoteUrl = config.getString(_H(Config().ntp.remote_tz_dst_ofs_url));
        if (*remoteUrl) {
            if (!timezoneData) {
                timezoneData = _debug_new TimezoneData();
            }
            if (WiFi.isConnected()) { // simulate event if WiFi is already connected
                TimezoneData::wifiConnectedCallback(WiFiCallbacks::EventEnum_t::CONNECTED, nullptr);
            }
        }

    } else {

		auto *str = _sharedEmptyString.c_str();
		configTime(0, 0, str, str, str);
        sntp_set_timezone(0);

        LoopFunctions::remove(TimezoneData::updateLoop);

        if (timezoneData) {
            delete timezoneData;
            timezoneData = nullptr;
        }
    }
}

void ntp_client_create_settings_form(AsyncWebServerRequest *request, Form &form) {

    form.add<bool>(F("ntp_enabled"), config.get<ConfigFlags>(_H(Config().flags)).ntpClientEnabled, [](bool value, FormField *field) {
        auto &flags = config._H_W_GET(Config().flags);
        flags.ntpClientEnabled = value;
    });

    form.add<sizeof Config().ntp.servers[0]>(F("ntp_server1"), config._H_W_STR(Config().ntp.servers[0]));
    form.addValidator(new FormValidHostOrIpValidator(true));

    form.add<sizeof Config().ntp.servers[1]>(F("ntp_server2"), config._H_W_STR(Config().ntp.servers[1]));
    form.addValidator(new FormValidHostOrIpValidator(true));

    form.add<sizeof Config().ntp.servers[2]>(F("ntp_server3"), config._H_W_STR(Config().ntp.servers[2]));
    form.addValidator(new FormValidHostOrIpValidator(true));

    form.add<sizeof Config().ntp.timezone>(F("ntp_timezone"), config._H_W_STR(Config().ntp.timezone));

#if USE_REMOTE_TIMEZONE
    form.add<sizeof Config().ntp.remote_tz_dst_ofs_url>(F("ntp_remote_tz_url"), config._H_W_STR(Config().ntp.remote_tz_dst_ofs_url));
#endif

    form.finalize();
}

#if AT_MODE_SUPPORTED

#include "at_mode.h"

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(NOW, "NOW", "Display current time");
PROGMEM_AT_MODE_HELP_COMMAND_DEF(TZ, "TZ", "<timezone>", "Set timezone", "Show timezone information");

bool ntp_client_at_mode_command_handler(Stream &serial, const String &command, int8_t argc, char **argv) {

    if (command.length() == 0) {

        at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(NOW));
        at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(TZ));

    } else if (constexpr_String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(NOW))) {
        time_t now = time(nullptr);
        char timestamp[64];
        if (!IS_TIME_VALID(now)) {
            serial.printf_P(PSTR("Time is currently not set (%lu). NTP is "), now);
            if (config.get<ConfigFlags>(_H(Config().flags)).ntpClientEnabled) {
                serial.println(FSPGM(enabled));
            } else {
                serial.println(FSPGM(disabled));
            }
        } else {
            strftime_P(timestamp, sizeof(timestamp), SPGM(strftime_date_time_zone), gmtime(&now));
            serial.printf_P(PSTR("+NOW %s\n"), timestamp);
            timezone_strftime_P(timestamp, sizeof(timestamp), SPGM(strftime_date_time_zone), timezone_localtime(&now));
            serial.printf_P(PSTR("+NOW %s\n"), timestamp);
        }
        return true;
    } else if (constexpr_String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(TZ))) {
        auto &timezone = get_default_timezone();
        if (argc == AT_MODE_QUERY_COMMAND) { // TZ?
            if (timezone.isValid()) {
                char buf[32];
                time_t zoneEnd = TimezoneData::getZoneEnd();
                if (!zoneEnd) {
                    strcpy_P(buf, PSTR("not scheduled"));
                } else {
                    timezone_strftime_P(buf, sizeof(buf), SPGM(strftime_date_time_zone), timezone_localtime(&zoneEnd));
                }
                serial.printf_P(PSTR("Timezone %s abbreviation %s offset %0.2d:%0.2u next update %s\n"), timezone.getTimezone().c_str(), timezone.getAbbreviation().c_str(), timezone.getOffset() / 3600, timezone.getOffset() % 60, buf);
            } else {
                serial.println(F("No valid timezone set"));
            }
        } else if (argc == 1) {
            if (config.get<ConfigFlags>(_H(Config().flags)).ntpClientEnabled) {
                config.setString(_H(Config().ntp.timezone), argv[0]);
                serial.printf_P(PSTR("Timezone set to %s\n"), config._H_STR(_Config.ntp.timezone));
                timezone_setup();
            } else {
                serial.print(F("NTP "));
                serial.println(FSPGM(disabled));
            }
        }
        return true;
    }
    return false;
}

#endif

// data is stored after retrieval already
// void ntp_client_prepare_deep_sleep(uint32_t time, RFMode mode) {
// }

void ntp_client_reconfigure_plugin() {
    timezone_setup();
}

PROGMEM_PLUGIN_CONFIG_DEF(
/* pluginName               */ ntp,
/* setupPriority            */ PLUGIN_PRIO_NTP,
/* allowSafeMode            */ false,
/* autoSetupWakeUp          */ false,
/* rtcMemoryId              */ NTP_CLIENT_RTC_MEM_ID,
/* setupPlugin              */ timezone_setup,
/* statusTemplate           */ TimezoneData::getStatus,
/* configureForm            */ ntp_client_create_settings_form,
/* reconfigurePlugin        */ ntp_client_reconfigure_plugin,
/* reconfigure Dependencies */ nullptr,
/* prepareDeepSleep         */ nullptr,
/* atModeCommandHandler     */ ntp_client_at_mode_command_handler
);

#endif

#include <pop_pack.h>
