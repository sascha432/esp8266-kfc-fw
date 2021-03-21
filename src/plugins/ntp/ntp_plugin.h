/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>

#ifndef DEBUG_NTP_CLIENT
#define DEBUG_NTP_CLIENT                        0
#endif

#ifndef NTP_HAVE_CALLBACKS
#define NTP_HAVE_CALLBACKS                      0
#endif

#ifndef NTP_LOG_TIME_UPDATE
#define NTP_LOG_TIME_UPDATE                     0
#endif

#ifndef NTP_STORE_STATUS
#define NTP_STORE_STATUS                        0
#endif

#if NTP_STORE_STATUS

struct NtpStatus {

    time_t _time;

    NtpStatus() : _time(0) {}
    NtpStatus(time_t now) : _time(now) {
    }

    operator bool () const {
        return IS_TIME_VALID(_time);
    }
};

#endif

#if NTP_HAVE_CALLBACKS

typedef std::function<void(time_t now)> TimeUpdatedCallback_t;

// gets called if the system time is updated
void addTimeUpdatedCallback(TimeUpdatedCallback_t callback);

#endif
