/**
 * Author: sascha_lammers@gmx.de
 */

#if NTP_CLIENT

#include <sntp.h>
#include <KFCTimezone.h>
#include <PrintHtmlEntitiesString.h>
#include "templates.h"
#include "event_scheduler.h"
#include "progmem_data.h"
#include "logger.h"

#define DEBUG_TIMEZONE  1

#if DEBUG_TIMEZONE
#include "debug_local_def.h"
#else
#include "debug_local_undef.h"
#endif


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
    static String getStatus();

    static void updateLoop(void *);

private:
    static time_t _zoneEnd;

    RemoteTimezone *_remoteTimezone;
    uint16_t _failureCount;
    EventScheduler::EventTimerPtr _updateTimer;
};

time_t TimezoneData::_zoneEnd = 0;

TimezoneData::TimezoneData() {
    _zoneEnd = 0;
    _failureCount = 0;
    _remoteTimezone = nullptr;
    _updateTimer = nullptr;
    add_wifi_event_callback(TimezoneData::wifiConnectedCallback, WIFI_CB_EVENT_CONNECTED);
}

TimezoneData::~TimezoneData() {
    remove_wifi_event_callback(TimezoneData::wifiConnectedCallback, WIFI_CB_EVENT_CONNECTED);
    removeUpdateTimer();
    deleteRemoteTimezone();
}

RemoteTimezone *TimezoneData::createRemoteTimezone() {
    if_debug_printf_P(PSTR("creating RemoteTimezone object %p\n"), _remoteTimezone);
    return (_remoteTimezone = new RemoteTimezone());
}

void TimezoneData::deleteRemoteTimezone() {
    if_debug_printf_P(PSTR("deleting RemoteTimezone object %p\n"), _remoteTimezone);
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

void TimezoneData::retry(const String &message) {

    removeUpdateTimer();
    deleteRemoteTimezone();

    // echo '<?php for($i = 0; $i < 30; $i++) { echo floor(10 * (1 + ($i * $i / 3)).", "; } echo "\n";' |php
    // 10, 13, 23, 40, 63, 93, 130, 173, 223, 280, 343, 413, 490, 573, 663, 760, 863, 973, 1090, 1213, 1343, 1480, 1623, 1773, 1930, 2093, 2263, 2440, 2623, 2813, ...
    uint32_t next_check = 10 * (1 + (_failureCount * _failureCount / 3));

    _updateTimer = Scheduler.addTimer((int64_t)next_check * (int64_t)1000, false, [](EventScheduler::EventTimerPtr timer) {
        if (WiFi.isConnected()) { // retry if WiFi is connected, otherwise wait for connected event
            wifiConnectedCallback(WIFI_CB_EVENT_CONNECTED, nullptr);
        }
    });
    _failureCount++;

    Logger_notice(F("Remote timezone: Update failed. Retrying (#%d) in %d second(s). %s"), _failureCount, next_check, message.c_str());
}

void TimezoneData::wifiConnectedCallback(uint8_t event, void *payload) {

    if_debug_printf_P(PSTR("Remote timezone: wifiConnectedCallback: updateRequired() = %d\n"), timezoneData->updateRequired());

    if (timezoneData->updateRequired()) {

        auto rtz = timezoneData->createRemoteTimezone();

        rtz->setUrl(_Config.get().ntp.remote_tz_dst_ofs_url);
        rtz->setTimezone(_Config.get().ntp.timezone);
        rtz->setStatusCallback([](bool status, const String message, time_t zoneEnd) {
            if_debug_printf_P(PSTR("remote timezone callback: status %d message %s zoneEnd %ld\n"), status, message.c_str(), (long)zoneEnd);
            if (status) {
                auto &timezone = get_default_timezone();
                auto offset = timezone.getOffset();
                sint8_t tzOfs = timezone.isValid() ? offset / 3600 : 0;
                sntp_set_timezone(tzOfs);
                // sntp_set_daylight(0);

                timezoneData->setZoneEnd(zoneEnd);
                add_loop_function(TimezoneData::updateLoop); // on success install loop function instead of timer since those are limited to 7 at a time
                auto tmp = timezoneData;
                Scheduler.addTimer(1000, false, [tmp](EventScheduler::EventTimerPtr timer) { // delete outside the callback or deleting the async http client causes a crash in ~RemoteTimezone()
                    delete tmp;
                });
                timezoneData = nullptr;

                char buf[32];
                timezone_strftime_P(buf, sizeof(buf), PSTR("%a, %d %b %Y %T %Z"), timezone_localtime(timezoneData->getZoneEndPtr()));
                Logger_notice(F("Remote timezone: %s offset %0.2d:%0.2u. Next check at %s"), timezone.getAbbreviation().c_str(), offset / 3600, offset % 60, buf);

            } else {
                timezoneData->retry(message);
            }
        });
        rtz->get();
    }
}


String TimezoneData::getStatus() {
    if (_Config.getOptions().isNTPClient()) {
        PrintHtmlEntitiesString out;
        auto &config = _Config.get();
        auto firstServer = true;
        auto &timezone = get_default_timezone();
        out.print(F("Timezone "));
        if (timezone.isValid()) {
            out.printf_P(PSTR("%s, %0.2d:%0.2u %s"), timezone.getTimezone().c_str(), timezone.getOffset() / 3600, timezone.getOffset() % 60, timezone.getAbbreviation().c_str());
        } else {
            out.printf_P(PSTR("%s, status invalid"), config.ntp.timezone);
        }
        for (int i = 0; i < 3; i++) {
            if (config.ntp.servers[i]) {
                if (firstServer) {
                    firstServer = false;
                    out.print(F(HTML_S(br) "Servers "));
                } else {
                    out.print(FSPGM(comma_));
                }
                out.print(config.ntp.servers[i]);
            }
        }
        return out;
    } else {
        return SPGM(Disabled);
    }
}

void TimezoneData::updateLoop(void *) {
    if (_zoneEnd != 0 && time(nullptr) >= _zoneEnd) {
        if_debug_printf_P(PSTR("Remote timezone: updateLoop triggered\n"));
        remove_loop_function(TimezoneData::updateLoop); // remove once triggered
        timezoneData = new TimezoneData();
        if (WiFi.isConnected()) { // simulate event if WiFi is already connected
            timezoneData->wifiConnectedCallback(WIFI_CB_EVENT_CONNECTED, nullptr);
        }
    }
}

/*
 * Enable SNTP and remote timezone check if configured
 */
void timezone_setup() {

	WebTemplate::registerVariable(F("NTP_STATUS"), TimezoneData::getStatus);

    if (_Config.getOptions().isNTPClient()) {

        if (!timezoneData) {
            timezoneData = new TimezoneData();
        }

        auto &config = _Config.get();
        configTime(0, 0, config.ntp.servers[0], config.ntp.servers[1], config.ntp.servers[2]);

        if (WiFi.isConnected()) { // simulate event if WiFi is already connected
            TimezoneData::wifiConnectedCallback(WIFI_CB_EVENT_CONNECTED, nullptr);
        }

    } else {

		auto *str = _sharedEmptyString.c_str();
		configTime(0, 0, str, str, str);
        sntp_set_timezone(0);

        remove_loop_function(TimezoneData::updateLoop);

        if (timezoneData) {
            delete timezoneData;
            timezoneData = nullptr;
        }
    }
}

#endif
