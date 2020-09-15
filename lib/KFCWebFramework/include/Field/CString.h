/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "Field/BaseField.h"

namespace FormUI {

    namespace Field {

        class CString : public BaseField {
        public:
            CString(const String &name, char *value, size_t maxSize, Field::Type type = Field::Type::TEXT) : 
                BaseField(name, value, type), _value(value), _size(maxSize)
            {
                if (BaseField::_value.length() >= maxSize) {
                    BaseField::_value.remove(maxSize - 1);
                }
            }

            virtual void copyValue() {
                strncpy(_value, getValue().c_str(), _size - 1)[_size - 1] = 0;
            }

        private:
            char *_value;
            size_t _size;
        };

    }

}
