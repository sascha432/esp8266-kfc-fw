/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>

#ifndef DEBUG_NTP_CLIENT
#define DEBUG_NTP_CLIENT                        1
#endif

#ifndef NTP_HAVE_CALLBACKS
#define NTP_HAVE_CALLBACKS                      0
#endif

#ifndef NTP_LOG_TIME_UPDATE
#define NTP_LOG_TIME_UPDATE                     1
#endif

#if NTP_HAVE_CALLBACKS

typedef std::function<void(time_t now)> TimeUpdatedCallback_t;

// gets called if the system time is updated
void addTimeUpdatedCallback(TimeUpdatedCallback_t callback);

#endif
