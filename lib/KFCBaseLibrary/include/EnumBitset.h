/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include "EnumBase.h"

#if DEBUG

// typedef
#define typedef_enum_bitset(name, type, ...) \
    enum class name##EnumClassDefinition { \
        __VA_ARGS__ \
    }; \
    name##EnumClassDefinition operator|(name##EnumClassDefinition value, name##EnumClassDefinition value2); \
    \
    class name : public EnumBitset<name##EnumClassDefinition, type> { \
    public: \
        using EnumBitset<name##EnumClassDefinition, type>::EnumBitset; \
        typedef name##EnumClassDefinition BIT; \
        typedef type TYPE; \
    }; \
    \
    static const char name##DebugEnumValues[] PROGMEM = { _STRINGIFY(__VA_ARGS__) };

#define define_enum_bitset(name) \
    template<> \
    const char *EnumBase<name##EnumClassDefinition, name::TYPE>::_enumDebugValues = name##DebugEnumValues; \
    \
    name##EnumClassDefinition operator|(name##EnumClassDefinition value, name##EnumClassDefinition value2) { \
        return (name##EnumClassDefinition)(static_cast<name::TYPE>(value) | static_cast<name::TYPE>(value2)); \
    }


#else

#define typedef_enum_bitset(name, type, ...) \
    enum class name##EnumClassDefinition { \
        __VA_ARGS__ \
    }; \
    name##EnumClassDefinition operator|(name##EnumClassDefinition value, name##EnumClassDefinition value2); \
    \
    class name : public EnumBitset<name##EnumClassDefinition, type> { \
    public: \
        using EnumBitset<name##EnumClassDefinition, type>::EnumBitset; \
        typedef name##EnumClassDefinition BIT; \
        typedef type TYPE; \
    };

#define define_enum_bitset(name) \
    \
    name##EnumClassDefinition operator|(name##EnumClassDefinition value, name##EnumClassDefinition value2) { \
        return (name##EnumClassDefinition)(static_cast<name::TYPE>(value) | static_cast<name::TYPE>(value2)); \
    }

#endif

template<class T, class ST>
class EnumBitset : public EnumBase<T, ST> {
public:
    using EnumBase<T, ST>::EnumBase;
    using EnumBase<T, ST>::_value;

    static const T ANY = static_cast<T>(~static_cast<ST>(0));
    static const T NONE = static_cast<T>(0);

    EnumBitset() : EnumBase<T, ST>(NONE) {
    }

    EnumBitset& operator+=(T value) {
        _value |= static_cast<ST>(value);
        return *this;
    }

    EnumBitset& operator+=(const EnumBitset<T, ST>& value) {
        _value |= value._value;
        return *this;
    }

    EnumBitset& operator-=(T value) {
        _value &= ~static_cast<ST>(value);
        return *this;
    }

    EnumBitset& operator-=(const EnumBitset<T, ST> &value) {
        _value &= ~value._value;
        return *this;
    }

    EnumBitset& operator=(const EnumBitset<T, ST> &value) {
        _value = value._value;
        return *this;
    }

    bool operator&(T value) const {
        return _value & static_cast<ST>(value);
    }

    inline String toString() {
        return toString(_value);
    }

    inline static String toString(ST value) {
        return __toString((T)value);
    }

#if DEBUG

    static String __toString(T value) {
        String out;
        auto map = EnumBase<T, ST>::__getValuesMap();
        for (const auto& item : map) {
            if ((ST)value & item.first) {
                if (out.length()) {
                    out += ',';
                }
                out += item.second;
            }
        }
        return out;
    }

    void dump(Print& output) const
    {
        output.printf_P(PSTR("Bitset: %u, (0x%0*x)"), _value, sizeof(_value) << 1, _value);
        auto value = _value;
        auto map = EnumBase<T, ST>::__getValuesMap();
        int num = 0;
        for (const auto& item : map) {
            if (value & item.first) {
                if (num++ != 0) {
                    output.print('|');
                }
                else {
                    output.print(F(" set "));
                }
                output.print(item.second);
                output.printf_P(PSTR("=0x%0*x"), sizeof(_value) << 1, item.first);
                value &= ~item.first;
            }
        }
        num = 0;
        for (const auto& item : map) {
            if (!(_value & item.first)) {
                if (num++ != 0) {
                    output.print('|');
                }
                else {
                    output.print(F(" unset "));
                }
                output.print(item.second);
                output.printf_P(PSTR("=0x%0*x"), sizeof(_value) << 1, item.first);
            }
        }
        if (value != 0) {
            output.printf_P(PSTR(" unknown bits set 0x%0*x"), sizeof(_value) << 1, value);
        }
        output.println();
    }
#else

    inline static String __toString(T value) {
        return String(value, 2);
    }

#endif
};
