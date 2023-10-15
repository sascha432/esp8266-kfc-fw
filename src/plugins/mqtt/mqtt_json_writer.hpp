/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <PrintString.h>
#include <JsonTools.h>
#include "mqtt_strings.h"

#define MQTT_JSON_WRITER_DEBUG 1

#if MQTT_JSON_WRITER_DEBUG
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#    include <debug_helper_disable_ptr_validation.h>
#endif

namespace MQTT {

    namespace Json {

        using namespace KFCJson;

        // UnnamedArrayWriter, UnnamedObjectWriter write JSON to the attached PrintStringInterface
        //
        // JsonStringWriter is a template with an embedded PrintString using the two base classes
        // to form 4 new classes
        //
        // UnnamedArray
        // UnnamedObject
        // NamedArray
        // NamedObject
        //
        // those can be used to create any JSON object inside the attached PrintString or use the
        // base classes UnnamedArrayWriter/UnnamedObjectWriter to attach another PrintStringInterface
        //
        // strings (keys and values) are stored as pointers, all other values as copy. To create new classes
        // with different behavior, the templates UnnamedVariant and NamedVariant can be used.
        //
        // To create a generator class, that generates its value when the JSON string is created,
        // see PrintToInterface
        //
        // following method must be available:
        //
        // [String|const char *|const __FlashStringHelper *] key() const
        // [main types + String|const __FlashStringHelper *] data() const
        //
        // the unnamed variants should inherit UnnamedBase, which returns INVALID as key.
        //
        // using ConstStrNamedShort = NamedVariant<const String &, int16_t>;
        // using StrNamedShort = NamedVariant<String, int16_t>;
        //
        // String, char * and const char * can be used with the Named* classes as well. The object or pointer
        // must be valid until the variant has been added to an array or object.
        //
        // auto obj1 = UnnamedObject();
        // for (int i = 0; i < 5; i++) {
        //     // create object in scope that survies append
        //     auto key = String("key") + String(i);
        //     obj1.append(NamedShort(key, 100 + i));
        //
        //     // the string will be destroyed before the append() call
        //     // obj1.append(NamedShort(String("key") + String(i), 100 + i)); // NOT VALID
        // }
        // Serial.println(obj1.toString()); // {"key0":100,"key1":101,"key2":102,"key3":103,"key4":104}
        //
        // json objects can be created inside the constructor or using the append function to
        // append more data. appendArray/appendObject can append data to nested objects. inserting
        // is not implemented
        //
        //
        // auto obj1 = UnnamedObject();
        // obj1.append(NamedArray(F("outer_array"), UnnamedObject(NamedArray(F("inner_array"), UnnamedObject(NamedNull(F("key1")))))));
        // obj1.appendObject(5, NamedNull(F("key2")));
        // obj1.append(NamedNull(F("key3")));
        // Serial.println(obj1.toString()); // {"outer_array":[{"inner_array":[{"key1":null,"key2":null}]}],"key3":null}
        //
        // other methods are
        // position = findNamed(name) and valueAt(position)

        using PrintStringInterface = PrintString;


        using FStr = const __FlashStringHelper *;

        struct PrintToInterface {
            // this is an abstract class with following methods
            // the class is not virtual to avoid the extra overhead
            //
            // as NamedVariant
            //
            // const __FlashStringHelper *key() const;
            // void printTo(PrintStringInterface &output) const;
            //
            // as UnnamedVariant and value of Un-/namedVariant
            //
            // void printTo(PrintStringInterface &output) const;
            //
            // printTo is responsible for quotes and encoding. the output can be any
            // valid json variant (nested array/objects)
            //
            // JsonPrintStringWrapper can be used to encode JSON values writing directly
            // to PrintStringInterface
            //
            // void printTo(PrintStringInterface &output) const
            // {
            //     output.print('"');
            //     JsonPrintStringWrapper wrapper(output);
            //     wrapper.print(F("my string \" with \" quotes that was generated at "));
            //     wrapper.strftime(F("%T %D"), time(nullptr));
            //     output.print('"');
            //  }
        };

        inline static FStr F_cast(const char *str) {
            __DBG_validatePointerCheck(str, VP_HPS);
            return reinterpret_cast<FStr>(str);
        }
        inline static FStr F_cast(char *str) {
            __DBG_validatePointerCheck(str, VP_HP);
            return reinterpret_cast<FStr>(const_cast<const char *>(str));
        }
        inline static FStr F_cast(const String &str) {
            __DBG_validatePointerCheck(str, VP_HS);
            return reinterpret_cast<FStr>(str.c_str());
        }

        enum class ErrorType : uint8_t {
            NONE = 0,
            OUT_OF_RANGE,
            NOT_END_OF_ARRAY,
            NOT_END_OF_OBJECT,
            CANNOT_INSERT,
        };

        static inline FStr errorStr(ErrorType error) {
            switch (error) {
            case ErrorType::OUT_OF_RANGE:
                return F("out of range");
            case ErrorType::NOT_END_OF_ARRAY:
                return F("not the end of an array");
            case ErrorType::NOT_END_OF_OBJECT:
                return F("not the end of an object");
            case ErrorType::CANNOT_INSERT:
                return F("cannot insert, only append");
            case ErrorType::NONE:
                break;
            }
            return F("no error");
        }

        namespace Helpers {

            template<typename _Ta>
            struct remove_reference_or_pointer {
                using type = typename std::remove_reference<typename std::remove_pointer<typename std::remove_cv<_Ta>::type>::type>::type;
            };

            template<typename _Ta>
            struct const_ref {
                static constexpr bool is_pointer = std::is_pointer<_Ta>::value;
                static constexpr bool is_reference = std::is_reference<_Ta>::value;
                static constexpr bool is_const = std::is_const<typename std::remove_pointer<_Ta>::type>::value;
                using nc_type = typename std::conditional<!is_pointer && !is_reference, typename std::add_lvalue_reference<_Ta>::type, _Ta>::type;
                using type = typename std::conditional<!is_pointer && !is_reference, typename std::add_lvalue_reference<typename std::add_const<_Ta>::type>::type, typename std::add_const<_Ta>::type>::type;
            };
        }

        struct UnnamedBase {
        };

        struct NamedBase {
        };

        class RemoveOuterBase;

        struct StoredString {
            StoredString(const char *value) : _value(__DBG_validatePointer(value, VP_HPS)) {
            }
            StoredString(const String &value) : _value(__DBG_validatePointer(value, VP_HS)) {
            }
            StoredString(String &&value) : _value(std::move(__DBG_validatePointer(value, VP_HS))) {
            }

