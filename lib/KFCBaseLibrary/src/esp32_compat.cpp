/**
 * Author: sascha_lammers@gmx.de
 */

#if defined(ESP32)

#include <Arduino.h>
#include <time.h>
#include <LoopFunctions.h>
#include "esp32_compat.h"

//TODO esp32
void analogWrite(uint8_t pin, uint16_t value)
{
}

bool can_yield()
{
    return true;
}

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
