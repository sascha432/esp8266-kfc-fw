/**
  Author: sascha_lammers@gmx.de
*/

#include "pin_monitor.h"
#include "monitor.h"
#include "debounce.h"
#include "pin.h"
#include <misc.h>
#include <FunctionalInterrupt.h>

#if DEBUG_PIN_MONITOR
#include <debug_helper_enable.h>
#endif


using namespace PinMonitor;

Monitor pinMonitor;

Monitor::Monitor() : _debounceTime(10), _pinMode(INPUT)
{
}

Monitor::~Monitor()
{
    end();
}

void Monitor::begin()
{
    __LDBG_printf("begin pins=%u", _pins.size());
}

void Monitor::end()
{
    __LDBG_printf("end pins=%u", _pins.size());
    detach(_pins.begin(), _pins.end());
    _pins.clear();
    _pinsActive.clear();
}

Pin &Monitor::attach(Pin *pin)
{
    _pins.emplace_back(pin);
    return _attach(*_pins.back().get());
}


Pin &Monitor::_attach(Pin &pin)
{
    auto pinNum = pin.getPin();
    auto iterator = std::find(_pinsActive.begin(), _pinsActive.end(), pinNum);
    if (iterator == _pinsActive.end()) {
        _pinsActive.emplace_back(pinNum, _debounceTime);
        iterator = _pinsActive.end() -1;
        __LDBG_printf("pin=%u pinMode=%u", pinNum, _pinMode);
        pinMode(pinNum, _pinMode);
    }
    auto &curPin = *iterator;
    ++curPin;

    __LDBG_printf("attaching pin=%u usage=%u", curPin.getPin(), curPin.getCount());
    if (curPin) {
        __LDBG_printf("attaching interrupt pin=%u", pinNum);
        attachScheduledInterrupt(pinNum, callback, CHANGE);
    }
    if (_pins.size() == 1) {
        _attachLoop();
    }
    return pin;
}

void Monitor::detach(Predicate pred)
{
    detach(std::remove_if(_pins.begin(), _pins.end(), pred), _pins.end());
}

void Monitor::detach(Iterator begin, Iterator end)
{
#if DEBUG_PIN_MONITOR
    for(const auto &pin: _pins) {
        auto pinNum = pin->getPin();
        auto iterator = std::find(_pinsActive.begin(), _pinsActive.end(), pinNum);
        if (iterator != _pinsActive.end()) {
            auto &curPin = *iterator;
            --curPin;
            __LDBG_printf("detaching pin=%u usage=%u", pin->getPin(), curPin.getCount());
            if (curPin) {
                pinMode(pinNum, INPUT);
                __LDBG_printf("detaching interrupt pin=%u", pinNum);
                detachInterrupt(digitalPinToInterrupt(pinNum));
            }
        }
        else {
            __LDBG_printf("pin=%u not attached to any interrupt handler", pinNum);
        }

    }
#endif
    _pins.erase(begin, end);
    if (_pins.empty()) {
        _detachLoop();
    }
}

void Monitor::detach(Pin *pin)
{
    detach(std::remove_if(_pins.begin(), _pins.end(), std::compare_unique_ptr(pin)), _pins.end());
}

void Monitor::loop()
{
    pinMonitor._loop();
}

void Monitor::callback(InterruptInfo info)
{
    pinMonitor._callback(info);
}

void Monitor::_attachLoop()
{
    __LDBG_printf("adding pin monitor loop");
    LoopFunctions::add(loop);
}

void Monitor::_detachLoop()
{
    __LDBG_printf("removing pin monitor loop");
    LoopFunctions::remove(loop);
}

#if DEBUG_PIN_MONITOR && DEBUG_PIN_MONITOR_SHOW_EVENTS_INTERVAL
#include <PrintString.h>
static std::array<uint16_t, kMaxPins * 2> _events;
static unsigned eventTimerCount = 0;
#endif


void Monitor::_loop()
{
#if DEBUG_PIN_MONITOR && DEBUG_PIN_MONITOR_SHOW_EVENTS_INTERVAL
    // display events once per second
    if ((millis() / DEBUG_PIN_MONITOR_SHOW_EVENTS_INTERVAL) % 2 == eventTimerCount % 2) {
        PrintString str;
        eventTimerCount++;
        int n = 0;
        for(auto count: _events) {
            if (count) {
                str.printf_P(PSTR("pin=%u %s=%u "), n % kMaxPins, (n >= kMaxPins) ? PSTR("HIGH") : PSTR("LOW"), count);
            }
            n++;
        }
        _events = {};
        if (str.length()) {
            __DBG_printf("Events: %s", str.c_str());
        }
    }
#endif

    uint32_t now = millis();
    for(auto &pin: _pinsActive) {
        auto &debounce = pin.getDebounce();
        auto state = debounce.pop_state(now);
        switch(state) {
            case StateType::IS_HIGH:
                __LDBG_printf("EVENT: state changed to HIGH");
                break;
            case StateType::IS_LOW:
                __LDBG_printf("EVENT: state changed to LOW");
                break;
            case StateType::IS_FALLING:
                __LDBG_printf("EVENT: state changed to FALLING");
                break;
            case StateType::IS_RISING:
                __LDBG_printf("EVENT: state changed to RISING");
                break;
            case StateType::NONE:
            default:
                break;
        }
    // for(const auto &pin: _pins) {
    //     if (info.pin == pin->getPin()) {
    //         auto value = pin->_config._isActive(info.value);

    //     }
    // }

    }

}

void Monitor::_callback(InterruptInfo info)
{
#if DEBUG_PIN_MONITOR && DEBUG_PIN_MONITOR_SHOW_EVENTS_INTERVAL
    _events[info.pin + (info.value ? kMaxPins : 0)]++;
#endif

    uint32_t now = millis();
    for(auto &pin: _pinsActive) {
        if (pin.getPin() == info.pin) {
            // each pin has its own debounce object preparing the events for the pin object
            auto &debounce = pin.getDebounce();
            debounce.event(info.value, info.micro, now);
        }
    }
}

void Monitor::_event(uint8_t pinNum, bool state, TimeType now)
{
    for(const auto &pin: _pins) {
        if (pin->getPin() == pinNum && pin->_config._disabled == false) {
            pin->event(state, now);
        }
    }
}
