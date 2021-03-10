/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include "pin_monitor.h"
#include "interrupt_event.h"
#include <stl_ext/array.h>
#include <stl_ext/type_traits.h>

#if DEBUG_PIN_MONITOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#include <cstddef>
#include <cassert>

namespace STL_STD_EXT_NAMESPACE_EX {

    // std::find for constexpr arrays

    #if !defined(__clang__) && __GNUC__ < 5
    // TODO: constexpr asserts does not work in gcc4, but we may use
    // "thow" workaround from
    // http://ericniebler.com/2014/09/27/assert-and-constexpr-in-cxx11/
    # define ce_assert(x) ((void)0)
    #else
    # define ce_assert(x) assert(x)
    #endif
    namespace cexpr {

        template <class It, class T>
        inline constexpr It
            find(It begin, It end, T const &value) noexcept
        {
            return !(begin != end && *begin != value)
                ? begin
                : find(begin + 1, end, value);
        }

        template<class Array>
        class array_iterator
        {
        public:
            using value_type = typename Array::value_type;

            constexpr array_iterator(const Array &array, size_t size = 0u) noexcept
                : array_(&array)
                , pos_(size)
            {}

            constexpr const value_type operator* () const noexcept
            {
                return ce_assert(pos_ < Array::size), (*array_)[pos_];
            }

    #if __cplusplus >= 201402L // C++14
            constexpr
    #endif
                array_iterator &operator ++() noexcept
            {
                return ce_assert(pos_ < Array::size), ++pos_, *this;
            }

            constexpr array_iterator operator+ (size_t n) const noexcept
            {
                return ce_assert(pos_ + n <= Array::size), array_iterator(*array_, pos_ + n);
            }

            friend constexpr bool
                operator != (const array_iterator &i1, const array_iterator &i2) noexcept
            {
                return i1.array_ != i2.array_ || i1.pos_ != i2.pos_;
            }

            friend constexpr size_t
                operator- (array_iterator const &i1, array_iterator const &i2) noexcept
            {
                return ce_assert(i1.array_ == i2.array_), i1.pos_ - i2.pos_;
            }

        private:
            const Array *array_;
            size_t pos_;
        };

        template<typename T, size_t N>
        class array
        {
        public:
            using value_type = T;
            using const_iterator = const array_iterator<array>;

            constexpr value_type const &
                operator[] (size_t idx) const noexcept
            {
                return array_[idx];
            }

            constexpr const_iterator begin() const noexcept
            {
                return const_iterator(*this);
            }

            constexpr const_iterator end()   const noexcept
            {
                return const_iterator(*this, N);
            }

            T array_[N];
            static constexpr size_t size = N;
        };

    }


    // array sequence maker

    template<typename _Type>
    struct ArrayFormatter {
        using type = _Type;
        static constexpr _Type format(_Type index) {
            return index;
        };
    };

    using IntArrayFormatter = ArrayFormatter<int>;


    template<typename _Formatter, int N, int... Rest>
    struct Array_impl {
        static constexpr auto &value = Array_impl<_Formatter, N - 1, _Formatter::format(N), Rest...>::value;
    };

    template<typename _Formatter, int... Rest>
    struct Array_impl<_Formatter, 0, Rest...> {
        static constexpr typename _Formatter::type value[] = { 0, Rest... };
    };

    template<typename _Formatter, int... Rest>
    constexpr typename _Formatter::type Array_impl<_Formatter, 0, Rest...>::value[];


    template<int _Size, typename _Formatter = IntArrayFormatter>
    struct Array {
        static_assert(_Size >= 0, "N must be at least 0");

        static constexpr auto &value = Array_impl<_Formatter, _Size>::value;

        Array() = delete;
        Array(const Array &) = delete;
        Array(Array &&) = delete;
    };

    static constexpr auto array1 = Array<16>::value;


}


namespace PinMonitor {

    void GPIOInterruptsEnable();
    void GPIOInterruptsDisable();

    class RotaryEncoder;

    namespace Interrupt {

        static constexpr uint8_t kMaxPinNum = 17;
        static constexpr bool kAllowUART = true;
        static constexpr bool kUsekPinsOnly = true;

        static constexpr auto kPins2 = stdex::cexpr::array<const uint8_t, 4>({14, 4, 12, 13});
        static constexpr auto kPinTypes2 = std::array<const HardwarePinType, 4>({HardwarePinType::DEBOUNCE, HardwarePinType::DEBOUNCE, HardwarePinType::DEBOUNCE, HardwarePinType::DEBOUNCE});