            String _value;
        };

        template<typename _Type, typename _ConstRef = typename Helpers::const_ref<_Type>::type>
        struct UnnamedVariant : UnnamedBase {
            UnnamedVariant(_ConstRef value) : _value(value) {}
            _ConstRef value() {
                return _value;
            }
            _ConstRef value() const {
                return _value;
            }
            _Type _value;
        };

        template<typename _KeyType, typename _ValueType, typename _KeyConstRef = typename Helpers::const_ref<_KeyType>::type, typename _ValueConstRef = typename Helpers::const_ref<_ValueType>::type>
        struct NamedVariant : UnnamedVariant<_ValueType, _ValueConstRef>, NamedBase {
            NamedVariant(_KeyType key, _ValueType value) : UnnamedVariant<_ValueType>(value), _key(key) {}

            template<typename _Ta, typename std::enable_if<std::is_same<_KeyType, FStr>::value, int>::type = 0>
            NamedVariant(const _Ta &key, _ValueType value) : UnnamedVariant<_ValueType>(value), _key(F_cast(key.c_str())) {}

            template<typename _Ta, typename std::enable_if<std::is_same<_KeyType, FStr>::value, int>::type = 0>
            NamedVariant(const _Ta *key, _ValueType value) : UnnamedVariant<_ValueType>(value), _key(F_cast(key)) {}

            _KeyConstRef key() {
                return key;
            }
            _KeyConstRef key() const {
                return _key;
            }
            _KeyType _key;
        };

        struct UnnamedFormattedInteger : PrintToInterface, UnnamedBase {
            UnnamedFormattedInteger(uint32_t value, FStr format = FSPGM(default_format_integer, "\"%05d\"")) :
                _format(format),
                _value(value)
            {
            }

            inline __attribute__((__always_inline__))
            void printTo(PrintStringInterface &output) const {
                output.printf_P(reinterpret_cast<PGM_P>(_format), _value);
            }

            FStr _format;
            uint32_t _value;
        };

        struct NamedFormattedInteger : UnnamedFormattedInteger, NamedBase {
            NamedFormattedInteger(FStr key, uint32_t value, FStr format = FSPGM(default_format_integer)) :
                UnnamedFormattedInteger(value, format),
                _key(key)
            {
            }

            inline __attribute__((__always_inline__))
            FStr key() const {
                return __DBG_validatePointer(_key, VP_HP);
            }

            FStr _key;
        };

        // if the value is not normal (nan, inf, -inf etc..) the output is null to meet JSON standards
        struct UnnamedFloat : PrintToInterface, UnnamedBase {
            UnnamedFloat(float value, uint8_t precision) :
                _precision(precision),
                _value(value)
            {
            }

            inline __attribute__((__always_inline__))
                void printTo(PrintStringInterface &output) const {
                if (std::isfinite(_value)) {
                    output.printf_P(PSTR("%.*f"), _precision, _value);
                }
                else {
                    output.print(F("null"));
                }
            }

            uint8_t _precision;
            float _value;
        };

        // if the value is not normal (nan, inf, -inf etc..) the output is null to meet JSON standards
        struct UnnamedDouble : PrintToInterface, UnnamedBase {
            UnnamedDouble(double value, uint8_t precision) :
                _precision(precision),
                _value(value)
            {
            }

            inline __attribute__((__always_inline__))
                void printTo(PrintStringInterface &output) const {
                if (std::isfinite(_value)) {
                    output.printf_P(PSTR("%.*f"), _precision, _value);
                }
                else {
                    output.print(F("null"));
                }
            }

            uint8_t _precision;
            double _value;
        };

        // if the value is not normal (nan, inf, -inf etc..) the output is null to meet JSON standards
        struct UnnamedFormattedFloat : PrintToInterface, UnnamedBase {
            UnnamedFormattedFloat(float value, FStr format = FSPGM(default_format_double, "%.2f")) :
                _format(__DBG_validatePointer(format, VP_HP)),
                _value(value)
            {
            }

            inline static FStr getPrecisionFormat(int precision) {
                switch (precision) {
                case 0:
                    return F("%.0f");
                case 1:
                    return F("%.1f");
                case 2:
                    return F("%.2f");
                case 3:
                    return F("%.3f");
                case 4:
                    return F("%.4f");
                case 5:
                    return F("%.5f");
                case 6:
                    return F("%.6f");
                case 7:
                    return F("%.7f");
                default:
                    break;
                }
                return F("%f");
            }

            inline __attribute__((__always_inline__))
                void printTo(PrintStringInterface &output) const {
                if (std::isfinite(_value)) {
                    __DBG_validatePointerCheck(_format, VP_HP);
                    output.printf_P(reinterpret_cast<PGM_P>(_format), _value);
                }
                else {
                    output.print(F("null"));
                }
            }

            FStr _format;
            float _value;
        };

        // if the value is not normal (nan, inf, -inf etc..) the output is null to meet JSON standards
        struct UnnamedFormattedDouble : PrintToInterface, UnnamedBase {
            UnnamedFormattedDouble(double value, FStr format = FSPGM(default_format_double, "%.2f")) :
                _format(__DBG_validatePointer(format, VP_HP)),
                _value(value)
            {
            }

            inline static FStr getPrecisionFormat(int precision) {
                switch (precision) {
                case 0:
                    return F("%.0f");
                case 1:
                    return F("%.1f");
                case 2:
                    return F("%.2f");
                case 3:
                    return F("%.3f");
                case 4:
                    return F("%.4f");
                case 5:
                    return F("%.5f");
                case 6:
                    return F("%.6f");
                case 7:
                    return F("%.7f");
                case 8:
                    return F("%.8f");
                case 9:
                    return F("%.9f");
                case 10:
                    return F("%.10f");
                case 11:
                    return F("%.11f");
                case 12:
                    return F("%.12f");
                default:
                    break;
                }
                return F("%f");
            }

            inline __attribute__((__always_inline__))
                void printTo(PrintStringInterface &output) const {
                if (std::isfinite(_value)) {
                    __DBG_validatePointerCheck(_format, VP_HP);
                    output.printf_P(reinterpret_cast<PGM_P>(_format), _value);
                }
                else {
                    output.print(F("null"));
                }
            }

            FStr _format;
            double _value;
        };

