/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <KFCForms.h>
#include "kfc_fw_config.h"
#include "syslog_plugin.h"

using SyslogClient = KFCConfigurationClasses::Plugins::SyslogClient;

void SyslogPlugin::createConfigureForm(FormCallbackType type, const String &formName, Form &form, AsyncWebServerRequest *request)
{
    using KFCConfigurationClasses::System;

    if (type == FormCallbackType::SAVE) {

        auto &cfg = SyslogClient::getWriteableConfig();
        if (cfg.port == 0) {
            cfg.port = 514;
        }
        System::Flags::getWriteable().syslogEnabled = SyslogClient::isEnabled(cfg.protocol_enum);
        return;

    } else if (!isCreateFormCallbackType(type)) {
        return;
    }

    auto &cfg = SyslogClient::getWriteableConfig();

    form.add<uint8_t>(F("syslog_enabled"), _H_W_STRUCT_VALUE(cfg, protocol));
    form.addValidator(new FormRangeValidatorEnum<SyslogClient::SyslogProtocolType>());

    form.add(F("syslog_host"), _H_CSTR_FUNC(SyslogClient::getHostname, SyslogClient::setHostname));
    form.addValidator(new FormValidHostOrIpValidator(FormValidHostOrIpValidator::ALLOW_ZEROCONF|FormValidHostOrIpValidator::ALLOW_EMPTY));

    form.add<uint16_t>(F("syslog_port"), _H_W_STRUCT_VALUE(cfg, port));
    form.addValidator(new FormRangeValidator(FSPGM(Invalid_port), 1, 65535, true));

    form.finalize();
}
