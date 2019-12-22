/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>

class FormValidHostOrIpValidator : public FormValidator {
public:
    typedef std::vector<String> StringVector;

    FormValidHostOrIpValidator(bool allowEmpty = false) : FormValidHostOrIpValidator(FSPGM(FormValidHostOrIpValidator_default_message), allowEmpty) {
    }
    FormValidHostOrIpValidator(const String &message, bool allowEmpty = false) : FormValidator(message) {
        _allowEmpty = allowEmpty;
    }

    FormValidHostOrIpValidator *addAllowString(const String &str) {
        _allowStrings.push_back(str);
        return this;
    }

    virtual bool validate() override {
        if (FormValidator::validate()) {
            const char *ptr = getField().getValue().c_str();
            if (_allowEmpty) {
                const char *trimmed = ptr;
                while (isspace(*trimmed)) {
                    trimmed++;
                }
                if (!*trimmed) {
                    return true;
                }
            }
            for(const auto &str: _allowStrings) {
                if (str.equals(ptr)) {
                    return true;
                }
            }
            IPAddress addr;
            if (!addr.fromString(getField().getValue())) {
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
    StringVector _allowStrings;
};
