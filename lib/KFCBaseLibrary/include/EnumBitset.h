/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include "EnumBase.h"

// provides
// object & ENUMVALS for testing if any bit has been set (value & bitmask != 0)
// object % ENUMVALS for testing if all bits have been set (value & bitmask == bitmask)
// object % (ENUMVAL1|ENUMVAL2) for testing if ENUMVAL1 and ENUMVAL1 are set
// object % (ENUMVALS&~ENUMVAL2) for testing all bits of ENUMVALS but ENUMVAL2
// object -= ENUMVALS for setting a bit
// object += ENUMVALS for clearing a bit
// object = ENUMVALS for setting a single bit
// object = ENUMVALS1|ENUMVALS2 for setting multple bits
// object = NONE / object.clear() clear all bits
// object = ANY set all bits (even undefined ones)
// object.toString() to convert to string (binary for uint8 or decimal for other types if debug mode is disabled)
// object.dump(Print &) display debug info
// ClassName::BIT::ENUMVALS
// ClassName::ENUMVALS

#if ENUM_BASE_STORE_ENUM_AS_STRING

// typedef
#define __typedef_enum_bitset(name, type, ...) \
    enum class name##EnumClassDefinition { \
        __VA_ARGS__ \
    }; \
    name##EnumClassDefinition operator|(name##EnumClassDefinition value, name##EnumClassDefinition value2); \
    \
    class name : public EnumBitset<name##EnumClassDefinition, type> { \
    public: \
        using EnumBitset<name##EnumClassDefinition, type>::EnumBitset; \
        typedef enum { \
            __VA_ARGS__ \
        } BIT; \
        typedef type TYPE; \
        name() : EnumBitset(NONE) { \
        } \
        name(BIT value) : EnumBitset<name##EnumClassDefinition, TYPE>(static_cast<name##EnumClassDefinition>(value)) {\
        } \
        inline operator BIT() { \
            return static_cast<BIT>(_value); \
        } \
        inline EnumBitset& operator=(BIT value) { \
            _value = static_cast<type>(value); \
            return *this; \
        } \
        inline bool operator==(BIT value) const { \
            return _value == static_cast<type>(value); \
        } \
        inline EnumBitset& operator+=(BIT value) { \
            _value |= static_cast<type>(value); \
            return *this; \
        } \
        inline EnumBitset& operator+=(const EnumBitset &value) { \
            _value |= value._value; \
            return *this; \
        } \
        inline EnumBitset& operator-=(BIT value) { \
            _value &= ~static_cast<type>(value); \
            return *this; \
        } \
        inline EnumBitset& operator-=(const EnumBitset &value) { \
            _value &= ~value._value; \
            return *this; \
        } \
        inline bool operator&(BIT value) { \
            return (_value & static_cast<type>(value)) != 0; \
        } \
        inline bool operator%(BIT value) { \
            return (_value & static_cast<type>(value)) == static_cast<type>(value); \
        } \
    }; \
    constexpr const char *name##DebugEnumValues = _STRINGIFY(__VA_ARGS__);

    //constexpr auto name##DebugEnumValuesLength = EnumBaseConstExpr::names_len(_STRINGIFY(__VA_ARGS__));
    //const ArrayConstExpr::ArrayHelper<EnumBaseConstExpr::ArrayNameAccess<name##DebugEnumValuesLength>, char, name##DebugEnumValuesLength> name##DebugEnumValues PROGMEM(EnumBaseConstExpr::ArrayNameAccess<name##DebugEnumValuesLength>(_STRINGIFY(__VA_ARGS__))._elements);

#define __define_enum_bitset(name) \
    template<> \
    const char* EnumBase<name##EnumClassDefinition, name::TYPE>::valuesPtr = name##DebugEnumValues; \
    \
    inline name##EnumClassDefinition operator| (name##EnumClassDefinition value, name##EnumClassDefinition value2) { \
        return (name##EnumClassDefinition)(static_cast<name::TYPE>(value) | static_cast<name::TYPE>(value2)); \
    } \
    inline name::BIT operator| (name::BIT value, name::BIT value2) { \
        return static_cast<name::BIT>(static_cast<name::TYPE>(value) | static_cast<name::TYPE>(value2)); \
    }

//const char* EnumBase<name##EnumClassDefinition, name::TYPE>::valuesPtr = name##DebugEnumValues.array;
//    const uint16_t EnumBase<name##EnumClassDefinition, name::TYPE>::valuesCount = EnumBaseConstExpr::count(name::values);


#else

