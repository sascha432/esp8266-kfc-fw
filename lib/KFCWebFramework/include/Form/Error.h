/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "Types.h"

namespace FormUI {

    namespace Form {

        class Error {
        public:
            using Vector = std::vector<Error>;

            Error() {
                _field = nullptr;
            }

            Error(const String & message) {
                _field = nullptr;
                _message = message;
            }

            Error(Field::BaseField * field, const String & message) {
                _field = field;
                _message = message;
            }

            const String &getName() const;
            const String &getMessage() const;
            const bool is(Field::BaseField &field) const;

        private:
            Field::BaseField *_field;
            String _message;
        };

    }

}
