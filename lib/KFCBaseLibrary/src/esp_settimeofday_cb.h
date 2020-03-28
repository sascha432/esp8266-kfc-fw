/**
 * Author: sascha_lammers@gmx.de
 */

// settimeofday_cb() emulation, requires
// build_flags = -Wl,--wrap=settimeofday

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
