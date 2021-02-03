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
#include "Field/Value.h"

#include "Utility/Debug.h"

namespace FormUI {

    namespace Field {

        template <typename Ta>
        class ValueCallback : public ValueTemplate<Ta>
        {
        public:
            using _VarType = typename ValueTemplate<Ta>::_VarType;
            using VarType = typename ValueTemplate<Ta>::VarType;
            using GetterSetterCallback = std::function<bool(_VarType &value, Field::BaseField &field, bool store)>;
            using Callback = GetterSetterCallback;
            using SetterCallback = std::function<void(_VarType &value, Field::BaseField &field)>;
            using ValueTemplate<Ta>::_stringToValue;
            using ValueTemplate<Ta>::_valueToString;

        protected:
            GetterSetterCallback _callback;

        public:
            ValueCallback(const char *name, const VarType &value, GetterSetterCallback callback, Type type = Type::SELECT) :
                ValueTemplate<Ta>(name, type),
                _callback(callback),
                _insideCallback(false)
            {
                _initValue(static_cast<VarType>(value));
            }

            ValueCallback(const char *name, const _VarType &value, SetterCallback callback, Type type = Type::SELECT) :
                ValueTemplate<Ta>(name, type),
                _callback([this, callback](_VarType &value, Field::BaseField &field, bool store) {
                    _insideCallback++;
                    if (store) {
                        callback(value, field);
                    }
                    _insideCallback--;
                    return false;
                }),
                _insideCallback(false)
            {
                _initValue(static_cast<VarType>(value));
            }

            ValueCallback(const char *name, GetterSetterCallback callback, Type type = Type::SELECT) :
                ValueTemplate<Ta>(name, type),
                _callback(callback),
                _insideCallback(false)
            {
                _initValue();
            }

        #ifndef _MSC_VER
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wstrict-aliasing"
        #endif

            virtual void copyValue() override {
                VarType tmp;
                _stringToValue(tmp, Field::BaseField::getValue());
                _insideCallback++;
                _callback(reinterpret_cast<_VarType &>(tmp), *this, true);
                _insideCallback--;
            }

            virtual bool setValue(const String &value) override {
                VarType tmp;
                _stringToValue(tmp, value);
                if (_insideCallback == 0) {
                    if (!(_callback(reinterpret_cast<_VarType &>(tmp), *this, true) && _callback(reinterpret_cast<_VarType &>(tmp), *this, false))) { // convert from user input and back to compare values
                        // callback failed
                        _stringToValue(tmp, value);
                    }
                }
                return Field::BaseField::setValue(_valueToString(tmp));
            }

        protected:
            void _initValue(VarType value = VarType())
            {
                VarType tmp;
                _insideCallback++;
                if (_callback(reinterpret_cast<_VarType &>(tmp), *this, false)) { // we have a callback, check if it supports getting the value
                    // replace on success
                    value = tmp;
                }
                _insideCallback--;
                Field::BaseField::initValue(_valueToString(value));
            }
            uint8_t _insideCallback;
        };

#ifndef _MSC_VER
#pragma GCC diagnostic pop
#endif


    }

}

#include <debug_helper_disable.h>
