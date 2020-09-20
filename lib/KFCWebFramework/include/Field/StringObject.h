/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "Field/Object.h"

#include "Utility/Debug.h"

namespace FormUI {

    namespace Field {

        class StringObject : public Object<String, Field::AssignCastAdapter<String>> {
        public:
            StringObject(const char *name, String &str, FormUI::InputFieldType type = FormUI::InputFieldType::TEXT) :
                Object(name, str, type)
            {
            }
        };

    }

}

#include <debug_helper_disable.h>
