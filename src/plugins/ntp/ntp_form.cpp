/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <ESPAsyncWebServer.h>
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
using KFCConfigurationClasses::Plugins;

void NTPPlugin::createConfigureForm(FormCallbackType type, const String &formName, Form &form, AsyncWebServerRequest *request)
{
    if (!isCreateFormCallbackType(type)) {
        return;
    }

    auto &cfg = System::Flags::getWriteable();
    auto &ntp = Plugins::NTPClient::getWriteableConfig();

    form.setFormUI(FSPGM(NTP_Client_Configuration, "NTP Client Configuration"));

    form.addWriteableStruct(cfg, is_ntp_client_enabled))->setFormUI((new FormUI(FormUI::Type::SELECT, FSPGM(NTP_Client, "NTP Client")))->setBoolItems());

    auto &group = form.addDivGroup(F("ntpcfg"), F("{'i':'#ntp_CL_EN','s':{'0':'$T.hide()','1':'$T.show()'}}"));

    form.addCStrGetterSetter(Plugins::NTPClient::getPosixTimezone, Plugins::NTPClient::setPosixTimezone))->setFormUI((new FormUI(FormUI::Type::SELECT, FSPGM(Timezone, "Timezone"))));
    form.addCStrGetterSetter(Plugins::NTPClient::getTimezoneName, Plugins::NTPClient::setTimezoneName))->setFormUI((new FormUI(FormUI::Type::HIDDEN)));

    form.addCStrGetterSetter(Plugins::NTPClient::getServer1, Plugins::NTPClient::setServer1))->setFormUI((new FormUI(FormUI::Type::TEXT, PrintString(FSPGM(NTP_Server___, "NTP Server %u"), 1))));
    form.addValidator(new FormValidHostOrIpValidator(true));

    form.addCStrGetterSetter(Plugins::NTPClient::getServer2, Plugins::NTPClient::setServer2))->setFormUI((new FormUI(FormUI::Type::TEXT, PrintString(FSPGM(NTP_Server___, "NTP Server %u"), 2))));
    form.addValidator(new FormValidHostOrIpValidator(true));

    form.addCStrGetterSetter(Plugins::NTPClient::getServer3, Plugins::NTPClient::setServer3))->setFormUI((new FormUI(FormUI::Type::TEXT, PrintString(FSPGM(NTP_Server___, "NTP Server %u"), 3))));
    form.addValidator(new FormValidHostOrIpValidator(true));

    form.addWriteableStruct(ntp, refreshInterval))->setFormUI((new FormUI(FormUI::Type::INTEGER, FSPGM(Refresh_Interval, "Refresh Interval")))->setSuffix(FSPGM(minutes__, "minutes \u00b1")));
    form.addValidator(new FormRangeValidator(F("Invalid refresh interval: %min%-%max% minutes"), ntp.kRefreshIntervalMin, ntp.kRefreshIntervalMax));

    group.end();

    form.finalize();
}
