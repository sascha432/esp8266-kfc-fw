/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "Validator/BaseValidator.h"

#include "Utility/Debug.h"

namespace FormUI {

    namespace Validator {

        class Length : public BaseValidator {
        public:
            Length(size_t aMin, size_t aMax, bool allowEmpty = false) :
                BaseValidator(FSPGM(FormLengthValidator_default_message)),
                _minValue(aMin),
                _maxValue(aMax),
                _allowEmpty(allowEmpty)
            {}

            Length(const String &message, size_t aMin, size_t aMax, bool allowEmpty = false) :
                BaseValidator(message.length() == 0 ? String(FSPGM(FormLengthValidator_default_message)) : message),
                _minValue(aMin),
                _maxValue(aMax),
                _allowEmpty(allowEmpty)
            {}

            virtual bool validate() override;
            virtual String getMessage() override;

        private:
            size_t _minValue;
            size_t _maxValue;
            bool _allowEmpty;
        };

    }

}

#include <debug_helper_disable.h>
