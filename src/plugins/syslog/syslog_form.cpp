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

void SyslogPlugin::createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request)
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

    form.addMemberVariable(F("sl_proto"), cfg, &SyslogClient::ConfigStructType::protocol, FormUI::Field::Type::SELECT);
    form.addValidator(FormUI::Validator::EnumRange<SyslogClient::SyslogProtocolType>());

    form.addStringGetterSetter("sl_host", SyslogClient::getHostname, SyslogClient::setHostname);
    form.addValidator(FormUI::Validator::Hostname(FormUI::AllowedType::EMPTY_AND_ZEROCONF));

    form.addCallbackSetter(F("sl_port"), cfg.getPortAsString(), [&cfg](const String &value, FormUI::Field::BaseField &field) {
        cfg.setPort(value.toInt(), cfg.isSecure());
        field.setValue(cfg.getPortAsString());
    });
    form.addValidator(FormUI::Validator::NetworkPort(true));

    form.finalize();
}