        struct NamedFloat : UnnamedFloat, NamedBase {
            NamedFloat(FStr key, float value, uint8_t precision) :
                UnnamedFloat(value, precision), _key(key)
            {
            }

            inline __attribute__((__always_inline__))
            FStr key() const {
                return __DBG_validatePointer(_key, VP_HP);
            }

            FStr _key;
        };

        struct NamedDouble : UnnamedDouble, NamedBase {
            NamedDouble(FStr key, double value, uint8_t precision) :
                UnnamedDouble(value, precision), _key(key)
            {
            }

            inline __attribute__((__always_inline__))
            FStr key() const {
                return __DBG_validatePointer(_key, VP_HP);
            }

            FStr _key;
        };

        struct NamedFormattedFloat : UnnamedFormattedFloat, NamedBase {
            NamedFormattedFloat(FStr key, float value, FStr format = FSPGM(default_format_double)) :
                UnnamedFormattedFloat(value, format), _key(key)
            {
            }

            inline __attribute__((__always_inline__))
            FStr key() const {
                return __DBG_validatePointer(_key, VP_HP);
            }

            FStr _key;
        };

        struct NamedFormattedDouble : UnnamedFormattedDouble, NamedBase {
            NamedFormattedDouble(FStr key, double value, FStr format = FSPGM(default_format_double)) :
                UnnamedFormattedDouble(value, format), _key(key)
            {
            }

            inline __attribute__((__always_inline__))
            FStr key() const {
                return __DBG_validatePointer(_key, VP_HP);
            }

            FStr _key;
        };

        // if the value is not normaled (nan, inf, -inf etc..) the output is null to meet JSON standards
        // if the value has more than one leading zero (1.00000, 1.23000), those get trimmed (1.0, 1.23)
        struct UnnamedTrimmedFormattedFloat : PrintToInterface, UnnamedBase {
            UnnamedTrimmedFormattedFloat(float value, FStr format = FSPGM(default_format_double)) :
                _format(__DBG_validatePointer(format, VP_HP)),
                _value(value)
            {
            }

            void printTo(PrintStringInterface &output) const {
                if (std::isfinite(_value)) {
                    auto length = output.length();
                    output.printf_P(reinterpret_cast<PGM_P>(_format), _value);
                    // find the dot
                    auto pos = output.indexOf('.', length);
                    if (pos != -1) {
                        // trim all zeros
                        output.rtrim('0');
                        // if the dot is the last character, append a 0
                        if (pos == static_cast<decltype(pos)>(output.length() - 1)) {
                            output += '0';
                        }
                    }
                }
                else {
                    output.print(F("null"));
                }
            }

            FStr _format;
            float _value;
        };

        // if the value is not normaled (nan, inf, -inf etc..) the output is null to meet JSON standards
        // if the value has more than one leading zero (1.00000, 1.23000), those get trimmed (1.0, 1.23)
        struct UnnamedTrimmedFormattedDouble : PrintToInterface, UnnamedBase {
            UnnamedTrimmedFormattedDouble(double value, FStr format = FSPGM(default_format_double)) :
                _format(__DBG_validatePointer(format, VP_HP)),
                _value(value)
            {
            }

            void printTo(PrintStringInterface &output) const {
                if (std::isfinite(_value)) {
                    auto length = output.length();
                    output.printf_P(reinterpret_cast<PGM_P>(_format), _value);
                    // find the dot
                    auto pos = output.indexOf('.', length);
                    if (pos != -1) {
                        // trim all zeros
                        output.rtrim('0');
                        // if the dot is the last character, append a 0
                        if (pos == static_cast<decltype(pos)>(output.length() - 1)) {
                            output += '0';
                        }
                    }
                }
                else {
                    output.print(F("null"));
                }
            }

            FStr _format;
            double _value;
        };

        struct NamedTrimmedFormattedFloat : UnnamedTrimmedFormattedFloat, NamedBase {
            NamedTrimmedFormattedFloat(FStr key, double value, FStr format = FSPGM(default_format_double)) :
                UnnamedTrimmedFormattedFloat(value, format),
                _key(key)
            {
            }

            inline __attribute__((__always_inline__))
            FStr key() const {
                return __DBG_validatePointer(_key, VP_HP);
            }

            FStr _key;
        };

        struct NamedTrimmedFormattedDouble : UnnamedTrimmedFormattedDouble, NamedBase {
            NamedTrimmedFormattedDouble(FStr key, double value, FStr format = FSPGM(default_format_double)) :
                UnnamedTrimmedFormattedDouble(value, format),
                _key(key)
            {
            }

            inline __attribute__((__always_inline__))
            FStr key() const {
                return __DBG_validatePointer(_key, VP_HP);
            }

            FStr _key;
        };

        struct ObjectType {};
        struct ArrayType {};

        class UnnamedArrayWriter;
        class UnnamedObjectWriter;

        template<typename _Ta, typename _Tb>
        class JsonStringWriter;

        class UnnamedArray;
        class UnnamedObject;
        class NamedArray;
        class NamedObject;

        using UnnamedShort = UnnamedVariant<int16_t>;
        using UnnamedUnsignedShort = UnnamedVariant<uint16_t>;
        using UnnamedInteger = UnnamedVariant<int32_t>;
        using UnnamedUnsigned = UnnamedVariant<uint32_t>;
        using UnnamedInt64 = UnnamedVariant<int64_t>;
        using UnnamedUint64 = UnnamedVariant<uint64_t>;
        using UnnamedBool = UnnamedVariant<bool>;
        using UnnamedChar = UnnamedVariant<char>;

        struct UnnamedNull : UnnamedBase {

            inline __attribute__((__always_inline__))
            FStr value() const {
                return F("null");
            }
        };

        struct UnnamedString : UnnamedVariant<FStr> {
            UnnamedString(FStr value) : UnnamedVariant<FStr>(value) {}
            UnnamedString(const char *value) : UnnamedVariant<FStr>(reinterpret_cast<FStr>(value)) {}
            UnnamedString(const String &value) : UnnamedVariant<FStr>(reinterpret_cast<FStr>(value.c_str())) {}
        };

        using NamedShort = NamedVariant<FStr, int16_t>;
        using NamedUnsignedShort = NamedVariant<FStr, uint16_t>;
        using NamedInt32 = NamedVariant<FStr, int32_t>;
        using NamedUint32 = NamedVariant<FStr, uint32_t>;
        using NamedInt = NamedInt32;
        using NamedUnsigned = NamedUint32;
        using NamedInt64 = NamedVariant<FStr, int64_t>;
        using NamedUint64 = NamedVariant<FStr, uint64_t>;
        using NamedChar = NamedVariant<FStr, char>;
        class NamedArray;
        class NamedObject;

