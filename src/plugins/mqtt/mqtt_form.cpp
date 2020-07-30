/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <KFCForms.h>
#include <kfc_fw_config.h>
#include <PrintHtmlEntitiesString.h>
#include "mqtt_plugin.h"

#if DEBUG_MQTT_CLIENT
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using KFCConfigurationClasses::System;

void MQTTPlugin::createConfigureForm(FormCallbackType type, const String &formName, Form &form, AsyncWebServerRequest *request)
{
    using ClientConfig = MQTTClient::ClientConfig;

    if (type == FormCallbackType::SAVE) {
        // update flags
        System::Flags::getWriteableConfig().is_mqtt_enabled = (ClientConfig::ConfigStructType::get_enum_mode(ClientConfig::getWriteableConfig()) != ClientConfig::ModeType::DISABLED);
        return;
    }
    else if (!isCreateFormCallbackType(type)) {
        return;
    }

    form.setFormUI(F("MQTT Client Configuration"));

    auto &cfg = ClientConfig::getWriteableConfig();

    auto modeItems = FormUI::ItemsList(
        ClientConfig::ModeType::DISABLED, FSPGM(Disabled),
        ClientConfig::ModeType::UNSECURE, FSPGM(Enabled)
    );
    auto qosItems = FormUI::ItemsList(
        ClientConfig::QosType::AT_MODE_ONCE, F("At most once (0)"),
        ClientConfig::QosType::AT_LEAST_ONCE, F("At least once (1)"),
        ClientConfig::QosType::EXACTLY_ONCE, F("Exactly once (2)")
#if ASYNC_TCP_SSL_ENABLED
       ,ClientConfig::ModeType::SECURE, F("TLS/Secure MQTT")
#endif
    );

    if (!System::Flags::getConfig().is_mqtt_enabled) {
        ClientConfig::ConfigStructType::set_enum_mode(cfg, ClientConfig::ModeType::DISABLED); // set to disabled before adding value to form
    }

    form.add<uint8_t>(FSPGM(mode), _H_W_STRUCT_VALUE(cfg, mode));
    form.addFormUI(FSPGM(Mode), modeItems);
    form.addValidator(FormRangeValidatorEnum<ClientConfig::ModeType>());

    auto &cfgGroup = form.addDivGroup(F("mqttcfg"), F("{'i':'#mode','e':'var $P=function(v){$(\\'#port\\').attr(\\'placeholder\\',v);}','s':{'0':'$T.hide()','1':'$T.show();$P(1883)','2':'$T.show();$P(8883)'}}"));

    form.add(FSPGM(host), _H_CSTR_FUNC(ClientConfig::getHostname, ClientConfig::setHostname));
    form.addFormUI(FSPGM(Hostname));
    form.addValidator(FormHostValidator(FormHostValidator::AllowedType::ALLOW_ZEROCONF));
    form.addValidator(FormLengthValidator(3, ClientConfig::kHostnameMaxSize));


    form.add(FSPGM(port), cfg.getPortAsString(), [&cfg](const String &value, FormField &field, bool store) {
        if (store) {
            cfg.setPort(value.toInt(), cfg.isSecure());
            field.setValue(cfg.getPortAsString());
        }
        return false;
    });
    form.addFormUI(FormUI::Type::INTEGER, FSPGM(Port), FormUI::PlaceHolder(1883));
    form.addValidator(FormNetworkPortValidator(true));

    form.add(F("mqttuser"), _H_CSTR_FUNC(ClientConfig::getUsername, ClientConfig::setUsername));
    form.addFormUI(FSPGM(Username), FormUI::PlaceHolder(FSPGM(Anonymous)));
    form.addValidator(FormLengthValidator(0, ClientConfig::kUsernameMaxSize));

    form.add(F("mqttpass"), _H_CSTR_FUNC(ClientConfig::getPassword, ClientConfig::setPassword));
    form.addFormUI(FormUI::Type::PASSWORD, FSPGM(Password));
    form.addValidator(FormLengthValidator(0, ClientConfig::kPasswordMaxSize));

    form.add(FSPGM(topic), _H_CSTR_FUNC(ClientConfig::getTopic, ClientConfig::setTopic));
    form.addFormUI(FSPGM(Topic));
    form.addValidator(FormLengthValidator(3, ClientConfig::kTopicMaxSize));

    form.add<uint8_t>(F("qos"), _H_W_STRUCT_VALUE(cfg, qos));
    form.addFormUI(F("Quality Of Service"), qosItems);
    form.addValidator(FormRangeValidatorEnum<ClientConfig::QosType>(F("Invalid value for QoS")));

    form.add<uint8_t>(FSPGM(keepalive), _H_W_STRUCT_VALUE(cfg, keepalive));
    form.addFormUI(FSPGM(Keep_Alive, "Keep Alive"), FormUI::Suffix(FSPGM(seconds)));
    form.addValidator(FormRangeValidatorType<decltype(cfg.keepalive)>());

#if MQTT_AUTO_DISCOVERY
    form.addWriteableStruct("aden", cfg, auto_discovery));
    form.addFormUI(FormUI::Type::SELECT, F("Home Assistant Auto Discovery"));

    auto &autoDiscoveryGroup = form.addDivGroup(F("adp"), F("{'i':'#aden','m':'$T.hide()','s':{'1':'$T.show()'}}"));

    form.add(FSPGM(prefix), _H_CSTR_FUNC(ClientConfig::getAutoDiscoveryPrefix, ClientConfig::setAutoDiscoveryPrefix));
    form.addFormUI(F("Auto Discovery Prefix"));
    form.addValidator(FormLengthValidator(0, ClientConfig::kAutoDiscoveryPrefixMaxSize));

    autoDiscoveryGroup.end();
#endif

    cfgGroup.end();

    form.finalize();
}
