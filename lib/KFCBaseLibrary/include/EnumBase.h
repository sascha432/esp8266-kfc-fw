/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <PrintString.h>
#include <vector>
#include <constexpr_tools.h>
#include "misc.h"

// declare creates the class
// define creates static variables and operators

#define DECLARE_ENUM(prefix, type, ...)                     __typedef_enum(prefix, type, __VA_ARGS__)
#define DEFINE_ENUM(prefix)                                 __define_enum(prefix)

#define DECLARE_ENUM_BITSET(prefix, type, ...)              __typedef_enum_bitset(prefix, type, __VA_ARGS__)
#define DEFINE_ENUM_BITSET(prefix)                          __define_enum_bitset(prefix)

// store enum names and values as string
// can be displayed with toString() and dump()
#ifndef ENUM_BASE_STORE_ENUM_AS_STRING
#define ENUM_BASE_STORE_ENUM_AS_STRING                      DEBUG
#endif

#if ENUM_BASE_STORE_ENUM_AS_STRING

// provides
// object = ENUMVALS for settings
// object == ENUMVALS for comparing
// object ? "any value other than zero" : "zero"
// object.toString() to convert to string (decimal value if debug mode is disabled)
// object.dump(Print &) display debug info
// ClassName::ENUM::ENUMVALS
// ClassName::ENUMVALS

#define __typedef_enum(name, type, ...) \
    enum class name##EnumClassDefinition { \
        __VA_ARGS__ \
    }; \
    \
    class name : public EnumBase<name##EnumClassDefinition, type> { \
    public: \
        using EnumBase<name##EnumClassDefinition, type>::EnumBase; \
        using EnumBase<name##EnumClassDefinition, type>::toString; \
        using EnumBase<name##EnumClassDefinition, type>::getValues; \
        using EnumBase<name##EnumClassDefinition, type>::getValuesAsString; \
        using EnumBase<name##EnumClassDefinition, type>::parseString; \
        using TYPE = type; \
        typedef enum { \
            __VA_ARGS__ \
        } ENUM; \
        name() : EnumBase<name##EnumClassDefinition, TYPE>() { \
        } \
        name(ENUM value) : EnumBase<name##EnumClassDefinition, TYPE>(static_cast<name##EnumClassDefinition>(value)) { \
        } \
        operator ENUM() { \
            return static_cast<ENUM>(_value); \
        } \
        EnumBase& operator=(ENUM value) { \
            _value = static_cast<TYPE>(value); \
            return *this; \
        } \
        inline bool operator==(ENUM value) const { \
            return _value == static_cast<TYPE>(value); \
        } \
    }; \
    constexpr const char *name##DebugEnumValues = _STRINGIFY(__VA_ARGS__);

    //constexpr auto name##DebugEnumValuesLength = EnumBaseConstExpr::names_len(_STRINGIFY(__VA_ARGS__));
    //const ArrayConstExpr::ArrayHelper<EnumBaseConstExpr::ArrayNameAccess<name##DebugEnumValuesLength>, char, name##DebugEnumValuesLength> name##DebugEnumValues PROGMEM(EnumBaseConstExpr::ArrayNameAccess<name##DebugEnumValuesLength>(_STRINGIFY(__VA_ARGS__))._elements);

//const char name##DebugEnumValues[] PROGMEM = { _STRINGIFY(__VA_ARGS__) };


#define __define_enum(name) \
    template<> \
    const char* EnumBase<name##EnumClassDefinition, name::TYPE>::valuesPtr = name##DebugEnumValues;

//const char *EnumBase<name##EnumClassDefinition, name::TYPE>::valuesPtr = name##DebugEnumValues.array;
//    const uint16_t EnumBase<name##EnumClassDefinition, name::TYPE>::valuesCount = EnumBaseConstExpr::count(name::values);



#else

#define __typedef_enum(name, type, ...) \
    enum class name##EnumClassDefinition { \
        __VA_ARGS__ \
    }; \
    class name : public EnumBase<name##EnumClassDefinition, type> { \
    public: \
        using EnumBase<name##EnumClassDefinition, type>::EnumBase; \
        typedef enum { \
            __VA_ARGS__ \
        } ENUM; \
        typedef type TYPE; \
        name() : EnumBase<name##EnumClassDefinition, TYPE>() { \
        } \
        name(ENUM value) : EnumBase<name##EnumClassDefinition, TYPE>(static_cast<name##EnumClassDefinition>(value)) { \
        } \
        EnumBase& operator=(ENUM value) { \
            _value = static_cast<TYPE>(value); \
            return *this; \
        } \
        inline bool operator==(ENUM value) const { \
            return _value == static_cast<TYPE>(value); \
        } \
    };

#define __define_enum(name) ;

#endif

namespace EnumBaseHelper {

    typedef int32_t ValueType;

    class ValueInfo {
    public:
        ValueInfo() : parsedValue(0), valueState(0) {}
        String name;
        String value;
        ValueType parsedValue;
        int8_t valueState;              // 0 = invalid, 1 valid
    };

    typedef std::vector<ValueInfo> ValueVector;

};

using EnumBaseHelper::ValueInfo;
using EnumBaseHelper::ValueType;
using EnumBaseHelper::ValueVector;

template<class T, class ST>
class EnumBase {
public:
    EnumBase() : _value(0) {
    }

    EnumBase(T value) : _value(static_cast<ST>(value)) {
    }

    EnumBase& operator=(T value) {
        _value = static_cast<ST>(value);
        return *this;
    }

    EnumBase& operator=(const EnumBase<T, ST>& value) {
        _value = value._value;
        return *this;
    }

