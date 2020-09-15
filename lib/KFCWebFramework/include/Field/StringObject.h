/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "Field/Object.h"

namespace FormUI {

    namespace Field {

        class StringObject : public Object<String, Field::AssignCastAdapter<String>> {
        public:
            StringObject(const String &name, String &str, Type type = Type::TEXT) :
                Object(name, str, type)
            {
            }
        };

    }

}