        struct NamedNull : NamedBase {
            NamedNull(FStr key) : _key(key) {}
            NamedNull(const String &key) : _key(F_cast(key.c_str())) {}
            NamedNull(const char *key) : _key(F_cast(key)) {}

            inline __attribute__((__always_inline__))
            FStr key() const {
                return __DBG_validatePointer(_key, VP_HP);
            }

            inline __attribute__((__always_inline__))
            FStr value() const {
                return F("null");
            }

            FStr _key;
        };

        struct NamedString : NamedVariant<FStr, FStr> {
            NamedString(FStr key, FStr value) : NamedVariant<FStr, FStr>(key, value) {}
            NamedString(FStr key, const char *value) : NamedVariant<FStr, FStr>(key, reinterpret_cast<FStr>(value)) {}
            NamedString(FStr key, const String &value) : NamedVariant<FStr, FStr>(key, reinterpret_cast<FStr>(value.c_str())) {}
        };

        struct UnnamedStoredString : UnnamedBase {
            UnnamedStoredString(const char *str) : _value(__DBG_validatePointer(str, VP_HPS)) {}
            UnnamedStoredString(const String &str) : _value(__DBG_validatePointer(str, VP_HS)) {}
            UnnamedStoredString(String &&str) : _value(std::move(__DBG_validatePointer(str, VP_HS))) {}

            const String &value() const {
                return __DBG_validatePointer(_value, VP_HPS);
            }

            String _value;
        };

        struct NamedStoredString : UnnamedStoredString, NamedBase {
            NamedStoredString(FStr key, const char *value) : UnnamedStoredString(value), _key(key) {}
            NamedStoredString(const char *key, const char *value) : UnnamedStoredString(value), _key(reinterpret_cast<FStr>(key)) {}
            NamedStoredString(const String &key, const char *value) : UnnamedStoredString(value), _key(reinterpret_cast<FStr>(key.c_str())) {}

            template<typename _Ta>
            NamedStoredString(FStr key, _Ta value) : UnnamedStoredString(std::forward<_Ta>(value)), _key(key) {}

            template<typename _Ta>
            NamedStoredString(const char *key, _Ta value) : UnnamedStoredString(std::forward<_Ta>(value)), _key(reinterpret_cast<FStr>(key)) {}

            template<typename _Ta>
            NamedStoredString(const String &key, _Ta value) : UnnamedStoredString(std::forward<_Ta>(value)), _key(reinterpret_cast<FStr>(key.c_str())) {}

            FStr key() const {
                return __DBG_validatePointer(_key, VP_HP);
            }

            FStr _key;
        };

        struct NamedBool : NamedVariant<FStr, bool> {
            NamedBool(FStr key, bool value) : NamedVariant(key, value) {}
        };

  /*      template<template<typename T1, typename T2> class _Ta, typename _Tb, typename _Tc>
        struct NamedPair {
            NamedPair(const _Ta<_Tb, _Tc> &pair) : _pair(pair) {}
            _Tb key() const {
                return _pair.first;
            }
            _Tc data() const {
                return _pair.second;
            }
            _Ta<_Tb, _Tc> _pair;
        };*/

        struct Brightness : NamedUnsignedShort {
            Brightness(uint16_t brightness) : NamedUnsignedShort(F("brightness"), brightness) {}
        };

        struct ColorTemperature : NamedUnsigned {
            ColorTemperature(uint32_t temp) : NamedUnsigned(F("color_temp"), temp) {}
        };

        struct State : NamedVariant<FStr, FStr> {
            State(bool value) : NamedVariant(F("state"), value ? FSPGM(mqtt_bool_on) : FSPGM(mqtt_bool_off)) {}
        };

        struct Effect : NamedString {
            Effect(FStr effect) : NamedString(F("effect"), effect) {}
            Effect(const String &effect) : NamedString(F("effect"), effect) {}
            Effect(const char *effect) : NamedString(F("effect"), effect) {}
        };

        struct Transition : NamedTrimmedFormattedFloat {
            Transition(float transition, uint8_t precision = 2) :
                NamedTrimmedFormattedFloat(F("transition"), transition, UnnamedFormattedFloat::getPrecisionFormat(precision))
            {
            }
        };

        class UnnamedArrayWriter {
        public:
            // constructor for empty arrays
            UnnamedArrayWriter(PrintStringInterface &output) : _output(output) {
                _output.print(F("[]"));
            }

            // constructor for arrays with elements
            template<class ..._Args>
            UnnamedArrayWriter(PrintStringInterface &output, _Args&& ... args) :
                _output(output)
            {
                output.print('[');
                add(std::forward<_Args>(args) ...);
                _setLast(']');
            }

        protected:
            friend JsonStringWriter<UnnamedArrayWriter, ArrayType>;
            friend JsonStringWriter<UnnamedObjectWriter, ObjectType>;

            // constructor for empty arrays
            UnnamedArrayWriter(ArrayType, PrintStringInterface &output) :
                UnnamedArrayWriter(output)
            {
            }

            // constructor for arrays with elements
            template<class ..._Args>
            UnnamedArrayWriter(ArrayType, PrintStringInterface &output, _Args&& ... args) :
                _output(output)
            {
                output.print('[');
                add(std::forward<_Args>(args) ...);
                _setLast(']');
            }

            // constructor for objects
            UnnamedArrayWriter(ObjectType, PrintStringInterface &output) : _output(output) {
            }

        public:
            template<class ..._Args>
            bool append(_Args&& ... args) {
                _append();
                add(std::forward<_Args>(args) ...);
                _setLast(']');
                return true;
            }

            template<typename _Ta, class ..._Args, typename std::enable_if<std::is_integral<_Ta>::value, int>::type = 0>
            inline ErrorType appendArray(_Ta level, _Args&& ...args) {
                auto error = validateLevel(static_cast<uint8_t>(level), ']');
                if (error != ErrorType::NONE) {
                    return error;
                }
                return appendLevel(static_cast<uint8_t>(level), std::forward<_Args>(args)...);
            }

