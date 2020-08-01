/**
 * Author: sascha_lammers@gmx.de
 */

#include "kfc_fw_config_plugin.h"
#include <KFCForms.h>

using KFCConfigurationClasses::MainConfig;
using KFCConfigurationClasses::System;
using KFCConfigurationClasses::Network;

static void createWifiModes(FormUI::ItemsList &items)
{
    items = FormUI::ItemsList(WIFI_OFF, FSPGM(Disabled), WIFI_STA, FSPGM(Station_Mode), WIFI_AP, FSPGM(Access_Point), WIFI_AP_STA, PrintString(F("%s and %s"), SPGM(Access_Point), SPGM(Station_Mode)));
}


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
            FormUI::ItemsList wifiModes;
            createWifiModes(wifiModes) ;

            auto channelItems = FormUI::ItemsList(0, F("Auto"));
            for(uint8_t i = 1; i <= config.getMaxWiFiChannels(); i++) {
                channelItems.emplace_back(std::move(String(i)), std::move(String(i)));
            }

            auto encryptionItems = FormUI::ItemsList(
                ENC_TYPE_NONE, F("Open / No encryption"),
                ENC_TYPE_WEP, F("WEP"),
                ENC_TYPE_TKIP, F("WPA TKIP"),
                ENC_TYPE_CCMP, F("WPA CCMP"),
                ENC_TYPE_AUTO, F("Auto")
            );
