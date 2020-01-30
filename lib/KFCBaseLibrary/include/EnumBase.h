/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "misc.h"

#if DEBUG

#include <map>

#define typedef_enum(name, type, ...) \
    enum class name##EnumClassDefinition{ \
        __VA_ARGS__ \
    }; \
    class name : public EnumBase<name##EnumClassDefinition, type> { \
    public: \
        using EnumBase<name##EnumClassDefinition, type>::EnumBase; \
        typedef name##EnumClassDefinition ENUM; \
        typedef type TYPE; \
    }; \
    static const char name##DebugEnumValues[] PROGMEM = { _STRINGIFY(__VA_ARGS__) };

#define define_enum(name) \
    template<> \
    const char *EnumBase<name##EnumClassDefinition, name::TYPE>::_enumDebugValues = name##DebugEnumValues;

#else

#define typedef_enum(name, type, ...) \
    enum class name##EnumClassDefinition{ \
        __VA_ARGS__ \
    }; \
    class name : public EnumBase<name##EnumClassDefinition, type> { \
    public: \
        using EnumBase<name##EnumClassDefinition, type>::EnumBase; \
        typedef name##EnumClassDefinition ENUM; \
        typedef type TYPE; \
    };

#define define_enum_debug(name) ;

#endif


template<class T, class ST>
class EnumBase {
public:
    EnumBase(T value) : _value(static_cast<ST>(value)) {
    }

    EnumBase& operator=(T value) {
        _value = (ST)value;
        return *this;
    }

    EnumBase& operator=(const EnumBase<T, ST>& value) {
        _value = value._value;
        return *this;
    }

    bool operator==(T value) const {
        return _value == static_cast<ST>(value);
    }

    operator bool() const {
        return _value != 0;
    }

    ST getValue() const {
        return _value;
    }

    T getEnum() const {
        return static_cast<T>(_value);
    }

    inline String toString() {
        return toString(_value);
    }

    inline static String toString(ST value) {
        return __toString(static_cast<T>(value));
    }

#if DEBUG
    typedef std::map<ST, String> ValueMap;

    static String __toString(T value) {
        auto map = __getValuesMap();
        for (const auto& item : map) {
            if ((ST)value == item.first) {
                return item.second;
            }
        }
        return String((ST)value);
    }

    void dump(Print& output) const
    {
        output.printf_P(PSTR("Enum: %u (0x%0*x) "), _value, sizeof(_value) << 1, _value);
        auto value = _value;
        auto map = __getValuesMap();
        bool found = false;
        for (const auto& item : map) {
            if (value == item.first) {
                output.print(item.second);
                output.printf_P(PSTR("=0x%0*x"), sizeof(_value) << 1, item.first);
                found = true;
                break;
            }
        }
        if (!found) {
            output.print(F(" - <invalid>"));
        }
        output.print(F(" ("));
        int num = 10;
        for (const auto& item : map) {
            output.print(item.second);
            output.printf_P(PSTR("=0x%0*x"), sizeof(_value) << 1, item.first);
            if (num-- == 0) {
                break;
            }
        }
        output.println(F(")"));
    }

    static ValueMap __getValuesMap() {
        ValueMap map;
        auto enumStr = String(FPSTR(_enumDebugValues));
        std::vector<char*> list;
        explode(enumStr.begin(), ",", list, " \r\n\t");
        ST num = 0;
        for (auto item : list) {
            auto value = strchr(item, '=');
            if (value) {
                *value++ = 0;
                num = static_cast<ST>(strtoll(value, nullptr, 0));
            }
            String tmp = item;
            tmp.trim();
            map[num] = tmp;
            num++;
        }
        return map;
    }

private:
    static const char* _enumDebugValues;
#else

    inline static String __toString(T value) {
        return String(value);
    }

#endif

protected:
    ST _value;
};
