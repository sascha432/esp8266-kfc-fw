/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>

class FormValidHostOrIpValidator : public FormValidator {
public:
    FormValidHostOrIpValidator(bool allowEmpty = false) : FormValidHostOrIpValidator(FPSTR(FormValidHostOrIpValidator_default_message), allowEmpty) {
    }
    FormValidHostOrIpValidator(const String &message, bool allowEmpty = false) : FormValidator(message) {
        _allowEmpty = allowEmpty;
    }

    virtual bool validate() override {
        if (FormValidator::validate()) {
            const char *ptr = getField()->getValue().c_str();
            if (_allowEmpty) {
                const char *trimmed = ptr;
                while (isspace(*trimmed)) {
                    trimmed++;
                }
                if (!*trimmed) {
                    return true;
                }
            }
            IPAddress addr;
            if (!addr.fromString(getField()->getValue())) {
                while(*ptr) {
                    if (!((*ptr >= 'A' && *ptr <= 'Z') || (*ptr >= 'a' && *ptr <= 'z') || (*ptr >= '0' && *ptr <= '9') || *ptr == '_' || *ptr == '-' || *ptr == '.')) {
                        return false;
                    }
                    ptr++;
                }
            }
            return true;
        }
        return false;
    }

private:
    bool _allowEmpty;
};