        protected:
            template<class ..._Args>
            ErrorType appendLevel(uint8_t level, _Args&& ... args) {
                uint32_t bits = 0;
                for (uint8_t i = 0; i < level; i++) {
                    auto type = _appendRemove();
                    if (type == -1) {
                        return ErrorType::NOT_END_OF_ARRAY;
                    }
                    bits |= _fromChar(i, type);
                }
                _appendLevel();
                add(std::forward<_Args>(args) ...);
                int8_t i = level - 1;
                _setLast(_toCharClose(i, bits));
                while (--i >= 0) {
                    _output.print(_toCharClose(i, bits));
                }
                return ErrorType::NONE;
            }

            inline __attribute__((__always_inline__))
            static uint32_t _fromChar(uint8_t n, uint8_t type) {
                return type ? (1U << n) : 0;
            }

            inline __attribute__((__always_inline__))
            static char _toCharClose(uint8_t n, uint32_t bits) {
                return bits & (1U << n) ? '}' : ']';
            }

        public:
            inline __attribute__((__always_inline__))
            const char *c_str() const {
                __DBG_validatePointerCheck(_output, VP_HS);
                return _output.c_str();
            }

            inline __attribute__((__always_inline__))
            size_t length() const {
                return _output.length();
            }

            inline __attribute__((__always_inline__))
            const String &toString() const {
                return _output;
            }

            inline __attribute__((__always_inline__))
            PrintStringInterface &toString() {
                return _output;
            }

            inline __attribute__((__always_inline__))
            void printTo(Print &print) const {
                print.print(_output);
            }

        protected:
            // error -1
            // object = 1
            // array = 0
            int8_t _appendRemove() {
                auto len = _output.length() - 1;
                auto ch = _output.charAt(len);
                auto type = _isValidTypeClose(ch);
                if (type == -1) {
                    return -1;

                }
                _output.remove(len);
                return type;
            }

            inline __attribute__((__always_inline__))
            char _getLast() const {
                auto len = _output.length();
                if (!len) {
                    return 0;
                }
                return _output.charAt(len - 1);
            }

            inline __attribute__((__always_inline__))
            void _appendLevel() {
                switch (_getLast()) {
                case '[':
                case ',':
                    break;
                default:
                    _output.print(',');
                    break;
                }
            }

            inline __attribute__((__always_inline__))
            void _append() {
                // empty arrays and objects start with [ and {
                if (_output.length() > 2) {
                    // not empty, append ","
                    _setLast(',');
                }
                else {
                    // empty, remove closing brace
                    _output.remove(1);
                }
            }

            inline __attribute__((__always_inline__))
            void _setLast(char ch) {
                if (_output.length() > 1) {
                    #if MQTT_JSON_WRITER_DEBUG
                        __DBG_assertf(_output.endsWith(']') || _output.endsWith('}') || _output.endsWith(','), "expected }, ] or ',': ...'%s'", _output.c_str() + std::max<int>(0, _output.length() - 10));
                    #endif
                    _output.setCharAt(_output.length() - 1, ch);
                }
                else {
                    // add closing brace
                    _output.print(ch);
                    #if MQTT_JSON_WRITER_DEBUG
                        __DBG_assertf(ch != '[' || (ch == '[' && _output.charAt(0) == '['), "expected [: '%.10s'", _output.c_str());
                        __DBG_assertf(ch != '{' || (ch == '{' && _output.charAt(0) == '{'), "expected {: '%.10s'", _output.c_str());
                        __DBG_assertf(_output.startsWith('{') || _output.startsWith('['), "expected { or [: '%.10s'", _output.c_str());
                    #endif
                }
            }

        public:
            ErrorType validateLevel(uint8_t level, char type) const {
                auto len = _output.length();
                if (level >= len - 2) {
                    return ErrorType::OUT_OF_RANGE;
                }
                auto cStr = _output.c_str() + len - level;
                if (type != *cStr && *cStr != ',') {
                    return type == ']' ? ErrorType::NOT_END_OF_ARRAY : ErrorType::NOT_END_OF_OBJECT;
                }
                String tmp = F("]}");
                if (strcspn(++cStr, tmp.c_str())) {
                    return ErrorType::CANNOT_INSERT;
                }
                return ErrorType::NONE;
            }

            int findNamed(FStr name) const {
                __DBG_validatePointerCheck(name, VP_HP);
                int ofs = 0;
                auto len = strlen_P(reinterpret_cast<PGM_P>(name));
                while ((ofs = _output.indexOf(name, ofs)) != -1) {

                    if (ofs == 0 || _output.charAt(ofs - 1) == '"') {
                        auto end = ofs + len;
                        if (end >= _output.length() - 1) {
                            return -1;
                        }
                        if (strncmp_P(_output.c_str() + end, PSTR("\":"), 2) == 0) {
                            return end + 2;
                        }
                    }
                    ofs++;
                }
                return -1;
            }

            inline __attribute__((__always_inline__))
            int findNamed(PGM_P name) const {
                return findNamed(reinterpret_cast<FStr>(name));
            }

            String valueAt(int pos) const {
                if (pos == -1) {
                    return String();
                }
                auto begin = _output.c_str();
                auto end = begin + _output.length();
                begin += pos;
                if (begin >= end) {
                    return String();
                }
                if (*begin == '"') {
                    ++begin;
                    auto start = begin;
                    while (begin < end) {
                        if (*begin++ == '"') {
                            return _output.substring(pos + 1, (begin - start) + pos);
                        }
                        if (*begin++ == '\\') {
                            if (*begin) {
                                ++begin;
                            }
                        }
                    }
                    return String();
                }
                else {
                    String tmp = F("]},");
                    int n = strcspn(begin, tmp.c_str());
                    if (n == 0) {
                        return String();
                    }
                    return _output.substring(pos, pos + n);
                }
            }

        protected:
            // error -1
            // object = 1
            // array = 0
            inline __attribute__((__always_inline__))
            static int8_t _isValidTypeClose(char ch) {
                return ch == ']' ? 0 : ch == '}' ? 1 : -1;
            }

            // error -1
            // object = 1
            // array = 0
            inline __attribute__((__always_inline__))
            static int8_t _isValidTypeOpen(char ch) {
                return ch == '[' ? 0 : ch == '{' ? 1 : -1;
            }

        protected:
            inline __attribute__((__always_inline__))
            void appendValue(const char *value) {
                __DBG_validatePointerCheck(value, VP_HPS);
                JsonTools::Utf8Buffer buffer;
                _output.print('"');
                JsonTools::printToEscaped(_output, value, strlen(value), &buffer);
                _output.print('"');
            }

