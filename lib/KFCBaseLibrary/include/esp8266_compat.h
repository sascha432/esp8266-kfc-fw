/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if defined(ESP8266)

#include <os_type.h>
#include <user_interface.h>

#define SPIFFS_info(info)                           KFCFS.info(info)
#define SPIFFS_openDir(dirname)                     KFCFS.openDir(dirname)

#define WiFi_isHidden(num)                          WiFi.isHidden(num)

extern "C" {

    bool can_yield();

    uint64_t micros64();

    void ets_timer_setfn (ETSTimer *ptimer, ETSTimerFunc *pfunction, void *parg);
    // void ets_timer_arm_new(ETSTimer *ptimer, int time_ms, int repeat_flag, int isMillis);
    void ets_timer_disarm (ETSTimer *ptimer);
    void ets_timer_done (ETSTimer *ptimer);

#if ARDUINO_ESP8266_VERSION_COMBINED == 0x020603
    void settimeofday_cb (void (*cb)(void));
#endif

}

#endif
