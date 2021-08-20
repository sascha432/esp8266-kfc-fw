/**
  Author: sascha_lammers@gmx.de
*/

// timer implementation for ESP32

#if ESP32

#include <esp_timer.h>

#ifndef OSTIMER_INLINE
#define OSTIMER_INLINE inline
#endif

std::list<ETSTimerEx *> ETSTimerEx::_timers;

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

    if (esp_timer_create(&args, &_timer) == ESP_OK) {
        _timers.push_back(this);
        _running = false;
        _locked = false;
    }
    else {
        clear();
    }
}

OSTIMER_INLINE void ETSTimerEx::arm(int32_t delay, bool repeat, bool millis)
{
    uint64_t delayMicros = millis ? delay * 1000 : delay;
    _running = true;
    _locked = false;
    if (repeat) {
        if (esp_timer_start_periodic(_timer, delayMicros) != ESP_OK) {
            _running = false;
        }
    }
    else {
        if (esp_timer_start_once(_timer, delayMicros) != ESP_OK) {
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
    return timer->isRunning() ? timer : nullptr;
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
