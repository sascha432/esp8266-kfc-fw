/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include <PrintHtmlEntitiesString.h>
#include <misc.h>
#include <stl_ext/memory.h>
#include <MicrosTimer.h>
#include "pin_monitor.h"
#include "debounce.h"
#include "pin.h"
#include "monitor.h"
#include <PrintString.h>
#include "rotary_encoder.h"
#include "logger.h"

#if PIN_MONITOR_POLLING_GPIO_EXPANDER_SUPPORT
#include <Wire.h>
#include <IOExpander.h>
#endif

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

namespace PinMonitor {

    Monitor pinMonitor;

    #if PIN_MONITOR_USE_POLLING

        PollingTimer pollingTimer;

        void PollingTimer::start()
        {
            // store initial values for GPIO ports
            _states = _getGPIOStates();
            #if PIN_MONITOR_POLLING_GPIO_EXPANDER_SUPPORT
                // store intial values for GPIO expander ports
                _expanderStates = _getIOExpanderStates();
            #endif
            startTimer(PIN_MONITOR_USE_POLLING_INTERVAL, true);
        }

        #if PIN_MONITOR_POLLING_GPIO_EXPANDER_SUPPORT

            uint16_t PollingTimer::_getIOExpanderStates() const
            {
                // read IOExpanders pins
                uint8_t maskBit = 0;
                uint16_t states = 0;
                for(const auto pin: PinMonitor::Interrupt::kIOExpanderPins) {
                    uint32_t pinMask = _BV(maskBit);
                    if (IOExpander::config.digitalRead(pin)) {
                        states |= pinMask;
                    }
                    maskBit++;
                }
                return states;
            }

        #endif

        void PollingTimer::run()
        {
            // read GPIO
            auto newStates = _getGPIOStates();
            if (newStates != _states) {
                __LDBG_printf("GPOI %s -> %s", decbin(_states).c_str(), decbin(newStates).c_str());
            }
            #if PIN_MONITOR_POLLING_GPIO_EXPANDER_SUPPORT
                auto newExpanderStates = _getIOExpanderStates();
                if (newExpanderStates != _expanderStates) {
                    __LDBG_printf("GPIO(Expander) %s -> %s", decbin(_expanderStates).c_str(), decbin(newExpanderStates).c_str());
                }
            #endif

            for(const auto &pinPtr: pinMonitor.getPins()) {
                #if PIN_MONITOR_ROTARY_ENCODER_SUPPORT
                    if (pinPtr->getHardwarePinType() == HardwarePinType::ROTARY) {
                        continue;
                    }
                #endif
                auto pin = pinPtr->getPin();
                if (pin < NUM_DIGITAL_PINS) {
                    uint32_t pinMask = _BV(pin);
                    if ((_states & pinMask) != (newStates & pinMask)) {
                        __DBG_printf("GPIO pin=%u state=%u", pin, (newStates & pinMask) != 0);
                        InterruptLock lock;
                        pinPtr->callback(pinPtr.get(), newStates, pinMask);
                    }
                }
                #if PIN_MONITOR_POLLING_GPIO_EXPANDER_SUPPORT
                    else {
                        uint8_t maskBit = 0;
                        for(const auto expanderPin: PinMonitor::Interrupt::kIOExpanderPins) {
                            if (expanderPin == pin) {
                                uint16_t pinMask = _BV(maskBit);
                                if ((_expanderStates & pinMask) != (newExpanderStates & pinMask)) {
                                    __LDBG_printf("GPIO(Expander) pin=%u state=%u", pin, (newExpanderStates & pinMask) != 0);
                                    InterruptLock lock;
                                    pinPtr->callback(pinPtr.get(), newExpanderStates, pinMask);
                                }
                                break;
                            }
                            maskBit++;
                        }
                    }
                #endif
            }
            _states = newStates;
            _expanderStates = newExpanderStates;
        }

    #endif

    Monitor::Monitor() :
        _lastRun(0),
        _loopTimer(nullptr),
        __LDBG_IF(_debugTimer(nullptr),)
        _pinModeFlag(INPUT),
        _debounceTime(kDebounceTimeDefault),
        _running(false)
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
        _running = true;
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
        GPIOInterruptsDisable();

        #if PIN_MONITOR_USE_POLLING
            pollingTimer.detach();
            while(OSTimer::isLocked(pollingTimer)) {
                optimistic_yield(100);
            }
        #endif

        if (_loopTimer) {
            delete _loopTimer;
            _loopTimer = nullptr;
        }
        _running = false;

