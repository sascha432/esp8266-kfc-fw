/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include "pin_monitor.h"
#include <PrintString.h>
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

        static constexpr size_t kEventQueueSize = PIN_MONITOR_EVENT_QUEUE_SIZE;
        static constexpr uint16_t kValueMask = 0b1111000000111111;
        static constexpr uint8_t kPinNumMask = 0b11111;
        static constexpr uint8_t kPinNumBit = 6;
        static constexpr uint8_t kGPIO16Bit = 11;

        struct __attribute__((packed)) Event {

            Event() : _time(0), _value(0)
            {}

            Event(uint32_t time, uint8_t pin, uint16_t value = GPI, bool pin16 = GP16I) :
                _time(time),
                _value((value & kValueMask) | ((pin & kPinNumMask) << kPinNumBit) | (pin16 << kGPIO16Bit))
            {}

            bool operator==(uint8_t pin) const {
                return _pinNum == pin;
            }
            bool operator!=(uint8_t pin) const {
                return _pinNum != pin;
            }

            uint32_t getTime() const {
                return _time;
            }

            bool value() const {
                return (_value & _BV(_pinNum)) != 0;
            }

            uint8_t pin() const {
                return _pinNum;
            }

            uint32_t gpiRegValue() const {
                return (_value & kValueMask) | (_pin16 << 16);
            }

            String toString() const {
                return PrintString(F("time=%u pin=%u (%u[0]%u[1]%u[2]%u[3]%u[4]%u[5]%u[12]%u[13]%u[14]%u[15]%u[16])"),
                    _time, _pinNum, _pin0, _pin1, _pin2, _pin3, _pin4, _pin5, _pin12, _pin13, _pin14, _pin15, _pin16
                );
            }

        protected:
            uint32_t _time;
            union {
                uint16_t _value;
                struct {
                    uint16_t _pin0: 1;          // GPIO0
                    uint16_t _pin1: 1;
                    uint16_t _pin2: 1;
                    uint16_t _pin3: 1;
                    uint16_t _pin4: 1;
                    uint16_t _pin5: 1;          // GPIO5
                    uint16_t _pinNum: 5;        // used to store the pin (6 - 11 = are used for the flash interface)
                    uint16_t _pin16: 1;         // GPIO16 (!)
                    uint16_t _pin12: 1;         // GPIO12
                    uint16_t _pin13: 1;
                    uint16_t _pin14: 1;
                    uint16_t _pin15: 1;         // GPIO15
                };
            };
        };

        using EventBuffer = stdex::fixed_circular_buffer<Event, kEventQueueSize>;

        static constexpr auto kEventSize = sizeof(Event);
        static constexpr auto kEventBufferSize = sizeof(EventBuffer);
        static constexpr float kEventBufferElementSize = sizeof(EventBuffer) / static_cast<float>(kEventQueueSize);

    }

}

#include <debug_helper_disable.h>
