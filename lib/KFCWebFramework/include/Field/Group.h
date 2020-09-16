/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <vector>
#include "Field/BaseField.h"

#include "Utility/Debug.h"

namespace FormUI {

    class Group : public Field::BaseField {
    public:
        Group(const Group &group) = delete;
        Group &operator=(const Group &group) = delete;

        Group(const String &name, bool expanded) : Field::BaseField(name, String(), Field::Type::GROUP) {
            _expanded = expanded;
        }
        virtual bool setValue(const String &value) override {
            return false;
        }
        virtual void copyValue() override {
        }
        Form::BaseForm &end();

        bool isExpanded() const {
            return _expanded;
        }
        void setExpanded(bool expanded) {
            _expanded = expanded;
        }
    };

}

#include <debug_helper_disable.h>
