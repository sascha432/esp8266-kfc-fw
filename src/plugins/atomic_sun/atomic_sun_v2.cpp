/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_ATOMIC_SUN_V2

// v2 uses a 4 channel mosfet dimmer with a different serial protocol

#include <PrintHtmlEntitiesString.h>
#include "../include/templates.h"
#include "progmem_data.h"
#include "plugins.h"
#include "atomic_sun_v2.h"
#include "../dimmer_module/dimmer_module_form.h"
#include "SerialTwoWire.h"
#include "WebUISocket.h"

#ifdef DEBUG_4CH_DIMMER
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#define DIMMER_COMPONENT SPGM(component_light)

#include "../../trailing_edge_dimmer/src/dimmer_protocol.h"

Driver_4ChDimmer::Driver_4ChDimmer() : MQTTComponent(LIGHT), Dimmer_Base() {
#if DEBUG_MQTT_CLIENT
    debug_printf_P(PSTR("Driver_4ChDimmer(): component=%p\n"), this);
#endif
}

void Driver_4ChDimmer::_begin() {
    Dimmer_Base::_begin();
    memset(&_channels, 0, sizeof(_channels));
    auto mqttClient = MQTTClient::getClient();
    if (mqttClient) {
        mqttClient->registerComponent(this);
    }
    _data.state.value = false;
    _data.brightness.value = 0;
    _data.color.value = 0;
    _storedBrightness = 0;
    _qos = 0;
    _getChannels();
    // calculate color and brightness values from dimmer channels
    int32_t sum = _channels[0] + _channels[1] + _channels[2] + _channels[3];
    if (sum) {
        _data.color.value = round((_channels[0] + _channels[1]) / sum * (500 - 153)) + 153;
        _data.brightness.value = sum * IOT_ATOMIC_SUN_MAX_BRIGHTNESS / 4;
        _data.state.value = true;
    } else {
        _data.brightness.value = 0;
        _data.color.value = 333;
        _data.state.value = false;
    }
}

void Driver_4ChDimmer::_end() {
    auto mqttClient = MQTTClient::getClient();
    if (mqttClient) {
        mqttClient->unregisterComponent(this);
    }
    Dimmer_Base::_end();
}

const String AtomicSunPlugin::getStatus() {
    PrintHtmlEntitiesString out;
    out = F("4 Channel MOSFET Dimmer enabled on Serial Port");
    _printStatus(out);
    return out;
}

void Driver_4ChDimmer::createAutoDiscovery(MQTTAutoDiscovery::Format_t format, MQTTAutoDiscoveryVector &vector) {
    if (_data.state.set.length() == 0) {
        _createTopics();
    }
    auto discovery = _debug_new MQTTAutoDiscovery();
    discovery->create(this, 0, format);
    discovery->addStateTopic(_data.state.state);
    discovery->addCommandTopic(_data.state.set);
    discovery->addPayloadOn(FSPGM(1));
    discovery->addPayloadOff(FSPGM(0));
    discovery->addBrightnessStateTopic(_data.brightness.state);
    discovery->addBrightnessCommandTopic(_data.brightness.set);
    discovery->addBrightnessScale(IOT_ATOMIC_SUN_MAX_BRIGHTNESS);
    discovery->addParameter(FSPGM(mqtt_color_temp_state_topic), _data.color.state);
    discovery->addParameter(FSPGM(mqtt_color_temp_command_topic), _data.color.set);
    discovery->finalize();
    vector.emplace_back(MQTTAutoDiscoveryPtr(discovery));

    String topic = MQTTClient::formatTopic(-1, F("/metrics/"));
    MQTTComponentHelper component(MQTTComponent::SENSOR);

    component.setNumber(2);
    discovery = component.createAutoDiscovery(0, format);
    discovery->addStateTopic(topic + F("int_temp"));
    discovery->addUnitOfMeasurement(F("\u00b0C"));
    discovery->finalize();
    vector.emplace_back(MQTTAutoDiscoveryPtr(discovery));

    discovery = component.createAutoDiscovery(1, format);
    discovery->addStateTopic(topic + F("ntc_temp"));
    discovery->addUnitOfMeasurement(F("\u00b0C"));
    discovery->finalize();
    vector.emplace_back(MQTTAutoDiscoveryPtr(discovery));

    discovery = component.createAutoDiscovery(2, format);
    discovery->addStateTopic(topic + F("vcc"));
    discovery->addUnitOfMeasurement(F("V"));
    discovery->finalize();
    vector.emplace_back(MQTTAutoDiscoveryPtr(discovery));

    discovery = component.createAutoDiscovery(3, format);
    discovery->addStateTopic(topic + F("frequency"));
    discovery->addUnitOfMeasurement(F("Hz"));
    discovery->finalize();
    vector.emplace_back(MQTTAutoDiscoveryPtr(discovery));
}


