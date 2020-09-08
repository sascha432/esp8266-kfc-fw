/**
  Author: sascha_lammers@gmx.de
*/

#include "pin_monitor.h"
#include "monitor.h"
#include "debounce.h"
#include "pin.h"
#include <PrintString.h>
#include <misc.h>
#include <MicrosTimer.h>

#if DEBUG_PIN_MONITOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using namespace PinMonitor;

Monitor pinMonitor;

Monitor::Monitor() : _lastRun(0), _loopTimer(nullptr), __DBG_IF(_debugTimer(nullptr),) _pinMode(INPUT), _debounceTime(kDebounceTimeDefault)
{
}

Monitor::~Monitor()
{
#if DEBUG
    endDebug();
#endif
    end();
}

void Monitor::begin(bool useTimer)
{
    __LDBG_printf("begin pins=%u handlers=%u use_timer=%u", _pins.size(), _handlers.size(), useTimer);
    if (useTimer && !_loopTimer) {
        _loopTimer = new Event::Timer();
    }
    else if (!useTimer && _loopTimer) {
        delete _loopTimer;
        _loopTimer = nullptr;
    }
    if (!_pins.empty()) {
        _attachLoop();
    }
}

void Monitor::end()
{
    __LDBG_printf("end pins=%u handlers=%u", _pins.size(), _handlers.size());
    if (_loopTimer) {
        delete _loopTimer;
        _loopTimer = nullptr;
    }
    _detach(_handlers.begin(), _handlers.end(), true);
}

#if DEBUG

void Monitor::beginDebug(Print &output, uint32_t interval)
{
    output.printf_P(PSTR("+PINM: pins=%u handlers=%u debounce=%ums loop_timer=%d\n"), _pins.size(), _handlers.size(), getDebounceTime(), _loopTimer ? *_loopTimer : -1);
    for(auto &pin: _pins) {
        output.printf_P(PSTR("+PINM: pin=%u this=%p usage=%u\n"), pin->getPin(), pin.get(), pin->getCount());
    }
    for(auto &handler: _handlers) {
        output.printf_P(PSTR("+PINM: handler=%p "));
        handler->dumpConfig(output);
        output.println();
    }
    if (!_debugTimer && !_handlers.empty()) {
        _debugTimer = new Event::Timer();
        _Timer(_debugTimer)->add(Event::milliseconds(interval), true, [this, &output](Event::CallbackTimerPtr) {
            PrintString str = F("+PINM: ");
            for(auto &handler: _handlers) {
                str.printf_P(PSTR("%p=[pin=%u events=%u value=%u] "), handler.get(), handler->getPin(), handler->_eventCounter, digitalRead(handler->getPin()));
            }
            output.println(str);
        });
    }
}

void Monitor::endDebug()
{
    if (_debugTimer) {
        delete _debugTimer;
        _debugTimer = nullptr;
    }
}

#endif

Pin &Monitor::attach(Pin *handler)
{
    _handlers.emplace_back(handler);
    return _attach(*_handlers.back().get());
}

Pin &Monitor::_attach(Pin &pin)
{
    auto pinNum = pin.getPin();
    bool pinsEmpty = _pins.empty();
    auto iterator = std::find_if(_pins.begin(), _pins.end(), [pinNum](const HardwarePinPtr &pin) {
        return pin->getPin() == pinNum;
    });
    if (iterator == _pins.end()) {

        pinMode(pinNum, _pinMode);
        _pins.emplace_back(new HardwarePin(pinNum));
        iterator = _pins.end() - 1;

        __LDBG_printf("pin=%u pinMode=%u", pinNum, _pinMode);
    }
    auto &curPin = *iterator->get();

    __LDBG_printf("attaching pin=%u usage=%u", curPin.getPin(), curPin.getCount() + 1);
    if (++curPin) {
        attachInterruptArg(digitalPinToInterrupt(pinNum), HardwarePin::callback, &curPin, CHANGE);
    }
    if (pinsEmpty) {
        _attachLoop();
    }
    return pin;
}

void Monitor::detach(Predicate pred)
{
    detach(std::remove_if(_handlers.begin(), _handlers.end(), pred), _handlers.end());
}

void Monitor::detach(const void *arg) {
    detach([arg](const PinPtr &pin) {
        return pin->getArg() == arg;
    });
}

