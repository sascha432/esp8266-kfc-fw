/**
  Author: sascha_lammers@gmx.de
*/

#include "pin_monitor.h"
#include "monitor.h"
#include "debounce.h"
#include "pin.h"
#include <PrintString.h>
#include <BitsToStr.h>
#include <misc.h>
#include <stl_ext/memory.h>
#include <MicrosTimer.h>
#include "rotary_encoder.h"
#include "logger.h"

#if PIN_MONITOR_USE_FUNCTIONAL_INTERRUPTS
#include <FunctionalInterrupt.h>
#else
#include "interrupt_impl.h"
#endif
#include "interrupt_event.h"

#if DEBUG_PIN_MONITOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif


using namespace PinMonitor;

Monitor pinMonitor;

Monitor::Monitor() :
    _lastRun(0),
    _loopTimer(nullptr),
    __LDBG_IF(_debugTimer(nullptr),)
    _pinMode(INPUT),
    _debounceTime(kDebounceTimeDefault)
{
}

Monitor::~Monitor()
{
// #if DEBUG
//     endDebug();
// #endif
    end();
}

//
// NOTE: using a timer will execute the callback events in a different context and some restrictons apply
// the execute time is limited to 5 milliseconds for example
//
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

    ETS_GPIO_INTR_DISABLE();
    eventBuffer.clear();
    ETS_GPIO_INTR_ENABLE();
}

// #if DEBUG

// void Monitor::beginDebug(Print &output, uint32_t interval)
// {
//     output.printf_P(PSTR("+PINM: pins=%u handlers=%u debounce=%ums loop_timer=%d\n"), _pins.size(), _handlers.size(), getDebounceTime(), _loopTimer ? *_loopTimer : -1);
//     for(auto &pin: _pins) {
//         output.printf_P(PSTR("+PINM: pin=%u this=%p usage=%u\n"), pin->getPin(), pin.get(), pin->getCount());
//     }
//     for(auto &handler: _handlers) {
//         output.printf_P(PSTR("+PINM: handler=%p "), handler.get());
//         // handler->dumpConfig(output);
//         output.println();
//     }
//     if (!_debugTimer && !_handlers.empty()) {
//         _debugTimer = new Event::Timer();
//         _Timer(_debugTimer)->add(Event::milliseconds(interval), true, [this, &output](Event::CallbackTimerPtr) {
//             PrintString str = F("+PINM: ");
//             for(auto &handler: _handlers) {
//                 str.printf_P(PSTR("%p=[pin=%u events=%u value=%u] "), handler.get(), handler->getPin(), handler->_eventCounter, digitalRead(handler->getPin()));
//             }
//             output.println(str);
//         });
//     }
// }

// void Monitor::endDebug()
// {
//     if (_debugTimer) {
//         delete _debugTimer;
//         _debugTimer = nullptr;
//     }
// }

// #endif

Pin &Monitor::attach(Pin *handler, HardwarePinType type)
{
    _handlers.emplace_back(handler);
    return _attach(*_handlers.back().get(), type);
}

