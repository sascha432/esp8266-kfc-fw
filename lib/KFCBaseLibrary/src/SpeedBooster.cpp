/**
 * Author: sascha_lammers@gmx.de
 */

#include "SpeedBooster.h"

#include <debug_helper_disable.h>

#if SPEED_BOOSTER_ENABLED

uint8_t SpeedBooster::_counter = 0;

SpeedBooster::SpeedBooster(bool autoEnable) : _autoEnable(autoEnable)
{
#if 0
    auto before = system_get_cpu_freq();
    _debug_printf_P(PSTR("counter=%d, CPU=%d->%d, boost=%d\n"), _counter, before, system_get_cpu_freq(), _enabled);
#endif
    _enabled = autoEnable;
    _counter++;
    if (_counter == 1 && system_get_cpu_freq() != SYS_CPU_160MHZ && _enabled) {
        system_update_cpu_freq(SYS_CPU_160MHZ);
    }
}

SpeedBooster::~SpeedBooster()
{
#if 0
    auto before = system_get_cpu_freq();
#endif
    _counter--;
    if (_counter == 0 && _enabled) {
        system_update_cpu_freq(SYS_CPU_80MHZ);
    }
    _debug_printf_P(PSTR("counter=%d, CPU=%d->%d, boost=%d\n"), _counter, before, system_get_cpu_freq(), _enabled);
}

bool SpeedBooster::isEnabled()
{
    return system_get_cpu_freq() == SYS_CPU_160MHZ;
}

void SpeedBooster::enable(bool value)
{
    if (value) {
        system_update_cpu_freq(SYS_CPU_160MHZ);
    }
    else {
        system_update_cpu_freq(SYS_CPU_80MHZ);
    }
}

bool SpeedBooster::isAvailable() {
    return true;
}

#else

SpeedBooster::SpeedBooster(bool autoEnable) {
}

SpeedBooster::~SpeedBooster() {
}

bool SpeedBooster::isEnabled() {
    return false;
}

void SpeedBooster::enable(bool value) {

}

bool SpeedBooster::isAvailable() {
    return false;
}

#endif

