/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>

class FormValidHostOrIpValidator : public FormValidator {
public:
    typedef std::vector<String> StringVector;
    static constexpr uint8_t ALLOW_HOST_IP = 0x00;
    static constexpr uint8_t ALLOW_EMPTY = 0x01;
    static constexpr uint8_t ALLOW_ZEROCONF = 0x02;

    FormValidHostOrIpValidator(uint8_t allowedTypes = ALLOW_HOST_IP) : FormValidHostOrIpValidator(FSPGM(FormValidHostOrIpValidator_default_message), allowedTypes) {
    }
    FormValidHostOrIpValidator(const String &message, uint8_t allowedTypes = ALLOW_HOST_IP) : FormValidator(message), _allowedTypes(allowedTypes) {
    }

    FormValidHostOrIpValidator *addAllowString(const String &str) {
        _allowStrings.push_back(str);
        return this;
    }

    virtual bool validate() override {
        if (FormValidator::validate()) {
            const char *ptr = getField().getValue().c_str();
            if (_allowedTypes & ALLOW_ZEROCONF) {
                if (strstr_P(ptr, PSTR("${zeroconf"))) { //TODO parse and validate zeroconf
                    return true;
                }
            }
            if (_allowedTypes & ALLOW_EMPTY) {
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
    uint8_t _allowedTypes;
    StringVector _allowStrings;
};
