/**
 * Author: sascha_lammers@gmx.de
 */

#if NTP_CLIENT

#include <Arduino_compat.h>

#ifndef DEBUG_NTP_CLIENT
#define DEBUG_NTP_CLIENT                        0
#endif

// TODO this fails if the wake up was caused by an external event and not the timer
// if an external event can be detected, skip setting the system time
#ifndef NTP_RESTORE_SYSTEM_TIME_AFTER_WAKEUP
#define NTP_RESTORE_SYSTEM_TIME_AFTER_WAKEUP    0
#endif

#ifndef NTP_RESTORE_TIMEZONE_AFTER_WAKEUP
#define NTP_RESTORE_TIMEZONE_AFTER_WAKEUP       0
#endif

#ifndef NTP_HAVE_CALLBACKS
#define NTP_HAVE_CALLBACKS                      0
#endif

#ifndef NTP_LOG_TIME_UPDATE
#define NTP_LOG_TIME_UPDATE                     0
#endif

#define NTP_CLIENT_RTC_MEM_ID                   3

#if NTP_RESTORE_SYSTEM_TIME_AFTER_WAKEUP
void ntp_client_prepare_deep_sleep(uint32_t time);
#endif

#if NTP_HAVE_CALLBACKS

#define NTP_IS_TIMEZONE_UPDATE(now)             (now == -1)

typedef std::function<void(time_t now)> TimeUpdatedCallback_t;

// gets called if the system time is updated (now = time(nullptr)) or timezone is set (NTP_IS_TIMEZONE_UPDATE(now) == true)
void addTimeUpdatedCallback(TimeUpdatedCallback_t callback);

#endif

#endif
