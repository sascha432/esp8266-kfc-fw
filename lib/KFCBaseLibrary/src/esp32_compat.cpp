/**
 * Author: sascha_lammers@gmx.de
 */

#if defined(ESP32)

#include <Arduino.h>
#include <time.h>
#include "esp32_compat.h"

enum dhcp_status wifi_station_dhcpc_status(void)  {
    return DHCP_STOPPED;
}

//TODO esp32
void analogWrite(uint8_t pin, uint16_t value) {
}

extern "C" {

static void (*_settimeofday_cb)(void) = nullptr;

void settimeofday_cb (void (*cb)(void))
{
    _settimeofday_cb = cb;
}

int __real_settimeofday(const struct timeval* tv, const struct timezone* tz);

int __wrap_settimeofday(const struct timeval* tv, const struct timezone* tz)
{
    // call original function first and invoke callback if set
    int result = __real_settimeofday(tv, tz);
    if (_settimeofday_cb) {
        _settimeofday_cb();
    }
    return result;
}

}

#endif