        // template<typename Ta, typename  Tb>
        // constexpr int findInArray(const Ta el,  const Tb &arr, const int index = 0, const int size = 0) {
        //     return index >= size ? -1 : el == arr[index] ? index : findInArray(el, arr, index + 1, size);
        // }
        // static constexpr auto testk = findInArray(44, kPins, 0, kPins.size());

        static constexpr auto testkkk = find(kPins2.begin(), kPins2.end(), 111) != kPins2.end();

        constexpr bool isPinUsable(const int pin) {
            return kUsekPinsOnly ? (find(kPins2.begin(), kPins2.end(), pin) != kPins2.end()) : !( (pin >= 6 && pin <= 11) || pin == 16 || ((pin == 1 || pin == 3) && kAllowUART == false) );
        }

        // constexpr bool isPinUsableX(const uint8_t pin) {
        //     return kUsekPinsOnly ?
        //         (find(kPins.begin(), kPins.end(), pin) != kPins.end()) :
        //         (((pin >= 6 && pin <= 11) || pin == 16) ? false : true);
        // }
        // constexpr bool isPinUsableX(const uint8_t pin) {
        //         (find(kPins.begin(), kPins.end(), pin) != kPins.end()) ;
        // }


        // constexpr int getUsablePinCount(const int pos, const int size, const int count = 0) {
        //     return (pos >= size) ?
        //         count : ((isPinUsable(pos) == false) ?
        //             getUsablePinCount(pos + 1, size, count) :
        //             getUsablePinCount(pos + 1, size, count + 1));
        // }

        // // constexpr size_t I(size_t pos) { return getUsablePinRecursive(pos); }
        // constexpr size_t K(size_t pos) { return getUsablePinRecursive(pos, -1, 0, -1, false); }
        // constexpr size_t J(size_t pos) { return isPinUsable(K(pos)) ? K(pos) : -1; }

        // static constexpr auto kUsablePins = getUsablePinCount(0, 17);
        // static constexpr auto kIndexToPin = std::array<int, 17>({ I(0), I(1), I(2), I(3), I(4), I(5), I(6), I(7), I(8), I(9), I(10), I(11), I(12), I(13), I(14), I(15), I(16) });
        // static constexpr auto kPinToIndex = std::array<int, 17>({ J(0), J(1), J(2), J(3), J(4), J(5), J(6), J(7), J(8), J(9), J(10), J(11), J(12), J(13), J(14), J(15), J(16) });



        template<typename N, bool _EnableUART>
        struct GPIOFormatter {
            using type = uint8_t;
            static constexpr N format(N pin) {
                return ((pin >= 6 && pin <= 11) || pin == 16 || (_EnableUART && (pin == 1 || pin == 3))) ? -1 : pin;
            };
        };

        static constexpr auto kPinToIndex = stdex::Array<16, GPIOFormatter<uint8_t, false>>::value;



        using Callback = void(*)(void *);

        enum class LookupType : uint8_t {
            NONE,
            PIN,
            HARDWARE_PIN,
            ROTARY_ENCODER,
            CALLBACK
        };

        template<LookupType _Type>
        struct EnumToPointer {
                // static constexpr LookupType value = _Type;
                using type = typename std::conditional<_Type == LookupType::PIN, Pin *, typename std::conditional<_Type == LookupType::HARDWARE_PIN, HardwarePin *, typename std::conditional<_Type == LookupType::ROTARY_ENCODER, RotaryEncoder *, typename std::conditional<_Type == LookupType::CALLBACK, Callback, nullptr_t>::type>::type>::type>::type;
                static constexpr auto value = _Type;
        };

        // struct ToPointer {
        //     struct {
        //         using HARDWARE_PIN = HardwarePin *;
        //         using ROTARY_ENCODER = RotaryEncoder *;
        //         using CALLBACK = Callback;

        //     }
        // };

        union PointerUnionType {
            operator bool() const {
                return _ptr != nullptr;
            }

            bool operator==(const PointerUnionType &ptr) const {
                return (_ptr == ptr._ptr);
            }

            bool operator!=(const PointerUnionType &ptr) const {
                return (_ptr != ptr._ptr);
            }

            operator Pin *() const {
                return _pin;
            }

            operator HardwarePin *() const {
                return _hwPin;
            }

            operator RotaryEncoder *() const {
                return _rotary;
            }

            operator Callback() const {
                return _callback;
            }

            PointerUnionType &operator=(const nullptr_t) {
                _ptr = nullptr;
                return *this;
            }

