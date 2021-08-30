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
    OSTIMER_INLINE ETSTimerEx::ETSTimerEx(const char *name) : _name(name),
#else
    OSTIMER_INLINE ETSTimerEx::ETSTimerEx() :
#endif
    _timer(nullptr),
    _running(false),
    _locked(false)
{
}

OSTIMER_INLINE ETSTimerEx::~ETSTimerEx()
{
    if (_timer) {
        disarm();
        done();
    }
    clear();
}

OSTIMER_INLINE void ETSTimerEx::create(esp_timer_cb_t callback, void *arg)
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
    if (esp_timer_create(&args, &_timer) == ESP_OK) {
        _timers.push_back(this);
        _running = false;
        _locked = false;
    }
    else {
        clear();
    }
}

OSTIMER_INLINE void ETSTimerEx::arm(int32_t delay, bool repeat, bool isMillis)
{
    uint64_t delayMicros = isMillis ? delay * 1000ULL : delay;
    _running = true;
    _locked = false;
    esp_timer_stop(_timer);
    if (repeat) {
        if (esp_timer_start_periodic(_timer, delayMicros) != ESP_OK) {
            __DBG_printf("esp_timer_start_periodic failed delay=%d", delay);
            _running = false;
        }
    }
    else {
        if (esp_timer_start_once(_timer, delayMicros) != ESP_OK) {
            __DBG_printf("esp_timer_start_once failed delay=%d", delay);
            _running = false;
        }
    }
}

OSTIMER_INLINE bool ETSTimerEx::isRunning() const
{
    return _timer != nullptr && esp_timer_is_active(_timer);
}

OSTIMER_INLINE bool ETSTimerEx::isDone() const
{
    return _timer == nullptr;
}

OSTIMER_INLINE bool ETSTimerEx::isLocked() const
{
    return _timer && _running && _locked;
}

OSTIMER_INLINE void ETSTimerEx::lock()
{
    if (_timer && !_locked) {
        _locked = true;
    }
}

OSTIMER_INLINE void ETSTimerEx::unlock()
{
    if (_timer && _locked) {
        _locked = false;
    }
}

OSTIMER_INLINE void ETSTimerEx::disarm()
{
    if (_timer) {
        esp_timer_stop(_timer);
    }
    _running = false;
}

OSTIMER_INLINE void ETSTimerEx::done()
{
    if (isRunning()) {
        disarm();
    }
    if (_timer) {
        esp_timer_delete(_timer);
        _timers.erase(std::remove(_timers.begin(), _timers.end(), this), _timers.end());
        _timer = nullptr;
    }
}

OSTIMER_INLINE void ETSTimerEx::clear()
{
    _timer = nullptr;
    _running = false;
    _locked = false;
}

OSTIMER_INLINE ETSTimerEx *ETSTimerEx::find()
{
    return find(this);
}

OSTIMER_INLINE ETSTimerEx *ETSTimerEx::find(ETSTimerEx *timer)
{
    auto iterator = std::find(_timers.begin(), _timers.end(), timer);
    if (iterator == _timers.end()) {
        return nullptr;
    }
    return timer;
}

OSTIMER_INLINE void ETSTimerEx::end()
{
    for(auto timer: _timers) {
        if (!timer->isDone()) {
            if (timer->isRunning()) {
                timer->disarm();
            }
            esp_timer_delete(timer->_timer);
            timer->_timer = nullptr;
        }
    }
    _timers.clear();
}

#endif

#ifndef _MSC_VER
#    pragma GCC pop_options
#endif
