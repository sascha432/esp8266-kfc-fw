/**
 * Author: sascha_lammers@gmx.de
 */

// settimeofday_cb() emulation, requires
// build_flags = -Wl,--wrap=settimeofday

#include <global.h>

#if ESP8266 && ARDUINO_ESP8266_MAJOR >= 3

#include <coredecls.h>
#include <sys/_tz_structs.h>

#elif ESP8266 && ARDUINO_ESP8266_MAJOR >= 2 && ARDUINO_ESP8266_MINOR >= 7

#error remove wrapper

#else

#include <LoopFunctions.h>

using BoolCB = std::function<void(bool)>;
using TrivialCB = std::function<void()>;

// static void (*_settimeofday_cb)(void) = nullptr;
static TrivialCB _settimeofday_cb = nullptr;

void settimeofday_cb(const BoolCB &cb)
{
    auto callback = cb;
    _settimeofday_cb = [callback]() {
        callback(true);
    };
}

void settimeofday_cb(const TrivialCB &cb)
{
    _settimeofday_cb = cb;
}

extern "C" {

    void settimeofday_cb (void (*cb)(void))
    {
        _settimeofday_cb = [cb]() {
            cb();
        };
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
