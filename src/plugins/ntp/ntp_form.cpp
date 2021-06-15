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

void NTPPlugin::createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request)
{
    if (!isCreateFormCallbackType(type)) {
        return;
    }

    auto &cfg = System::Flags::getWriteableConfig();
    auto &ntp = Plugins::NTPClient::getWriteableConfig();

    auto &ui = form.createWebUI();
    ui.setTitle(FSPGM(NTP_Client_Configuration, "NTP Client Configuration"));
    ui.setContainerId(F("ntp_settings"));
    ui.setStyle(FormUI::WebUI::StyleType::ACCORDION);

    auto &mainGroup = form.addCardGroup(FSPGM(config));

    form.addObjectGetterSetter<System::Flags::ConfigStructType, bool>(F("ntp_en"), cfg, System::Flags::ConfigStructType::get_bit_is_ntp_client_enabled, System::Flags::ConfigStructType::set_bit_is_ntp_client_enabled);
    form.addFormUI(FSPGM(NTP_Client, "NTP Client"), FormUI::BoolItems());

    auto &group = form.addDivGroup(F("ntpcfg"), F("{'i':'#ntp_en','s':{'0':'$T.hide()','1':'$T.show()'}}"));

    form.addStringGetterSetter(F("posix_tz"), Plugins::NTPClient::getPosixTimezone, Plugins::NTPClient::setPosixTimezone);
    form.addFormUI(FormUI::Type::SELECT, FSPGM(Timezone, "Timezone"));
    form.addStringGetterSetter(F("tz_name"), Plugins::NTPClient::getTimezoneName, Plugins::NTPClient::setTimezoneName);
    form.addFormUI(FormUI::Type::HIDDEN);

    form.addStringGetterSetter(F("ntpsvr1"), Plugins::NTPClient::getServer1, Plugins::NTPClient::setServer1);
    form.addFormUI(F("NTP Server 1"));
    form.addValidator(FormUI::Validator::Hostname(FormUI::AllowedType::EMPTY_OR_HOST_OR_IP));

#if SNTP_MAX_SERVERS >= 2
    form.addStringGetterSetter(F("ntpsvr2"), Plugins::NTPClient::getServer2, Plugins::NTPClient::setServer2);
    form.addFormUI(F("NTP Server 2"));
    form.addValidator(FormUI::Validator::Hostname(FormUI::AllowedType::EMPTY_OR_HOST_OR_IP));
#endif

#if SNTP_MAX_SERVERS >= 3
    form.addStringGetterSetter(F("ntpsvr3"), Plugins::NTPClient::getServer3, Plugins::NTPClient::setServer3);
    form.addFormUI(F("NTP Server 3"));
    form.addValidator(FormUI::Validator::Hostname(FormUI::AllowedType::EMPTY_OR_HOST_OR_IP));
#endif

#if SNTP_MAX_SERVERS >= 4
    form.addStringGetterSetter(F("ntpsvr4"), Plugins::NTPClient::getServer4, Plugins::NTPClient::setServer3);
    form.addFormUI(F("NTP Server 4"));
    form.addValidator(FormUI::Validator::Hostname(FormUI::AllowedType::EMPTY_OR_HOST_OR_IP));
#endif

    form.addObjectGetterSetter(F("ntpri"), ntp, ntp.get_bits_refreshInterval, ntp.set_bits_refreshInterval);
    form.addFormUI(FormUI::Type::NUMBER, FSPGM(Refresh_Interval, "Refresh Interval"), FormUI::Suffix(FSPGM(UTF8_minutes_plusminus5percent)));
    ntp.addRangeValidatorFor_refreshInterval(form);

    group.end();
    mainGroup.end();

    form.finalize();
}
