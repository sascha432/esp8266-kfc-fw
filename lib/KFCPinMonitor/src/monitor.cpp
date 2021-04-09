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
#include <PrintHtmlEntitiesString.h>

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

#if PIN_MONITOR_ROTARY_ENCODER_SUPPORT
    ETS_GPIO_INTR_DISABLE();
    eventBuffer.clear();
    ETS_GPIO_INTR_ENABLE();
#endif
}


void Monitor::printStatus(Print &output)
{
#if PIN_MONITOR_USE_GPIO_INTERRUPT
    output.print(F("Interrupt Handler: Custom GPIO Interrupt handler" HTML_S(br)));
#elif PIN_MONITOR_USE_FUNCTIONAL_INTERRUPTS == 0
    output.print(F("Interrupt Handler: Optimized Functional Interrupts" HTML_S(br)));
#else
    output.print(F("Interrupt Handler: Default Arduino Interrupts" HTML_S(br)));
#endif
#if PIN_MONITOR_BUTTON_GROUPS
    output.print(F("Button Groups: Enabled" HTML_S(br)));
#else
    output.print(F("Button Groups: Disabled" HTML_S(br)));
#endif
    output.printf_P(PSTR("Loop Mode: %s %s" HTML_S(br) "Default Mode: Active %s" HTML_S(br)  "Handlers/Pins: %u/%u " HTML_S(br)),
        _loopTimer ? PSTR("Timer") : PSTR("loop()"),
        (_loopTimer && *_loopTimer) || (!_loopTimer && !_pins.empty()) ? PSTR("Active") : PSTR("Inactive"),
        PIN_MONITOR_ACTIVE_STATE == ActiveStateType::ACTIVE_HIGH ? PSTR("High") : PSTR("Low"),
        _handlers.size(), _pins.size()
    );
    for(const auto &handler: _handlers) {
        const auto pinNum = handler->getPin();
        output.printf_P(PSTR("Pin %u, %s, %s, events %u"),
            pinNum,
            handler->isEnabled() ? PSTR("enabled") : PSTR("disabled"),
            handler->isInverted() ? PSTR("active low") : PSTR("active high"),
            handler->_eventCounter);

        auto iterator = std::find_if(_pins.begin(), _pins.end(), [pinNum](const HardwarePinPtr &pin) {
            return pin->getPin() == pinNum;
        });
        if (iterator == _pins.end()) {
            output.print(F(", no hardware pin assigned" HTML_S(br)));
        }
        else {
            const auto &pin = *iterator;
            output.printf_P(PSTR(", type %s" HTML_S(br)), pin->getHardwarePinTypeStr());
        }
    }
}

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
#if PIN_MONITOR_DEBOUNCED_PUSHBUTTON
            case HardwarePinType::DEBOUNCE:
                _pins.emplace_back(new DebouncedHardwarePin(pinNum, digitalRead(pinNum)));
                break;
#endif
#if PIN_MONITOR_SIMPLE_PIN
            case HardwarePinType::SIMPLE:
                _pins.emplace_back(new SimpleHardwarePin(pinNum));
                break;
#endif
#if PIN_MONITOR_ROTARY_ENCODER_SUPPORT
            case HardwarePinType::ROTARY:
                _pins.emplace_back(new RotaryHardwarePin(pinNum, pin));
                break;
#endif
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
                attachInterruptArg(digitalPinToInterrupt(pinNum), HardwarePin::callback, &curPin);
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
#if PIN_MONITOR_USE_GPIO_INTERRUPT == 0
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
#if PIN_MONITOR_USE_GPIO_INTERRUPT
    PinMonitor::GPIOInterruptsDisable();
#endif
    if (_loopTimer) {
        _loopTimer->remove();
    }
    else {
        LoopFunctions::remove(loop);
    }
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

