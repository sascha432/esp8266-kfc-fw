/**
 * Author: sascha_lammers@gmx.de
 */

#include "switch.h"
#include <LoopFunctions.h>
#include <PrintHtmlEntities.h>
#include <KFCForms.h>
#include <ESPAsyncWebServer.h>
#include <EventTimer.h>
#include <StreamString.h>
#include <WebUISocket.h>
#include "PluginComponent.h"
#include "plugins.h"

#if DEBUG_IOT_SWITCH
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

PROGMEM_STRING_DEF(iot_switch_states_file, "/switch.states");

SwitchPlugin plugin;

SwitchPlugin::SwitchPlugin() : MQTTComponent(ComponentTypeEnum_t::SWITCH), _states(0), _pins({IOT_SWITCH_CHANNEL_PINS})
{
    REGISTER_PLUGIN(this);
}

void SwitchPlugin::setup(SetupModeType mode)
{
    _readConfig();
    _readStates();
    for (size_t i = 0; i < _pins.size(); i++) {
        if (_configs[i].state == SwitchStateEnum::RESTORE) {
            _setChannel(i, _states & (1 << i));
        }
        else {
            _setChannel(i, _configs[i].state);
        }
        pinMode(_pins[i], OUTPUT);
    }
    auto mqttClient = MQTTClient::getClient();
    if (mqttClient) {
        mqttClient->registerComponent(this);
    }
#if IOT_SWITCH_PUBLISH_MQTT_INTERVAL
    _updateTimer.add(IOT_SWITCH_PUBLISH_MQTT_INTERVAL, true, [this](EventScheduler::TimerPtr) {
        _publishState(MQTTClient::getClient());
    });
#endif
}

void SwitchPlugin::shutdown()
{
#if IOT_SWITCH_PUBLISH_MQTT_INTERVAL
    _updateTimer.remove();
#endif
#if IOT_SWITCH_STORE_STATES
    if (_delayedWrite.active()) {
        _delayedWrite->detach(); // stop timer and store now
        _delayedWrite->getCallback()(nullptr);
    }
#endif
}

void SwitchPlugin::reconfigure(PGM_P source)
{
    _readConfig();
}

void SwitchPlugin::getStatus(Print &output)
{
    output.printf_P(PSTR("%u channel switch"), _pins.size());
    for (size_t i = 0; i < _pins.size(); i++) {
        output.printf_P(PSTR(HTML_S(br) "%s - %s"), _getChannelName(i).c_str(), _getChannel(i) ? SPGM(On) : SPGM(Off));
    }
}

void SwitchPlugin::createConfigureForm(AsyncWebServerRequest *request, Form &form)
{
    using KFCConfigurationClasses::Plugins;

    form.setFormUI(F("Switch Configuration"));

    FormUI::ItemsList states;
    states.emplace_back(String(SwitchStateEnum::OFF), FSPGM(Off));
    states.emplace_back(String(SwitchStateEnum::ON), FSPGM(On));
    states.emplace_back(String(SwitchStateEnum::RESTORE), F("Restore Last State"));

    FormUI::ItemsList webUI;
    webUI.emplace_back(String(WebUIEnum::NONE), F("None"));
    webUI.emplace_back(String(WebUIEnum::HIDE), F("Hide"));
    webUI.emplace_back(String(WebUIEnum::NEW_ROW), F("New row after switch"));

    for (size_t i = 0; i < _pins.size(); i++) {

        FormGroup *group = nullptr;
        if (_pins.size() > 1) {
            group = &form.addGroup(PrintString(FSPGM(channel__u), i), PrintString(F("Channel %u"), i), true);
        }

        form.add(PrintString(F("name[%u]"), i), _names[i], [this, i](const String &name, FormField &, bool) {
            _names[i] = name;
            return false;
        }, FormField::InputFieldType::TEXT)->setFormUI(new FormUI(FormUI::TEXT, F("Name")));

        form.add<SwitchStateEnum>(PrintString(F("state[%u]"), i), _configs[i].state, [this, i](SwitchStateEnum state, FormField &, bool) {
            _configs[i].state = state;
            return false;
        }, FormField::InputFieldType::SELECT)->setFormUI((new FormUI(FormUI::SELECT, F("Default State")))->addItems(states));

        form.add<WebUIEnum>(PrintString(F("webui[%u]"), i), _configs[i].webUI, [this, i](WebUIEnum webUI, FormField &, bool) {
            _configs[i].webUI = webUI;
            return false;
        }, FormField::InputFieldType::SELECT)->setFormUI((new FormUI(FormUI::SELECT, F("WebUI")))->addItems(webUI));

        if (group) {
            group->end();
        }
    }

    form.setValidateCallback([this](Form &form) {
        if (form.isValid()) {
            _debug_println(F("Storing config"));
            Plugins::IOTSwitch::setConfig(_names, _configs);
            return true;
        }
        return false;
    });

    form.finalize();
}

