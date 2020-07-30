/**
 * Author: sascha_lammers@gmx.de
 */

#include "kfc_fw_config_plugin.h"
#include <KFCForms.h>

using KFCConfigurationClasses::MainConfig;
using KFCConfigurationClasses::System;
using KFCConfigurationClasses::Network;

void KFCConfigurationPlugin::createConfigureForm(FormCallbackType type, const String &formName, Form &form, AsyncWebServerRequest *request)
{
    if (type == FormCallbackType::SAVE) {
        if (String_equals(formName, SPGM(password))) {
            auto &flags = System::Flags::getWriteableConfig();
            flags.is_default_password = false;
        }
        else if (String_equals(formName, SPGM(device))) {
            config.setConfigDirty(true);
        }
    }
    else if (isCreateFormCallbackType(type)) {

        auto &flags = System::Flags::getWriteableConfig();

        if (String_equals(formName, SPGM(wifi))) {

            auto &softAp = Network::SoftAP::getWriteableConfig();

            form.addGetterSetter("wmode", flags.getWifiMode, flags.setWifiMode, &flags));
            form.addValidator(FormRangeValidator(F("Invalid mode"), WIFI_OFF, WIFI_AP_STA));

            form.addCStrGetterSetter("wssid", Network::WiFi::getSSID, Network::WiFi::setSSID));
            form.addValidator(FormLengthValidator(1, Network::WiFi::kSSIDMaxSize));

            form.addCStrGetterSetter("wpwd", Network::WiFi::getPassword, Network::WiFi::setPassword));
            form.addValidator(FormLengthValidator(Network::WiFi::kPasswordMinSize, Network::WiFi::kPasswordMaxSize));

            form.addCStrGetterSetter("apssid", Network::WiFi::getSoftApSSID, Network::WiFi::setSoftApSSID));
            form.addValidator(FormLengthValidator(1, Network::WiFi::kSoftApSSIDMaxSize));

            form.addCStrGetterSetter("appwd", Network::WiFi::getSoftApPassword, Network::WiFi::setSoftApPassword));
            form.addValidator(FormLengthValidator(Network::WiFi::kSoftApPasswordMinSize, Network::WiFi::kSoftApPasswordMaxSize));

            form.addWriteableStruct("apch", softAp, channel));
            form.addValidator(FormRangeValidator(1, config.getMaxWiFiChannels()));

            form.addWriteableStruct("apenc", softAp, encryption));
            form.addValidator(FormEnumValidator<uint8_t, WiFiEncryptionTypeArray().size()>(F("Invalid encryption"), createWiFiEncryptionTypeArray()));

            form.addWriteableStruct("aphs", flags, is_softap_ssid_hidden), FormField::Type::CHECK);

            form.addWriteableStruct("apsm", flags, is_softap_standby_mode_enabled), FormField::Type::SELECT);

        }
        else if (String_equals(formName, SPGM(network, "network"))) {

            auto &network = Network::Settings::getWriteableConfig();
            auto &softAp = Network::SoftAP::getWriteableConfig();

            form.setFormUI(FSPGM(Network_Configuration, "Network Configuration"));

            form.addCStrGetterSetter("dhost", System::Device::getName, System::Device::setName));
            form.addFormUI(FSPGM(Hostname, "Hostname"));
            form.addValidator(FormLengthValidator(4, System::Device::kNameMaxSize));

            form.addWriteableStruct("stdhcp", flags, is_station_mode_dhcp_enabled));
            form.addFormUI(FSPGM(DHCP, "DHCP"), FormUI::BoolItems());

            form.addWriteableStructIPAddress("lip", network, _localIp));
            form.addFormUI(FSPGM(IP_Address, "IP Address"));
            form.addWriteableStructIPAddress("lsn", network, _subnet));
            form.addFormUI(FSPGM(Subnet, "Subnet"));
            form.addWriteableStructIPAddress("lgw", network, _gateway));
            form.addFormUI(FSPGM(Gateway, "Gateway"));
            form.addWriteableStructIPAddress("dns1", network, _dns1));
            form.addFormUI(FSPGM(DNS_1, "DNS 1"));
            form.addWriteableStructIPAddress("dns2", network, _dns2));
            form.addFormUI(FSPGM(DNS_2, "DNS 2"));

            form.addWriteableStruct("apdhcp", flags, is_softap_dhcpd_enabled));

            form.addWriteableStructIPAddress("dstart", softAp, dhcpStart));
            form.addWriteableStructIPAddress("dend", softAp, dhcpEnd));
            form.addWriteableStructIPAddress("daddr", softAp, address));
            form.addWriteableStructIPAddress("dsn", softAp, subnet));
        }
        else if (String_equals(formName, SPGM(device, "device"))) {

            auto &cfg = System::Device::getWriteableConfig();

            form.setFormUI(FSPGM(Device_Configuration, "Device Configuration"));

            form.addCStrGetterSetter("dtitle", System::Device::getTitle, System::Device::setTitle));
            form.addFormUI(FSPGM(Title));
            form.addValidator(FormLengthValidator(1, System::Device::kTitleMaxSize));

            form.addWriteableStruct("smrd", cfg, safe_mode_reboot_timeout_minutes));
            form.addFormUI(FormUI::Type::INTEGER, F("Reboot Delay Running In Safe Mode"), FormUI::Suffix(FSPGM(minutes)));
            form.addValidator(FormRangeValidator(5, 3600, true));

            form.addWriteableStruct("mdnsen", flags, is_mdns_enabled));
            form.addFormUI(F("mDNS Announcements"), FormUI::BoolItems(FSPGM(Enabled), F("Disabled (Zeroconf is still available)")));

            form.addWriteableStruct("zcto", cfg, zeroconf_timeout));
            form.addFormUI(FormUI::Type::INTEGER, FSPGM(Zeroconf_Timeout, "Zeroconf Timeout"), FormUI::Suffix(FSPGM(milliseconds)));
            form.addValidator(FormRangeValidator(System::Device::kZeroConfMinTimeout, System::Device::kZeroConfMaxTimeout));

            form.addWriteableStruct("zclg", cfg, zeroconf_logging));
            form.addFormUI(FSPGM(Zeroconf_Logging, "Zeroconf Logging"), FormUI::BoolItems());

            form.addWriteableStruct("ssdpen", flags, is_ssdp_enabled));
            form.addFormUI(FSPGM(SSDP_Discovery, "SSDP Discovery"), FormUI::BoolItems());

            form.addWriteableStruct("wuclt", cfg, webui_cookie_lifetime_days));
            form.addFormUI(FormUI::Type::INTEGER, FormUI::Label(F("Allow to store credentials in a cookie to login automatically:<br><span class=\"oi oi-shield p-2\"></span><small>If the cookie is stolen, it is not going to expire and changing the password is the only options to invalidate it.</small>"), true), FormUI::Suffix(FSPGM(days)));
            form.addValidator(FormRangeValidator(System::Device::kWebUICookieMinLifetime, System::Device::kWebUICookieMaxLifetime, true));

            form.addWriteableStruct("waen", flags, is_webalerts_enabled));
            form.addFormUI(FSPGM(Web_Alerts, "Web Alerts"), FormUI::BoolItems());
            form.addWriteableStruct("wuen", flags, is_webui_enabled));
            form.addFormUI(FSPGM(WebUI, "Web UI"), FormUI::BoolItems());

            form.addWriteableStruct("stled", cfg, status_led_mode));
            form.addFormUI(FSPGM(Status_LED_Mode, "Status LED Mode"), FormUI::BoolItems(F("Solid when connected to WiFi"), F("Turn off when connected to WiFi")));

        }
        else if (String_equals(formName, SPGM(password))) {

            form.setFormUI(FSPGM(Change_Password, "Change Password"));

            auto password = String(System::Device::getPassword());
            form.add(FSPGM(password), password, FormField::Type::TEXT);
            form.addValidator(FormMatchValidator(F("The entered password is not correct"), [](FormField &field) {
                return field.getValue().equals(System::Device::getPassword());
            }));

            form.add(F("password2"), password, FormField::Type::TEXT).
                setValue(emptyString);

            // form.addValidator(FormRangeValidator(F("The password has to be at least %min% characters long"), System::Device::kPasswordMinSize, 0));
            form.addValidator(FormRangeValidator(System::Device::kPasswordMinSize, System::Device::kPasswordMaxSize));

            form.add(F("password3"), emptyString, FormField::Type::TEXT);
            form.addValidator(FormMatchValidator(F("The password confirmation does not match"), [](FormField &field) {
                return field.equals(field.getForm().getField(F("password2")));
            }));

        }
        else {
            return;
        }
        form.finalize();
    }
}
