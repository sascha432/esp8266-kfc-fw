/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if defined(ESP8266)

#define SPIFFS_info(info)           SPIFFS.info(info)
#define SPIFFS_openDir(dirname)     SPIFFS.openDir(dirname)

#define WiFi_isHidden(num)          WiFi.isHidden(num)

typedef os_timer_func_t *           os_timer_func_t_ptr;

#define os_timer_create(timer, cb, arg) \
    timer = new os_timer_t(); \
    os_timer_setfn(timer, cb, arg);

#define os_timer_delete(timer)      delete timer;

#endif
