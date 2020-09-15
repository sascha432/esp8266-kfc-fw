/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "Field/BaseField.h"

namespace FormUI {

    namespace Field {

        // _Ta = Type
        // _Tb = object to string cast
        // _Tc = string to object cast
        template<typename _Ta, typename _Tb = String, typename _Tc = _Tb>
        class AssignCastAdapter {
        public:
            AssignCastAdapter(_Ta &object) : _object(object) {}

            String toString() const {
                return static_cast<_Tb>(_object);
            }
            bool fromString(const String &str) {
                _object = static_cast<_Tc>(str);
                return true;
            }
        protected:
            _Ta &_object;
        };

        // supports any object with toString()/fromString() method. i.e. IPAddress
        // alternative:
        // Object<String, AssignCastAdapter<String>>(...)

        template <typename _Ta, typename _Tb = typename std::add_lvalue_reference<_Ta>::type>
        class Object : public BaseField {
        public:
            Object(const String &name, _Ta &obj, Type type = Type::TEXT) :
                BaseField(name, String(), type),
                _converter(obj)
            {
                initValue(_converter.toString());
            }

            Object(const String &name, _Ta *obj, Type type = Type::TEXT) :
                BaseField(name, String(), type),
                _converter(*obj)
            {
                initValue(_converter.toString());
            }

            virtual void copyValue() {
                if (!_converter.fromString(Field::BaseField::getValue())) {
                    return;
                }
                Field::BaseField::setValue(_converter.toString());
            }

        protected:
            _Tb _converter;
        };

    }

}

