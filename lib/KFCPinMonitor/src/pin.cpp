/**
  Author: sascha_lammers@gmx.de
*/

#include "pin_monitor.h"
#include "pin.h"
#include "rotary_encoder.h"
#include "Schedule.h"

#if DEBUG_PIN_MONITOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using namespace PinMonitor;

// 84-168 byte IRAM
void ICACHE_RAM_ATTR HardwarePin::callback(void *arg)
{
    switch(reinterpret_cast<HardwarePin *>(arg)->_type) {
#if PIN_MONITOR_HAVE_ROTARY_ENCODER
        // +84 byte IRAM
        case HardwarePinType::ROTARY: {
                auto &pin = *reinterpret_cast<RotaryHardwarePin *>(arg);
                auto &ref = pin._encoder._states.get_write_ref();
                ref = digitalRead(pin._encoder._pin1) | (digitalRead(pin._encoder._pin2) << 1);
            }
            break;
#endif
        // 84 byte IRAM
        // case HardwarePinType::SIMPLE:
        // case HardwarePinType::DEBOUNCE:
        default: {
                auto &pin = *reinterpret_cast<SimpleHardwarePin *>(arg);
                pin._micros = micros();
                pin._value = digitalRead(pin._pin);
                pin._intCount++;
            }
            break;
    }
}

#if DEBUG
void Pin::dumpConfig(Print &output)
{
    output.printf_P(PSTR("pin=%u"), _pin);
}
#endif
