/**
 * Author: sascha_lammers@gmx.de
 */

#include "MicrosTimer.h"

uint32_t get_time_diff(uint32_t start, uint32_t end) {
    if (end >= start) {
        return end - start;
    }
    // handle overflow
    return end + ~start + 1;
}

MicrosTimer::MicrosTimer() : _valid(false) {
}

MicrosTimer::timer_t MicrosTimer::getTime(timer_t time) {
    if (!_valid) {
        return 0;
    }
    if (time >= _start) {
        time -= _start;
    }
    else { // timer_t overflow
        time += ~_start + 1;
        if (time >= (~0UL >> 1)) {    // most likely micros() skipped an overflow and is out of sync
            _valid = false;
            return 0;
        }
    }
    return time;
}

void MicrosTimer::start(timer_t time) {
    _start = time;
    _valid = true;
}
