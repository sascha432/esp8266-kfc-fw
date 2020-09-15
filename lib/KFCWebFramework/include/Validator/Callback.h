/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <type_traits>
#include "Validator/BaseValidator.h"

namespace FormUI {

    namespace Validator {

        template <typename _Ta>
        class CallbackTemplate : public BaseValidator {
        public:
            using Callback = std::function<bool(const _Ta &value, Field::BaseField &field)>;

            CallbackTemplate(Callback callback, const String &message = String()) :
                BaseValidator(message),
                _callback(callback)
            {
            }

            virtual bool validate() override {
                return BaseValidator::validate() && _validate<_Ta>(getField());
            }

        private:
            template<typename _Tb, typename std::enable_if<std::is_floating_point<_Tb>::value, int>::type = 0>
            bool _validate(Field::BaseField &field) {
                return _callback(static_cast<_Tb>(field.getValue().toFloat()), field);
            }

            template<typename _Tb, typename std::enable_if<std::is_integral<_Tb>::value, int>::type = 0>
            bool _validate(Field::BaseField &field) {
                return _callback(static_cast<_Tb>(field.getValue().toInt()), field);
            }

            template<typename _Tb, typename std::enable_if<std::is_same<_Tb, String>::value, int>::type = 0>
            bool _validate(Field::BaseField &field) {
                return _callback(field.getValue(), field);
            }

            template<typename _Tb, typename std::enable_if<std::is_same<_Tb, const char *>::value, int>::type = 0>
            bool _validate(Field::BaseField &field) {
                return _callback(field.getValue().c_str(), field);
            }


        private:
            Callback _callback;
        };

    }

}
