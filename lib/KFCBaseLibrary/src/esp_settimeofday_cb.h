/**
 * Author: sascha_lammers@gmx.de
 */

// settimeofday_cb() emulation, requires
// build_flags = -Wl,--wrap=settimeofday

#include <global.h>

#if ESP8266 && ARDUINO_ESP8266_VERSION_COMBINED >= 0x030000

#include <coredecls.h>
#include <sys/_tz_structs.h>

#elif ESP8266 && ARDUINO_ESP8266_VERSION_COMBINED >= 0x020701

#error remove wrapper

#else

#include <LoopFunctions.h>

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
        LoopFunctions::callOnce(_settimeofday_cb);
    }
    return result;
}

}

#endif