Pin &Monitor::_attach(Pin &pin, HardwarePinType type)
{
    __LDBG_IF(auto typeStr = PSTR("new pin:"));
    auto pinNum = pin.getPin();
    bool pinsEmpty = _pins.empty();
    auto iterator = std::find_if(_pins.begin(), _pins.end(), [pinNum](const HardwarePinPtr &pin) {
        return pin->getPin() == pinNum;
    });
    if (iterator == _pins.end()) {

        pinMode(pinNum, _pinMode);
        switch(type) {
            case HardwarePinType::DEBOUNCE:
                _pins.emplace_back(new DebouncedHardwarePin(pinNum, digitalRead(pinNum)));
                break;
            case HardwarePinType::SIMPLE:
                _pins.emplace_back(new SimpleHardwarePin(pinNum));
                break;
            case HardwarePinType::ROTARY:
                _pins.emplace_back(new RotaryHardwarePin(pinNum, pin));
                break;
            default:
                __LDBG_panic("invalid pin type %u", type);
        }
        iterator = _pins.end() - 1;

        __LDBG_IF(typeStr = PSTR("updating pin:"));
    }
    auto &curPin = *iterator->get();

    __LDBG_printf("%s attaching pin=%u usage=%u arg=%p", typeStr, curPin.getPin(), curPin.getCount() + 1, pin.getArg());
    ++curPin;
    #if !PIN_MONITOR_USE_GPIO_INTERRUPT
        if (curPin) {
            #if PIN_MONITOR_USE_FUNCTIONAL_INTERRUPTS
                // +72 byte IRAM
                attachInterruptArg(digitalPinToInterrupt(pinNum), HardwarePin::callback, &curPin, CHANGE);
            #else
                // 0 byte IRAM
                ___attachInterruptArg(digitalPinToInterrupt(pinNum), HardwarePin::callback, &curPin);
            #endif
        }
    #endif
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
#if PIN_MONITOR_USE_GPIO_INTERRUPT
#else
#if PIN_MONITOR_USE_FUNCTIONAL_INTERRUPTS
                // +160 byte IRAM
                detachInterrupt(digitalPinToInterrupt(pinNum));
#else
                // 0 byte IRAM
                detachInterrupt(digitalPinToInterrupt(pinNum));
#endif
#endif
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
    detach(std::remove_if(_handlers.begin(), _handlers.end(), stdex::compare_unique_ptr(handler)), _handlers.end());
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

void Monitor::feed(uint32_t micros1, uint32_t states, bool activeHigh)
{
    for(const auto &event: PinMonitor::eventBuffer) {
        __DBG_printf("F-QUEUE: %s", event.toString().c_str());
    }

    uint32_t values = activeHigh ? states : ~states; // invert states for activelow
    // the container does not have a insert method
    // create a temporary copy with events to prepend and append all existing events
    Interrupt::EventBuffer prependEvents;

    for(auto &pinPtr: _pins) {
        auto debounce = pinPtr->getDebounce();
        if (debounce) {
            uint8_t pin = pinPtr->getPin();
            if (states & _BV(pin)) {
                auto iterator = std::find(PinMonitor::eventBuffer.begin(), PinMonitor::eventBuffer.end(), pin);
                if (iterator != PinMonitor::eventBuffer.end() && iterator->value() == activeHigh) {
                    // the first state of the PIN is active to inactive and we need to add the missing initial pulse
                    prependEvents.emplace_back(1500, pin, static_cast<uint16_t>(values));
                }
            }
        }
    }

    if (prependEvents.size()) {
        ETS_GPIO_INTR_DISABLE();
        // append all existing events
        prependEvents.copy(PinMonitor::eventBuffer.begin(), PinMonitor::eventBuffer.end());
        PinMonitor::eventBuffer = std::move(prependEvents);
        ETS_GPIO_INTR_ENABLE();

        for(const auto &event: PinMonitor::eventBuffer) {
            __DBG_printf("U-QUEUE: %s", event.toString().c_str());
        }
    }
    _lastRun = millis();
}

// NOTE: this method should be executes at least once per millisecond
// user experience degrades if the interval is longer than 5 milliseconds, malfunctions might occur >50-100ms
// in the case the main loop uses a lot delays, the pin monitor loop can be installed as a timer, which can be repeated at a minimum of 5ms
//
// see pinMonitor.begin(true)....
//
void Monitor::_loop()
{
    uint32_t now = millis();

    // process all new events
    if (eventBuffer.size()) {

        static uint16_t maxEventSize = 0;
        if (eventBuffer.size() > maxEventSize) {
            maxEventSize = eventBuffer.size();
            __DBG_printf("maxEventSize increased %u", maxEventSize);
        }

        for(auto &event: eventBuffer) {
            __DBG_printf("L-QUEUE: %s", event.toString().c_str());
        }

        ETS_GPIO_INTR_DISABLE();
        auto bufIterator = eventBuffer.begin();
        while (bufIterator != eventBuffer.end()) {
            auto event = *bufIterator;
            ETS_GPIO_INTR_ENABLE();

            __DBG_printf("L-FEED: %s", event.toString().c_str());

            auto iterator = std::find_if(_pins.begin(), _pins.end(), [&event](const HardwarePinPtr &ptr) {
                return ptr->getPin() == event.pin();
            });
            if (iterator != _pins.end()) {
                const auto &pinPtr = *iterator;
                switch(pinPtr->_type) {
                    case HardwarePinType::DEBOUNCE:
                        {
                            auto debounce = pinPtr->getDebounce();
                            if (debounce) {
                                _event(event.pin(), debounce->debounce(event.value(), 1, event.getTime(), now, event.getTime()), now);
                            }
                        }
                        break;
                    case HardwarePinType::ROTARY:
                        {
                            auto pinIterator = std::find_if(_handlers.begin(), _handlers.end(), [&event](const PinPtr &ptr) {
                                return ptr->getPin() == event.pin();
                            });
                            if (pinIterator != _handlers.end()) {
                                auto encoder = reinterpret_cast<RotaryEncoderPin *>(pinIterator->get())->getEncoder();
                                encoder->processEvent(event);
                            }
                        }
                        break;
                    case HardwarePinType::SIMPLE:
                        _event(event.pin(), event.value() ? StateType::IS_HIGH : StateType::IS_LOW, now);
                        break;
                    default:
                        break;
                }
            }

            ETS_GPIO_INTR_DISABLE();
            ++bufIterator;
        }
        if (bufIterator == eventBuffer.end()) {
            eventBuffer.clear();
        }
        ETS_GPIO_INTR_ENABLE();
    }


    if (now == _lastRun) { // runs up to 10-15x per millisecond
        return;
    }

    uint32_t time = micros();
    now = millis();

    // inject "empty" events once per millisecond into the pins with debouncing to update states that are time based
    for(const auto &pinPtr: _pins) {
        auto debounce = pinPtr->getDebounce();
        if (debounce) {
            auto &pin = *reinterpret_cast<DebouncedHardwarePin *>(pinPtr.get());
            _event(pin.getPin(), debounce->debounce(digitalRead(pin.getPin()), 0, time, now, time), now);
        }
    }
    for(const auto &handler: _handlers) {
        handler->loop();
    }
    _lastRun = millis();
}

void Monitor::_event(uint8_t pinNum, StateType state, uint32_t now)
{
    for(const auto &handler: _handlers) {
        StateType tmp;
        __DBG_assert_panic(handler.get() != nullptr, "event handler=nullptr pin=%u state=%u", pinNum, state);
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