uint8_t Driver_4ChDimmer::getAutoDiscoveryCount() const {
    return 6;
}

void Driver_4ChDimmer::_createTopics() {
    _data.state.set = MQTTClient::formatTopic(getNumber(), F("/set"));
    _data.state.state = MQTTClient::formatTopic(getNumber(), F("/state"));
    _data.brightness.set = MQTTClient::formatTopic(getNumber(), F("/brightness/set"));
    _data.brightness.state = MQTTClient::formatTopic(getNumber(), F("/brightness/state"));
    _data.color.set = MQTTClient::formatTopic(getNumber(), F("/color/set"));
    _data.color.state = MQTTClient::formatTopic(getNumber(), F("/color/state"));
}

void Driver_4ChDimmer::onConnect(MQTTClient *client) {

    _qos = MQTTClient::getDefaultQos();
    _metricsTopic = MQTTClient::formatTopic(-1, F("/metrics/"));

    _createTopics();

    client->subscribe(this, _data.state.set, _qos);
    client->subscribe(this, _data.brightness.set, _qos);
    client->subscribe(this, _data.color.set, _qos);

    publishState(client);
}

void Driver_4ChDimmer::onMessage(MQTTClient *client, char *topic, char *payload, size_t len) {

    int value = atoi(payload);

    if (_data.state.set.equals(topic)) {

        // on/off only changes brightness if the state is different
        bool result;
        if (value) {
            result = on();
        } else {
            result = off();
        }
        if (!result) { // publish state of device state has not been changed
            publishState(client);
        }

    } else if (_data.brightness.set.equals(topic)) {

        float fadetime;

        // if brightness changes, also turn dimmer on or off
        if (value > 0) {
            fadetime = _data.state.value ? getFadeTime() : getOnOffFadeTime();
            _data.state.value = true;
            _data.brightness.value = std::min(value, IOT_ATOMIC_SUN_MAX_BRIGHTNESS);
        } else {
            fadetime = _data.state.value ? getOnOffFadeTime() : getFadeTime();
            _data.state.value = false;
            _data.brightness.value = 0;
        }
        _setChannels(fadetime);
        publishState(client);

    } else if (_data.color.set.equals(topic)) {

        _data.color.value = value;
        _setChannels(getFadeTime());
        publishState(client);

    }
}

void Driver_4ChDimmer::publishState(MQTTClient *client) {
    _debug_printf_P(PSTR("Driver_4ChDimmer::publishState()\n"));
    if (!client) {
        client = MQTTClient::getClient();
    }
    if (client) {
        client->publish(_data.state.state, _qos, 1, String(_data.state.value));
        client->publish(_data.brightness.state, _qos, 1, String(_data.brightness.value));
        client->publish(_data.color.state, _qos, 1, String(_data.color.value));
    }

    JsonUnnamedObject json(2);
    json.add(JJ(type), JJ(ue));
    auto &events = json.addArray(JJ(events), 2);
    auto obj = &events.addObject(3);
    obj->add(JJ(id), F("dimmer_channel0"));
    obj->add(JJ(value), _data.brightness.value);
    obj->add(JJ(state), _data.state.value);
    obj = &events.addObject(2);
    obj->add(JJ(id), F("dimmer_channel1"));
    obj->add(JJ(value), _data.color.value);
    WsWebUISocket::broadcast(WsWebUISocket::getSender(), json);
}

void Driver_4ChDimmer::_printStatus(PrintHtmlEntitiesString &out) {
    out.print(F(", Fading enabled" HTML_S(br) "Power "));
    if (_data.state.value) {
        out.print(F("on"));
    } else {
        out.print(F("off"));
    }
    out.printf_P(PSTR(", brightness %.1f%%"), _data.brightness.value / (float)IOT_ATOMIC_SUN_MAX_BRIGHTNESS * 100);
    out.printf_P(PSTR(", color temperature %d K" HTML_S(br)), 1000000 / _data.color.value);
    Dimmer_Base::_printStatus(out);
}

bool Driver_4ChDimmer::on(uint8_t channel) {
    if (!_data.state.value) {
        _data.brightness.value = _storedBrightness;
        _data.state.value = true;
        _setChannels(getOnOffFadeTime());
        publishState();
        return true;
    }
    return false;
}

