/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#if IOT_SWITCH

#ifndef DEBUG_IOT_SWITCH
#   define DEBUG_IOT_SWITCH 0
#endif

#ifndef IOT_SWITCH_ON_STATE
#   define IOT_SWITCH_ON_STATE HIGH
#endif

// interval to publish/refresh the state of all channels in milliseconds, 0 to disable
#ifndef IOT_SWITCH_PUBLISH_MQTT_INTERVAL
#   define IOT_SWITCH_PUBLISH_MQTT_INTERVAL (120 * 1000)
#endif

// store states in RTC memory
// states are loaded in preinit() to reduce recovery time to <50ms after a reset
// the boot-up configuration of the switch is ignored and only a power failure will erase the entry
#ifndef IOT_SWITCH_STORE_STATES_RTC_MEM
#   define IOT_SWITCH_STORE_STATES_RTC_MEM 1
#endif

// store states on file system
// persistent storage for power failure
#ifndef IOT_SWITCH_STORE_STATES_FS
#   define IOT_SWITCH_STORE_STATES_FS 1
#endif

// write delay on the file system in milliseconds
#ifndef IOT_SWITCH_STORE_STATES_WRITE_DELAY
#   if IOT_SWITCH_STORE_STATES_RTC_MEM
#       define IOT_SWITCH_STORE_STATES_WRITE_DELAY (10 * 1000)
#   else
#       define IOT_SWITCH_STORE_STATES_WRITE_DELAY (60 * 1000)
#   endif
#endif

#ifndef IOT_SWITCH_CHANNEL_NUM
#   error IOT_SWITCH_CHANNEL_NUM not defined
#endif

#ifndef IOT_SWITCH_CHANNEL_PINS
#   error IOT_SWITCH_CHANNEL_PINS not defined
#endif

#if IOT_SWITCH_CHANNEL_NUM > 12
#   error a maximum of 12 channels is supported
#endif

#if IOT_SWITCH_STORE_STATES_RTC_MEM && !defined(IOT_SWITCH_NO_DECL)

extern "C" void SwitchPlugin_rtcMemLoadState();

#endif

#endif
