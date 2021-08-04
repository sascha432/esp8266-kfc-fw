/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
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

            // value = GPI
            // pin16 = GP16I
            Event(uint32_t time, uint8_t pin, uint16_t value, bool pin16) :
                _time(time),
                _value(value)
                // _value((value & kValueMask) | ((pin & kPinNumMask) << kPinNumBit) | (pin16 << kGPIO16Bit))
            {
                #if DEBUG
                    if (pin >= NUM_DIGITAL_PINS) {
                        __DBG_panic("invalid pin=%u", pin);
                    }
                #endif
                _pinNum = pin;
                _pin16 = pin16;
            }

            // value = GPI
            // GPIO16 is ignored
            Event(uint32_t time, uint8_t pin, uint16_t value) :
                _time(time),
                _value(value)
                // _value((value & kValueMask) | ((pin & kPinNumMask) << kPinNumBit))
            {
                #if DEBUG
                    if (pin >= NUM_DIGITAL_PINS) {
                        __DBG_panic("invalid pin=%u", pin);
                    }
                #endif
                _pinNum = pin;
                _pin16 = 0;
            }

            inline __attribute__((__always_inline__))
            bool operator==(uint8_t pin) const {
                return _pinNum == pin;
            }

            inline __attribute__((__always_inline__))
            bool operator!=(uint8_t pin) const {
                return _pinNum != pin;
            }

            inline __attribute__((__always_inline__))
            uint32_t getTime() const {
                return _time;
            }

            inline __attribute__((__always_inline__))
            bool value() const {
                return (_value & _BV(_pinNum)) != 0;
            }

            inline __attribute__((__always_inline__))
            bool value(uint8_t pin) const {
                return (_value & _BV(pin)) != 0;
            }

            inline __attribute__((__always_inline__))
            uint8_t pin() const {
                return _pinNum;
            }

            // include GPIO16
            uint32_t gpiReg16Value() const {
                return (_value & kValueMask) | (_pin16 << 16);
            }

            inline __attribute__((__always_inline__))
            uint16_t gpiRegValue() const {
                return _value & kValueMask;
            }

            String toString() const {
                return PrintString(F("time=%u pin=%u (%u[0]%u[1]%u[2]%u[3]%u[4]%u[5]%u[12]%u[13]%u[14]%u[15]%u[16])"),
                    getTime(), pin(), value(0), value(1), value(2), value(3), value(4), value(5), value(12), value(13), value(14), value(15), _pin16
                );
            }

            inline __attribute__((__always_inline__))
            void setTime(uint32_t time) {
                _time = time;
            }

            inline __attribute__((__always_inline__))
            void setValue(uint16_t value) {
                _value = value;
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

#if DEBUG_PIN_MONITOR
#include <debug_helper_disable.h>
#endif