void Monitor::_detach(Iterator begin, Iterator end, bool clear)
{
    for(const auto &pin: _handlers) {
        auto pinNum = pin->getPin();
        auto iterator = std::find_if(_pins.begin(), _pins.end(), [pinNum](const HardwarePinPtr &pin) {
            return pin->getPin() == pinNum;
        });
        if (iterator != _pins.end()) {
            auto &curPin = *iterator->get();
            __LDBG_printf("detaching pin=%u usage=%u remove=%u", pin->getPin(), curPin.getCount(), curPin.getCount() == 1);
            if (!--curPin) {
                __LDBG_printf("detaching interrupt pin=%u", pinNum);
                detachInterrupt(digitalPinToInterrupt(pinNum));
                pinMode(pinNum, INPUT);

                if (clear == false) {
                    _pins.erase(iterator);
                }
            }
        }
        else {
            __LDBG_printf("pin=%u not attached to any interrupt handler", pinNum);
        }
    }
    if (clear) {
        _detachLoop();
        __LDBG_printf("handlers/pins clear");
        _handlers.clear();
        _pins.clear();
    }
    else {
        __LDBG_printf("handlers erase begin=%d end=%u size=%u", std::distance(_handlers.begin(), begin), std::distance(_handlers.begin(), end), _handlers.size());
        _handlers.erase(begin, end);
        if (_pins.empty()) {
            _detachLoop();
        }
    }
    __LDBG_printf("handlers=%u pins=%u", _handlers.size(), _pins.size());
}

void Monitor::detach(Pin *handler)
{
    detach(std::remove_if(_handlers.begin(), _handlers.end(), std::compare_unique_ptr(handler)), _handlers.end());
}

void Monitor::loop()
{
    pinMonitor._loop();
}

void Monitor::loopTimer(Event::CallbackTimerPtr)
{
    pinMonitor._loop();
}

void Monitor::_attachLoop()
{
    __LDBG_printf("attach=loop timer=%d", _loopTimer ? (bool)*_loopTimer : -1);
    if (_loopTimer) {
        _Timer(_loopTimer)->add(Event::kMinDelay, true, loopTimer, Event::PriorityType::TIMER);
    }
    else {
        LoopFunctions::add(loop);
    }
}

void Monitor::_detachLoop()
{
    __LDBG_printf("detach=loop timer=%d", _loopTimer ? (bool)*_loopTimer : -1);
    if (_loopTimer) {
        _loopTimer->remove();
    }
    else {
        LoopFunctions::remove(loop);
    }
}



void Monitor::_loop()
{
    uint32_t now = millis();
    if (now == _lastRun) { // runs up to 10-15x per millisecond
        return;
    }
    _lastRun = now;

    for(auto &pinPtr: _pins) {
        auto &pin = *pinPtr;
        auto &debounce = pin.getDebounce();

        noInterrupts();
        auto micros = pin._micros;
        auto intCount = pin._intCount;
        bool value = pin._value;
        pin._intCount = 0;
        interrupts();

#if DEBUG_PIN_MONITOR_EVENTS
        auto state = debounce.debounce(value, intCount, micros, now);
        if (state != StateType::NONE) {
            __LDBG_printf("EVENT: pin=%u state=%s time=%u bounced=%u", pin.getPin(), stateType2Level(state), get_time_diff(debounce._startDebounce, now), std::max(0, debounce._bounceCounter - 1));
            // print --- of the state does not change for 1 second
            static Event::Timer timer;
            static uint32_t counter;
            auto tmp = ++counter;
            _Timer(timer).add(Event::milliseconds(1000), false, [tmp](Event::CallbackTimerPtr) {
                if (tmp == counter) {
                    __LDBG_print("---");
                }
            });
        }
        _event(pin.getPin(), state, now);
#else
        _event(pin.getPin(), debounce.debounce(value, intCount, micros, now), now);
#endif
    }
    for(const auto &handler: _handlers) {
        handler->loop();
    }
}

void Monitor::_event(uint8_t pinNum, StateType state, uint32_t now)
{
    for(const auto &handler: _handlers) {
        StateType tmp;
        if (handler->getPin() == pinNum && handler->isEnabled() && (tmp = handler->_getStateIfEnabled(state)) != StateType::NONE) {
            handler->_eventCounter++;
            handler->event(tmp, now);
        }
    }
}

const __FlashStringHelper *Monitor::stateType2String(StateType state)
{
    switch(state) {
        case StateType::PRESSED:
            return F("PRESSED");
        case StateType::RELEASED:
            return F("RELEASED");
        case StateType::IS_FALLING:
            return F("RELEASING");
        case StateType::IS_RISING:
            return F("PRESSING");
        case StateType::RISING_BOUNCED:
            return F("RELEASE_BOUNCED");
        case StateType::FALLING_BOUNCED:
            return F("PRESS_BOUNCED");
        default:
        case StateType::NONE:
            break;
    }
    return F("NONE");
}

const __FlashStringHelper *Monitor::stateType2Level(StateType state)
{
    switch(state) {
        case StateType::IS_HIGH:
            return F("HIGH");
        case StateType::IS_LOW:
            return F("LOW");
        case StateType::IS_FALLING:
            return F("FALLING");
        case StateType::IS_RISING:
            return F("RISING");
        case StateType::RISING_BOUNCED:
            return F("RISING_BOUNCED");
        case StateType::FALLING_BOUNCED:
            return F("FALLING_BOUNCED");
        default:
        case StateType::NONE:
            break;
    }
    return F("NONE");
}