            void *_ptr;
            Pin *_pin;
            HardwarePin *_hwPin;
            RotaryEncoder *_rotary;
            Callback _callback;
        };

        class LookupResult {
        public:
            LookupResult() : _type(LookupType::NONE), _ptr({}), _pin() {}
            LookupResult(LookupType type, const PointerUnionType &ptr, uint8_t pin) : _type(type), _ptr(ptr), _pin(pin) {}

            operator bool() const {
                return (_type != LookupType::NONE) && static_cast<bool>(_ptr);
            }

            LookupType getType() const {
                return _type;
            }

            //
            // returns nullptr if the passed LookupType does not match getType()
            //
            //  auto object = result.get<LookupType::ROTARY_ENCODER>();
            //  if (object) {
            //      object->do_something();
            //  }
            //
            // template<LookupType _Type, typename _Ta = typename to_pointer_type<_Type>::type>
            // _Ta get() const {
            //     return static_cast<_Ta>(_ptr);
            // }

            //
            //  auto object = static_cast<to_pointer_type<LookupType::CALLBACK>::type>(result)(ptr);
            //  object->do_something();
            //
            //
            // template<template<LookupType> class _Ta, LookupType _Type>
            // operator _Ta<_Type>() const {
            //     // if (_Type != getType()) {
            //     //     return nullptr;
            //     // }
            //     return static_cast<_Ta<_Type>>(_ptr);
            // }

            template<typename _Ta>
            operator _Ta() const {
                // if (_Type != getType()) {
                //     return nullptr;
                // }
                return static_cast<_Ta>(_ptr);
            }

        private:
            LookupType _type;
            PointerUnionType _ptr;
            uint8_t _pin;
        };

        class LookupTable {
        public:
            LookupTable() : _ptrs{} {}

            void clear() {
                memset(&_types[0], 0, sizeof(_types));
                memset(&_ptrs[0], 0, sizeof(_ptrs));
            }

            const LookupResult operator[](int index) const {
                return get(static_cast<uint8_t>(index));
            }

            const LookupResult get(uint8_t pin) const {
                if (pin < kMaxPinNum) {
                    return LookupResult(_types[pin], _ptrs[pin], pin);
                }
                return LookupResult();
            }

            LookupType getType(uint8_t pin) const {
                if (pin < kMaxPinNum) {
                    return _types[pin];
                }
                return LookupType::NONE;
            }

            PointerUnionType getPointer(uint8_t pin) const {
                if (pin < kMaxPinNum) {
                    return _ptrs[pin];
                }
                return PointerUnionType({});
            }

            // void set(uint8_t pin, LookupType type, PointerUnionType ptrs) {
            //     _types[pin] = type;
            //     _ptrs[pin] = ptrs;
            // }

            // void remove(uint8_t pin, LookupType type, PointerUnionType ptrs) {
            //     if (pin < kMaxPinNum) {
            //         if (_types[pin] == type && _ptrs[pin] == ptrs) {
            //             _types[pin] = LookupType::NONE;
            //             _ptrs[pin] = nullptr;
            //         }
            //     }
            // }

        private:
            LookupType _types[kMaxPinNum];
            PointerUnionType _ptrs[kMaxPinNum];
        };


//           for (int i = 0; i <= 16; ++i) {
//     if (!isFlashInterfacePin(i))
//         pinMode(i, INPUT);
//   }

        static constexpr auto kLookupTableSize = sizeof(LookupTable);

        static constexpr auto kPins = std::array<const uint8_t, 4>({14, 4, 12, 13});
        static constexpr auto kPinTypes = std::array<const HardwarePinType, 4>({HardwarePinType::DEBOUNCE, HardwarePinType::DEBOUNCE, HardwarePinType::DEBOUNCE, HardwarePinType::DEBOUNCE});
        constexpr uint32_t getInterruptPinIndex(const uint8_t pin, size_t index = 0)
        {
            return pin == kPins[index] ? index : getInterruptPinIndex(pin, index + 1);
        }

        constexpr uint32_t generatePinMask(uint32_t mask = 0, size_t index = 0)
        {
            return index < kPins.size() ? generatePinMask(mask | _BV(kPins[index]), index + 1) : mask;
        }

        static constexpr uint32_t kPinMask = generatePinMask();

    }

    extern Interrupt::EventBuffer eventBuffer;
    extern Interrupt::LookupTable lookupTable;
    extern uint16_t interrupt_levels;

}

extern void ICACHE_RAM_ATTR pin_monitor_interrupt_handler(void *ptr);

#include <debug_helper_disable.h>
