/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <JsonBaseReader.h>
#include <PrintString.h>

#include "StreamString.h"

#define DEBUG_MQTT_JSON 1

#if DEBUG_MQTT_JSON
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

namespace MQTT {

    namespace Json {

        struct RGB {
            float x;
            float y;
            float h;
            float s;
            uint8_t r;
            uint8_t g;
            uint8_t b;
            union {
                struct {
                    bool has_r: 1;
                    bool has_g: 1;
                    bool has_b: 1;
                    bool has_x: 1;
                    bool has_y: 1;
                    bool has_h: 1;
                    bool has_s: 1;
                };
                uint8_t _flags;
            };

            RGB() : _flags(0) {}

            bool hasRGB() const {
                return has_r && has_g && has_b;
            }
            bool hasXY() const {
                return has_x && has_y;
            }
            bool hasHS() const {
                return has_h && has_s;
            }

            String toString() const {
                if (hasRGB()) {
                    return PrintString(F("#%02x%02x%02x"), static_cast<unsigned>(r), static_cast<unsigned>(g), static_cast<unsigned>(b));
                }
                return String();
            }
        };

        class Reader : public ::JsonBaseReader {
        public:
            Reader();
            Reader(Stream* stream);

            virtual bool processElement();

            virtual bool recoverableError(JsonErrorEnum_t type) {
                // String err;
                // error(err, type);
                // Serial.printf("ERROR %u: %s", type, err.c_str());
                return true;
            }

            void clear() {
                brightness = -1;
                color_temp = -1;
                color = RGB();
                state = -1;
                transition = NAN;
                white_value = -1;
                effect = String();
            }

        public:
            int32_t brightness;
            int32_t color_temp;
            RGB color;
            int8_t state;
            float transition;
            int32_t white_value;
            String effect;
        };

        template<typename _Ta>
        struct UnnamedValue {
            UnnamedValue(_Ta value) : _value(value) {}
            _Ta _value;
        };

        template<typename _Ta, typename _Tb>
        struct NamedValue : public UnnamedValue<_Tb> {
            NamedValue(_Ta key, _Tb value) : UnnamedValue<_Tb>(value), _key(key) {}
            _Ta _key;
        };

        struct FormattedDouble {
            static constexpr uint8_t DEFAULT = 6;
            static constexpr uint8_t BITS_TRIMMED = 0x80;
            static constexpr uint8_t BITS_MASK = BITS_TRIMMED;
            static constexpr uint8_t TRIMMED(uint8_t precision) {
                return precision | BITS_TRIMMED;
            }
            // preicison = number of decimal places to print
            // to trim trailing zeros, use FormattedDouble::TRIMMED(precision) or simply negative values
            // -3 will round to 3 digits and remove all trailing zeros expect for one
            FormattedDouble(double value, uint8_t precision = DEFAULT) : _value(value), _precision(precision) {}
            void printTo(PrintString &output) const {
                output.print(_value, _precision & ~BITS_MASK, _precision & BITS_TRIMMED);
            }
            double _value;
            uint8_t _precision;
        };

        class JsonUnnamedObjectWriter;
        class JsonUnnamedArrayWriter;

        using FStr = const __FlashStringHelper *;

        using Short = UnnamedValue<int16_t>;
        using UnsignedShort = UnnamedValue<uint16_t>;
        using Integer = UnnamedValue<int32_t>;
        using Unsigned = UnnamedValue<uint32_t>;
        using Int64 = UnnamedValue<int64_t>;
        using Uint64 = UnnamedValue<uint64_t>;
        using FString = UnnamedValue<FStr>;
        using CString = UnnamedValue<const char *>;
        using Object = UnnamedValue<const JsonUnnamedObjectWriter &>;
        using Array = UnnamedValue<const JsonUnnamedArrayWriter &>;

        using NamedShort = NamedValue<FStr, int16_t>;
        using NamedUnsignedShort = NamedValue<FStr, uint16_t>;
        using NamedInt32 = NamedValue<FStr, int32_t>;
        using NamedUint32 = NamedValue<FStr, uint32_t>;
        using NamedInt = NamedInt32;
        using NamedUnsigned = NamedUint32;
        using NamedInt64 = NamedValue<FStr, int64_t>;
        using NamedUint64 = NamedValue<FStr, uint64_t>;
        using NamedDouble = NamedValue<FStr, FormattedDouble>;
        using NamedFString = NamedValue<FStr, FStr>;
        using NamedCString = NamedValue<FStr, const char *>;
        using NamedObject = NamedValue<FStr, const JsonUnnamedObjectWriter &>;
        using NamedArray = NamedValue<FStr, const JsonUnnamedArrayWriter &>;

        struct NamedBool : NamedValue<FStr, FStr> {
            NamedBool(FStr key, bool value, FStr strTrue = FSPGM(true), FStr strFalse = FSPGM(false)) : NamedValue(key, value ? strTrue : strFalse) {}
        };

