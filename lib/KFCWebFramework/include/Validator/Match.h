/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "Validator/BaseValidator.h"

#include "Utility/Debug.h"

namespace FormUI {

    namespace Validator {

        class Match : public BaseValidator {
        public:
            using Callback = std::function<bool(Field::BaseField &field)>;

            Match(const String &message, Callback callback) :
                BaseValidator(message),
                _callback(callback)
            {
            }

            virtual bool validate() override {
                if (BaseValidator::validate()) {
                    return _callback(getField());
                }
                return false;
            }

        private:
            Callback _callback;
        };

    }

}

#include <debug_helper_disable.h>
