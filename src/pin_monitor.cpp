/**
  Author: sascha_lammers@gmx.de
*/

#if PIN_MONITOR

#include "pin_monitor.h"
#include <EventScheduler.h>
#include "plugins.h"

#if DEBUG_PIN_MONITOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

PinMonitor *PinMonitor::_instance = nullptr;

PinMonitor::PinMonitor() {
}

PinMonitor::~PinMonitor()
{
    for(auto &pin: _pins) {
        detachInterrupt(digitalPinToInterrupt(pin.pin));
    }
}

PinMonitor *PinMonitor::createInstance()
{
    if (!_instance) {
        _instance = new PinMonitor();
    }
    return _instance;
}

void PinMonitor::deleteInstance()
{
    if (_instance) {
        // redundant, destructor detaches all interrupts
        // for(auto pin: _instance->_pins) {
        //     _instance->removePin(pin.pin, nullptr);
        // }
        delete _instance;
        _instance = nullptr;
    }
}

void PinMonitor::callback(InterruptInfo info)
{
    _debug_printf_P(PSTR("PinMonitor::callback(): pin=%u,value=%u,micros=%u\n"), info.pin, info.value, info.micro);
    auto monitor = PinMonitor::getInstance();
    if (monitor) {
        for(auto &pin: monitor->_pins) {
            if (pin.pin == info.pin) {
                pin.value = info.value;
                pin.micro = info.micro;
                pin.callback(&pin);
            }
        }
    }
}

PinMonitor::Pin_t *PinMonitor::addPin(uint8_t pin, Callback_t callback, void *arg, uint8_t mode)
{
    if (_findPin(pin, arg) == _pins.end()) {
        _debug_printf_P(PSTR("PinMonitor::addPin(): adding pin %u, mode %u, callback %p, arg %p\n"), pin, mode, callback, arg);
        Pin_t _pin;
        memset(&_pin, 0, sizeof(_pin));
        _pin.pin = pin;
        _pin.value = digitalRead(pin);
        _pin.callback = callback;
        _pin.arg = arg;
        _pins.push_back(_pin);
        attachScheduledInterrupt(digitalPinToInterrupt(pin), PinMonitor::callback, CHANGE);
        pinMode(pin, mode);
        return &_pins.back();
    }
    return nullptr;

}

bool PinMonitor::removePin(uint8_t pin, void *arg)
{
    auto _pin = _findPin(pin, arg);
    if (_pin == _pins.end()) {
        return false;
    }
    _debug_printf_P(PSTR("PinMonitor::removePin(): removing pin %u, callback %p, arg %p\n"), pin, _pin->callback, _pin->arg);
    detachInterrupt(digitalPinToInterrupt(pin));
    _pins.erase(_pin);
    return true;
}

void PinMonitor::dumpPins(Stream &output)
{
    output.printf_P(PSTR("+PINM count=%u\n"), _pins.size());
    if (_pins.size()) {
        for(auto &pin: _pins) {
            output.printf_P(PSTR("+PINM pin=%u value=%d micro=%u callback=%p arg=%p\n"), pin.pin, pin.value, pin.micro, pin.callback, pin.arg);
        }
    }
}

PinMonitor::PinsVectorIterator PinMonitor::_findPin(uint8_t pin, void *arg)
{
    return std::find_if(_pins.begin(), _pins.end(), [pin, arg](const Pin_t &_pin) {
        return (_pin.pin == pin) && (!arg || _pin.arg == arg);
    });
}

#endif
