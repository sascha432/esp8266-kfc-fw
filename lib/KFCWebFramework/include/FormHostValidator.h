/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <EnumHelper.h>

class FormHostValidator : public FormValidator {
public:
    enum class AllowedType : uint8_t {
        ALLOW_HOST_OR_IP =             0x00,
        ALLOW_EMPTY =                  0x01,
        ALLOW_ZEROCONF =               0x02,
        ALLOW_EMPTY_AND_ZEROCONF =     ALLOW_EMPTY|ALLOW_ZEROCONF
    };

    FormHostValidator(AllowedType allowedTypes = AllowedType::ALLOW_HOST_OR_IP) : FormHostValidator(FSPGM(FormHostValidator_default_message), allowedTypes) {
    }
    FormHostValidator(const String &message, AllowedType allowedTypes = AllowedType::ALLOW_HOST_OR_IP) : FormValidator(message.length() == 0 ? String(FSPGM(FormHostValidator_default_message)) : message), _allowedTypes(allowedTypes) {
    }

    virtual bool validate() override {
        if (FormValidator::validate()) {
            auto tmpStr = getField().getValue();
            tmpStr.trim();
            getField().setValue(tmpStr);
            const char *str = tmpStr.c_str();
            if (EnumHelper::Bitset::has(_allowedTypes, AllowedType::ALLOW_ZEROCONF)) {
                if (strstr_P(str, SPGM(_var_zeroconf))) { //TODO parse and validate zeroconf
                    return true;
                }
            }
            if (EnumHelper::Bitset::has(_allowedTypes, AllowedType::ALLOW_EMPTY) && tmpStr.length() == 0) {
                return true;
                //const char *tmp = str;
                //while (isspace(*tmp)) {
                //    tmp++;
                //}
                //if (!*tmp) {
                //    return true;
                //}
            }
            IPAddress addr;
            // check if we can create an IP address from the value
            if (!addr.fromString(tmpStr)) {
                while(*str) {
                    if (!(isalnum(*str) || *str == '_' || *str == '-' || *str == '.')) {
                        return false;
                    }
                    str++;
                }
            }
            return true;
        }
        return false;
    }

private:
    AllowedType _allowedTypes;

};

class FormHostValidatorEx : public FormHostValidator {
public:
    using StringVector = std::vector<String>;
    using FormHostValidator::FormHostValidator;

    FormHostValidatorEx &push_back(const String &str) {
        _allowStrings.push_back(str);
        return *this;
    }

    FormHostValidatorEx &emplace_back(String &&str) {
        _allowStrings.emplace_back(std::move(str));
        return *this;
    }

    virtual bool validate() override {
        auto result = FormHostValidator::validate();
        if (result) {
            return true;
        }
        const auto &tmpStr = getField().getValue();
        for (const auto &str : _allowStrings) {
            if (tmpStr == str) {
                return true;
            }
        }
        return false;
    }

public:
    StringVector _allowStrings;
};