            inline __attribute__((__always_inline__))
            void appendValue(FStr value) {
                __DBG_validatePointerCheck(value, VP_HP);
                JsonTools::Utf8Buffer buffer;
                _output.print('"');
                JsonTools::printToEscaped(_output, value, &buffer);
                _output.print('"');
            }

            inline __attribute__((__always_inline__))
            void appendValue(const String &value) {
                __DBG_validatePointerCheck(value, VP_HS);
                JsonTools::Utf8Buffer buffer;
                _output.print('"');
                JsonTools::printToEscaped(_output, value, &buffer);
                _output.print('"');
            }

            inline __attribute__((__always_inline__))
            void appendValue(const PrintStringInterface &value) {
                _output.print(value);
            }

            template<typename _Ta, typename std::enable_if<std::is_base_of<PrintToInterface, _Ta>::value, int>::type = 0>
            inline void appendValue(const _Ta &arg) {
                arg.printTo(_output);
            }

            inline __attribute__((__always_inline__))
            void appendValue(nullptr_t) {
                _output.print(F("null"));
            }

            inline __attribute__((__always_inline__))
            void appendValue(bool value) {
                _output.print(value ? F("true") : F("false"));
            }

            inline __attribute__((__always_inline__))
            void appendValue(char value) {
                _output.print('"');
                _output.print(value);
                _output.print('"');
            }

            inline __attribute__((__always_inline__))
            void appendValue(int8_t value) {
                _output.print(static_cast<int>(value));
            }

            inline __attribute__((__always_inline__))
            void appendValue(uint8_t value) {
                _output.print(static_cast<unsigned>(value));
            }

            inline __attribute__((__always_inline__))
            void appendValue(int16_t value) {
                _output.print(static_cast<int>(value));
            }

            inline __attribute__((__always_inline__))
            void appendValue(uint16_t value) {
                _output.print(static_cast<unsigned>(value));
            }

            inline __attribute__((__always_inline__))
            void appendValue(int32_t value) {
                _output.print(value);
            }

            inline __attribute__((__always_inline__))
            void appendValue(uint32_t value) {
                _output.print(value);
            }

            inline __attribute__((__always_inline__))
            void appendValue(int64_t value) {
                _output.print(value);
            }

            inline __attribute__((__always_inline__))
            void appendValue(uint64_t value) {
                _output.print(value);
            }

            inline __attribute__((__always_inline__))
            void appendValue(float value) {
                _output.print(value);
            }

            inline __attribute__((__always_inline__))
            void appendValue(double value) {
                _output.print(value);
            }

            template<typename _Ta, typename std::enable_if<!std::is_base_of<PrintToInterface, _Ta>::value, int>::type = 0>
            void appendValue(_Ta value) {
                _output.print(value);
            }

        protected:
            inline __attribute__((__always_inline__))
            void add() {
            }

            void add(const UnnamedObject &writer);
            void add(const UnnamedArray &writer);

            // template<typename _Ta, typename std::enable_if<std::is_base_of<UnnamedObject, _Ta>::value && !std::is_base_of<NamedObject, _Ta>::value, int>::type = 0>
            // inline __attribute__((__always_inline__))
            // void add(const _Ta &writer)
            // {
            //     _output.reserve(_output.length() + writer.length() + 1);
            //     writer.printTo(_output);
            //     _output.print(',');
            // }

            // template<typename _Ta, typename std::enable_if<std::is_base_of<UnnamedArray, _Ta>::value && !std::is_base_of<NamedArray, _Ta>::value, int>::type = 0>
            // inline __attribute__((__always_inline__))
            // void add(const _Ta &writer)
            // {
            //     _output.reserve(_output.length() + writer.length() + 1);
            //     writer.printTo(_output);
            //     _output.print(',');
            // }


            inline __attribute__((__always_inline__))
            void add(const String &str) {
                _output.print(str);
                _output.print(',');
            }

            template<template<typename> class _Ta, typename _Tb, typename std::enable_if<!std::is_base_of<PrintToInterface, _Ta<_Tb>>::value, int>::type = 0>
            void add(const _Ta<_Tb> &container) {
                appendValue(container.value());
                _output.print(',');
            }

            template<typename _Ta, typename std::enable_if<!std::is_base_of<PrintToInterface, _Ta>::value, int>::type = 0>
            void add(const _Ta &container) {
                appendValue(container.value());
                _output.print(',');
            }

            template<template<typename> class _Ta, typename _Tb, typename std::enable_if<std::is_base_of<PrintToInterface, _Ta<_Tb>>::value, int>::type = 0>
            void add(const _Ta<_Tb> &print) {
                print.printTo(_output);
                _output.print(',');
            }

            template<typename _Ta, typename std::enable_if<std::is_base_of<PrintToInterface, _Ta>::value, int>::type = 0>
            void add(const _Ta &print) {
                print.printTo(_output);
                _output.print(',');
            }

            template<typename _Ta, typename ... _Args>
            void add(const _Ta &arg, _Args&& ...args) {
                add(arg);
                add(std::forward<_Args>(args)...);
            }

        protected:
            PrintStringInterface &_output;
        };

        class UnnamedObjectWriter : public UnnamedArrayWriter {
        public:
            // constructor for empty objects
            UnnamedObjectWriter(PrintStringInterface &output) :
                UnnamedArrayWriter(ObjectType(), output)
            {
                _output.print(F("{}"));
            }

            // constructor for objects with elements
            template<class ..._Args>
            UnnamedObjectWriter(PrintStringInterface &output, _Args&& ... args) :
                UnnamedArrayWriter(ObjectType(), output)
            {
                output.print('{');
                add(std::forward<_Args>(args) ...);
                _setLast('}');
            }

        protected:
            friend JsonStringWriter<UnnamedArrayWriter, ArrayType>;
            friend JsonStringWriter<UnnamedObjectWriter, ObjectType>;

            UnnamedObjectWriter(ObjectType, PrintStringInterface &output) :
                UnnamedArrayWriter(ObjectType(), output)
            {
                _output.print(F("{}"));
            }

            template<class ..._Args>
            UnnamedObjectWriter(ObjectType, PrintStringInterface &output, _Args&& ... args) :
                UnnamedArrayWriter(ObjectType(), output)
            {
                output.print('{');
                add(std::forward<_Args>(args) ...);
                _setLast('}');
            }

        public:
            template<class ..._Args>
            ErrorType append(_Args&& ... args) {
                _append();
                add(std::forward<_Args>(args) ...);
                _setLast('}');
                return ErrorType::NONE;
            }