bool Driver_4ChDimmer::off(uint8_t channel) {
    if (_data.state.value) {
        _storedBrightness = _data.brightness.value;
        _data.state.value = false;
        _data.brightness.value = 0;
        _setChannels(getOnOffFadeTime());
        publishState();
        return true;
    }
    return false;
}

int16_t Driver_4ChDimmer::getChannel(uint8_t channel) const {
    if (channel) {
            return _data.color.value;
    } else {
        return _data.brightness.value;
    }

}
bool Driver_4ChDimmer::getChannelState(uint8_t channel) const {
    return _data.state.value;
}

void Driver_4ChDimmer::setChannel(uint8_t channel, int16_t level, float time) {
    if (channel) {
        _data.color.value = level;
        _setChannels(time);
    } else {
        _data.brightness.value = level;
        _data.state.value = level != 0;
        _setChannels(time);
    }
}

uint8_t Driver_4ChDimmer::getChannelCount() const {
    return 2;
}

void Driver_4ChDimmer::_setChannels(float fadetime) {
    if (fadetime == -1) {
        fadetime = getFadeTime();
    }
    uint8_t color = (_data.color.value - 153) * 255 / (500 - 153);
    auto channel12 = _data.brightness.value * color * 2 / 255;
    auto channel34 = _data.brightness.value * (255 - color) * 2 / 255;
    _debug_printf_P(PSTR("Driver_4ChDimmer::setBrightness() time=%.3f brightness=%d color=%d ch1/2=%d ch3/4=%d\n"), fadetime, _data.brightness.value, color, channel12, channel34);

    _fade(0, channel12, fadetime);
    _fade(1, channel12, fadetime);
    _fade(2, channel34, fadetime);
    _fade(3, channel34, fadetime);
    writeEEPROM();
}

// get brightness values from dimmer
void Driver_4ChDimmer::_getChannels() {
    _debug_printf_P(PSTR("Driver_4ChDimmer::_getChannels()\n"));
    if (_lockWire()) {
        _wire.beginTransmission(DIMMER_I2C_ADDRESS);
        _wire.write(DIMMER_REGISTER_COMMAND);
        _wire.write(DIMMER_COMMAND_READ_CHANNELS);
        _wire.write((uint8_t)(sizeof(_channels) / sizeof(_channels[0])) << 4);
        if (_endTransmission() == 0) {
            if (_wire.requestFrom(DIMMER_I2C_ADDRESS, sizeof(_channels)) == sizeof(_channels)) {
                _wire.readBytes(reinterpret_cast<uint8_t *>(&_channels), sizeof(_channels));
                _debug_printf_P(PSTR("Driver_4ChDimmer::_getChannels(): %u, %u, %u, %u\n"), _channels[0], _channels[1], _channels[2], _channels[3]);
            }
        }
        _unlockWire();
    }
}

AtomicSunPlugin dimmer_plugin;

PGM_P AtomicSunPlugin::getName() const {
    return PSTR("atomicsun");
}

AtomicSunPlugin::PluginPriorityEnum_t AtomicSunPlugin::getSetupPriority() const {
    return (AtomicSunPlugin::PluginPriorityEnum_t)100;
}

void AtomicSunPlugin::setup(PluginSetupMode_t mode) {
    _begin();
}

void AtomicSunPlugin::reconfigure(PGM_P source) {
    if (!source) {
        writeConfig();
    }
}

bool AtomicSunPlugin::hasWebUI() const {
    return true;
}

void AtomicSunPlugin::createWebUI(WebUI &webUI) {

    auto row = &webUI.addRow();
    row->setExtraClass(F("title"));
    row->addGroup(F("Atomic Sun"), true);

    row = &webUI.addRow();
    row->addSlider(F("dimmer_channel0"), F("Atomic Sun Brightness"), 0, IOT_ATOMIC_SUN_MAX_BRIGHTNESS);

    row = &webUI.addRow();
    row->addColorSlider(F("dimmer_channel1"), F("Atomic Sun Color"));

    row = &webUI.addRow();
    row->addBadgeSensor(F("dimmer_vcc"), F("Atomic Sun VCC"), F("V"));
    row->addBadgeSensor(F("dimmer_frequency"), F("Atomic Sun Frequency"), F("Hz"));
    row->addBadgeSensor(F("dimmer_int_temp"), F("Atomic Sun ATmega"), F("\u00b0C"));
    row->addBadgeSensor(F("dimmer_ntc_temp"), F("Atomic Sun NTC"), F("\u00b0C"));
}

WebUIInterface *AtomicSunPlugin::getWebUIInterface() {
    return this;
}

bool AtomicSunPlugin::hasStatus() const {
    return true;
}

#endif