void SwitchPlugin::createWebUI(WebUI &webUI)
{
    auto row = &webUI.addRow();
    row->setExtraClass(FSPGM(title));
    row->addGroup(F("Switch"), false);

    row = &webUI.addRow();
    for (size_t i = 0; i < _pins.size(); i++) {
        if (_configs[i].webUI != WebUIEnum::HIDE) {
            PrintString name;
            if (_names[i].length()) {
                name = _names[i];
            }
            else {
                name = PrintString(F("Channel %u"), i);
            }
            row->addSwitch(PrintString(FSPGM(channel__u), i), name, true, true);
        }
        if (_configs[i].webUI == WebUIEnum::NEW_ROW) {
            row = &webUI.addRow();
        }
    }

}

void SwitchPlugin::getValues(JsonArray &array)
{
    _debug_println();

    for (size_t i = 0; i < _pins.size(); i++) {
        auto obj = &array.addObject(2);
        obj->add(JJ(id), PrintString(FSPGM(channel__u), i));
        obj->add(JJ(value), (int)_getChannel(i));
        obj->add(JJ(state), true);
    }
}

void SwitchPlugin::setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState)
{
    _debug_printf_P(PSTR("id=%s value=%s hasValue=%u state=%u hasState=%u\n"), id.c_str(), value.c_str(), hasValue, state, hasState);

    auto ptr = id.c_str();
    if (!strncmp_P(ptr, PSTR("channel_"), 8) && hasValue) {
        bool state = value.toInt();
        ptr += 8;
        uint8_t channel = (uint8_t)atoi(ptr);
        if (channel <_pins.size()) {
            _setChannel(channel, state);
            _publishState(MQTTClient::getClient(), channel);
        }
    }
}

MQTTComponent::MQTTAutoDiscoveryPtr SwitchPlugin::nextAutoDiscovery(MQTTAutoDiscovery::FormatType format, uint8_t num)
{
    if (num >= getAutoDiscoveryCount()) {
        return nullptr;
    }
    auto discovery = new MQTTAutoDiscovery();
    auto channel = PrintString(FSPGM(channel__u), num);
    discovery->create(this, channel, format);
    discovery->addStateTopic(MQTTClient::formatTopic(channel, FSPGM(_state)));
    discovery->addCommandTopic(MQTTClient::formatTopic(channel, FSPGM(_set)));
    discovery->addPayloadOn(1);
    discovery->addPayloadOff(0);
    discovery->finalize();
    return discovery;
}

uint8_t SwitchPlugin::getAutoDiscoveryCount() const
{
    return _pins.size();
}

void SwitchPlugin::onConnect(MQTTClient *client)
{
    _debug_println();
    MQTTComponent::onConnect(client);
    auto qos = client->getDefaultQos();
    for (size_t i = 0; i < _pins.size(); i++) {
        client->subscribe(this, MQTTClient::formatTopic(PrintString(FSPGM(channel__u), i), FSPGM(_set)), qos);
    }
    _publishState(client);
}

