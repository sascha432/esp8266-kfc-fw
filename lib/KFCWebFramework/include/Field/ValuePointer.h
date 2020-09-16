/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <stl_ext/type_traits.h>
#include <limits>
#include <misc.h>
#include "Field/Value.h"

#include "Utility/Debug.h"

namespace FormUI {

    namespace Field {

        template <typename Ta>
        class ValuePointer : public ValueTemplate<Ta> {
        public:
            using _VarType = typename ValueTemplate<Ta>::_VarType;
            using VarType = typename ValueTemplate<Ta>::VarType;
            using ValueTemplate<Ta>::_stringToValue;
            using ValueTemplate<Ta>::_valueToString;

        protected:
            VarType *__value;

        public:
            ValuePointer(const String &name, VarType *value, Type type = Type::SELECT) : ValueTemplate<Ta>(name, type), __value(_initValue(value)) {
            }
            ValuePointer(const String &name, VarType &value, Type type = Type::SELECT) : ValueTemplate<Ta>(name, type), __value(_initValue(&value)) {
            }

            virtual void copyValue() override  {
                _stringToValue(*__value, Field::BaseField::getValue());
            }

            virtual bool setValue(const String &value) override {
                VarType tmp;
                _stringToValue(tmp, value);
                return Field::BaseField::setValue(_valueToString(tmp));
            }

            VarType getValue() const {
                VarType tmp;
                _stringToValue(tmp, Field::BaseField::getValue());
                return tmp;
            }

            // these modify the value that was passed to FormValue, not the value of the form
            void _setValue(VarType value) {
                *__value = value;
            }

            const VarType &_getValue() const {
                return *__value;
            }

        protected:
            VarType *_initValue(VarType *value) {
                Field::BaseField::initValue(_valueToString(*value));
                return value;
            }
        };

    }

}

#include <debug_helper_disable.h>