    inline bool operator==(T value) const {
        return _value == static_cast<ST>(value);
    }

    explicit inline operator bool() const {
        return _value != 0;
    }

    inline ST getValue() const {
        return _value;
    }

    inline T getEnum() const {
        return static_cast<T>(_value);
    }

    inline String toString() {
        return __toString(static_cast<T>(_value));
    }

    inline static String toString(ST value) {
        return __toString(static_cast<T>(value));
    }

#if ENUM_BASE_STORE_ENUM_AS_STRING

    static String __toString(T value) {
        ValueVector items;
        __getValues(items);
        for (const auto& item : items) {
            if (item.valueState && (static_cast<ValueType>(value) == item.parsedValue)) {
                return item.name;
            }
        }
        return String(static_cast<ValueType>(value));
    }

    static void __printParsedValue(Print &output, const ValueInfo& item) {
        if (item.valueState) {
            output.printf_P(PSTR("=0x%0*x"), sizeof(_value) << 1, (unsigned)item.parsedValue);
        }
        else {
            output.printf_P(PSTR("=%s"), item.value.c_str());
        }
    }

    void dump(Print& output) const
    {
        output.printf_P(PSTR("Enum: %u (0x%0*x) "), (unsigned)_value, sizeof(_value) << 1, (unsigned)_value);
        auto value = _value;
        ValueVector items;
        __getValues(items);
        bool found = false;
        for (const auto& item : items) {
            if (item.valueState && value == item.parsedValue) {
                output.print(item.name);
                __printParsedValue(output, item);
                found = true;
                break;
            }
        }
        if (!found) {
            output.print(F(" - <invalid>"));
        }
        output.print(F(" ("));
        int num = 10;
        for (const auto& item : items) {
            output.print(item.name);
            __printParsedValue(output, item);
            if (num-- == 0) {
                break;
            }
        }
        output.println(F(")"));
    }

    static const char* end(PGM_P ptr, const char *ends) {
        int ch;
        while (0 != (ch = pgm_read_byte(ptr))) {
            if (strchr(ends, ch)) {
                return ptr;
            }
            ptr++;
        }
        return ptr;
    }

    static const char* start(PGM_P ptr) {
        int ch;
        while (0 != (ch = pgm_read_byte(ptr))) {
            if (ch != ' ' && ch != ',' && ch != '=') {
                return ptr;
            }
            ptr++;
        }
        return ptr;
    }

    static ValueVector getValues() {
        ValueVector items;
        __getValues(items);
        return items;
    }

    static String getValuesAsString(const __FlashStringHelper *sep) {
        PrintString str;
        ValueVector items;
        __getValues(items);
        int n = 0;
        for(auto &item: items) {
            if (n++ != 0) {
                str.print(sep);
            }
            if (item.valueState == 1) {
                str.printf_P(PSTR("%s=%04x"), item.name.c_str(), item.parsedValue);
            }
        }
        return str;
    }

    static bool __matchItems(char *ptr, size_t len, const ValueVector &items, ValueType &value, StringVector *results) {
        int count = 0;
        ValueInfo tmp;
        auto slen = strlen(ptr);
        for(auto &item: items) {
            // debug_printf("%s=%u %s,%u cnt %u\n", item.name.c_str(), item.parsedValue, ptr, len, count)
            if (item.valueState == 1) {
                if (!strncasecmp(ptr, item.name.c_str(), len)) {
                    count++;
                    tmp = item;
                    if (len == slen) {
                        count = 1;
                        break;
                    }
                }
            }
        }
        if (count == 0 && len > 3) {
            return __matchItems(ptr, len - 1, items, value, results);
        }
        if (count == 1) {
            value |= tmp.parsedValue;
            if (results) {
                results->emplace_back(tmp.name);
            }
            return true;
        }
        return false;
    }

    static ValueType parseString(String str, StringVector *results) {
        auto tokenSep = " |";
        auto ptr = strtok(str.begin(), tokenSep);
        ValueVector items;
        ValueType result = 0;
        __getValues(items);
        while(ptr) {
            if (!__matchItems(ptr, strlen(ptr), items, result, results)) {
                result |= strtoll(ptr, nullptr, 0);
            }
            ptr = strtok(nullptr, tokenSep);
        }
        return result;
    }

    static void __getValues(ValueVector &items) {
        ValueType num = 0;
        bool valid = false;
        auto ptr = valuesPtr;
        while(pgm_read_byte(ptr)) {
            ValueInfo item;
            PrintString value;

            // name
            ptr = start(ptr);
            auto ptr2 = end(ptr, "=, ");
            value.write_P(ptr, ptr2 - ptr);
            item.name = std::move(value);
            value = PrintString();

            // value
            ptr = start(ptr2);
            ptr2 = end(ptr, ",");
            value.write_P(ptr, ptr2 - ptr);
            value.trim();

            // parse value
            if (ptr2 != ptr) {
                char* endPtr;
                num = static_cast<ValueType>(strtoll(value.c_str(), &endPtr, 0));
                valid = endPtr && !*endPtr;
                if (valid) {
                    item.parsedValue = num;
                    item.value = std::move(value);
                }
                else {
                    item.parsedValue = (ValueType)-1;
                    value.replace(F(" "), emptyString);
                    item.value = '(';
                    item.value += value;
                    item.value += ')';
                }
            } else if (!valid && ptr2 == ptr) {
                num++;
            }
            item.valueState = valid;
            items.push_back(item);
            ptr = ptr2;
        }
    }

private:
    static const char *valuesPtr;
    //static const uint16_t valuesCount;
#else

    inline static String __toString(T value) {
        return String(static_cast<ST>(value));
    }

#endif

protected:
    ST _value;
};