//ESP32
// <option value="0" %APENC_0%>Open</option>
// <option value="1" %APENC_1%>WEP</option>
// <option value="2" %APENC_2%>WPA/PSK</option>
// <option value="3" %APENC_3%>WPA2/PSK</option>
// <option value="4" %APENC_4%>WPA/WPA2/PSK</option>
// <option value="5" %APENC_5%>WPA2 Enterprise</option>


            auto &ui = form.getFormUIConfig();
            ui.setStyle(FormUI::StyleType::ACCORDION);
            ui.setTitle(F("WiFi Configuration"));
            ui.setContainerId(F("wifi_settings"));

            auto &modeGroup = form.addCardGroup(F("mode"), emptyString, true);

            form.addObjectGetterSetter(F("wifi_mode"), flags, flags.get_wifi_mode, flags.set_wifi_mode);
            form.addValidator(FormRangeValidator(F("Invalid mode"), WIFI_OFF, WIFI_AP_STA));
            form.addFormUI(F("WiFi Mode"), wifiModes);

            auto &stationGroup = modeGroup.end().addCardGroup(F("station"), FSPGM(Station_Mode), true);

            form.addCStringGetterSetter(F("st_ssid"), Network::WiFi::getSSID, Network::WiFi::setSSIDCStr);
            Network::WiFi::addSSIDLengthValidator(form);
            form.addFormUI(F("SSID"));

            form.addCStringGetterSetter(F("st_pass"), Network::WiFi::getPassword, Network::WiFi::setPasswordCStr);
            Network::WiFi::addPasswordLengthValidator(form);
            form.addFormUI(FormUI::Type::PASSWORD, F("Passphrase"));

            auto &apModeGroup = stationGroup.end().addCardGroup(F("softap"), FSPGM(Access_Point), true);

            form.addObjectGetterSetter(F("ap_standby"), flags, flags.get_bit_is_softap_standby_mode_enabled, flags.set_bit_is_softap_standby_mode_enabled);
            form.addFormUI(F("AP Mode"), FormUI::BoolItems(F("Enable AP Mode if station is disconnected"), F("Enable AP Mode permanently")));

            auto &ssidHidden = form.addObjectGetterSetter(F("ap_hid"), flags, flags.get_bit_is_softap_ssid_hidden, flags.set_bit_is_softap_ssid_hidden);
            form.addFormUI(FormUI::Type::HIDDEN);

            ;

            form.addCStringGetterSetter(F("ap_ssid"), Network::WiFi::getSoftApSSID, Network::WiFi::setSoftApSSIDCStr);
            Network::WiFi::addSoftApSSIDLengthValidator(form);
            form.addFormUI(F("SSID"), FormUI::Suffix(
                PrintString(F("<span class=\"button-checkbox\" data-on-icon=\"oi oi-task\" data-off-icon=\"oi oi-ban\"><button type=\"button\" class=\"btn\" data-color=\"primary\">HIDDEN</button><input type=\"checkbox\" class=\"hidden\" name=\"_%s\" id=\"_%s\" value=\"1\" data-label=\"HIDDEN\"></span>"),
                    ssidHidden.getName().c_str(), ssidHidden.getName().c_str() //, flags.is_softap_ssid_hidden ? PSTR(" checked") : emptyString.c_str()
                ))
            );

            form.addCStringGetterSetter(F("ap_pass"), Network::WiFi::getSoftApPassword, Network::WiFi::setSoftApPasswordCStr);
            Network::WiFi::addSoftApPasswordLengthValidator(form);
            form.addFormUI(FormUI::Type::PASSWORD, F("Passphrase"));

            form.addMemberVariable(F("ap_ch"), softAp, &Network::SoftAP::ConfigStructType::channel);
            form.addValidator(FormRangeValidator(1, config.getMaxWiFiChannels(), true));
            form.addFormUI(F("Channel"), channelItems);

            form.addMemberVariable("ap_enc", softAp, &Network::SoftAP::ConfigStructType::encryption);
            form.addValidator(FormEnumValidator<uint8_t, WiFiEncryptionTypeArray().size()>(F("Invalid encryption"), createWiFiEncryptionTypeArray()));
            form.addFormUI(F("Encryption"), encryptionItems);


            apModeGroup.end();

        }
        else if (String_equals(formName, SPGM(network))) {

            auto &network = Network::Settings::getWriteableConfig();
            auto &softAp = Network::SoftAP::getWriteableConfig();

            auto &ui = form.getFormUIConfig();
            ui.setStyle(FormUI::StyleType::ACCORDION);
            ui.setTitle(FSPGM(Network_Configuration));
            ui.setContainerId(F("network_settings"));

            auto &deviceGroup = form.addCardGroup(FSPGM(device), FSPGM(Device), true);

            form.addCStringGetterSetter("dev_host", System::Device::getName, System::Device::setNameCStr);
            form.addFormUI(FSPGM(Hostname));
            form.addValidator(FormLengthValidator(4, System::Device::kNameMaxSize));

            form.addCStringGetterSetter("dev_title", System::Device::getTitle, System::Device::setTitleCStr);
            form.addFormUI(FSPGM(Title));
            form.addValidator(FormLengthValidator(4, System::Device::kNameMaxSize));

            deviceGroup.end();
            auto &stationGroup = form.addCardGroup(F("station"), FSPGM(Station_Mode));

            form.addObjectGetterSetter(F("st_dhcp"), flags, flags.get_bit_is_station_mode_dhcp_enabled, flags.set_bit_is_station_mode_dhcp_enabled);
            form.addFormUI(FSPGM(DHCP_Client), FormUI::BoolItems());

            form.addIPGetterSetter(F("st_ip"), network, network.get_ipv4_local_ip, network.set_ipv4_local_ip);
            form.addFormUI(FSPGM(IP_Address));
            form.addIPGetterSetter(F("st_subnet"), network, network.get_ipv4_subnet, network.set_ipv4_subnet);
            form.addFormUI(FSPGM(Subnet));
            form.addIPGetterSetter(F("st_gw"), network, network.get_ipv4_gateway, network.set_ipv4_gateway);
            form.addFormUI(FSPGM(Gateway));
            form.addIPGetterSetter(F("st_dns1"), network, network.get_ipv4_dns1, network.set_ipv4_dns1);
            form.addFormUI(FSPGM(DNS_1));
            form.addIPGetterSetter(F("st_dns2"), network, network.get_ipv4_dns2, network.set_ipv4_dns2);
            form.addFormUI(FSPGM(DNS_2));

            stationGroup.end();
            auto &apGroup = form.addCardGroup(F("apmode"), FSPGM(Access_Point));

            form.addObjectGetterSetter(F("ap_dhcpd"), flags, flags.get_bit_is_softap_dhcpd_enabled, flags.set_bit_is_softap_dhcpd_enabled);
            form.addFormUI(FSPGM(DHCP_Server), FormUI::BoolItems());

            form.addIPGetterSetter(F("ap_ip"), softAp, softAp.get_ipv4_address, softAp.set_ipv4_address);
            form.addFormUI(FSPGM(IP_Address));
            form.addIPGetterSetter(F("ap_subnet"), softAp, softAp.get_ipv4_subnet, softAp.set_ipv4_subnet);
            form.addFormUI(FSPGM(Subnet));
            form.addIPGetterSetter(F("ap_dhcpds"), softAp, softAp.get_ipv4_dhcp_start, softAp.set_ipv4_dhcp_start);
            form.addFormUI(F("DHCP Start IP"));
            form.addIPGetterSetter(F("ap_dhcpde"), softAp, softAp.get_ipv4_dhcp_end, softAp.set_ipv4_dhcp_end);
            form.addFormUI(F("DHCP End IP"));

            apGroup.end();

        }
        else if (String_equals(formName, SPGM(device, "device"))) {

            auto &cfg = System::Device::getWriteableConfig();

            form.setFormUI(FSPGM(Device_Configuration));

            form.addCStrGetterSetter("dtitle", System::Device::getTitle, System::Device::setTitle));
            form.addFormUI(FSPGM(Title));
            form.addValidator(FormLengthValidator(1, System::Device::kTitleMaxSize));

            form.addWriteableStruct("smrd", cfg, safe_mode_reboot_timeout_minutes));
            form.addFormUI(FormUI::Type::INTEGER, F("Reboot Delay Running In Safe Mode"), FormUI::Suffix(FSPGM(minutes)));
            form.addValidator(FormRangeValidator(5, 3600, true));

            form.addWriteableStruct("mdnsen", flags, is_mdns_enabled));
            form.addFormUI(F("mDNS Announcements"), FormUI::BoolItems(FSPGM(Enabled), F("Disabled (Zeroconf is still available)")));

            form.addWriteableStruct("zcto", cfg, zeroconf_timeout));
            form.addFormUI(FormUI::Type::INTEGER, FSPGM(Zeroconf_Timeout), FormUI::Suffix(FSPGM(milliseconds)));
            form.addValidator(FormRangeValidator(System::Device::kZeroConfMinTimeout, System::Device::kZeroConfMaxTimeout));

            form.addWriteableStruct("zclg", cfg, zeroconf_logging));
            form.addFormUI(FSPGM(Zeroconf_Logging), FormUI::BoolItems());

            form.addWriteableStruct("ssdpen", flags, is_ssdp_enabled));
            form.addFormUI(FSPGM(SSDP_Discovery), FormUI::BoolItems());

            form.addWriteableStruct("wuclt", cfg, webui_cookie_lifetime_days));
            form.addFormUI(FormUI::Type::INTEGER, FormUI::Label(F("Allow to store credentials in a cookie to login automatically:<br><span class=\"oi oi-shield p-2\"></span><small>If the cookie is stolen, it is not going to expire and changing the password is the only options to invalidate it.</small>"), true), FormUI::Suffix(FSPGM(days)));
            form.addValidator(FormRangeValidator(System::Device::kWebUICookieMinLifetime, System::Device::kWebUICookieMaxLifetime, true));

            form.addWriteableStruct("waen", flags, is_webalerts_enabled));
            form.addFormUI(FSPGM(Web_Alerts), FormUI::BoolItems());
            form.addWriteableStruct("wuen", flags, is_webui_enabled));
            form.addFormUI(FSPGM(WebUI), FormUI::BoolItems());

            form.addWriteableStruct("stled", cfg, status_led_mode));
            form.addFormUI(FSPGM(Status_LED_Mode), FormUI::BoolItems(F("Solid when connected to WiFi"), F("Turn off when connected to WiFi")));

        }
        else if (String_equals(formName, SPGM(password))) {

            form.setFormUI(FSPGM(Change_Password));

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
