/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <EnumHelper.h>

class FormHostValidator : public FormValidator {
public:
    typedef std::vector<String> StringVector;

    enum class AllowedType : uint8_t {
        ALLOW_HOST_OR_IP =             0x00,
        ALLOW_EMPTY =                  0x01,
        ALLOW_ZEROCONF =               0x02,
        ALLOW_EMPTY_AND_ZEROCONF =     ALLOW_EMPTY|ALLOW_ZEROCONF
    };

    FormHostValidator(AllowedType allowedTypes = AllowedType::ALLOW_HOST_OR_IP) : FormHostValidator(FSPGM(FormHostValidator_default_message), allowedTypes) {
    }
    FormHostValidator(const String &message, AllowedType allowedTypes = AllowedType::ALLOW_HOST_OR_IP) : FormValidator(message), _allowedTypes(allowedTypes) {
    }

    FormHostValidator *addAllowString(const String &str) {
        _allowStrings.push_back(str);
        return this;
    }

    virtual bool validate() override {
        if (FormValidator::validate()) {
            const char *ptr = getField().getValue().c_str();
            if (EnumHelper::Bitset::has(_allowedTypes, AllowedType::ALLOW_ZEROCONF)) {
                if (strstr_P(ptr, PSTR("${zeroconf"))) { //TODO parse and validate zeroconf
                    return true;
                }
            }
            if (EnumHelper::Bitset::has(_allowedTypes, AllowedType::ALLOW_EMPTY)) {
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
                    if (!(isalnum(*ptr) || *ptr == '_' || *ptr == '-' || *ptr == '.')) {
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
    AllowedType _allowedTypes;
    StringVector _allowStrings;
};
