/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <kfc_fw_config.h>
#include <kfc_fw_config_plugin.h>
#include <stl_ext/type_traits.h>
#include <KFCForms.h>
#include <templates.h>

using KFCConfigurationClasses::MainConfig;
using KFCConfigurationClasses::System;
using KFCConfigurationClasses::Network;

static FormUI::Container::List createWifiModes()
{
    return FormUI::Container::List(
        WIFI_OFF, FSPGM(Disabled),
        WIFI_STA, FSPGM(Station_Mode),
        WIFI_AP, FSPGM(Access_Point),
        WIFI_AP_STA, F("Access Point and Station Mode")
    );
}


void KFCConfigurationPlugin::createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request)
{
    // auto scheduledTasks = File2String<const __FlashStringHelper *>(FSPGM(scheduler_config_file));
    // if (type == FormCallbackType::CREATE_POST) {
    //     if (formName.equals(FSPGM(device))) {
    //         auto field = form.getField(F("schet"));
    //         if (field && field->hasChanged()) {
    //             auto crc = crc16_update(field->getValue().c_str(), field->getValue().length());
    //             _formatSchedulerList(field->getValue());
    //             if (crc != crc16_update(field->getValue().c_str(), field->getValue().length())) {
    //                 __DBG_print("scheduler changed on createpost");
    //                 field->setChanged(true);
    //             }
    //         }
    //     }
    // }
    // else
    if (type == FormCallbackType::SAVE) {
        if (formName.equals(FSPGM(password))) {
            auto field = form.getField(FSPGM(npwd));
            if (field) {
                auto &flags = System::Flags::getWriteableConfig();
                flags.is_default_password = field->getValue() != FSPGM(defaultPassword);
                System::Device::setPassword(field->getValue());
            }
        }
        else if (formName.equals(FSPGM(device))) {
            config.setConfigDirty(true);
            // auto field = form.getField(F("schet"));
            // if (field && field->hasChanged()) {
            //     __DBG_print("saving scheduler");
            //     scheduledTasks.fromString(field->getValue());
            // }
        }
    }
    else if (isCreateFormCallbackType(type)) {

        auto &flags = System::Flags::getWriteableConfig();

        if (formName.equals(FSPGM(wifi))) {

            auto &softAp = Network::SoftAP::getWriteableConfig();
            FormUI::Container::List wifiModes(createWifiModes());

            FormUI::Container::List channelItems(0, FSPGM(Auto));
            for(uint8_t i = 1; i <= config.getMaxWiFiChannels(); i++) {
                channelItems.push_back(i, i);
            }

            FormUI::Container::List encryptionItems(
                ENC_TYPE_NONE, F("Open / No encryption"),
                ENC_TYPE_WEP, F("WEP"),
                ENC_TYPE_TKIP, F("WPA TKIP"),
                ENC_TYPE_CCMP, F("WPA CCMP"),
                ENC_TYPE_AUTO, FSPGM(Auto)
            );
//ESP32
// <option value="0" %APENC_0%>Open</option>
// <option value="1" %APENC_1%>WEP</option>
// <option value="2" %APENC_2%>WPA/PSK</option>
// <option value="3" %APENC_3%>WPA2/PSK</option>
// <option value="4" %APENC_4%>WPA/WPA2/PSK</option>
// <option value="5" %APENC_5%>WPA2 Enterprise</option>


            auto &ui = form.createWebUI();

            ui.setStyle(FormUI::WebUI::StyleType::ACCORDION);
            ui.setTitle(FSPGM(WiFi_Configuration, "WiFi Configuration"));
            ui.setContainerId(FSPGM(wifi_settings));

            auto &modeGroup = form.addCardGroup(FSPGM(mode));

            form.addObjectGetterSetter(FSPGM(wifi_mode), flags, flags.get_wifi_mode, flags.set_wifi_mode);
            form.addValidator(FormUI::Validator::EnumRange<WiFiMode, WIFI_OFF, WIFI_AP_STA, 0>(FSPGM(Invalid_mode)));
            form.addFormUI(FSPGM(WiFi_Mode, "WiFi Mode"), wifiModes);

            auto &stationGroup = modeGroup.end().addCardGroup(FSPGM(station), FSPGM(Station_Mode), true);

            form.addStringGetterSetter(F("wssid"), Network::WiFi::getSSID, Network::WiFi::setSSID);
            Network::WiFi::addSSIDLengthValidator(form);
            form.addFormUI(FSPGM(SSID), FormUI::SuffixHtml(F("<button class=\"btn btn-default\" type=\"button\" id=\"wifi_scan_button\" data-toggle=\"modal\" data-target=\"#network_dialog\">Scan...</button>")));

            form.addStringGetterSetter(F("st_pass"), Network::WiFi::getPassword, Network::WiFi::setPassword);
            Network::WiFi::addPasswordLengthValidator(form);
            form.addFormUI(FormUI::Type::PASSWORD, FSPGM(Passphrase, "Passphrase"));

            stationGroup.end();
            auto &apModeGroup = form.addCardGroup(F("softap"), FSPGM(Access_Point), true);

            form.addObjectGetterSetter(F("ap_standby"), flags, flags.get_bit_is_softap_standby_mode_enabled, flags.set_bit_is_softap_standby_mode_enabled);
            form.addFormUI(F("AP Mode"), FormUI::BoolItems(F("Enable AP Mode if station is disconnected"), F("Enable AP Mode permanently")));

            auto &ssidHidden = form.addObjectGetterSetter(F("ap_hid"), flags, flags.get_bit_is_softap_ssid_hidden, flags.set_bit_is_softap_ssid_hidden);
            form.addFormUI(FormUI::Type::HIDDEN);

            form.addStringGetterSetter(F("ap_ssid"), Network::WiFi::getSoftApSSID, Network::WiFi::setSoftApSSID);
            Network::WiFi::addSoftApSSIDLengthValidator(form);
            form.addFormUI(FSPGM(SSID, "SSID"), FormUI::CheckboxButtonSuffix(ssidHidden, FSPGM(HIDDEN, "HIDDEN")));

            form.addStringGetterSetter(F("ap_pass"), Network::WiFi::getSoftApPassword, Network::WiFi::setSoftApPassword);
            Network::WiFi::addSoftApPasswordLengthValidator(form);
            form.addFormUI(FormUI::Type::PASSWORD, FSPGM(Passphrase));

            form.addObjectGetterSetter(F("ap_ch"), softAp, softAp.get_bits_channel, softAp.set_bits_channel);
            form.addValidator(FormUI::Validator::Range(1, config.getMaxWiFiChannels(), true));
            form.addFormUI(FSPGM(Channel), channelItems);

            form.addObjectGetterSetter(F("ap_enc"), softAp, softAp.get_bits_encryption, softAp.set_bits_encryption);
            form.addValidator(FormUI::Validator::Enum<const uint8_t, kWiFiEncryptionTypes.size()>(F("Invalid encryption"), kWiFiEncryptionTypes));
            form.addFormUI(FSPGM(Encryption), encryptionItems);

            apModeGroup.end();

        }
        else if (formName.equals(FSPGM(network))) {

            auto &network = Network::Settings::getWriteableConfig();
            auto &softAp = Network::SoftAP::getWriteableConfig();

            auto &ui = form.createWebUI();
            ui.setStyle(FormUI::WebUI::StyleType::ACCORDION);
            ui.setTitle(FSPGM(Network_Configuration));
            ui.setContainerId(FSPGM(network_settings));

            auto &deviceGroup = form.addCardGroup(FSPGM(device), FSPGM(Device), true);

            form.addStringGetterSetter(FSPGM(dev_hostn), System::Device::getName, System::Device::setName);
            form.addFormUI(FSPGM(Hostname));
            System::Device::addNameLengthValidator(form);

            form.addStringGetterSetter(FSPGM(dev_title), System::Device::getTitle, System::Device::setTitle);
            form.addFormUI(FSPGM(Title));
            System::Device::addTitleLengthValidator(form);

            auto &stationGroup = deviceGroup.end().addCardGroup(FSPGM(station), FSPGM(Station_Mode));

            form.addObjectGetterSetter(F("st_dhcp"), flags, flags.get_bit_is_station_mode_dhcp_enabled, flags.set_bit_is_station_mode_dhcp_enabled);
            form.addFormUI(FSPGM(DHCP_Client), FormUI::BoolItems());

            form.addObjectGetterSetter(F("st_ip"), FormGetterSetter(network, local_ip)).setOptional(true);
            form.addFormUI(FSPGM(IP_Address));
            network.addHostnameValidatorFor_local_ip(form);

            form.addObjectGetterSetter(F("st_subnet"), FormGetterSetter(network, subnet)).setOptional(true);
            form.addFormUI(FSPGM(Subnet));
            network.addHostnameValidatorFor_subnet(form);

            form.addObjectGetterSetter(F("st_gw"), FormGetterSetter(network, gateway)).setOptional(true);
            form.addFormUI(FSPGM(Gateway));
            network.addHostnameValidatorFor_gateway(form);

            form.addObjectGetterSetter(F("st_dns1"), FormGetterSetter(network, dns1)).setOptional(true);
            form.addFormUI(FSPGM(DNS_1));
            network.addHostnameValidatorFor_dns1(form);

            form.addObjectGetterSetter(F("st_dns2"), FormGetterSetter(network, dns2)).setOptional(true);
            form.addFormUI(FSPGM(DNS_2));
            network.addHostnameValidatorFor_dns2(form);

            auto &apGroup = stationGroup.end().addCardGroup(FSPGM(ap_mode), FSPGM(Access_Point));

            form.addObjectGetterSetter(F("ap_dhcpd"), flags, flags.get_bit_is_softap_dhcpd_enabled, flags.set_bit_is_softap_dhcpd_enabled);
            form.addFormUI(FSPGM(DHCP_Server), FormUI::BoolItems());

            form.addObjectGetterSetter(F("ap_ip"), FormGetterSetter(softAp, address));
            form.addFormUI(FSPGM(IP_Address));
            softAp.addHostnameValidatorFor_address(form);

            form.addObjectGetterSetter(F("ap_subnet"), FormGetterSetter(softAp, subnet));
            form.addFormUI(FSPGM(Subnet));
            softAp.addHostnameValidatorFor_subnet(form);

            form.addObjectGetterSetter(F("ap_dhcpds"), FormGetterSetter(softAp, dhcp_start)).setOptional(true);
            form.addFormUI(F("DHCP Start IP"));
            softAp.addHostnameValidatorFor_dhcp_start(form);

            form.addObjectGetterSetter(F("ap_dhcpde"), FormGetterSetter(softAp, dhcp_end)).setOptional(true);
            form.addFormUI(F("DHCP End IP"));
            softAp.addHostnameValidatorFor_dhcp_end(form);

            apGroup.end();

        }
        else if (formName.equals(FSPGM(device))) {

            auto &cfg = System::Device::getWriteableConfig();

            auto &ui = form.createWebUI();
            ui.setStyle(FormUI::WebUI::StyleType::ACCORDION);
            ui.setTitle(FSPGM(Device_Configuration));
            ui.setContainerId(F("device_settings"));

            auto &deviceGroup = form.addCardGroup(FSPGM(device), FSPGM(Device), true);

            form.addStringGetterSetter(FSPGM(dev_title), System::Device::getTitle, System::Device::setTitle);
            form.addFormUI(FSPGM(Title));
            System::Device::addTitleLengthValidator(form);

            form.addObjectGetterSetter(F("safem_to"), cfg, cfg.get_bits_safe_mode_reboot_timeout_minutes, cfg.set_bits_safe_mode_reboot_timeout_minutes);
            form.addFormUI(F("Reboot Delay Running In Safe Mode"), FormUI::Suffix(FSPGM(minutes)), FormUI::IntAttribute(F("disabled-value"), 0));
            cfg.addRangeValidatorFor_safe_mode_reboot_timeout_minutes(form, true);

#if __LED_BUILTIN != IGNORE_BUILTIN_LED_PIN_ID
            auto ledItems = FormUI::List(
                System::DeviceConfig::StatusLEDModeType::OFF, F("Disable status LED"),
                System::DeviceConfig::StatusLEDModeType::SOLID_WHEN_CONNECTED, F("Solid when connected to WiFi"),
                System::DeviceConfig::StatusLEDModeType::OFF_WHEN_CONNECTED, F("Off when connected to WiFi")
            );

            form.addObjectGetterSetter(F("led_mode"), cfg, System::Device::ConfigStructType::get_int_status_led_mode, System::Device::ConfigStructType::set_int_status_led_mode);
            form.addFormUI(FSPGM(Status_LED_Mode), ledItems);
#endif

            form.addStringGetterSetter(F("pbl"), System::Firmware::getPluginBlacklist, System::Firmware::setPluginBlacklist);
            form.addFormUI(F("Plugin Blacklist"), FormUI::Suffix(F("Comma Separated")));

            deviceGroup.end();
            auto &discoveryGroup = form.addCardGroup(F("ndo"), F("Network"), true);

#if ENABLE_DEEP_SLEEP

            form.addObjectGetterSetter(F("dslpsip"), flags, System::Flags::ConfigStructType::get_bit_use_static_ip_during_wakeup, System::Flags::ConfigStructType::set_bit_use_static_ip_during_wakeup);
            form.addFormUI(F("DHCP during wakeup"), FormUI::BoolItems(F("Use static IP address if possible"), F("Always use DHCP if enabled")));
#endif

#if MDNS_PLUGIN

            form.addObjectGetterSetter(F("mdns_en"), flags, System::Flags::ConfigStructType::get_bit_is_mdns_enabled, System::Flags::ConfigStructType::set_bit_is_mdns_enabled);
            form.addFormUI(F("mDNS Announcements"), FormUI::BoolItems(FSPGM(Enabled), F("Disabled (Zeroconf is still available)")));

#if MDNS_NETBIOS_SUPPORT
            form.addObjectGetterSetter(F("nbns_en"), flags, System::Flags::ConfigStructType::get_bit_is_netbios_enabled, System::Flags::ConfigStructType::set_bit_is_netbios_enabled);
            form.addFormUI(F("NetBIOS Enabled"), FormUI::BoolItems(F("Enabled (requires mDNS)"), F("Disabled")));
#endif

            form.addObjectGetterSetter(F("zconf_to"), cfg, cfg.get_bits_zeroconf_timeout, cfg.set_bits_zeroconf_timeout);
            form.addFormUI(FSPGM(Zeroconf_Timeout), FormUI::Suffix(FSPGM(milliseconds)));
            cfg.addRangeValidatorFor_zeroconf_timeout(form);

            form.addObjectGetterSetter(F("zconf_log"), cfg, System::Device::ConfigStructType::get_bits_zeroconf_logging, System::Device::ConfigStructType::set_bits_zeroconf_logging);
            form.addFormUI(FSPGM(Zeroconf_Logging), FormUI::BoolItems());

#endif

            form.addObjectGetterSetter(F("ssdp_en"), flags, System::Flags::ConfigStructType::get_bit_is_ssdp_enabled, System::Flags::ConfigStructType::set_bit_is_ssdp_enabled);
            form.addFormUI(FSPGM(SSDP_Discovery), FormUI::BoolItems());

            discoveryGroup.end();
            auto &webUIGroup = form.addCardGroup(F("webui"), FSPGM(WebUI), true);

            form.addObjectGetterSetter(F("wui_en"), flags, System::Flags::ConfigStructType::get_bit_is_webui_enabled, System::Flags::ConfigStructType::set_bit_is_webui_enabled);
            form.addFormUI(FSPGM(WebUI), FormUI::BoolItems());

            form.addObjectGetterSetter(F("scookie_lt"), cfg, System::Device::ConfigStructType::get_bits_webui_cookie_lifetime_days, System::Device::ConfigStructType::set_bits_webui_cookie_lifetime_days);
            form.addFormUI(F("Allow to store credentials in a cookie to login automatically"), FormUI::Suffix(FSPGM(days)));
            cfg.addRangeValidatorFor_webui_cookie_lifetime_days(form);

            webUIGroup.end();

            auto &schedulerGroup = form.addCardGroup(F("scheduler"), F("Task Scheduler"), true);

            // form.add(F("schet"), scheduledTasks.toString(), FormUI::InputFieldType::TEXTAREA);
            // form.addFormUI(F("Scheduler"), FormUI::Type::TEXTAREA, FormUI::IntAttribute(F("rows"), 8));

            schedulerGroup.end();

        }
        else if (formName.equals(FSPGM(password))) {

            auto &ui = form.createWebUI();
            ui.setStyle(FormUI::WebUI::StyleType::ACCORDION);
            ui.setTitle(FSPGM(Change_Password));
            ui.setContainerId(F("password_settings"));

            auto &group = form.addCardGroup(FSPGM(config));

            auto password = String(System::Device::getPassword());

            form.add(FSPGM(password), emptyString, FormUI::InputFieldType::TEXT);
            form.addFormUI(FormUI::Type::PASSWORD, F("Current Password"));
            form.addValidator(FormUI::Validator::Match(F("The entered password is not correct"), [](FormUI::Field::BaseField &field) {
                return field.getValue().equals(System::Device::getPassword());
            }));

            form.add(FSPGM(npwd), emptyString, FormUI::InputFieldType::TEXT);
            form.addFormUI(FormUI::Type::NEW_PASSWORD, F("New Password"));
            form.addValidator(FormUI::Validator::Length(System::Device::kPasswordMinSize, System::Device::kPasswordMaxSize));

            form.add(F("cpwd"), emptyString, FormUI::InputFieldType::TEXT);
            form.addFormUI(FormUI::Type::NEW_PASSWORD, F("Confirm New Password"));
            form.addValidator(FormUI::Validator::Match(F("The password confirmation does not match"), [](FormUI::Field::BaseField &field) {
                return field.equals(field.getForm().getField(FSPGM(npwd)));
            }));

            group.end();

        }
        else {
            return;
        }
        form.finalize();
    }
}

void KFCConfigurationPlugin::_formatSchedulerList(String &items)
{
    //TODO
    StringVector list;
    explode(items.c_str(), '\n', list);
    items.remove(0);
    for(auto &item: list) {
        item.trim();
        if (item.length()) {
            auto sep = item.indexOf(',');
            if (sep == -1) {
                items += F("!INVALID,");
                items += item;
                items += '\n';
            }
            else {
                auto col1 = item.begin();
                auto col2 = col1 + sep;
                *col2++ = 0;
                while(isspace(*col2)) {
                    col2++;
                }
                if (strchr(col1, '!')) {
                }
                else {
                    char *endptr = nullptr;
                    auto value = strtof(col1, &endptr);
                    if (value < 1) {
                        items += F("!INVALID ");
                    }
                    if (!*col2) {
                        items += F("!INVALID ");
                    }
                }
                items += col1;
                items += ',';
                items += col2;
                items += '\n';
            }
        }
    }
}