#define __typedef_enum_bitset(name, type, ...) \
    enum class name##EnumClassDefinition { \
        __VA_ARGS__ \
    }; \
    name##EnumClassDefinition operator|(name##EnumClassDefinition value, name##EnumClassDefinition value2); \
    \
    class name : public EnumBitset<name##EnumClassDefinition, type> { \
    public: \
        using EnumBitset<name##EnumClassDefinition, type>::EnumBitset; \
        typedef enum { \
            __VA_ARGS__ \
        } BIT; \
        typedef type TYPE; \
        name() : EnumBitset(NONE) { \
        } \
        name(BIT value) : EnumBitset<name##EnumClassDefinition, TYPE>(static_cast<name##EnumClassDefinition>(value)) {\
        } \
        EnumBitset& operator=(BIT value) { \
            _value = static_cast<type>(value); \
            return *this; \
        } \
        inline bool operator==(BIT value) const { \
            return _value == static_cast<type>(value); \
        } \
        inline EnumBitset& operator+=(BIT value) { \
            _value |= static_cast<type>(value); \
            return *this; \
        } \
        inline EnumBitset& operator+=(const EnumBitset &value) { \
            _value |= value._value; \
            return *this; \
        } \
        inline EnumBitset& operator-=(BIT value) { \
            _value &= ~static_cast<type>(value); \
            return *this; \
        } \
        inline EnumBitset& operator-=(const EnumBitset &value) { \
            _value &= ~value._value; \
            return *this; \
        } \
        inline bool operator&(BIT value) { \
            return (_value & static_cast<type>(value)) != 0; \
        } \
        inline bool operator%(BIT value) { \
            return (_value & static_cast<type>(value)) == static_cast<type>(value); \
        } \
    };


#define __define_enum_bitset(name) \
    inline name##EnumClassDefinition operator|(name##EnumClassDefinition value, name##EnumClassDefinition value2) { \
        return (name##EnumClassDefinition)(static_cast<name::TYPE>(value) | static_cast<name::TYPE>(value2)); \
    } \
    inline name::BIT operator| (name::BIT value, name::BIT value2) { \
        return static_cast<name::BIT>(static_cast<name::TYPE>(value) | static_cast<name::TYPE>(value2)); \
    }

#endif

using EnumBaseHelper::ValueInfo;
using EnumBaseHelper::ValueType;
using EnumBaseHelper::ValueVector;

template<class T, class ST>
class EnumBitset : public EnumBase<T, ST> {
public:
    using EnumBase<T, ST>::EnumBase;
    using EnumBase<T, ST>::__printParsedValue;
    using EnumBase<T, ST>::__getValues;
    using EnumBase<T, ST>::_value;

    static const T ANY = static_cast<T>(~static_cast<ST>(0));
    static const T NONE = static_cast<T>(0);

    EnumBitset(T value) : EnumBase<T, ST>(static_cast<T>(value)) {
    }

    inline void clear() {
        _value = NONE;
    }

    inline EnumBitset& operator+=(T value) {
        _value |= static_cast<ST>(value);
        return *this;
    }

    inline EnumBitset& operator+=(const EnumBitset<T, ST>& value) {
        _value |= value._value;
        return *this;
    }

    inline EnumBitset& operator-=(T value) {
        _value &= ~static_cast<ST>(value);
        return *this;
    }

    inline EnumBitset& operator=(const EnumBitset<T, ST>& value) {
        _value = value._value;
        return *this;
    }

    inline EnumBitset& operator=(T value) {
        _value = static_cast<ST>(value);
        return *this;
    }

    bool operator&(T value) const {
        return (_value & static_cast<ST>(value)) != 0;
    }

    bool operator%(T value) const {
        return (_value & static_cast<ST>(value)) == static_cast<ST>(value);
    }

    inline String toString() {
        return toString(_value);
    }

    inline static String toString(ST value) {
        return __toString(static_cast<T>(value));
    }

#if ENUM_BASE_STORE_ENUM_AS_STRING

    static String __toString(T value) {
        PrintString out;
        ValueVector items;
        // ValueVector items;
        __getValues(items);
        for (const auto& item : items) {
            if (item.valueState && (static_cast<ValueType>(value) & item.parsedValue)) {
                if (out.length()) {
                    out += ',';
                }
                out += item.name;
            }
        }
        out.printf_P(PSTR(" %u, (0x%0*x)"), (unsigned)value, sizeof(value) << 1, (unsigned)value);
        return out;
    }

    void dump(Print& output) const {
        output.printf_P(PSTR("Bitset: %u, (0x%0*x)"), (unsigned)_value, sizeof(_value) << 1, (unsigned)_value);
        ValueVector items;
        __getValues(items);
        int num = 0;
        auto value = _value;
        for (auto& item : items) {
            if (item.valueState && (_value & item.parsedValue)) {
                value &= ~item.parsedValue;
                if (num++ == 0) {
                    output.print(F(" set "));
                }
                else {
                    output.print('|');
                }
                output.print(item.name);
                __printParsedValue(output, item);
                item.valueState = -1;
            }
        }
        num = 0;
        for (const auto& item : items) {
            if (item.valueState >= 0) {
                if (num++ == 0) {
                    output.print(F(" unset "));
                }
                else {
                    output.print('|');
                }
                output.print(item.name);
                __printParsedValue(output, item);
            }
        }
        if (value != 0) {
            output.printf_P(PSTR(" unknown bits set 0x%0*x"), sizeof(_value) << 1, (unsigned)value);
        }
        output.println();
    }
#else

    inline static String __toString(T value) {
        return String(static_cast<ST>(value), sizeof(_value) == 1 ? 2 : 10);
    }

#endif
};
