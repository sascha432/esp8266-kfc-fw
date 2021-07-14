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
    _pins({IOT_SWITCH_CHANNEL_PINS})
{
    REGISTER_PLUGIN(this, "SwitchPlugin");
}

void SwitchPlugin::setup(SetupModeType mode, const PluginComponents::DependenciesPtr &dependencies)
{
    _readConfig();
    _readStates();
    for (uint8_t i = 0; i < _pins.size(); i++) {
        if (_configs[i].getState() == SwitchStateEnum::RESTORE) {
            _setChannel(i, _states[i]);
        }
        else {
            _setChannel(i, static_cast<bool>(_configs[i].getState()));
        }
        pinMode(_pins[i], OUTPUT);
    }
    MQTT::Client::registerComponent(this);

#if IOT_SWITCH_PUBLISH_MQTT_INTERVAL
    // update states once per minute
    _Timer(_updateTimer).add(IOT_SWITCH_PUBLISH_MQTT_INTERVAL, true, [this](Event::CallbackTimerPtr timer) {
        _publishState();
    });
#endif
    // update states when wifi has been connected
    WiFiCallbacks::add(WiFiCallbacks::EventType::CONNECTED, [this](WiFiCallbacks::EventType event, void *payload) {
        _publishState();
    }, this);

    // update states when wifi is already connected
    if (WiFi.isConnected()) {
        _publishState();
    }
}

void SwitchPlugin::shutdown()
{
    WiFiCallbacks::remove(WiFiCallbacks::EventType::CONNECTED, this);
#if IOT_SWITCH_PUBLISH_MQTT_INTERVAL
    _updateTimer.remove();
#endif
#if IOT_SWITCH_STORE_STATES
    if (_delayedWrite) {
        // stop timer
        _delayedWrite.remove();
        // write states
        _writeStatesNow();
    }
#endif
    MQTT::Client::unregisterComponent(this);
}

void SwitchPlugin::reconfigure(const String &source)
{
    _readConfig();
    if (isConnected()) {
        client().publishAutoDiscovery(MQTT::RunFlags::FORCE);
    }
}

void SwitchPlugin::getStatus(Print &output)
{
    output.printf_P(PSTR("%u Channel Switch"), _pins.size());
    for (uint8_t i = 0; i < _pins.size(); i++) {
        output.printf_P(PSTR(HTML_S(br) "%s - %s"), _names[i].toString(i).c_str(), _getChannel(i) ? SPGM(On) : SPGM(Off));
    }
}

void SwitchPlugin::createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request)
{
    if (type == FormCallbackType::SAVE) {
        __LDBG_printf("Storing config");
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
        WebUIEnum::NONE, F("Display Channel Name"),
        WebUIEnum::TOP, F("Display On Top (Large)"),
        WebUIEnum::HIDE, F("Hide Switch"),
        WebUIEnum::NEW_ROW, F("New Row After Switch")
    );

    PROGMEM_DEF_LOCAL_VARNAMES(_VAR_, IOT_SWITCH_CHANNEL_NUM, chan, name, state, webui);

    for (uint8_t i = 0; i < _pins.size(); i++) {
        auto &group = form.addCardGroup(F_VAR(chan, i), PrintString(F("Channel %u"), i), true);

        form.add(F_VAR(name, i), _names[i]);
        form.addFormUI(F("Name"));

        form.addObjectGetterSetter(F_VAR(state, i), _configs[i], _configs[i].get_bits__state, _configs[i].set_bits__state);
        form.addFormUI(F("Default State"), states);

        form.addObjectGetterSetter(F_VAR(webui, i), _configs[i], _configs[i].get_bits__webUI, _configs[i].set_bits__webUI);
        form.addFormUI(F("WebUI"), webUI);

        group.end();
    }

    form.finalize();
}

void SwitchPlugin::createWebUI(WebUINS::Root &webUI)
{
    webUI.addRow(WebUINS::Row(WebUINS::Group(F("Switch"), false)));

    for (uint8_t i = 0; i < _pins.size(); i++) {
        WebUINS::Row row;
        __LDBG_printf("name=%s webui=%u pos=%u", _names[i].toString(i).c_str(), _configs[i].getWebUI(), _configs[i].getWebUINamePosition());
        if (_configs[i].getWebUI() != WebUIEnum::HIDE) {
            row.append(WebUINS::Switch(PrintString(FSPGM(channel__u), i), _names[i].toString(i), true, _configs[i].getWebUINamePosition()));
        }
        if (_configs[i].getWebUI() == WebUIEnum::NEW_ROW) {
            webUI.addRow(row);
            webUI.addRow(WebUINS::Row());
        }
        else {
            webUI.appendToLastRow(row);
        }
    }
    // __LDBG_printf("webui=%s", webUI.toString().c_str());
}

void SwitchPlugin::getValues(WebUINS::Events &array)
{
    for (uint8_t i = 0; i < _pins.size(); i++) {
        array.append(WebUINS::Values(PrintString(FSPGM(channel__u), i), _getChannel(i) ? 1 : 0, true));
    }
}