void SwitchPlugin::onMessage(MQTTClient *client, char *topic, char *payload, size_t len)
{
    _debug_printf_P(PSTR("topic=%s payload=%s\n"), topic, payload);
    for (size_t i = 0; i < _pins.size(); i++) {
        if (MQTTClient::formatTopic(PrintString(FSPGM(channel__u), i), FSPGM(_set)).equals(topic)) {
            bool state = atoi(payload);
            _setChannel(i, state);
            _publishState(client, i);
        }
    }
}

void SwitchPlugin::_setChannel(uint8_t channel, bool state)
{
    _debug_printf_P(PSTR("channel=%u state=%u\n"), channel, state);
    digitalWrite(_pins[channel], state ? IOT_SWITCH_ON_STATE : !IOT_SWITCH_ON_STATE);
    if (state) {
        _states |= (1 << channel);
    }
    else {
        _states &= ~(1 << channel);
    }
    _writeStates();
}

bool SwitchPlugin::_getChannel(uint8_t channel) const
{
    return (digitalRead(_pins[channel]) == IOT_SWITCH_ON_STATE);
}

String SwitchPlugin::_getChannelName(uint8_t channel) const
{
    return _names[channel].length() ? _names[channel] : PrintString(F("Channel %u"), channel);
}

void SwitchPlugin::_readConfig()
{
    KFCConfigurationClasses::Plugins::IOTSwitch::getConfig(_names, _configs);
}

void SwitchPlugin::_readStates()
{
#if IOT_SWITCH_STORE_STATES
    File file = SPIFFS.open(FSPGM(iot_switch_states_file), fs::FileOpenMode::read);
    if (file) {
        if (file.read(reinterpret_cast<uint8_t *>(&_states), sizeof(_states)) != sizeof(_states)) {
            _states = 0;
        }
    }
#endif
    _debug_printf_P(PSTR("states=%s\n"), String(_states, 2).c_str());
}

void SwitchPlugin::_writeStates()
{
    _debug_printf_P(PSTR("states=%s\n"), String(_states, 2).c_str());
#if IOT_SWITCH_STORE_STATES
    _delayedWrite.remove();
    _delayedWrite.add(IOT_SWITCH_STORE_STATES_WRITE_DELAY, false, [this](EventScheduler::TimerPtr) {
        _debug_printf_P(PSTR("SwitchPlugin::_writeStates(): delayed write states=%s\n"), String(_states, 2).c_str());
        File file = SPIFFS.open(FSPGM(iot_switch_states_file), fs::FileOpenMode::write);
        if (file) {
            file.write(reinterpret_cast<const uint8_t *>(&_states), sizeof(_states));
        }
    });
#endif
}

void SwitchPlugin::_publishState(MQTTClient *client, int8_t channel)
{
    if (client) {
        auto qos = client->getDefaultQos();
        for (size_t i = 0; i < _pins.size(); i++) {
            if (channel == -1 || (uint8_t)channel == i) {
                _debug_printf_P(PSTR("pin=%u state=%u\n"), _pins[i], _getChannel(i));
                client->publish(MQTTClient::formatTopic(PrintString(FSPGM(channel__u), i), FSPGM(_state)), qos, true, String(_getChannel(i) ? 1 : 0));
            }
        }
    }

    JsonUnnamedObject json(2);
    json.add(JJ(type), JJ(ue));
    auto &events = json.addArray(JJ(events));
    JsonUnnamedObject *obj;
    for (size_t i = 0; i < _pins.size(); i++) {
        if (channel == -1 || (uint8_t)channel == i) {
            obj = &events.addObject(2);
            obj->add(JJ(id), PrintString(FSPGM(channel__u), i));
            obj->add(JJ(value), (int)_getChannel(i));
            obj->add(JJ(state), true);
        }
    }
    if (events.size()) {
        auto buffer = std::shared_ptr<StreamString>(new StreamString());
        json.printTo(*buffer);

        LoopFunctions::callOnce([this, buffer]() {
            WsClient::broadcast(WsWebUISocket::getWsWebUI(), WsWebUISocket::getSender(), buffer->c_str(), buffer->length());
        });
    }
}
