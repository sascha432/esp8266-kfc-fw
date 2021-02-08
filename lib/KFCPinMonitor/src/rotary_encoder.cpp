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
    __DBG_printf("EVENT state=%s time=%u", Monitor::stateType2String(state), now);
}

void RotaryEncoderPin::event(EventType eventType, uint32_t now)
{
    __LDBG_printf("PURE VIRTUAL: event_type=%u now=%u", eventType, now);
}

void RotaryEncoderPin::loop()
{

}
