/**
 * Author: sascha_lammers@gmx.de
 */

#include "dimmer.h"
#include <plugins.h>
#include <plugins_menu.h>
#include <HeapStream.h>
#include <WebUISocket.h>
#include "../include/templates.h"
#include "../src/plugins/mqtt/mqtt_client.h"

#if DEBUG_IOT_DIMMER
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

using namespace Dimmer;

Plugin Dimmer::dimmer_plugin;

PROGMEM_DEFINE_PLUGIN_OPTIONS(
    Plugin,
    IOT_DIMMER_PLUGIN_NAME,
    IOT_DIMMER_PLUGIN_FRIENDLY_NAME,
    "",                 // web_templates
    "dimmer",           // forms
    "",                 // reconfigure_dependencies
    PluginComponent::PriorityType::DIMMER_MODULE,
    PluginComponent::RTCMemoryId::DIMMER,
    static_cast<uint8_t>(PluginComponent::MenuType::NONE),
    false,              // allow_safe_mode
    false,              // setup_after_deep_sleep
    true,               // has_get_status
    true,               // has_config_forms
    true,               // has_web_ui
    false,              // has_web_templates
    true,               // has_at_mode
    0                   // __reserved
);

Plugin::Plugin() :
    PluginComponent(PROGMEM_GET_PLUGIN_OPTIONS(Plugin)),
    MQTTComponent(MQTT::ComponentType::LIGHT),
    _storedBrightness(0),
    _brightness(0)
    #if IOT_DIMMER_X9C_POTI
        , _poti(ATMODE_X9C_PIN_CS, ATMODE_X9C_PIN_INC, ATMODE_X9C_PIN_UD)
    #endif
{
    REGISTER_PLUGIN(this, "Dimmer::Plugin");
}

MQTT::AutoDiscovery::EntityPtr Plugin::getAutoDiscovery(FormatType format, uint8_t num)
{
    auto discovery = new AutoDiscovery::Entity();
    auto baseTopic = MQTT::Client::getBaseTopicPrefix();
    if (discovery->createJsonSchema(this, F("dimmer"), format)) {
        discovery->addStateTopic(MQTT::Client::formatTopic(F("/state")));
        discovery->addCommandTopic(MQTT::Client::formatTopic(F("/command")));
        discovery->addBrightnessScale(IOT_DIMMER_MAX_BRIGHTNESS - IOT_DIMMER_MIN_BRIGHTNESS);
        discovery->addParameter(F("brightness"), true);
        discovery->addName(F("Dimmer"));
        discovery->addObjectId(baseTopic + PrintString(F("dimmer_channel_%u"), 1));
    }
    return discovery;
}

void Plugin::onConnect()
{
    _Timer(_publishTimer).remove();
    subscribe(MQTT::Client::formatTopic(F("/command")));
    publishState();
}

void Plugin::onMessage(const char *topic, const char *payload, size_t len)
{
    __LDBG_printf("topic=%s payload=%s", topic, payload);
    auto stream = HeapStream(payload, len);
    auto reader = MQTT::Json::Reader(&stream);
    if (reader.parse()) {
        #if DEBUG_IOT_DIMMER
            PrintString str;
            reader.dump(str);
            __DBG_printf("%s", str.c_str());
        #endif
        onJsonMessage(reader);
    }
    else {
        __LDBG_printf("parsing json failed=%s payload=%s", reader.getLastErrorMessage().c_str(), payload);
    }
}

void Plugin::onJsonMessage(const MQTT::Json::Reader &json)
{
    __LDBG_printf("state=%d brightness=%d", json.state, json.brightness);
    if (json.state != -1) {
        if (json.state && !_brightness) {
            on();
            publishState();
        }
        else if (!json.state && _brightness) {
            off();
            publishState();
        }
    }
    if (json.brightness != -1) {
        if (json.brightness == 0 && _brightness) {
            off();
            publishState();
        }
        else if (json.brightness) {
            setLevel(json.brightness);
            publishState();
        }
    }
}

void Plugin::publishState()
{
    _Timer(_publishTimer).throttle(333, [this](Event::CallbackTimerPtr) {
        _publish();
    });
}

void Plugin::on()
{
    setLevel(_storedBrightness);
}

void Plugin::off()
{
    _storedBrightness = _brightness;
    setLevel(0);
}

