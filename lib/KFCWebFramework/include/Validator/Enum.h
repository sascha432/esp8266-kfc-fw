/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <stl_ext/type_traits.h>
#include "BaseValidator.h"
#include <array>
#include <cstring>

namespace FormUI {

    namespace Validator {

        template <typename _Ta, size_t N>
        class Enum : public BaseValidator {
        public:
            using ArrayType = std::array<_Ta, N>;
            using EnumType = _Ta;
            using IntType = std::relaxed_underlying_type_t<_Ta>;

            Enum(ArrayType values) : 
                BaseValidator(FSPGM(FormEnumValidator_default_message)), 
                _values(values) 
            {
            }

            Enum(const String &message, ArrayType values) : 
                BaseValidator(message), 
                _values(values) 
            {
            }

            virtual bool validate() override {
                if (BaseValidator::validate()) {
                    auto tmp = static_cast<EnumType>(getField().getValue().toInt());
                    return std::find(_values.begin(), _values.end(), tmp) != _values.end();
                }
                return false;
            }

            virtual String getMessage() override
            {
                String message = BaseValidator::getMessage();
                if (strstr_P(message.c_str(), PSTR("%allowed%"))) {
                    String allowed = String('[');
                    auto last = _values.size();
                    for(auto value: _values) {
                        allowed += String(static_cast<IntType>(value));
                        if (--last != 0) {
                            allowed += '|';
                        }
                   }
                    allowed += ']';
                    message.replace(F("%allowed%"), allowed);
                }
                return message;
            }

        private:
            ArrayType _values;
        };

    }
}