        _detach(_handlers.begin(), _handlers.end(), true);

        #if PIN_MONITOR_ROTARY_ENCODER_SUPPORT
            eventBuffer.clear();
        #endif
    }

    void Monitor::printStatus(Print &output)
    {
        #if PIN_MONITOR_USE_POLLING
            output.printf_P(PSTR("GPIO Polling (%ums) without Interrupts" HTML_S(br)), PIN_MONITOR_USE_POLLING_INTERVAL);
            #if PIN_MONITOR_POLLING_GPIO_EXPANDER_SUPPORT
                output.print(F("I/O-Expander Support: Enabled" HTML_S(br)));
            #endif
        #elif PIN_MONITOR_USE_FUNCTIONAL_INTERRUPTS == 0
            output.print(F("Interrupt Handler: Optimized Functional Interrupts" HTML_S(br)));
        #elif PIN_MONITOR_USE_GPIO_INTERRUPT
            output.print(F("Interrupt Handler: Custom GPIO Interrupt Handler" HTML_S(br)));
        #else
            output.print(F("Interrupt Handler: Default Arduino Interrupts" HTML_S(br)));
        #endif
        #if PIN_MONITOR_ROTARY_ENCODER_SUPPORT
            #if PIN_MONITOR_USE_POLLING
                output.print(F("Rotary Encoder Support: Custom GPIO Interrupt Handler" HTML_S(br)));
            #else
                output.print(F("Rotary Encoder Support: Enabled" HTML_S(br)));
            #endif
        #else
            output.print(F("Rotary Encoder Support: Disabled" HTML_S(br)));
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
        if (_running) {
            __DBG_panic("PinMonitor::Monitor::_attach() called while running");
        }
        __LDBG_IF(auto typeStr = PSTR("new pin:"));
        auto pinNum = pin.getPin();
        bool pinsEmpty = _pins.empty();
        auto iterator = std::find_if(_pins.begin(), _pins.end(), [pinNum](const HardwarePinPtr &pin) {
            return pin->getPin() == pinNum;
        });
        if (iterator == _pins.end()) {
            pinMode(pinNum, getPinMode());
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
            iterator = std::prev(_pins.end());

            __LDBG_IF(typeStr = PSTR("updating pin:"));
        }
        auto &curPin = *iterator->get();

        __LDBG_printf("%s attaching pin=%u usage=%u arg=%p", typeStr, curPin.getPin(), curPin.getCount() + 1, pin.getArg());
        ++curPin;
        #if PIN_MONITOR_USE_GPIO_INTERRUPT == 0 && PIN_MONITOR_USE_POLLING == 0
            // code for functional interrupts only
            if (curPin) {
                #if PIN_MONITOR_USE_FUNCTIONAL_INTERRUPTS
                    // +72 byte IRAM
                    attachInterruptArg(digitalPinToInterrupt(pinNum), HardwarePin::callback, &curPin, CHANGE);
                #else
                    // 0 byte IRAM
                    _attachInterruptArg(digitalPinToInterrupt(pinNum), HardwarePin::callback, &curPin);
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
        if (_running) {
            __DBG_panic("PinMonitor::Monitor::_detach() called while running");
        }
        detach(std::remove_if(_handlers.begin(), _handlers.end(), pred), _handlers.end());
    }

    void Monitor::detach(const void *arg) {
        detach([arg](const PinPtr &pin) {
            return pin->getArg() == arg;
        });
    }

    void Monitor::_detach(Iterator begin, Iterator end, bool clear)
    {
        if (_running) {
            __DBG_panic("PinMonitor::Monitor::_detach() called while running");
        }
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
                    #if PIN_MONITOR_USE_POLLING
                        // no interrupts
                    #elif PIN_MONITOR_USE_GPIO_INTERRUPT == 0
                        #if PIN_MONITOR_USE_FUNCTIONAL_INTERRUPTS
                            // +160 byte IRAM
                            detachInterrupt(digitalPinToInterrupt(pinNum));
                        #else
                            // 0 byte IRAM
                            _detachInterrupt(digitalPinToInterrupt(pinNum));
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
        if (_running) {
            __DBG_panic("PinMonitor::Monitor::_detach() called while running");
        }
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
        #if PIN_MONITOR_USE_POLLING
            pollingTimer.start();
        #endif
        GPIOInterruptsEnable();
    }

    void Monitor::_detachLoop()
    {
        __LDBG_printf("detach=loop timer=%d", _loopTimer ? (bool)*_loopTimer : -1);
        GPIOInterruptsDisable();
        #if PIN_MONITOR_USE_POLLING
            pollingTimer.detach();
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
        if (now == _lastRun) { // limit to once per millisecond
            return;
        }
        _lastRun = now;

        #define MEASURE_PROCESSING_TIME 0

        #if PIN_MONITOR_ROTARY_ENCODER_SUPPORT
            {
                #if MEASURE_PROCESSING_TIME
                    uint32_t eventNum = 0;
                    uint32_t start = micros();
                #endif

                // process all new events
                // the GPIO interrupts must be disabled while reading from eventBuffer or writing to it
                // new events might be added while processing
                GPIOInterruptLock lock;
                if (eventBuffer.size()) {
                    auto bufIterator = eventBuffer.begin();
                    while (bufIterator != eventBuffer.end()) {
                        auto event = *bufIterator;
                        // destroy object to unlock GPIO interrupts
                        lock.~GPIOInterruptLock();
                        #if MEASURE_PROCESSING_TIME
                            eventNum++;
                        #endif

                        auto pinIterator = std::find_if(_handlers.begin(), _handlers.end(), [&event](const PinPtr &ptr) {
                            return ptr->getPin() == event.pin();
                        });
                        __LDBG_printf("GPIO pin=%u rotary_encoder=%u", event.pin(), pinIterator != _handlers.end());
                        if (pinIterator != _handlers.end()) {
                            auto encoder = reinterpret_cast<RotaryEncoderPin *>(pinIterator->get())->getEncoder();
                            encoder->processEvent(event);
                        }

                        // create in place to lock interrupts again
                        // the destructor has been called already
                        new(static_cast<void *>(&lock)) GPIOInterruptLock();
                        ++bufIterator;
                    }
                    if (bufIterator == eventBuffer.end()) {
                        eventBuffer.clear();
                    }
                    #if MEASURE_PROCESSING_TIME
                        uint32_t dur = micros() - start;
                        __DBG_printf("processing rotarty events size=%u time=%u", eventNum, dur);
                    #endif
                    // update time
                    now = millis();
                }
            }
        #endif

        #if PIN_MONITOR_DEBOUNCED_PUSHBUTTON || PIN_MONITOR_SIMPLE_PIN
            #if MEASURE_PROCESSING_TIME
                uint32_t start = micros();
                uint32_t eventNum = 0;
            #endif
            for(const auto &pinPtr: _pins) {
                switch(pinPtr->_type) {
                    #if PIN_MONITOR_DEBOUNCED_PUSHBUTTON
                        case HardwarePinType::DEBOUNCE: {
                                auto &pin = *reinterpret_cast<DebouncedHardwarePin *>(pinPtr.get());
                                DebouncedHardwarePin::Events event;
                                {
                                    InterruptLock lock;
                                    event = pin.getEventsClear();
                                }
                                if (event._interruptCount) {
                                    #if MEASURE_PROCESSING_TIME
                                        eventNum++;
                                    #endif
                                    _event(pin.getPin(), pin.getDebounce()->debounce(event._value, event._interruptCount, event._micros, now), now);
                                }
                            }
                            break;
                    #endif
                    #if PIN_MONITOR_SIMPLE_PIN
                        case HardwarePinType::SIMPLE: {
                                #if MEASURE_PROCESSING_TIME
                                    eventNum++;
                                #endif
                                auto &pin = *reinterpret_cast<SimpleHardwarePin *>(pinPtr.get());
                                SimpleHardwarePin::Events event;
                                {
                                    InterruptLock lock;
                                    event = pin.getEventsClear();
                                }
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
            #if MEASURE_PROCESSING_TIME
                if (eventNum) {
                    uint32_t dur = micros() - start;
                    __DBG_printf("processing pins=%u time=%u events=%u", _pins.size(), dur, eventNum);
                }
            #endif
        #endif

        #if PIN_MONITOR_DEBOUNCED_PUSHBUTTON

            uint32_t time = micros();
            now = millis();

            // inject "empty" events once per millisecond into the pins with debouncing to update states that are time based
            for(const auto &pinPtr: _pins) {
                auto debounce = pinPtr->getDebounce();
                if (debounce) {
                    auto pinNum = pinPtr->getPin();
                    _event(pinNum, debounce->debounce(GPIO::read() & _BV(pinNum), 0, time, now), now);
                }
            }
        #endif

        for(const auto &handler: _handlers) {
            handler->loop();
        }
        _lastRun = millis(); // update time again
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

}
