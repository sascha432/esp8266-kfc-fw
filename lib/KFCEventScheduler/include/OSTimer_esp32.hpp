/**
  Author: sascha_lammers@gmx.de
*/

// timer implementation for ESP32

#pragma once

#ifndef _MSC_VER
#    pragma GCC push_options
#    pragma GCC optimize("O3")
#endif

#if ESP32

#include <esp_timer.h>

#if DEBUG_OSTIMER
    #define DEBUG_OSTIMER_VALIDATE(timer, func) timer->__debug_ostimer_validate_result(func, _STRINGIFY(func), __BASENAME_FILE__, __LINE__)

    inline esp_err_t ETSTimerEx::__debug_ostimer_validate_result(esp_err_t error, const char *func, PGM_P file, int line) const
    {
        if (isDebugContextActive()) {
            if (error != ESP_OK) {
                debug_prefix_args(file, line);
                ___DBG_printEtsTimer_E(*this, PrintString(F("%s error=%x"), func, error).c_str());
            }
        }
        return error;
    }
#else
    DEBUG_OSTIMER_VALIDATE(timer, func) func
#endif

#if DEBUG_OSTIMER
    inline ETSTimerEx::ETSTimerEx(const char *name) : _name(strdup_P(name)),
#else
    inline ETSTimerEx::ETSTimerEx() :
#endif
    _timer(nullptr),
    _running(false),
    _locked(false)
{
}

inline __attribute__((__always_inline__)) ETSTimerEx::~ETSTimerEx()
{
    done();
    free(const_cast<char *>(_name));
}

inline void ETSTimerEx::create(esp_timer_cb_t callback, void *arg)
{
    esp_timer_create_args_t args = {
        callback,
        arg,
        esp_timer_dispatch_t::ESP_TIMER_TASK,
        #if DEBUG_OSTIMER
            _name
        #else
            "OSTimer"
        #endif
    };

    done();
    if (DEBUG_OSTIMER_VALIDATE(this, esp_timer_create(&args, &_timer)) == ESP_OK) {
        _timers.push_back(this);
        _running = false;
        _locked = false;
    }
    else {
        #if DEBUG_OSTIMER
            __DBG_panic("%s: esp_timer_create failed", name());
        #else
            __DBG_panic("esp_timer_create failed");
        #endif
    }
}

inline void ETSTimerEx::arm(int32_t delay, bool repeat, bool isMillis)
{
    #if DEBUG_OSTIMER
        if (!_timer) {
            __DBG_panic("%s: timer is nullptr", name());
        }
    #endif
    uint64_t delayMicros = isMillis ? delay * 1000ULL : delay;
    if (_running) {
        DEBUG_OSTIMER_VALIDATE(this, esp_timer_stop(_timer));
    }
    _running = true;
    _locked = false;
    if (repeat) {
        if (DEBUG_OSTIMER_VALIDATE(this, esp_timer_start_periodic(_timer, delayMicros)) != ESP_OK) {
            _running = false;
        }
    }
    else {
        if (DEBUG_OSTIMER_VALIDATE(this, esp_timer_start_once(_timer, delayMicros)) != ESP_OK) {
            _running = false;
        }
    }
}

inline __attribute__((__always_inline__)) bool ETSTimerEx::isRunning() const
{
    return _timer != nullptr && _running && esp_timer_is_active(_timer);
}

inline __attribute__((__always_inline__)) bool ETSTimerEx::isNew() const
{
    return _timer == nullptr;
}

inline __attribute__((__always_inline__)) bool ETSTimerEx::isDone() const
{
    return _timer == nullptr;
}

inline __attribute__((__always_inline__)) bool ETSTimerEx::isLocked() const
{
    return _timer && _running && _locked;
}

inline __attribute__((__always_inline__)) void ETSTimerEx::lock()
{
    if (_timer && !_locked) {
        _locked = true;
    }
}

inline __attribute__((__always_inline__)) void ETSTimerEx::unlock()
{
    if (_locked) {
        _locked = false;
    }
}

inline void ETSTimerEx::disarm()
{
    if (_timer && _running && esp_timer_is_active(_timer)) {
        DEBUG_OSTIMER_VALIDATE(this, esp_timer_stop(_timer));
    }
    _running = false;
}

inline void ETSTimerEx::done()
{
    if (_timer) {
        if (_running && esp_timer_is_active(_timer)) {
            DEBUG_OSTIMER_VALIDATE(this, esp_timer_stop(_timer));
        }
        auto iterator = std::remove(_timers.begin(), _timers.end(), this);
        if (iterator != _timers.end()) {
            DEBUG_OSTIMER_VALIDATE(this, esp_timer_delete(_timer));
            _timers.erase(iterator, _timers.end());
        }
    }
    clear();
}

inline __attribute__((__always_inline__)) void ETSTimerEx::clear()
{
    _timer = nullptr;
    _running = false;
    _locked = false;
}

#if DEBUG_OSTIMER_FIND

inline __attribute__((__always_inline__)) ETSTimerEx *ETSTimerEx::find()
{
    return find(this);
}

inline ETSTimerEx *ETSTimerEx::find(ETSTimerEx *timer)
{
    auto iterator = std::find(_timers.begin(), _timers.end(), timer);
    if (iterator == _timers.end()) {
        return nullptr;
    }
    return timer;
}

#endif

inline void ETSTimerEx::end()
{
    for(auto timer: _timers) {
        if (timer->_timer) {
            if (timer->_running) {
                DEBUG_OSTIMER_VALIDATE(timer, esp_timer_stop(timer->_timer));
            }
            DEBUG_OSTIMER_VALIDATE(timer, esp_timer_delete(timer->_timer));
            timer->clear();
        }
    }
    _timers.clear();
}

#endif

#ifndef _MSC_VER
#    pragma GCC pop_options
#endif
