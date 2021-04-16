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

void SwitchPlugin::setup(SetupModeType mode, const PluginComponents::DependenciesPtr &dependencies)
{
    _readConfig();
    _readStates();
    for (size_t i = 0; i < _pins.size(); i++) {
        if (_configs[i].getState() == SwitchStateEnum::RESTORE) {
            _setChannel(i, _states & (1 << i));
        }
        else {
            _setChannel(i, _configs[i].getState());
        }
        pinMode(_pins[i], OUTPUT);
    }
    MQTT::Client::registerComponent(this);
#if IOT_SWITCH_PUBLISH_MQTT_INTERVAL
    _Timer(_updateTimer).add(IOT_SWITCH_PUBLISH_MQTT_INTERVAL, true, [this](Event::CallbackTimerPtr timer) {
        _publishState();
    });
#endif
}

void SwitchPlugin::shutdown()
{
    MQTT::Client::unregisterComponent(this);
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

void SwitchPlugin::createWebUI(WebUINS::Root &webUI)
{
    webUI.addRow(WebUINS::Row(WebUINS::Group(F("Switch"), false)));

    for (size_t i = 0; i < _pins.size(); i++) {
        WebUINS::Row row;
        if (_configs[i].webUI != WebUIEnum::HIDE) {
            PrintString name;
            if (_names[i].length()) {
                name = _names[i];
            }
            else {
                name = PrintString(F("Channel %u"), i);
            }
            row.append(WebUINS::Switch(PrintString(FSPGM(channel__u), i), name, true, WebUINS::NamePositionType::HIDE));
        }
        if (_configs[i].webUI == WebUIEnum::NEW_ROW) {
            webUI.addRow(row);
        }
        else {
            webUI.appendToLastRow(row);
        }
    }
}

void SwitchPlugin::getValues(WebUINS::Events &array)
{
    for (size_t i = 0; i < _pins.size(); i++) {
        array.append(WebUINS::Values(PrintString(FSPGM(channel__u), i), _getChannel(i) ? 1 : 0, true));
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
    for (size_t i = 0; i < _pins.size(); i++) {
        subscribe(MQTT::Client::formatTopic(PrintString(FSPGM(channel__u), i), FSPGM(_set)));
    }
    _publishState();
}

void SwitchPlugin::onMessage(const char *topic, const char *payload, size_t len)
{
    __LDBG_printf("topic=%s payload=%s", topic, payload);
    for (size_t i = 0; i < _pins.size(); i++) {
        PrintString topicPrefix(FSPGM(channel__u), i); // "channel_XX" fits into SSO
        if (topicPrefix.endEquals(payload)) {
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

void SwitchPlugin::_publishState(int8_t channel)
{
    if (isConnected()) {
        for (size_t i = 0; i < _pins.size(); i++) {
            if (channel == -1 || (uint8_t)channel == i) {
                __LDBG_printf("pin=%u state=%u", _pins[i], _getChannel(i));
                publish(MQTT::Client::formatTopic(PrintString(FSPGM(channel__u), i), FSPGM(_state)), true, MQTT::Client::toBoolOnOff(_getChannel(i)));
            }
        }
    }

    using namespace MQTT::Json;
    if (WebUISocket::hasAuthenticatedClients()) {
        auto events = NamedArray(F("events"));
        for (size_t i = 0; i < _pins.size(); i++) {
            if (channel == -1 || (uint8_t)channel == i) {
                PrintString channel(FSPGM(channel__u), i);
                events.append(UnnamedObject(
                    NamedString(J(id), channel),
                    NamedUnsignedShort(J(value), _getChannel(i)),
                    NamedBool(J(state), true)
                ));
            }
        }
        WebUISocket::broadcast(WebUISocket::getSender(), UnnamedObject(events).toString());
    }
}
