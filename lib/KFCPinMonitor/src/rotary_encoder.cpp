/**
 * Author: sascha_lammers@gmx.de
 */

#include <MicrosTimer.h>
#include "rotary_encoder.h"
#include "monitor.h"

#if DEBUG_PIN_MONITOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using namespace PinMonitor;


RotaryEncoderPin::~RotaryEncoderPin() {
    if (_direction == 1) {
        delete reinterpret_cast<RotaryEncoder *>(const_cast<void *>(getArg()));
    }
}

void RotaryEncoderPin::event(StateType state, uint32_t now)
{
    __DBG_printf("EVENT direction=%u state=%s time=%u", _direction, Monitor::stateType2String(state), now);
}

void RotaryEncoderPin::event(EventType eventType, uint32_t now)
{
    __LDBG_printf("PURE VIRTUAL: event_type=%u now=%u", eventType, now);
}

void RotaryEncoderPin::loop()
{
    if (_direction == 1) {
        reinterpret_cast<RotaryEncoder *>(const_cast<void *>(getArg()))->loop();
    }
}

void RotaryEncoder::attachPins(uint8_t pin1, ActiveStateType state1, uint8_t pin2, ActiveStateType state2)
{
    pinMonitor.attachPinType<RotaryEncoderPin>(HardwarePinType::SIMPLE, pin1, 0, this, state1);
    pinMonitor.attachPinType<RotaryEncoderPin>(HardwarePinType::SIMPLE, pin2, 1, this, state2);
    for(const auto &pin: pinMonitor.getPins()) {
        auto pinNum = pin->getPin();
        if (pinNum == pin1) {
            _pin1 = pin.get();
        }
        else if (pinNum == pin2) {
            _pin2 = pin.get();
        }
    }
}

void RotaryEncoder::loop()
{
    // _lastState = _pin1.
}
