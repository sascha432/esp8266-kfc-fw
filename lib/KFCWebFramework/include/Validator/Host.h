/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <EnumHelper.h>
#include "Validator/BaseValidator.h"
#include "Field/BaseField.h"
#include "Form/BaseForm.h"

namespace FormUI {

    namespace Validator {

        class Hostname : public BaseValidator {
        public:
            using AllowedType = HostnameValidator::AllowedType;

            Hostname(AllowedType allowedTypes = AllowedType::HOST_OR_IP, const String &message = String()) :
                BaseValidator(message, FSPGM(FormHostValidator_default_message)),
                _allowedTypes(allowedTypes)
            {
                __DBG_assert_printf(allowedTypes != AllowedType::NONE, "allowed type NONE");
            }
            //Hostname(const String &message, AllowedType allowedTypes = AllowedType::HOST_OR_IP) :
            //    BaseValidator(message, FSPGM(FormHostValidator_default_message)),
            //    _allowedTypes(allowedTypes)
            //{
            //    __LDBG_assert_printf(allowedTypes != AllowedType::NONE, "allowed type NONE");
            //}

            virtual bool validate() override {
                if (BaseValidator::validate()) {
                    auto tmpStr = getField().getValue();
                    tmpStr.trim();
                    getField().setValue(tmpStr);
                    const char *str = tmpStr.c_str();
                    if (EnumHelper::Bitset::has(_allowedTypes, AllowedType::ZEROCONF)) {
                        if (strstr_P(str, SPGM(_var_zeroconf))) { //TODO parse and validate zeroconf
                            return true;
                        }
                    }
                    if (EnumHelper::Bitset::has(_allowedTypes, AllowedType::EMPTY) && tmpStr.length() == 0) {
                        return true;
                    }
                    if (EnumHelper::Bitset::has(_allowedTypes, AllowedType::IPADDRESS)) {
                        IPAddress addr;
                        if (addr.fromString(tmpStr) && addr.isSet() && addr != 0xffffffffU) {
                            return true;
                        }
                    }
                    if (EnumHelper::Bitset::has(_allowedTypes, AllowedType::HOSTNAME)) {
                        while(*str) {
                            if (!(isalnum(*str) || *str == '_' || *str == '-' || *str == '.')) {
                                return false;
                            }
                            str++;
                        }
                        return true;
                    }
                }
                return false;
            }

        private:
            AllowedType _allowedTypes;

        };

        // allows additional values from the list that are not valid hostnames or ips
        class HostnameEx : public Hostname {
        public:
            using StringVector = std::vector<String>;
            using Hostname::Hostname;

            HostnameEx &push_back(const String &str) {
                _allowStrings.push_back(str);
                return *this;
            }

            HostnameEx &emplace_back(String &&str) {
                _allowStrings.emplace_back(std::move(str));
                return *this;
            }

            virtual bool validate() override {
                auto result = Hostname::validate();
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

    }

}