            template<typename _Ta, class ..._Args, typename std::enable_if<std::is_integral<_Ta>::value, int>::type = 0>
            ErrorType appendObject(_Ta level, _Args&& ...args) {
                auto error = validateLevel(static_cast<uint8_t>(level), '}');
                if (error != ErrorType::NONE) {
                    return error;
                }
                return appendLevel(static_cast<uint8_t>(level), std::forward<_Args>(args)...);
            }

            template<typename _Ta, class ..._Args, typename std::enable_if<std::is_integral<_Ta>::value, int>::type = 0>
            ErrorType appendArray(_Ta level, _Args&& ...args) {
                return UnnamedArrayWriter::appendArray(static_cast<uint8_t>(level), std::forward<_Args>(args)...);
            }

        protected:
            template<class ..._Args>
            ErrorType appendLevel(uint8_t level, _Args&& ... args) {
                uint32_t bits = 0;
                for (uint8_t i = 0; i < level; i++) {
                    auto type = _appendRemove();
                    if (type == -1) {
                        return ErrorType::NOT_END_OF_OBJECT;
                    }
                    bits |= _fromChar(i, type);
                }
                _appendLevel();
                add(std::forward<_Args>(args) ...);
                int8_t i = level - 1;
                _setLast(_toCharClose(i, bits));
                while (--i >= 0) {
                    _output.print(_toCharClose(i, bits));
                }
                return ErrorType::NONE;
            }

        private:
            inline __attribute__((__always_inline__))
            void appendKey(const String &key) {
                __DBG_validatePointerCheck(key, VP_HS);
                _output.printf_P(PSTR("\"%s\":"), key.c_str());
            }

            inline __attribute__((__always_inline__))
            void appendKey(const char *key) {
                __DBG_validatePointerCheck(key, VP_HPS);
                _output.printf_P(PSTR("\"%s\":"), key);
            }

            inline __attribute__((__always_inline__))
            void appendKey(FStr key) {
                __DBG_validatePointerCheck(key, VP_HP);
                _output.printf_P(PSTR("\"%s\":"), reinterpret_cast<PGM_P>(key));
            }

            inline __attribute__((__always_inline__))
            void add() {
            }

            // inline __attribute__((__always_inline__))
            // void add(const UnnamedObjectWriter &writer) {
            //     _output.reserve(_output.length() + writer.length() + 1);
            //     writer.printTo(_output);
            //     _output.print(',');
            // }

            // inline __attribute__((__always_inline__))
            // void add(const UnnamedArrayWriter &writer) {
            //     _output.reserve(_output.length() + writer.length() + 1);
            //     writer.printTo(_output);
            //     _output.print(',');
            // }

            // void add(const NamedObject &writer);
            // void add(const NamedArray &writer);

            template<typename _Ta, typename std::enable_if<std::is_base_of<NamedObject, _Ta>::value, int>::type = 0>
            inline __attribute__((__always_inline__))
            void add(const _Ta &writer)
            {
                // __LDBG_printf("UnnamedObjectWriter::add(NamedObject)");
                _output.reserve(_output.length() + writer.length() + 1);
                writer.printTo(_output);
                _output.print(',');
            }

            template<typename _Ta, typename std::enable_if<std::is_base_of<NamedArray, _Ta>::value, int>::type = 0>
            inline __attribute__((__always_inline__))
            void add(const _Ta &writer)
            {
                // __LDBG_printf("UnnamedObjectWriter::add(NamedArray)");
                _output.reserve(_output.length() + writer.length() + 1);
                writer.printTo(_output);
                _output.print(',');
            }


            template<template<typename, typename> class _Ta, typename _Tb, typename _Tc, typename std::enable_if<!std::is_base_of<PrintToInterface, _Ta<_Tb, _Tc>>::value &&std::is_base_of<NamedBase, _Ta<_Tb, _Tc>>::value, int>::type = 0>
            void add(const _Ta<_Tb, _Tc> &container) {
                appendKey(container.key());
                appendValue(container.value());
                _output.print(',');
            }

            template<typename _Ta, typename std::enable_if<std::is_base_of<PrintToInterface, _Ta>::value && std::is_base_of<NamedBase, _Ta>::value, int>::type = 0>
            void add(const _Ta &container) {
                appendKey(container.key());
                container.printTo(_output);
                _output.print(',');
            }

            template<typename _Ta, typename std::enable_if<!std::is_base_of<PrintToInterface, _Ta>::value && std::is_base_of<NamedBase, _Ta>::value, int>::type = 0>
            void add(const _Ta &container) {
                appendKey(container.key());
                appendValue(container.value());
                _output.print(',');
            }

            template<typename _Ta, typename std::enable_if<
                std::is_same<const String &, _Ta>::value ||
                std::is_same<const char *, _Ta>::value ||
                std::is_same<char *, _Ta>::value ||
                std::is_same<const __FlashStringHelper *, _Ta>::value, int>::type = 0>
                void add(_Ta) {
                static_assert(!(
                    std::is_same<const String &, _Ta>::value ||
                    std::is_same<const char *, _Ta>::value ||
                    std::is_same<char *, _Ta>::value ||
                    std::is_same<const __FlashStringHelper *, _Ta>::value), "type not allowed");
            }

            int _debugLevel{0};

            template<typename _Ta, typename ... _Args>
            void add(const _Ta &arg, _Args&& ...args) {
                _debugLevel++;
                if (_debugLevel > 16) {
                    __DBG_printf("JSON recursion arg=%p", std::addressof(arg));
#if DEBUG && 0
                    // enable to trace
                    panic();
#endif
                    return;
                }
                add(arg);
                add(std::forward<_Args>(args)...);
                _debugLevel--;
            }
        };

        template<typename _Ta, typename _Tb>
        class JsonStringWriter {
        public:
            template<class ..._Args>
            JsonStringWriter(_Args&& ...args) :
                _writer(_Tb(), _output, std::forward<_Args>(args)...)
            {
            }
            ~JsonStringWriter() {
                _output.clear();
            }

            inline __attribute__((__always_inline__))
            JsonStringWriter &operator=(nullptr_t) {
                _output.clear();
            }

            inline __attribute__((__always_inline__))
            void clear() {
                _output.clear();
            }

            template<class ..._Args>
            void append(_Args&& ...args) {
                _writer.append(std::forward<_Args>(args)...);
            }