void Plugin::setLevel(uint32_t level)
{
    _brightness = level;
    #if IOT_DIMMER_X9C_POTI
        __LDBG_printf("poti=%d level=%u", IOT_DIMMER_MAP_BRIGHTNESS(level), level);
        _poti.setValue(IOT_DIMMER_MAP_BRIGHTNESS(level));
    #endif
}

void Plugin::_publish()
{
    if (isConnected()) {
        using namespace MQTT::Json;
        publish(MQTT::Client::formatTopic(F("/state")), true, UnnamedObject(
                State(_brightness != 0),
                Brightness(_brightness),
                Transition(0)
            ).toString()
        );
    }
    _saveState();
}

void Plugin::setup(SetupModeType mode, const PluginComponents::DependenciesPtr &dependencies)
{
    MQTT::Client::registerComponent(this);
    _readConfig();
    #if IOT_DIMMER_X9C_POTI
        _poti.begin();
        _poti.resetMax();
        _brightness = _storedBrightness;
        setLevel(_brightness);
    #endif
}

void Plugin::shutdown()
{
    #if IOT_DIMMER_X9C_POTI
        _poti.end();
    #endif
    _Timer(_publishTimer).remove();
    MQTT::Client::unregisterComponent(this);
}

void Plugin::createWebUI(WebUINS::Root &webUI)
{
    webUI.addRow(WebUINS::Group(F(IOT_DIMMER_TITLE), false));

    auto slider = WebUINS::Slider(F("d-br"), F(IOT_DIMMER_BRIGHTNESS_TITLE));
    slider.append(WebUINS::NamedInt32(J(range_min), IOT_DIMMER_MIN_BRIGHTNESS));
    slider.append(WebUINS::NamedInt32(J(range_max), IOT_DIMMER_MAX_BRIGHTNESS));
    webUI.addRow(slider);
}

// void Plugin::createMenu()
// {
//     auto root = bootstrapMenu.getMenuItem(navMenu.config);

//     auto subMenu = root.addSubMenu(getFriendlyName());
//     subMenu.addMenuItem(F("General"), F("dimmer-fw?type=read-config&redirect=dimmer/general.html"));
//     subMenu.addMenuItem(F("Channel Configuration"), F("dimmer/channels.html"));
//     #if IOT_DIMMER_MODULE_HAS_BUTTONS
//         subMenu.addMenuItem(F("Button Configuration"), F("dimmer/buttons.html"));
//     #endif
//     subMenu.addMenuItem(F("Advanced Firmware Configuration"), F("dimmer-fw?type=read-config&redirect=dimmer/advanced.html"));
//     subMenu.addMenuItem(F("Cubic Interpolation"), F("dimmer-ci.html"));
// }

void Plugin::getStatus(Print &out)
{
    out.print(F("DC Dimmer"));
}

void Plugin::createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request)
{
}

void Plugin::getValues(WebUINS::Events &array)
{
    array.append(WebUINS::Values(F("d-br"), _brightness, true));
}

void Plugin::setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState)
{
    if (id == F("d-br")) {
        int val = value.toInt();
        __LDBG_printf("has_value=%d value=%d has_state=%d state=%d", hasValue, val, hasState, state);

        if (hasValue) {
            hasState = true;
            state = (val != 0);
        }
        if (hasState) {
            if (state && !_brightness) {
                on();
            }
            else if (!state && _brightness) {
                off();
            }
        }
        if (hasValue) {
            setLevel(val);
        }
        publishState();
    }
}

void Plugin::reconfigure(const String &source)
{
    _readConfig();
    setLevel(_brightness);
}

void Plugin::_readConfig()
{
    _loadState();
}

void Plugin::_loadState()
{
    States states;
    if (RTCMemoryManager::read(RTCMemoryManager::RTCMemoryId::DIMMER, &states, states.size()) == states.size()) {
        _storedBrightness = states.getStoredBrightness();
        _brightness = states.getLevel();
        __LDBG_printf("stored=%u level=%u", _storedBrightness, _brightness);
    }
    else {
        __LDBG_printf("failed to load states from RTC memory");
        _storedBrightness = 0;
        _brightness = 0;
    }
}

void Plugin::_saveState()
{
    States states(_brightness, _brightness);
    if (!RTCMemoryManager::write(RTCMemoryManager::RTCMemoryId::DIMMER, &states, states.size())) {
        __LDBG_printf("failed to store states in RTC memory");
    }
    else {
        __LDBG_printf("stored=%u level=%u", _storedBrightness, _brightness);
    }
}
