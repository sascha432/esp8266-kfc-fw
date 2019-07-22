/**
 * Author: sascha_lammers@gmx.de
 */

#if NTP_CLIENT

#include <Arduino_compat.h>

#ifndef DEBUG_NTP_CLIENT
#define DEBUG_NTP_CLIENT                        1
#endif

// TODO this fails if the wake up was caused by an external event and not the timer
// if an external event can be detected, skip setting the system time
#ifndef NTP_RESTORE_SYSTEM_TIME_AFTER_WAKEUP
#define NTP_RESTORE_SYSTEM_TIME_AFTER_WAKEUP    1
#endif

#ifndef NTP_RESTORE_TIMEZONE_AFTER_WAKEUP
#define NTP_RESTORE_TIMEZONE_AFTER_WAKEUP       1
#endif

#define NTP_CLIENT_RTC_MEM_ID                   3

#if NTP_RESTORE_SYSTEM_TIME_AFTER_WAKEUP
void ntp_client_prepare_deep_sleep(uint32_t time);
#endif

#endif
