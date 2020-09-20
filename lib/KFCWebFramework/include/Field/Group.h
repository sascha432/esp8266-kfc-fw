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

        Group(const char *name, bool expanded) : Field::BaseField(name, String(), Field::Type::GROUP) {
            _expanded = expanded;
            _groupOpen = true;
        }

        // groups do not have values
        virtual bool setValue(const String &value) override {
            return false;
        }

        virtual void copyValue() override {
        }

        Form::BaseForm &end();

        inline bool isExpanded() const {
            return _expanded;
        }

        inline void setExpanded(bool expanded) {
            _expanded = expanded;
        }

        inline bool isOpen() const {
            return _groupOpen;
        }
    };

}

#include <debug_helper_disable.h>