#if PIN_MONITOR_ROTARY_ENCODER_SUPPORT
    // process all new events
    if (eventBuffer.size()) {

        static uint16_t maxEventSize = 0;
        if (eventBuffer.size() > maxEventSize) {
            maxEventSize = eventBuffer.size();
            __DBG_printf("maxEventSize increased %u", maxEventSize);
        }


#if 1
        {
            ETS_GPIO_INTR_DISABLE();
            auto bufIterator = eventBuffer.begin();
            while (bufIterator != eventBuffer.end()) {
                auto event = *bufIterator;
                ETS_GPIO_INTR_ENABLE();
                __DBG_printf("L-QUEUE: %s", event.toString().c_str());
                ETS_GPIO_INTR_DISABLE();
                ++bufIterator;
            }
            ETS_GPIO_INTR_ENABLE();
        }
#endif
        // struct DebounceSummary {
        //     uint32_t _micros;
        //     uint16_t _interruptCount;
        //     bool _value;
        //     Debounce *_debounce;
        //     DebounceSummary() : _interruptCount(0) {}
        // };
        // DebounceSummary dsPins[17] = {};

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
                // const auto &pinPtr = *iterator;
                // switch(pinPtr->_type) {
                    // case HardwarePinType::DEBOUNCE:
                    //     {
                    //         auto debounce = pinPtr->getDebounce();
                    //         if (debounce) {
                    //             auto &tmp = dsPins[event.pin()];
                    //             tmp._interruptCount++;
                    //             tmp._micros = event.getTime();
                    //             tmp._value = event.value();
                    //             tmp._debounce = debounce;
                    //             // _event(event.pin(), debounce->debounce(event.value(), 1, event.getTime(), now, event.getTime()), now);
                    //         }
                    //     }
                    //     break;
                    // case HardwarePinType::ROTARY:
                        // {
                            auto pinIterator = std::find_if(_handlers.begin(), _handlers.end(), [&event](const PinPtr &ptr) {
                                return ptr->getPin() == event.pin();
                            });
                            if (pinIterator != _handlers.end()) {
                                auto encoder = reinterpret_cast<RotaryEncoderPin *>(pinIterator->get())->getEncoder();
                                encoder->processEvent(event);
                            }
                        // }
                        // break;
                    // case HardwarePinType::SIMPLE:
                    //     _event(event.pin(), event.value() ? StateType::IS_HIGH : StateType::IS_LOW, now);
                    //     break;
                    // default:
                        // break;
                // }
            }

            ETS_GPIO_INTR_DISABLE();
            ++bufIterator;
        }
        if (bufIterator == eventBuffer.end()) {
            eventBuffer.clear();
        }
        ETS_GPIO_INTR_ENABLE();

    }
    now = millis();
#endif

#if PIN_MONITOR_DEBOUNCED_PUSHBUTTON || PIN_MONITOR_SIMPLE_PIN
    for(const auto &pinPtr: _pins) {
        switch(pinPtr->_type) {
#if PIN_MONITOR_DEBOUNCED_PUSHBUTTON
            case HardwarePinType::DEBOUNCE: {
                    auto &pin = *reinterpret_cast<DebouncedHardwarePin *>(pinPtr.get());
                    auto event = pin.getEventsClear();
                    if (event._interruptCount) {
                        _event(pin.getPin(), pin.getDebounce()->debounce(event._value, event._interruptCount, event._micros, now), now);
                    }
                }
                break;
#endif
#if PIN_MONITOR_SIMPLE_PIN
            case HardwarePinType::SIMPLE: {
                    auto &pin = *reinterpret_cast<SimpleHardwarePin *>(pinPtr.get());
                    auto event = pin.getEventClear();
                    if (event != SimpleHardwarePin::SimpleEventType::NONE) {
                        Serial.printf_P(PSTR("SIMPLE=%u event=%u\n"), pin.getPin(), event);
                        _event(pin.getPin(), event == SimpleHardwarePin::SimpleEventType::HIGH_VALUE ? StateType::IS_HIGH : StateType::IS_LOW, now);
                    }
                }
                break;
#endif
            default:
                break;
        }
    }
#endif

    if (now == _lastRun) { // may run up to 10-15x per millisecond
        return;
    }

#if PIN_MONITOR_DEBOUNCED_PUSHBUTTON

    uint32_t time = micros();
    now = millis();

    // inject "empty" events once per millisecond into the pins with debouncing to update states that are time based
    for(const auto &pinPtr: _pins) {
        auto debounce = pinPtr->getDebounce();
        if (debounce) {
            auto pinNum = pinPtr->getPin();
            _event(pinNum, debounce->debounce(GPI & _BV(pinNum), 0, time, now), now);
        }
    }
#endif

    for(const auto &handler: _handlers) {
        handler->loop();
    }
    _lastRun = millis();
}

void Monitor::_event(uint8_t pinNum, StateType state, uint32_t now)
{
    for(const auto &handler: _handlers) {
        if (handler.get()) {
            StateType tmp;
            if (handler->getPin() == pinNum && handler->isEnabled() && (tmp = handler->_getStateIfEnabled(state)) != StateType::NONE) {
                handler->_eventCounter++;
                handler->event(tmp, now);
            }
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
