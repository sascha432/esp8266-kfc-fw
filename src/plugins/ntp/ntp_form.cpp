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

    auto &cfg = System::Flags::getWriteableConfig();
    auto &ntp = Plugins::NTPClient::getWriteableConfig();

    auto &ui = form.getFormUIConfig();
    ui.setTitle(FSPGM(NTP_Client_Configuration, "NTP Client Configuration"));
    ui.setContainerId(F("ntp_settings"));
    ui.setStyle(FormUI::StyleType::ACCORDION);

    auto &mainGroup = form.addCardGroup(FSPGM(config), String(), true);

    form.addObjectGetterSetter<System::Flags::ConfigStructType, bool>(F("ntp_en"), cfg, System::Flags::ConfigStructType::get_bit_is_ntp_client_enabled, System::Flags::ConfigStructType::set_bit_is_ntp_client_enabled);
    form.addFormUI(FSPGM(NTP_Client, "NTP Client"), FormUI::BoolItems());

    auto &group = form.addDivGroup(F("ntpcfg"), F("{'i':'#ntp_en','s':{'0':'$T.hide()','1':'$T.show()'}}"));

    form.addCStringGetterSetter(F("posix_tz"), Plugins::NTPClient::getPosixTimezone, Plugins::NTPClient::setPosixTimezoneCStr);
    form.addFormUI(FormUI::Type::SELECT, FSPGM(Timezone, "Timezone"));
    form.addCStringGetterSetter(F("tz_name"), Plugins::NTPClient::getTimezoneName, Plugins::NTPClient::setTimezoneNameCStr);
    form.addFormUI(FormUI::Type::HIDDEN);

    form.addCStringGetterSetter(F("ntpsvr1"), Plugins::NTPClient::getServer1, Plugins::NTPClient::setServer1CStr);
    form.addFormUI(FormUI::Label(PrintString(FSPGM(NTP_Server___, "NTP Server %u"), 1)));
    form.addValidator(FormHostValidator(FormHostValidator::AllowedType::ALLOW_EMPTY));

    form.addCStringGetterSetter(F("ntpsvr2"), Plugins::NTPClient::getServer2, Plugins::NTPClient::setServer2CStr);
    form.addFormUI(FormUI::Label(PrintString(FSPGM(NTP_Server___), 2)));
    form.addValidator(FormHostValidator(FormHostValidator::AllowedType::ALLOW_EMPTY));

    form.addCStringGetterSetter(F("ntpsvr3"), Plugins::NTPClient::getServer3, Plugins::NTPClient::setServer3CStr);
    form.addFormUI(FormUI::Label(PrintString(FSPGM(NTP_Server___), 3)));
    form.addValidator(FormHostValidator(FormHostValidator::AllowedType::ALLOW_EMPTY));

    form.addMemberVariable(F("ntpri"), ntp, &Plugins::NTPClient::ConfigStructType::refreshInterval);
    form.addFormUI(FormUI::Type::INTEGER, FSPGM(Refresh_Interval, "Refresh Interval"), FormUI::FPSuffix(FSPGM(minutes__5_, "minutes \u00b15%")));
    form.addValidator(FormRangeValidator(F("Invalid refresh interval: %min%-%max% minutes"), ntp.kRefreshIntervalMin, ntp.kRefreshIntervalMax));

    group.end();
    mainGroup.end();

    form.finalize();
}
