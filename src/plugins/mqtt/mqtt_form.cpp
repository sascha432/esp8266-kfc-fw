/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <KFCForms.h>
#include <kfc_fw_config.h>
#include <PrintHtmlEntitiesString.h>
#include "mqtt_plugin.h"
#include "plugins_menu.h"

#if DEBUG_MQTT_CLIENT
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using KFCConfigurationClasses::System;

using namespace MQTT;

#if MQTT_AUTO_DISCOVERY
void Plugin::createMenu()
{
    bootstrapMenu.getMenuItem(navMenu.config).addMenuItem(getFriendlyName(), F("mqtt.html"));

    auto device = bootstrapMenu.getMenuItem(navMenu.device);
    device.addDivider();
    device.addMenuItem(F("Publish MQTT Auto Discovery"), F("mqtt-publish-ad.html"));
}
#endif

void Plugin::createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request)
{
    using namespace KFCConfigurationClasses::Plugins::MQTTConfigNS;

    if (type == FormCallbackType::SAVE) {
        // update flags
        System::Flags::getWriteableConfig().is_mqtt_enabled = (MqttClient::getConfig()._get_enum_mode() != ModeType::DISABLED);
        return;
    }
    else if (!isCreateFormCallbackType(type)) {
        return;
    }

    auto &cfg = MqttClient::getWriteableConfig();

    FormUI::Container::List modeItems(
        ModeType::DISABLED, FSPGM(Disabled),
        ModeType::UNSECURE, FSPGM(Enabled)
    );
    FormUI::Container::List qosItems(
        QosType::AT_MOST_ONCE, F("At most once (0)"),
        QosType::AT_LEAST_ONCE, F("At least once (1)"),
        QosType::EXACTLY_ONCE, F("Exactly once (2)")
#if ASYNC_TCP_SSL_ENABLED
       ,ModeType::SECURE, F("TLS/Secure MQTT")
#endif
    );

    if (!System::Flags::getConfig().is_mqtt_enabled) {
        cfg._set_enum_mode(ModeType::DISABLED); // set to disabled before adding value to form
    }

    auto &ui = form.createWebUI();
    ui.setTitle(F("MQTT Client Configuration"));
    ui.setContainerId(F("mqtt_setttings"));
    ui.setStyle(FormUI::WebUI::StyleType::ACCORDION);

    auto &cfgGroup = form.addDivGroup(F(""), F("{'i':'#mode','e':'var $T=$(\\'#mode\\').closest(\\'.card\\').nextAll().not(\\':last\\');var $P=function(v){$(\\'#port\\').attr(\\'placeholder\\',v);}','s':{'0':'$T.hide()','1':'$T.show();$P(1883)','2':'$T.show();$P(8883)'}}"));
    cfgGroup.end();

    auto &commonGroup = form.addCardGroup(FSPGM(config));

    form.addObjectGetterSetter(FSPGM(mode), FormGetterSetter(cfg, mode));
    form.addFormUI(FSPGM(Mode), modeItems);
    form.addValidator(FormUI::Validator::EnumRange<ModeType>());

    auto &connGroup = commonGroup.end().addCardGroup(F("conn"), FSPGM(Connection), true);

    form.addStringGetterSetter(FSPGM(host), MqttClient::getHostname, MqttClient::setHostname);
    form.addFormUI(FSPGM(Hostname), FormUI::ZeroconfSuffix());
    form.addValidator(FormUI::Validator::Hostname(FormUI::AllowedType::HOST_OR_IP_OR_ZEROCONF));
    form.addValidator(FormUI::Validator::Length(3, MqttClient::kHostnameMaxSize));

    form.addCallbackSetter(FSPGM(port), cfg.getPortAsString(), [&cfg](const String &value, FormField &field) {
        cfg.setPort(value.toInt());
        field.setValue(cfg.getPortAsString());
    });
    form.addFormUI(FormUI::Type::NUMBER, FSPGM(Port), FormUI::PlaceHolder(MqttConfigType::kPortDefault));
    form.addValidator(FormUI::Validator::NetworkPort(true));

    form.addObjectGetterSetter(F("kat"), cfg, cfg.get_bits_keepalive, cfg.set_bits_keepalive);
    form.addFormUI(FSPGM(Keep_Alive), FormUI::Suffix(FSPGM(seconds)));
    cfg.addRangeValidatorFor_keepalive(form);

    form.addObjectGetterSetter(F("ami"), cfg, cfg.get_bits_auto_reconnect_min, cfg.set_bits_auto_reconnect_min);
    form.addFormUI(F("Minimum Auto Reconnect Time:"), FormUI::Suffix(F("milliseconds")), FormUI::IntAttribute(F("disabled-value"), 0));
    cfg.addRangeValidatorFor_auto_reconnect_min(form, true);

    form.addObjectGetterSetter(F("amx"), cfg, cfg.get_bits_auto_reconnect_max, cfg.set_bits_auto_reconnect_max);
    form.addFormUI(F("Maximum Auto Reconnect Time:"), FormUI::Suffix(F("milliseconds")));
    cfg.addRangeValidatorFor_auto_reconnect_max(form);

    form.addObjectGetterSetter(F("ari"), cfg, cfg.get_bits_auto_reconnect_incr, cfg.set_bits_auto_reconnect_incr);
    form.addFormUI(F("Auto Reconnect Increment:"), FormUI::Suffix(F("%")));
    cfg.addRangeValidatorFor_auto_reconnect_incr(form);

    auto &serverGroup = connGroup.end().addCardGroup(FSPGM(mqtt), F("Server Settings"), true);

    form.addStringGetterSetter(F("mu"), MqttClient::getUsername, MqttClient::setUsername);
    form.addFormUI(FSPGM(Username), FormUI::PlaceHolder(FSPGM(Anonymous)));
    form.addValidator(FormUI::Validator::Length(0, MqttClient::kUsernameMaxSize));

    form.addStringGetterSetter(F("mqttpass"), MqttClient::getPassword, MqttClient::setPassword);
    form.addFormUI(FormUI::Type::PASSWORD, FSPGM(Password));
    MqttClient::addPasswordLengthValidator(form, true);

    form.addStringGetterSetter(F("t"), MqttClient::getBaseTopic, MqttClient::setBaseTopic);
    MqttClient::addBaseTopicLengthValidator(form);
    form.addFormUI(FSPGM(Topic));

#if MQTT_GROUP_TOPIC

    form.addStringGetterSetter(F("gt"), ClientConfig::getGroupTopic, ClientConfig::setGroupTopic);
    ClientConfig::addGroupTopicLengthValidator(form, true);
    form.addFormUI(F("Group Topic"));

#endif

    form.addObjectGetterSetter(F("qos"), cfg, cfg.get_enum_qos, cfg.set_enum_qos);
    form.addFormUI(F("Quality Of Service"), qosItems);
    form.addValidator(FormUI::Validator::EnumRange<QosType>(F("Invalid value for QoS")));

    serverGroup.end();

#if MQTT_AUTO_DISCOVERY
    auto &hassGroup = form.addCardGroup(F("adp"), F("Home Assistant"), true);

    form.addObjectGetterSetter(F("ade"), cfg, cfg.get_bits_auto_discovery, cfg.set_bits_auto_discovery);
    form.addFormUI(F("Auto Discovery"), FormUI::BoolItems());

    auto &autoDiscoveryGroup = form.addDivGroup(F("adp"), F("{'i':'#ade','m':'$T.hide()','s':{'1':'$T.show()'}}"));

    form.addStringGetterSetter(F("pf"), MqttClient::getAutoDiscoveryPrefix, MqttClient::setAutoDiscoveryPrefix);
    form.addFormUI(F("Auto Discovery Prefix"));
    MqttClient::addAutoDiscoveryPrefixLengthValidator(form, true);
    form.addValidator(FormUI::Validator::Length(0, MqttClient::kAutoDiscoveryPrefixMaxSize));

    form.addObjectGetterSetter(F("add"), FormGetterSetter(cfg, auto_discovery_delay));
    form.addFormUI(F("Auto Discovery Delay"), FormUI::Suffix(FSPGM(seconds)), FormUI::IntAttribute(F("disabled-value"), 0), FormUI::FPStringAttribute(F("disabled-value-targets"), F("#adi")));
    cfg.addRangeValidatorFor_auto_discovery_delay(form, true);

    form.addObjectGetterSetter(F("adi"), FormGetterSetter(cfg, auto_discovery_rebroadcast_interval));
    form.addFormUI(F("Auto Discovery Rebroadcast"), FormUI::Suffix(FSPGM(minutes)), FormUI::IntAttribute(F("disabled-value"), 0));
    cfg.addRangeValidatorFor_auto_discovery_rebroadcast_interval(form, true);

    #if MQTT_AUTO_DISCOVERY_USE_NAME

        form.addStringGetterSetter(F("adn"), MqttClient::getAutoDiscoveryName, MqttClient::setAutoDiscoveryName);
        form.addFormUI(F("Auto Discovery Name"), FormUI::PlaceHolder(F("Disabled")));
        MqttClient::addAutoDiscoveryNameLengthValidator(form, true);

    #endif

    autoDiscoveryGroup.end();
    hassGroup.end();
#endif

    form.finalize();
}
