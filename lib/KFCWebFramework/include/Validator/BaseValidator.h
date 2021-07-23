/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include "Types.h"
#include "Field/BaseField.h"
#include "Utility/ForwardList.h"

#include "Utility/Debug.h"

namespace FormUI {

    namespace Validator {

        class BaseValidator : public Utility::ForwardList<BaseValidator> {
        public:
            BaseValidator();
            BaseValidator(const String &message);

            // if message is empty, default message will be used
            BaseValidator(const String &message, const __FlashStringHelper *defaultMessage);
            virtual ~BaseValidator() {}

            void setField(Field::BaseField *field);
            Field::BaseField &getField();

            void setMessage(const String &message);
            virtual String getMessage();
            virtual bool validate();

            bool operator==(const Field::BaseField *field) const {
                return _field == field;
            }

        private:
            Field::BaseField *_field;
            String _message;

        private:
            friend Form::BaseForm;
        };

    }
}

#include <debug_helper_disable.h>
