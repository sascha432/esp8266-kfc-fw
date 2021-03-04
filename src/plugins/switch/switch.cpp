/**
 * Author: sascha_lammers@gmx.de
 */

#include "switch.h"
#include <EventScheduler.h>
#include <fs_mapping.h>
#include <PrintHtmlEntities.h>
#include <KFCForms.h>
#include <ESPAsyncWebServer.h>
#include <StreamString.h>
#include <WebUISocket.h>
#include "PluginComponent.h"
#include "Utility/ProgMemHelper.h"
#include "plugins.h"

#if DEBUG_IOT_SWITCH
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using KFCConfigurationClasses::Plugins;

SwitchPlugin plugin;

PROGMEM_DEFINE_PLUGIN_OPTIONS(
    SwitchPlugin,
    "switch",           // name
    "Switch",           // friendly name
    "",                 // web_templates
    "switch",           // config_forms
    "",                 // reconfigure_dependencies
    PluginComponent::PriorityType::SWITCH,
    PluginComponent::RTCMemoryId::NONE,
    static_cast<uint8_t>(PluginComponent::MenuType::AUTO),
    false,              // allow_safe_mode
    false,              // setup_after_deep_sleep
    true,               // has_get_status
    true,               // has_config_forms
    true,               // has_web_ui
    false,              // has_web_templates
    false,              // has_at_mode
    0                   // __reserved
);

SwitchPlugin::SwitchPlugin() :
    PluginComponent(PROGMEM_GET_PLUGIN_OPTIONS(SwitchPlugin)),
    MQTTComponent(ComponentType::SWITCH),
    _states(0),
    _pins({IOT_SWITCH_CHANNEL_PINS})
{
    REGISTER_PLUGIN(this, "SwitchPlugin");
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
    _Timer(_updateTimer).add(IOT_SWITCH_PUBLISH_MQTT_INTERVAL, true, [this](Event::CallbackTimerPtr timer) {
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
    if (_delayedWrite) {
        _delayedWrite->_invokeCallback(nullptr);
        _delayedWrite.remove(); // stop timer and store now
    }
#endif
}

void SwitchPlugin::reconfigure(const String &source)
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

void SwitchPlugin::createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request)
{
    if (type == FormCallbackType::SAVE) {
        __LDBG_println(F("Storing config"));
        Plugins::IOTSwitch::setConfig(_names, _configs);
        return;
    }
    else if (!isCreateFormCallbackType(type)) {
        return;
    }

    auto &ui = form.createWebUI();
    ui.setTitle(F("Switch Configuration"));
    ui.setContainerId(F("switch_settings"));
    ui.setStyle(FormUI::WebUI::StyleType::ACCORDION);

    FormUI::Container::List states(
        SwitchStateEnum::OFF, FSPGM(Off),
        SwitchStateEnum::ON, FSPGM(On),
        SwitchStateEnum::RESTORE, F("Restore Last State")
    );

    FormUI::Container::List webUI(
        WebUIEnum::NONE, F("None"),
        WebUIEnum::HIDE, F("Hide"),
        WebUIEnum::NEW_ROW, F("New row after switch")
    );

    PROGMEM_DEF_LOCAL_VARNAMES(_VAR_, IOT_SWITCH_CHANNEL_NUM, chan, name, state, webui);

    for (size_t i = 0; i < _pins.size(); i++) {
        auto &group = form.addCardGroup(F_VAR(chan, i), PrintString(F("Channel %u"), i), true);

        form.add(F_VAR(name, i),  _names[i]);
        form.addFormUI(F("Name"));

        form.addPointerTriviallyCopyable(F_VAR(state, i), &_configs[i].state);
        form.addFormUI(F("Default State"), states);

        form.addPointerTriviallyCopyable(F_VAR(webui, i), &_configs[i].webUI);
        form.addFormUI(F("WebUI"), webUI);

        group.end();
    }

    form.finalize();
}

void SwitchPlugin::createWebUI(WebUIRoot &webUI)
{
    auto row = &webUI.addRow();
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
            row->addSwitch(PrintString(FSPGM(channel__u), i), name, true, WebUIRow::NamePositionType::HIDE);
        }
        if (_configs[i].webUI == WebUIEnum::NEW_ROW) {
            row = &webUI.addRow();
        }
    }
}