void SwitchPlugin::setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState)
{
    __LDBG_printf("id=%s value=%s has_value=%u state=%u has_state=%u", id.c_str(), value.c_str(), hasValue, state, hasState);
    if (id.startsWith(F("channel_")) && hasValue) {
        auto state = static_cast<bool>(value.toInt());
        auto channel = static_cast<uint8_t>(atoi(id.c_str() + 8));
        if (channel < _pins.size()) {
            _setChannel(channel, state);
            _publishState(channel);
        }
    }
}

MQTT::AutoDiscovery::EntityPtr SwitchPlugin::getAutoDiscovery(MQTT::FormatType format, uint8_t num)
{
    auto discovery = new MQTT::AutoDiscovery::Entity();
    auto channel = PrintString(FSPGM(channel__u), num);
    if (!discovery->create(this, channel, format)) {
        return discovery;
    }
    discovery->addName(_names[num].toString(num));
    discovery->addStateTopic(MQTTClient::formatTopic(channel, FSPGM(_state)));
    discovery->addCommandTopic(MQTTClient::formatTopic(channel, FSPGM(_set)));
    return discovery;
}

uint8_t SwitchPlugin::getAutoDiscoveryCount() const
{
    return _pins.size();
}

void SwitchPlugin::onConnect()
{
    // subscribe to all channels
    for (uint8_t i = 0; i < _pins.size(); i++) {
        subscribe(MQTT::Client::formatTopic(PrintString(FSPGM(channel__u), i), FSPGM(_set)));
    }
    _publishState();
}

void SwitchPlugin::onMessage(const char *topic, const char *payload, size_t len)
{
    // handle MQTT messages to toggle channels
    __LDBG_printf("topic=%s payload=%s", topic, payload);
    for (uint8_t i = 0; i < _pins.size(); i++) {
        PrintString topicSuffix(FSPGM(channel__u), i);
        topicSuffix += FSPGM(_set);
        if (topicSuffix.endEquals(topic)) {
            _setChannel(i, MQTTClient::toBool(payload, false));
            _publishState(i);
        }
    }
}

void SwitchPlugin::_setChannel(uint8_t channel, bool state)
{
    __LDBG_printf("channel=%u state=%u", channel, state);
    digitalWrite(_pins[channel], state ? IOT_SWITCH_ON_STATE : !IOT_SWITCH_ON_STATE);
    _states.setState(channel, state);
    _writeStatesDelayed();
}

bool SwitchPlugin::_getChannel(uint8_t channel) const
{
    return (digitalRead(_pins[channel]) == IOT_SWITCH_ON_STATE);
}

void SwitchPlugin::_readConfig()
{
    Plugins::IOTSwitch::getConfig(_names, _configs);
}

void SwitchPlugin::_readStates()
{
#if IOT_SWITCH_STORE_STATES
    auto file = KFCFS.open(FSPGM(iot_switch_states_file, "/.pvt/switch.states"), fs::FileOpenMode::read);
    if (!file || !_states.read(file)) {
        // reset on read error
        _states = States();
    }
#endif
    __LDBG_printf("states=%s", _states.toString().c_str());
}

void SwitchPlugin::_writeStatesDelayed()
{
    __LDBG_printf("states=%s", _states.toString().c_str());
#if IOT_SWITCH_STORE_STATES
    _Timer(_delayedWrite).add(IOT_SWITCH_STORE_STATES_WRITE_DELAY, false, [this](Event::CallbackTimerPtr) {
        __LDBG_printf("delayed write states=%s", _states.toString().c_str());
        _writeStatesNow();
    });
#endif
}

void SwitchPlugin::_writeStatesNow()
{
    __LDBG_printf("states=%s", _states.toString().c_str());
    auto file = KFCFS.open(FSPGM(iot_switch_states_file), fs::FileOpenMode::write);
    if (file && !_states.write(file)) {
        __LDBG_printf("failed to write states");
    }
}

void SwitchPlugin::_publishState(int8_t channel)
{
    if (isConnected()) {
        for (uint8_t i = 0; i < _pins.size(); i++) {
            if (channel == -1 || static_cast<uint8_t>(channel) == i) {
                __LDBG_printf("pin=%u state=%u", _pins[i], _getChannel(i));
                publish(MQTT::Client::formatTopic(PrintString(FSPGM(channel__u), i), FSPGM(_state)), true, MQTT::Client::toBoolOnOff(_getChannel(i)));
            }
        }
    }

    if (WebUISocket::hasAuthenticatedClients()) {
        WebUINS::Events events;
        for (uint8_t i = 0; i < _pins.size(); i++) {
            PrintString channel(FSPGM(channel__u), i);
            __LDBG_printf("channel=%s", channel.c_str());
            events.append(WebUINS::Values(channel, _getChannel(i) ? 1 : 0, true));
        }
        __LDBG_printf("events=%s", events.toString().c_str());
        WebUISocket::broadcast(WebUISocket::getSender(), WebUINS::UpdateEvents(events));
    }
}
