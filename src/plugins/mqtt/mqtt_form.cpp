/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <KFCForms.h>
#include <kfc_fw_config.h>
#include <PrintHtmlEntitiesString.h>
#include "../src/generated/FlashStringGeneratorAuto.h"
#include "mqtt_plugin.h"

#if DEBUG_MQTT_CLIENT
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

void MQTTPlugin::createConfigureForm(FormCallbackType type, const String &formName, Form &form, AsyncWebServerRequest *request)
{
    using ClientConfig = MQTTClient::ClientConfig;

    if (type == FormCallbackType::SAVE) {
        // update flags
        MQTTClient::Flags::getWriteable().mqttEnabled = (ClientConfig::getWriteableConfig().mode_enum != ClientConfig::ModeType::DISABLED);
        return;
    }
    else if (!isCreateFormCallbackType(type)) {
        return;
    }

    form.setFormUI(F("MQTT Client Configuration"));

    auto &cfg = ClientConfig::getWriteableConfig();

    FormUI::ItemsList modeItems;
    FormUI::ItemsList qosItems;

    modeItems.emplace_back(enumToString(ClientConfig::ModeType::DISABLED), FSPGM(Disabled));
#if ASYNC_TCP_SSL_ENABLED
    modeItems.emplace_back(enumToString(ClientConfig::ModeType::AUTO), F("Automatically depending on the port"));
#endif
    modeItems.emplace_back(enumToString(ClientConfig::ModeType::UNSECURE), FSPGM(Enabled));
#if ASYNC_TCP_SSL_ENABLED
    modeItems.emplace_back(enumToString(ClientConfig::ModeType::SECURE), F("TLS/Secure MQTT"));
#endif

    qosItems.emplace_back(enumToString(ClientConfig::QosType::AT_MODE_ONCE), F("At most once (0)"));
    qosItems.emplace_back(enumToString(ClientConfig::QosType::AT_LEAST_ONCE), F("At least once (1)"));
    qosItems.emplace_back(enumToString(ClientConfig::QosType::EXACTLY_ONCE), F("Exactly once (2)"));

    form.add<uint8_t>(FSPGM(mode), _H_W_STRUCT_VALUE(cfg, mode))->setFormUI((new FormUI(FormUI::TypeEnum_t::SELECT, FSPGM(Mode)))->addItems(modeItems));
    form.addValidator(new FormRangeValidatorEnum<ClientConfig::ModeType>());

    auto &cfgGroup = form.addDivGroup(F("mqttcfg"), F("{'i':'#mode','e':'var $P=function(v){$(\\'#port\\').attr(\\'placeholder\\',v);}','s':{'0':'$T.show()','1':'$T.show();$P(\\'\\')','2':'$T.show();$P(1883)','3':'$T.show();$P(8883)'}}"));

    form.add(FSPGM(host), _H_STR_FUNC(ClientConfig::getHostname, ClientConfig::setHostname))->setFormUI((new FormUI(FormUI::TypeEnum_t::TEXT, FSPGM(Hostname))));
    form.addValidator(new FormValidHostOrIpValidator(FormValidHostOrIpValidator::ALLOW_ZEROCONF));
    form.addValidator(new FormLengthValidator(3, ClientConfig::kHostnameMaxSize));

    form.add<uint16_t>(FSPGM(port), _H_W_STRUCT_VALUE(cfg, port))->setFormUI((new FormUI(FormUI::TypeEnum_t::INTEGER, FSPGM(Port)))->setPlaceholder(String(1883)));
    form.addValidator(new FormTCallbackValidator<uint16_t>([&cfg](uint16_t value, FormField &field) {
#if ASYNC_TCP_SSL_ENABLED
        if (value == 0 && static_cast<FormBitValue<ConfigFlags_t, 3> *>(field.getForm().getField(F("mqtt_mode")))->getValue() == MQTT_MODE_SECURE) {
            if (cfg.port != 0) {
                value = cfg.port;
            }
            field.setValue(String(value));
        } else
#endif
        if (value == 0) {
            if (cfg.port != 0) {
                value = cfg.port;
            }
            field.setValue(String(value));
        }
        return true;
    }));
    form.addValidator(new FormRangeValidator(FSPGM(Invalid_port), 1, 65535));

    form.add(F("mqttuser"), _H_STR_FUNC(ClientConfig::getUsername, ClientConfig::setUsername))->setFormUI((new FormUI(FormUI::TypeEnum_t::TEXT, FSPGM(Username)))->setPlaceholder(FSPGM(Anonymous)));
    form.addValidator(new FormLengthValidator(0, ClientConfig::kUsernameMaxSize));

    form.add(F("mqttpass"), _H_STR_FUNC(ClientConfig::getPassword, ClientConfig::setPassword))->setFormUI((new FormUI(FormUI::TypeEnum_t::PASSWORD, FSPGM(Password))));
    form.addValidator(new FormLengthValidator(0, ClientConfig::kPasswordMaxSize));

    form.add(FSPGM(topic), _H_STR_FUNC(ClientConfig::getTopic, ClientConfig::setTopic))->setFormUI((new FormUI(FormUI::TypeEnum_t::TEXT, FSPGM(Topic))));
    form.addValidator(new FormLengthValidator(3, ClientConfig::kTopicMaxSize));

    form.add<uint8_t>(F("qos"), _H_W_STRUCT_VALUE(cfg, qos))->setFormUI((new FormUI(FormUI::TypeEnum_t::SELECT, F("Quality Of Service")))->addItems(qosItems));
    form.addValidator(new FormRangeValidatorEnum<ClientConfig::QosType>(F("Invalid value for QoS")));

    form.add<uint8_t>(FSPGM(keepalive), _H_W_STRUCT_VALUE(cfg, keepalive))->setFormUI((new FormUI(FormUI::TypeEnum_t::TEXT, FSPGM(Keep_Alive, "Keep Alive")))->setSuffix(FSPGM(seconds)));
    form.addValidator(new FormRangeValidatorType<decltype(cfg.keepalive)>());

#if MQTT_AUTO_DISCOVERY
    form.add<bool>(F("aden"), _H_W_STRUCT_VALUE(cfg, auto_discovery))->setFormUI((new FormUI(FormUI::TypeEnum_t::SELECT, F("Home Assistant Auto Discovery")))->setBoolItems());

    auto &autoDiscoveryGroup = form.addDivGroup(F("adp"), F("{'i':'#aden','m':'$T.hide()','s':{'2':'$T.show()'}}"));

    form.add(FSPGM(prefix), _H_STR_FUNC(ClientConfig::getAutoDiscoveryPrefix, ClientConfig::setAutoDiscoveryPrefix))->setFormUI((new FormUI(FormUI::TypeEnum_t::TEXT, F("Auto Discovery Prefix"))));
    form.addValidator(new FormLengthValidator(0, ClientConfig::kDiscoveryPrefixMaxSize));

    autoDiscoveryGroup.end();
#endif
    cfgGroup.end();

    form.finalize();
}