            template<typename _Ta1, class ..._Args, typename std::enable_if<std::is_integral<_Ta1>::value, int>::type = 0>
            ErrorType appendArray(_Ta1 level, _Args&& ...args) {
                return _writer.appendArray(static_cast<uint8_t>(level), std::forward<_Args>(args)...);
            }

            template<typename _Ta1, class ..._Args, typename std::enable_if<std::is_integral<_Ta1>::value, int>::type = 0>
            ErrorType appendObject(_Ta1 level, _Args&& ...args) {
                return _writer.appendObject(static_cast<uint8_t>(level), std::forward<_Args>(args)...);
            }

            inline __attribute__((__always_inline__))
            size_t length() const {
                return _writer.length();
            }

            inline __attribute__((__always_inline__))
            void printTo(Print &print) const {
                _writer.printTo(print);
            }

            inline __attribute__((__always_inline__))
            const String &toString() const {
                return _writer.toString();
            }

            inline __attribute__((__always_inline__))
            String &toString() {
                return _writer.toString();
            }

            inline __attribute__((__always_inline__))
            const char *c_str() const {
                return _writer.c_str();
            }

            inline __attribute__((__always_inline__))
            const PrintStringInterface &value() const {
                return _output;
            }

            inline __attribute__((__always_inline__))
            int findNamed(PGM_P name) const {
                return _writer.findNamed(name);
            }

            inline __attribute__((__always_inline__))
            int findNamed(FStr name) const {
                return _writer.findNamed(name);
            }

            inline __attribute__((__always_inline__))
            String valueAt(int pos) const {
                return _writer.valueAt(pos);
            }

        protected:
            friend RemoveOuterBase;

            PrintString _output;
            _Ta _writer;
        };

        class UnnamedArray : public JsonStringWriter<UnnamedArrayWriter, ArrayType> {
        public:
            template<typename ... _Args>
            UnnamedArray(_Args&& ...args) :
                JsonStringWriter<UnnamedArrayWriter, ArrayType>(std::forward<_Args>(args)...)
            {
            }

        protected:
            friend RemoveOuterBase;

            PrintString &_getOutput() {
                return _output;
            }

            const PrintString &_getOutput() const {
                return _output;
            }
        };

        class UnnamedObject : public JsonStringWriter<UnnamedObjectWriter, ObjectType> {
        public:
            template<typename ... _Args>
            UnnamedObject(_Args&& ...args) :
                JsonStringWriter<UnnamedObjectWriter, ObjectType>(std::forward<_Args>(args)...)
            {
            }

        protected:
            friend RemoveOuterBase;

            PrintString &_getOutput() {
                return _output;
            }

            const PrintString &_getOutput() const {
                return _output;
            }
        };

        class NamedArray : public UnnamedArray {
        public:
            template<typename ... _Args>
            NamedArray(FStr key, _Args&& ...args) :
                UnnamedArray(std::forward<_Args>(args)...),
                _key(key)
            {
            }

            inline __attribute__((__always_inline__))
            FStr key() {
                return _key;
            }

            inline __attribute__((__always_inline__))
            FStr key() const {
                return _key;
            }

            inline __attribute__((__always_inline__))
            void printTo(Print &print) const
            {
                print.printf_P(PSTR("\"%s\":"), reinterpret_cast<PGM_P>(_key));
                UnnamedArray::printTo(print);
            }

            inline __attribute__((__always_inline__))
            size_t length() const {
                return UnnamedArray::length() + strlen_P(reinterpret_cast<PGM_P>(_key)) + 3;
            }

        private:
            FStr _key;
        };

        class NamedObject : public UnnamedObject {
        public:
            template<typename ... _Args>
            NamedObject(FStr key, _Args&& ...args) :
                UnnamedObject(std::forward<_Args>(args)...),
                _key(key)
            {
            }

            inline __attribute__((__always_inline__))
            FStr key() {
                return _key;
            }

            inline __attribute__((__always_inline__))
            void printTo(Print &print) const
            {
                print.printf_P(PSTR("\"%s\":"), reinterpret_cast<PGM_P>(_key));
                UnnamedObject::printTo(print);
            }

            inline __attribute__((__always_inline__))
            size_t length() const {
                return UnnamedObject::length() + strlen_P(reinterpret_cast<PGM_P>(_key)) + 3;
            }

        private:
            FStr _key;
        };

        inline __attribute__((__always_inline__))
        void UnnamedArrayWriter::add(const UnnamedObject &writer)
        {
            _output.reserve(_output.length() + writer.length() + 1);
            writer.printTo(_output);
            _output.print(',');
        }

        inline __attribute__((__always_inline__))
        void UnnamedArrayWriter::add(const UnnamedArray &writer)
        {
            _output.reserve(_output.length() + writer.length() + 1);
            writer.printTo(_output);
            _output.print(',');
        }

        // inline __attribute__((__always_inline__))
        // void UnnamedObjectWriter::add(const NamedObject &writer)
        // {
        //     __LDBG_printf("UnnamedObjectWriter::add(NamedObject)");
        //     _output.reserve(_output.length() + writer.length() + 1);
        //     writer.printTo(_output);
        //     _output.print(',');
        // }

        // inline __attribute__((__always_inline__))
        // void UnnamedObjectWriter::add(const NamedArray &writer)
        // {
        //     __LDBG_printf("UnnamedObjectWriter::add(NamedArray)");
        //     _output.reserve(_output.length() + writer.length() + 1);
        //     writer.printTo(_output);
        //     _output.print(',');
        // }

        // append values of an array/object to an array/object
        class RemoveOuterBase : public PrintToInterface, public  UnnamedBase {
        public:
            void printTo(PrintStringInterface &output) const {
                output.print(_value);
            }

            const String &toString() const {
                return _value;
            }

        protected:
            RemoveOuterBase(const String &value) {
                _value.concat(value.c_str() + 1, value.length() - 2);
                //_value.remove(_value.length() - 1, 1);
            }

            String _value;
        };

        // append values of an array to an array
        class RemoveOuterArray : public RemoveOuterBase {
        public:
            inline RemoveOuterArray(const UnnamedArray &arr) : RemoveOuterBase(arr.toString()) {
            }
        };

        // append keys/values of an object to an object
        class RemoveOuterObject : public RemoveOuterBase {
        public:
            inline RemoveOuterObject(const UnnamedObject &obj) : RemoveOuterBase(obj.toString()) {
            }
        };

    }

}

#if MQTT_JSON_WRITER_DEBUG
#include <debug_helper_disable.h>
#endif
