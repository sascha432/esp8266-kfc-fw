/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <KFCForms.h>
#include "kfc_fw_config.h"
#include "syslog_plugin.h"

#if DEBUG_SYSLOG
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using SyslogClient = KFCConfigurationClasses::Plugins::SyslogClient;

void SyslogPlugin::createConfigureForm(FormCallbackType type, const String &formName, Form &form, AsyncWebServerRequest *request)
{
    using KFCConfigurationClasses::System;

    if (type == FormCallbackType::SAVE) {

        auto &cfg = SyslogClient::getWriteableConfig();
        System::Flags::getWriteableConfig().is_syslog_enabled = SyslogClient::isEnabled(cfg.protocol_enum);
        return;

    } else if (!isCreateFormCallbackType(type)) {
        return;
    }

    auto &cfg = SyslogClient::getWriteableConfig();

    form.addMemberVariable(F("sl_proto"), cfg, &SyslogClient::ConfigStructType::protocol, FormField::Type::SELECT);
    form.addValidator(FormRangeValidatorEnum<SyslogClient::SyslogProtocolType>());

    form.addCStringGetterSetter("sl_host", SyslogClient::getHostname, SyslogClient::setHostnameCStr);
    form.addValidator(FormHostValidator(FormHostValidator::AllowedType::ALLOW_EMPTY_AND_ZEROCONF));

    form.add(F("sl_port"), cfg.getPortAsString(), [&cfg](const String &value, FormField &field, bool store) {
        if (store) {
            cfg.setPort(value.toInt(), cfg.isSecure());
            field.setValue(cfg.getPortAsString());
        }
        return false;
    });
    form.addValidator(FormNetworkPortValidator(true));

    form.finalize();
}
