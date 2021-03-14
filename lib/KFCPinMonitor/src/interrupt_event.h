/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include "pin_monitor.h"
#include <stl_ext/utility.h>
#include <stl_ext/fixed_circular_buffer.h>

#if DEBUG_PIN_MONITOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

namespace PinMonitor {

    namespace Interrupt {

        struct PinAndMask {
            using mask_type = uint16_t;

            constexpr PinAndMask(const int index) : pin(index), mask(_BV(index)) {
            }
            constexpr operator int() const {
                return pin;
            }
            const uint8_t pin;
            const mask_type mask;

            template<typename _Ta>
            static constexpr uint32_t mask_of(const _Ta &arr, const size_t index = 0, const uint32_t mask = 0) {
                return index < arr.size() ? (_BV(arr[index]) | mask_of(arr, index + 1, mask)) : mask;
            }
        };

        static constexpr size_t kEventBufferSize = PIN_MONITOR_EVENT_QUEUE_SIZE;

        struct __attribute__((packed)) Event {

            using mask_type = PinAndMask::mask_type;

            Event() : _time(0), _value(0), _pin(0) {}
            Event(uint32_t time, uint8_t pin, mask_type value) : _time(time), _value(value), _pin(pin) {}

            bool operator==(uint8_t pin) const {
                return _pin == pin;
            }
            bool operator!=(uint8_t pin) const {
                return _pin != pin;
            }

            uint32_t getTime() const {
                return _time;
            }

            bool value() const {
                return (_value & _BV(_pin)) != 0;
            }

            uint8_t pin() const {
                return _pin;
            }

            uint16_t gpiRegValue() const {
                return _value;
            }

        protected:
            uint32_t _time;
            mask_type _value;
            uint8_t _pin;
        };

        using EventBuffer = stdex::fixed_circular_buffer<Event, kEventBufferSize>;

        static constexpr size_t kEventBufferObjectSize = sizeof(EventBuffer);
        static constexpr float kEventBufferElementSize = sizeof(EventBuffer) / static_cast<float>(kEventBufferSize);

    }

}

#include <debug_helper_disable.h>
