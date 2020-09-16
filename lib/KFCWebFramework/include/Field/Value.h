/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <type_traits>
#include <stl_ext/type_traits.h>
#include <limits>
#include <misc.h>
#include "Field/BaseField.h"

#include "Utility/Debug.h"

namespace FormUI {

    namespace Field {

        template <class _Ta>
        class ValueTemplate : public BaseField
        {
        public:
            using _VarType = typename::std::remove_pointer<typename::std::remove_cv<typename std::remove_reference<_Ta>::type>::type>::type;
            using VarType = typename std::relaxed_underlying_type<_VarType>::type;

            ValueTemplate(const String &name, const String &value = String(), Type type = Type::NONE) : BaseField(name, value, type) {}
            ValueTemplate(const String &name, Type type = Type::NONE) : BaseField(name, String(), type) {}

        protected:
            // Setter
            void _stringToValue(String &value, const String &str) const {
                value = str;
            }

            void _stringToValue(IPAddress &value, const String &str) const {
                value = (uint32_t)0;
                if (!value.fromString(str)) {
                    value = (uint32_t)0;
                }
            }

            void _stringToValue(double &value, const String &str) const {
                value = str.toFloat();
            }

            void _stringToValue(float &value, const String &str) const {
                value = str.toFloat();
            }

            void _stringToValue(unsigned long &value, const String &str) const {
                value = str.toInt();
            }

            void _stringToValue(long &value, const String &str) const {
                value = str.toInt();
            }

            void _stringToValue(uint32_t &value, const String &str) const {
                value = str.toInt();
            }

            void _stringToValue(uint16_t &value, const String &str) const {
                value = (uint16_t)str.toInt();
            }

            void _stringToValue(uint8_t &value, const String &str) const {
                value = (uint8_t)str.toInt();
            }

            void _stringToValue(int32_t &value, const String &str) const {
                value = str.toInt();
            }

            void _stringToValue(int16_t &value, const String &str) const {
                value = (int16_t)str.toInt();
            }

            void _stringToValue(int8_t &value, const String &str) const {
                value = (int8_t)str.toInt();
            }

            void _stringToValue(bool &value, const String &str) const {
                value = (bool)str.toInt();
            }

        protected:
            // Getter

            String _valueToString(const String &str) const {
                return str;
            }

            String _valueToString(const IPAddress &address) const {
                return address.toString();
            }

            String _valueToString(float value) const {
                return _valueToString((double)value, std::numeric_limits<float>::digits10);
            }

            String _valueToString(double value, int8_t digits = std::numeric_limits<double>::digits10) const {
                // calculate maximum precision and strip trailing zeros
                uint64_t tmp = (uint64_t)(value < 0 ? -value : value);
                while (tmp > 0) {
                    digits--;
                    tmp /= 10;
                }
                if (digits > 0) {
                    String str = String(value, digits);
                    while (str.charAt(str.length() - 1) == '0') {
                        str.remove(str.length() - 1, 1);
                    }
                    if (str.charAt(str.length() - 1) == '.') {
                        str += '0';
                    }
                    return str;
                }
                return String(value);
            }

            String _valueToString(unsigned long value) const {
                return String(value);
            }

            String _valueToString(long value) const {
                return String(value);
            }

            String _valueToString(uint32_t value) const {
                return String(value);
            }

            String _valueToString(int32_t value) const {
                return String(value);
            }

            String _valueToString(uint16_t value) const {
                return String(value);
            }

            String _valueToString(int16_t value) const {
                return String(value);
            }

            String _valueToString(uint8_t value) const {
                return String(value);
            }

            String _valueToString(int8_t value) const {
                return String(value);
            }

            String _valueToString(bool value) const {
                return String(value);
            }
        };


    }

}

#include <debug_helper_disable.h>
