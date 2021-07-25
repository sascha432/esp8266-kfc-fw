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

using Plugins = KFCConfigurationClasses::PluginsType;
using namespace KFCConfigurationClasses::Plugins::SyslogConfigNS;

void SyslogPlugin::createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request)
{
    using KFCConfigurationClasses::System;

    if (type == FormCallbackType::SAVE) {

        auto &cfg = Plugins::SyslogClient::getWriteableConfig();
        System::Flags::getWriteableConfig().is_syslog_enabled = SyslogClient::isEnabled(cfg.protocol);
        return;

    } else if (!isCreateFormCallbackType(type)) {
        return;
    }

    auto &cfg = Plugins::SyslogClient::getWriteableConfig();

    form.addObjectGetterSetter(F("sl_proto"), FormGetterSetter(cfg, protocol), FormUI::Field::Type::SELECT);
    form.addValidator(FormUI::Validator::EnumRange<SyslogProtocolType>());

    form.addStringGetterSetter(F("sl_host"), SyslogClient::getHostname, SyslogClient::setHostname);
    form.addValidator(FormUI::Validator::Hostname(FormUI::AllowedType::EMPTY_OR_HOST_OR_IP_OR_ZEROCONF));

    form.addCallbackSetter(F("sl_port"), cfg.getPortAsString(), [&cfg](const String &value, FormUI::Field::BaseField &field) {
        cfg.setPort(value.toInt());
        field.setValue(cfg.getPortAsString());
    });
    form.addValidator(FormUI::Validator::NetworkPort(true));

    form.finalize();
}
