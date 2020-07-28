/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <KFCForms.h>
#include "PluginComponent.h"
#include "ntp_plugin_class.h"
#include "ntp_plugin.h"
#include "kfc_fw_config.h"

#if DEBUG_NTP_CLIENT
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using KFCConfigurationClasses::System;

void NTPPlugin::createConfigureForm(FormCallbackType type, const String &formName, Form &form, AsyncWebServerRequest *request)
{
    if (!isCreateFormCallbackType(type)) {
        return;
    }

    auto &cfg = System::Flags::getWriteable();

    // form.setFormUI(FSPGM(NTP_Client_Configuration, "NTP Client Configuration"));

    form.addWriteableStruct(cfg, is_ntp_client_enabled));

    form.add(F("ntp_server1"), _H_STR_VALUE(Config().ntp.servers[0]));
    form.addValidator(new FormValidHostOrIpValidator(true));

    form.add(F("ntp_server2"), _H_STR_VALUE(Config().ntp.servers[1]));
    form.addValidator(new FormValidHostOrIpValidator(true));

    form.add(F("ntp_server3"), _H_STR_VALUE(Config().ntp.servers[2]));
    form.addValidator(new FormValidHostOrIpValidator(true));

    form.add(F("ntp_timezone"), _H_STR_VALUE(Config().ntp.timezone));
    form.add(F("ntp_posix_tz"), _H_STR_VALUE(Config().ntp.posix_tz));

    form.add<uint16_t>(F("ntp_refresh"), config._H_GET(Config().ntp.ntpRefresh), [](uint16_t value, FormField &, bool) {
        config._H_SET(Config().ntp.ntpRefresh, value);
        return false;
    });
    form.addValidator(new FormRangeValidator(F("Invalid refresh interval: %min%-%max% minutes"), 60, 43200));

    debug_printf_P(PSTR("ntp_timezone=%s ntp_posix_tz=%s\n"), config._H_STR(Config().ntp.timezone), config._H_STR(Config().ntp.posix_tz));

    form.finalize();
}