void SwitchPlugin::getValues(JsonArray &array)
{
    __LDBG_println();

    for (size_t i = 0; i < _pins.size(); i++) {
        auto obj = &array.addObject(2);
        obj->add(JJ(id), PrintString(FSPGM(channel__u), i));
        obj->add(JJ(value), (int)_getChannel(i));
        obj->add(JJ(state), true);
    }
}

void SwitchPlugin::setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState)
{
    __LDBG_printf("id=%s value=%s hasValue=%u state=%u hasState=%u", id.c_str(), value.c_str(), hasValue, state, hasState);

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

MQTT::AutoDiscovery::EntityPtr SwitchPlugin::gettAutoDiscovery(MQTT::FormatType format, uint8_t num)
{
    auto discovery =__LDBG_new(MQTT::AutoDiscovery::Entity);
    auto channel = PrintString(FSPGM(channel__u), num);
    discovery->create(this, channel, format);
    discovery->addStateTopic(MQTTClient::formatTopic(channel, FSPGM(_state)));
    discovery->addCommandTopic(MQTTClient::formatTopic(channel, FSPGM(_set)));
    discovery->addPayloadOnOff();
    return discovery;
}

uint8_t SwitchPlugin::getAutoDiscoveryCount() const
{
    return _pins.size();
}

void SwitchPlugin::onConnect()
{
    for (size_t i = 0; i < _pins.size(); i++) {
        client().subscribe(this, MQTTClient::formatTopic(PrintString(FSPGM(channel__u), i), FSPGM(_set)));
    }
    _publishState();
}

void SwitchPlugin::onMessage(const char *topic, const char *payload, size_t len)
{
    __LDBG_printf("topic=%s payload=%s", topic, payload);
    for (size_t i = 0; i < _pins.size(); i++) {
        if (MQTTClient::formatTopic(PrintString(FSPGM(channel__u), i), FSPGM(_set)).equals(topic)) {
            bool state = atoi(payload);
            _setChannel(i, state);
            _publishState(i);
        }
    }
}

void SwitchPlugin::_setChannel(uint8_t channel, bool state)
{
    __LDBG_printf("channel=%u state=%u", channel, state);
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
    Plugins::IOTSwitch::getConfig(_names, _configs);
}

void SwitchPlugin::_readStates()
{
#if IOT_SWITCH_STORE_STATES
    auto file = SPIFFSWrapper::open(FSPGM(iot_switch_states_file, "/.pvt/switch.states"), fs::FileOpenMode::read);
    if (file) {
        if (file.read(reinterpret_cast<uint8_t *>(&_states), sizeof(_states)) != sizeof(_states)) {
            _states = 0;
        }
    }
#endif
    __LDBG_printf("states=%s", String(_states, 2).c_str());
}

void SwitchPlugin::_writeStates()
{
    __LDBG_printf("states=%s", String(_states, 2).c_str());
#if IOT_SWITCH_STORE_STATES
    _Timer(_delayedWrite).add(IOT_SWITCH_STORE_STATES_WRITE_DELAY, false, [this](Event::CallbackTimerPtr timer) {
        __LDBG_printf("delayed write states=%s", String(_states, 2).c_str());
        auto file = SPIFFSWrapper::open(FSPGM(iot_switch_states_file), fs::FileOpenMode::write);
        if (file) {
            file.write(reinterpret_cast<const uint8_t *>(&_states), sizeof(_states));
        }
    });
#endif
}

void SwitchPlugin::_publishState(MQTTClient *client, int8_t channel)
{
    __DBG_println();
    if (client && client->isConnected()) {
        for (size_t i = 0; i < _pins.size(); i++) {
            if (channel == -1 || (uint8_t)channel == i) {
                __LDBG_printf("pin=%u state=%u", _pins[i], _getChannel(i));
                client->publish(MQTTClient::formatTopic(PrintString(FSPGM(channel__u), i), FSPGM(_state)), true, String(_getChannel(i) ? 1 : 0));
            }
        }
    }

    if (WebUISocket::hasAuthenticatedClients()) {
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
            WebUISocket::broadcast(WebUISocket::getSender(), json);
            // WsClient::broadcast(WebUISocket::getServerSocket(), WebUISocket::getSender(), buffer->c_str(), buffer->length());

            // auto buffer = std::shared_ptr<StreamString>(new StreamString());
            // json.printTo(*buffer);

            // LoopFunctions::callOnce([this, buffer]() {
            //     WsClient::broadcast(WebUISocket::getServerSocket(), WebUISocket::getSender(), buffer->c_str(), buffer->length());
            // });
        }
    }
}