        struct Brightness : NamedUnsignedShort {
            Brightness(uint16_t brightness) : NamedUnsignedShort(F("brightness"), brightness) {}
        };

        struct ColorTemperature : NamedUnsignedShort {
            ColorTemperature(uint16_t temp) : NamedUnsignedShort(F("color_temp"), temp) {}
        };

        struct State : NamedValue<FStr, FStr> {
            State(bool value) : NamedValue(F("state"), value ? F("ON") : F("OFF")) {}
        };

        struct Effect : NamedValue<FStr, FStr> {
            Effect(FStr effect) : NamedValue(F("effect"), effect) {}
        };

        struct EffectString : NamedValue<FStr, String> {
            EffectString(const String &effect) : NamedValue(F("effect"), effect) {}
            EffectString(const char *effect) : NamedValue(F("effect"), effect) {}
        };

        struct Transition : NamedValue<FStr, FormattedDouble &>, FormattedDouble {
            Transition(float transition, uint8_t precision = 0) : NamedValue<FStr, FormattedDouble &>(F("transition"), *this), FormattedDouble(transition, precision) {}
        };

        // struct RGB :

        class JsonUnnamedArrayWriter {
        public:
            struct ObjectType {};

            template<class ...Args>
            JsonUnnamedArrayWriter(Args&& ... args) : _output(F("[]")) {
                add(std::forward<Args>(args) ...);
            }

            void printTo(Print &print) const {
                print.print(_output);
            }

        protected:
            JsonUnnamedArrayWriter(ObjectType) : _output(F("{}")) {}

            inline const char *c_str() const {
                return _output.c_str();
            }

            inline const String &toString() const {
                return _output;
            }

        protected:
            inline void appendValue(const char *value) {
                _output.printf_P(PSTR("\"%s\""), value);
            }

            inline void appendValue(FStr value) {
                _output.printf_P(PSTR("\"%s\""), value);
            }

            inline void appendValue(const String &value) {
                _output.printf_P(PSTR("\"%s\""), value.c_str());
            }

            void appendValue(const JsonUnnamedObjectWriter &value);

            // avoid using Printable
            // this method is just to note that printTo does not require a virtual class
            //
            // void printTo(Print &print);
            // void printTo(PrintString &print);
            inline void appendValue(const Printable &value) {
                value.printTo(_output);
            }

            template<typename _Ta>
            inline void appendValue(const _Ta &value) {
                value.printTo(_output);
            }

            template<typename _Ta>
            void appendValue(_Ta key) {
                _output += key;
            }

            inline void prepareAppend() {
                auto len = _output.length();
                if (len == 2) {
                    _output.remove(1, 1);
                } else {
                    _output.setCharAt(len - 1, ',');
                }
            }

        private:
            inline void add() {
                __LDBG_printf("add()");
            }

            template<template<typename> class _Ta, typename _Tb>
            void add(_Ta<_Tb> value) {
                __LDBG_printf("add(arg)");
                prepareAppend();
                appendValue(value);
            }

            template<typename _Ta, typename ... _Args>
            void add(_Ta &&arg, _Args&& ...args) {
                __LDBG_printf("add(arg, ...)");
                add(arg);
                add(std::forward<_Args>(args)...);
            }

        protected:
            PrintString _output;
        };

        class JsonUnnamedObjectWriter : protected JsonUnnamedArrayWriter {
        public:

            template<class ...Args>
            JsonUnnamedObjectWriter(Args&& ... args) : JsonUnnamedArrayWriter(JsonUnnamedArrayWriter::ObjectType()) {
                add(std::forward<Args>(args) ...);
            }

            inline const char *c_str() const {
                return _output.c_str();
            }

            inline const String &toString() const {
                return _output;
            }

            inline void printTo(Print &print) const {
                print.print(_output);
            }

        private:
            inline void add() {
                __LDBG_printf("add()");
            }

            inline void appendKey(const String &key) {
                _output.printf_P(PSTR("\"%s\":"), key.c_str());
            }

            inline void appendKey(const char *key) {
                _output.printf_P(PSTR("\"%s\":"), key);
            }

            inline void appendKey(FStr key) {
                _output.printf_P(PSTR("\"%s\":"), key);
            }

            template<template<typename, typename> class _Ta, typename _Tb, typename _Tc>
            void add(_Ta<_Tb, _Tc> container) {
                __LDBG_printf("add container %s=%s", __S(container._key), __S(container._value));
                prepareAppend();
                appendKey(container._key);
                appendValue(container._value);
            }

            template<typename _Ta, typename ... _Args>
            void add(_Ta &&arg, _Args&& ...args) {
                __LDBG_printf("add(arg, ...)");
                add(arg);
                add(std::forward<_Args>(args)...);
            }
        };

        using Writer = JsonUnnamedObjectWriter;

        inline void JsonUnnamedArrayWriter::appendValue(const JsonUnnamedObjectWriter &value)
        {
            value.printTo(_output);
        }

    }

}

#include <debug_helper_disable.h>
