/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <global.h>

#if defined(ESP8266)

#include <os_type.h>
#include <user_interface.h>

#define WiFi_isHidden(num)                          WiFi.isHidden(num)

extern "C" {

    bool can_yield();

    uint64_t micros64();

    void ets_timer_setfn (ETSTimer *ptimer, ETSTimerFunc *pfunction, void *parg);
    // void ets_timer_arm_new(ETSTimer *ptimer, int time_ms, int repeat_flag, int isMillis);
    void ets_timer_disarm (ETSTimer *ptimer);
    void ets_timer_done (ETSTimer *ptimer);

    extern "C" char *_tzname[2];

#if ARDUINO_ESP8266_MAJOR == 2 && ARDUINO_ESP8266_MINOR == 6

    void settimeofday_cb (void (*cb)(void));

#endif


#if ARDUINO_ESP8266_MAJOR >= 3

    #include <sys/_tz_structs.h>

    int settimeofday(const struct timeval* tv, const struct timezone* tz);

    bool wifi_softap_get_dhcps_lease(struct dhcps_lease *please);
    bool wifi_softap_set_dhcps_lease(struct dhcps_lease *please);

#endif

}

// currently not implemented
struct portMuxType {
    portMuxType() {}
    void enter() {
    }
    void exit() {
    }
    void enterISR() {
    }
    void exitISR() {
    }
};

// scope level auto enter/exit
struct portMuxLock {
    portMuxLock(portMuxType &mux) : _mux(mux) {
        // _mux.enter();
    }
    ~portMuxLock() {
        // _mux.exit();
    }
    // portMuxType &_mux;
};

// scope level auto enter/exit
struct portMuxLockISR {
    portMuxLockISR(portMuxType &mux) : _mux(mux) {
        // _mux.enterISR();
    }
    ~portMuxLockISR() {
        // _mux.exitISR();
    }
    // portMuxType &_mux;
};

#endif
