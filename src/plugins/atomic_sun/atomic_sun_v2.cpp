/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_ATOMIC_SUN_V2

// v2 uses a 4 channel mosfet dimmer with a different serial protocol

#include <PrintHtmlEntitiesString.h>
#include <StreamString.h>
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

#include "../../trailing_edge_dimmer/src/dimmer_protocol.h"

Driver_4ChDimmer::Driver_4ChDimmer() : MQTTComponent(LIGHT), Dimmer_Base(), _publishTimer(nullptr)
{
#if DEBUG_MQTT_CLIENT
    debug_printf_P(PSTR("Driver_4ChDimmer(): component=%p\n"), this);
#endif
}

void Driver_4ChDimmer::_begin()
{
    Dimmer_Base::_begin();
    memset(&_channels, 0, sizeof(_channels));
    memset(&_storedChannels, 0, sizeof(_storedChannels));
    auto mqttClient = MQTTClient::getClient();
    if (mqttClient) {
        mqttClient->registerComponent(this);
    }
    _data.state.value = false;
    _data.brightness.value = 0;
    _data.color.value = 0;
    _data.lockChannels.value = false;
    _qos = MQTTClient::getDefaultQos();
    _getChannels();
    _channelsToBrightness();
}

void Driver_4ChDimmer::_end()
{
    if (_publishTimer) {
        Scheduler.removeTimer(_publishTimer);
        _publishTimer = nullptr;
    }
    auto mqttClient = MQTTClient::getClient();
    if (mqttClient) {
        mqttClient->unregisterComponent(this);
    }
    Dimmer_Base::_end();
}

void AtomicSunPlugin::getStatus(Print &output)
{
    output.print(F("4 Channel MOSFET Dimmer "));
    if (_isEnabled()) {
        output.print(F("enabled on "));
#if IOT_DIMMER_MODULE_INTERFACE_UART
        output.print(F("Serial Port"));
#else
        output.print(F("I2C"));
#endif
        _printStatus(output);
    }
    else {
        output.print(F("disabled"));
    }
}
void Driver_4ChDimmer::createAutoDiscovery(MQTTAutoDiscovery::Format_t format, MQTTAutoDiscoveryVector &vector)
{
    _createTopics();

    uint8_t num = 0;

    auto discovery = _debug_new MQTTAutoDiscovery();
    discovery->create(this, num++, format);
    discovery->addStateTopic(_data.state.state);
    discovery->addCommandTopic(_data.state.set);
    discovery->addPayloadOn(FSPGM(1));
    discovery->addPayloadOff(FSPGM(0));
    discovery->addBrightnessStateTopic(_data.brightness.state);
    discovery->addBrightnessCommandTopic(_data.brightness.set);
    discovery->addBrightnessScale(IOT_ATOMIC_SUN_MAX_BRIGHTNESS * 4);
    discovery->addParameter(FSPGM(mqtt_color_temp_state_topic), _data.color.state);
    discovery->addParameter(FSPGM(mqtt_color_temp_command_topic), _data.color.set);
    discovery->finalize();
    vector.emplace_back(MQTTAutoDiscoveryPtr(discovery));

    discovery = _debug_new MQTTAutoDiscovery();
    discovery->create(this, num++, format);
    discovery->addStateTopic(_data.lockChannels.state);
    discovery->addCommandTopic(_data.lockChannels.set);
    discovery->addPayloadOn(FSPGM(1));
    discovery->addPayloadOff(FSPGM(0));
    discovery->finalize();
    vector.emplace_back(MQTTAutoDiscoveryPtr(discovery));

    for(uint8_t i = 0; i < 4; i++) {
        discovery = _debug_new MQTTAutoDiscovery();
        discovery->create(this, num++, format);
        discovery->addStateTopic(_data.channels[i].state);
        discovery->addCommandTopic(_data.channels[i].set);
        discovery->addBrightnessStateTopic(_data.channels[i].brightnessState);
        discovery->addBrightnessCommandTopic(_data.channels[i].brightnessSet);
        discovery->addBrightnessScale(IOT_ATOMIC_SUN_MAX_BRIGHTNESS);
        discovery->addPayloadOn(FSPGM(1));
        discovery->addPayloadOff(FSPGM(0));
        discovery->finalize();
        vector.emplace_back(MQTTAutoDiscoveryPtr(discovery));
    }

    MQTTComponentHelper component(MQTTComponent::SENSOR);

    num = 0;
    component.setNumber(0);
    discovery = component.createAutoDiscovery(num, format);
    discovery->addStateTopic(_getMetricsTopics(0));
    discovery->addUnitOfMeasurement(F("\u00b0C"));
    discovery->finalize();
    vector.emplace_back(MQTTAutoDiscoveryPtr(discovery));
    num++;

    discovery = component.createAutoDiscovery(num, format);
    discovery->addStateTopic(_getMetricsTopics(1));
    discovery->addUnitOfMeasurement(F("\u00b0C"));
    discovery->finalize();
    vector.emplace_back(MQTTAutoDiscoveryPtr(discovery));
    num++;

    discovery = component.createAutoDiscovery(num, format);
    discovery->addStateTopic(_getMetricsTopics(2));
    discovery->addUnitOfMeasurement(F("V"));
    discovery->finalize();
    vector.emplace_back(MQTTAutoDiscoveryPtr(discovery));
    num++;

    discovery = component.createAutoDiscovery(num, format);
    discovery->addStateTopic(_getMetricsTopics(3));
    discovery->addUnitOfMeasurement(F("Hz"));
    discovery->finalize();
    vector.emplace_back(MQTTAutoDiscoveryPtr(discovery));
    num++;

#if IOT_ATOMIC_SUN_CALC_POWER
    discovery = component.createAutoDiscovery(num, format);
    discovery->addStateTopic(_getMetricsTopics(4));
    discovery->addUnitOfMeasurement(F("W"));
    discovery->finalize();
    vector.emplace_back(MQTTAutoDiscoveryPtr(discovery));

#endif
}

uint8_t Driver_4ChDimmer::getAutoDiscoveryCount() const
{
#if IOT_ATOMIC_SUN_CALC_POWER
    return 11;
#else
    return 10;
#endif
}

void Driver_4ChDimmer::_createTopics()
{
    if (_data.state.set.length() == 0) {

        uint8_t num = 0;
        _data.state.set = MQTTClient::formatTopic(num, F("/set"));
        _data.state.state = MQTTClient::formatTopic(num, F("/state"));
        _data.brightness.set = MQTTClient::formatTopic(num, F("/brightness/set"));
        _data.brightness.state = MQTTClient::formatTopic(num, F("/brightness/state"));
        _data.color.set = MQTTClient::formatTopic(num, F("/color/set"));
        _data.color.state = MQTTClient::formatTopic(num, F("/color/state"));
        num++;
        _data.lockChannels.set = MQTTClient::formatTopic(num, F("/set"));
        _data.lockChannels.state = MQTTClient::formatTopic(num, F("/state"));
        num++;
        for(uint8_t i = 0; i < 4; i++) {
            _data.channels[i].set = MQTTClient::formatTopic(num, F("/set"));
            _data.channels[i].state = MQTTClient::formatTopic(num, F("/state"));
            _data.channels[i].brightnessSet = MQTTClient::formatTopic(num, F("/brightness/set"));
            _data.channels[i].brightnessState = MQTTClient::formatTopic(num, F("/brightness/state"));
            num++;
        }
    }
}

void Driver_4ChDimmer::onConnect(MQTTClient *client)
{
    _qos = MQTTClient::getDefaultQos();

    _createTopics();

    client->subscribe(this, _data.state.set, _qos);
    client->subscribe(this, _data.brightness.set, _qos);
    client->subscribe(this, _data.color.set, _qos);
    client->subscribe(this, _data.lockChannels.set, _qos);
    for(uint8_t i = 0; i < 4; i++) {
        client->subscribe(this, _data.channels[i].set, _qos);
    }

    publishState(client);
}

void Driver_4ChDimmer::onMessage(MQTTClient *client, char *topic, char *payload, size_t len)
{
    int value = atoi(payload);
    _debug_printf_P(PSTR("Driver_4ChDimmer::onMessage(): topic %s, value %d\n"), topic, value);

    if (_data.state.set.equals(topic)) {

        // on/off only changes brightness if the state is different
        bool result;
        if (value) {
            result = on();
        }
        else {
            result = off();
        }
        if (!result) { // publish state of device state has not been changed
            publishState(client);
        }

    }
    else if (_data.brightness.set.equals(topic)) {

        float fadetime;

        // if brightness changes, also turn dimmer on or off
        if (value > 0) {
            fadetime = _data.state.value ? getFadeTime() : getOnOffFadeTime();
            _data.state.value = true;
            _data.brightness.value = std::min(value, IOT_ATOMIC_SUN_MAX_BRIGHTNESS * 4);
        } else {
            fadetime = _data.state.value ? getOnOffFadeTime() : getFadeTime();
            _data.state.value = false;
            _data.brightness.value = 0;
        }
        _brightnessToChannels();
        _setChannels(fadetime);
        publishState(client);

    }
    else if (_data.color.set.equals(topic)) {

        _data.color.value = value * 100.0;
        if (_data.state.value) {
            _brightnessToChannels();
            _setChannels(getFadeTime());
        }
        publishState(client);

    }
    else if (_data.lockChannels.set.equals(topic)) {

        _setLockChannels(value);
        _brightnessToChannels();
        _setChannels(getFadeTime());
        publishState(client);

    }
    else {
        uint8_t channel = _data.channels[0].set.equals(topic) ? 1 : 0;
        channel += _data.channels[1].set.equals(topic) ? 2 : 0;
        channel += _data.channels[2].set.equals(topic) ? 3 : 0;
        channel += _data.channels[3].set.equals(topic) ? 4 : 0;
        if (channel) {
            channel--;
            _channels[channel] = value;
            _channelsToBrightness();
            _setChannels(getFadeTime());
            publishState(client);
        }
    }
}

void Driver_4ChDimmer::setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState)
{
    _debug_printf_P(PSTR("Driver_4ChDimmer::setValue %s, value %s, hasValue %u, state %u, hasState %u\n"), id.c_str(), value.c_str(), hasValue, state, hasState);

    auto ptr = id.c_str();
    if (!strncmp_P(ptr, PSTR("dimmer_"), 7)) {
        ptr += 7;
        if (!strcmp_P(ptr, PSTR("brightness"))) {
            if (hasValue) {
                int val =  value.toInt();
                float fadetime;
                // if brightness changes, also turn dimmer on or off
                if (val > 0) {
                    fadetime = _data.state.value ? getFadeTime() : getOnOffFadeTime();
                    _data.state.value = true;
                    _data.brightness.value = std::min(val, IOT_ATOMIC_SUN_MAX_BRIGHTNESS * 4);
                } else {
                    fadetime = _data.state.value ? getOnOffFadeTime() : getFadeTime();
                    _data.state.value = false;
                    _data.brightness.value = 0;
                }
                _brightnessToChannels();
                _setChannels(fadetime);
                publishState();
            }
        }
        else if (!strcmp_P(ptr, PSTR("color"))) {
            if (hasValue) {
                _data.color.value = value.toFloat();
                if (_data.state.value) {
                    _brightnessToChannels();
                    _setChannels(getFadeTime());
                }
                publishState();
            }
        }
        else if (!strcmp_P(ptr, PSTR("lock"))) {
            if (hasValue) {
                _setLockChannels(value.toInt());
                _brightnessToChannels();
                _setChannels(getFadeTime());
                publishState();
            }
        }
        else if (!strncmp_P(ptr, PSTR("channel"), 7)) {
            Dimmer_Base::setValue(id, value, hasValue, state, hasState);
            publishState();
        }
    }
}

void Driver_4ChDimmer::getValues(JsonArray &array)
{
    _debug_printf_P(PSTR("Driver_4ChDimmer::getValues()\n"));
    Dimmer_Base::getValues(array);

    auto obj = &array.addObject(2);
    obj->add(JJ(id), F("dimmer_brightness"));
    obj->add(JJ(value), _data.brightness.value);

    obj = &array.addObject(2);
    obj->add(JJ(id), F("dimmer_color"));
    obj->add(JJ(value), _data.color.value);

    obj = &array.addObject(2);
    obj->add(JJ(id), F("dimmer_lock"));
    obj->add(JJ(value), (int)_data.lockChannels.value);

#if IOT_ATOMIC_SUN_CALC_POWER
    obj = &array.addObject(2);
    obj->add(JJ(id), F("dimmer_power"));
    obj->add(JJ(value), JsonNumber(_calcTotalPower(), 1));
#endif
}


void Driver_4ChDimmer::publishState(MQTTClient *client)
{
    _debug_printf_P(PSTR("Driver_4ChDimmer::publishState()\n"));

#if IOT_ATOMIC_SUN_CALC_POWER
    auto power = _calcTotalPower();
#endif

    if (!client) {
        client = MQTTClient::getClient();
    }
    if (client) {
        client->publish(_data.state.state, _qos, 1, String(_data.state.value));
        client->publish(_data.brightness.state, _qos, 1, String(_data.brightness.value));
        client->publish(_data.color.state, _qos, 1, String(_data.color.value / 100.0, 2));
        for(uint8_t i = 0; i < 4; i++) {
            client->publish(_data.channels[i].state, _qos, 1, String(_channels[i]));
        }
        client->publish(_data.lockChannels.state, _qos, 1, String(_data.lockChannels.value));
#if IOT_ATOMIC_SUN_CALC_POWER
        client->publish(_getMetricsTopics(4), _qos, 1, String(power, 1));
#endif
    }

    JsonUnnamedObject json(2);
    json.add(JJ(type), JJ(ue));
    auto &events = json.addArray(JJ(events));
    JsonUnnamedObject *obj;

    obj = &events.addObject(3);
    obj->add(JJ(id), F("dimmer_brightness"));
    obj->add(JJ(value), _data.brightness.value);
    obj->add(JJ(state), _data.state.value);

    obj = &events.addObject(2);
    obj->add(JJ(id), F("dimmer_color"));
    obj->add(JJ(value), _data.color.value);

    obj = &events.addObject(2);
    obj->add(JJ(id), F("dimmer_lock"));
    obj->add(JJ(value), (int)_data.lockChannels.value);

    for(uint8_t i = 0; i < 4; i++) {
        obj = &events.addObject(2);
        obj->add(JJ(id), PrintString(F("dimmer_channel%u"), i));
        obj->add(JJ(value), _channels[i]);
        // obj->add(JJ(state), _channels[i] > 0);
    }

#if IOT_ATOMIC_SUN_CALC_POWER
    obj = &events.addObject(2);
    obj->add(JJ(id), F("dimmer_power"));
    obj->add(JJ(value), JsonNumber(power, 1));
#endif

    StreamString buffer;
    json.printTo(buffer);

    Scheduler.removeTimer(_publishTimer);
    Scheduler.addTimer(&_publishTimer, 100, false, [this, buffer](EventScheduler::TimerPtr timer) {
        WsClient::broadcast(WsWebUISocket::getWsWebUI(), WsWebUISocket::getSender(), buffer.c_str(), buffer.length());
        _publishTimer = nullptr;
    });
}

void Driver_4ChDimmer::_printStatus(Print &out)
{
    out.print(F(", Fading enabled" HTML_S(br) "Power "));
    if (_data.state.value) {
        out.print(F("on"));
    } else {
        out.print(F("off"));
    }
    out.printf_P(PSTR(", brightness %.1f%%"), _data.brightness.value * 100.0 / IOT_ATOMIC_SUN_MAX_BRIGHTNESS);
    out.printf_P(PSTR(", color temperature %d K" HTML_S(br)), (int)_data.color.value != 0 ? (((1000000 * 100) / (int)_data.color.value)) : 0);
    Dimmer_Base::_printStatus(out);
}

bool Driver_4ChDimmer::on(uint8_t channel)
{
    if (!_data.state.value) {
        memcpy(_channels, _storedChannels, sizeof(_channels));
        _channelsToBrightness();
        _setChannels(getOnOffFadeTime());
        publishState();
        return true;
    }
    return false;
}

bool Driver_4ChDimmer::off(uint8_t channel)
{
    if (_data.state.value) {
        memcpy(_storedChannels, _channels, sizeof(_storedChannels));
        _data.state.value = false;
        _data.brightness.value = 0;
        _setChannels(getOnOffFadeTime());
        publishState();
        return true;
    }
    return false;
}

// convert channels to brightness and color
void Driver_4ChDimmer::_channelsToBrightness()
{
    // calculate color and brightness values from dimmer channels
    int32_t sum = _channels[0] + _channels[1] + _channels[2] + _channels[3];
    if (sum) {
        _data.color.value = ((_channels[CHANNEL_WW1] + _channels[CHANNEL_WW2]) * COLOR_RANGE) / (double)sum + COLOR_MIN;
        _data.brightness.value = sum;
        _data.state.value = true;
    } else {
        _data.brightness.value = 0;
        _data.color.value = COLOR_MIN + (COLOR_RANGE / 2);
        _data.state.value = false;
    }
    _calcRatios();
    _debug_printf_P(PSTR("Driver_4ChDimmer::_brightnessToChannels(): ww=%u,%u cw=%u,%u = brightness=%u,color=%f, ratio=%f,%f\n"),
        _channels[CHANNEL_WW1],
        _channels[CHANNEL_WW2],
        _channels[CHANNEL_CW1],
        _channels[CHANNEL_CW2],
        _data.brightness.value,
        _data.color.value,
        _ratio[0],
        _ratio[1]
    );
}

// convert brightness and color to channels
void Driver_4ChDimmer::_brightnessToChannels()
{
    double color = (_data.color.value - COLOR_MIN) / COLOR_RANGE;
    uint16_t ww = _data.brightness.value * color;
    uint16_t cw = _data.brightness.value * (1.0 - color);
    _channels[CHANNEL_WW2] = ww / _ratio[0];
    _channels[CHANNEL_WW1] = ww - _channels[CHANNEL_WW2];
    _channels[CHANNEL_CW2] = cw / _ratio[1];
    _channels[CHANNEL_CW1] = cw - _channels[CHANNEL_CW2];
    _debug_printf_P(PSTR("Driver_4ChDimmer::_brightnessToChannels(): brightness=%u,color=%f(%f) = ww=%u,%u cw=%u,%u, ratio=%f,%f\n"),
        _data.brightness.value,
        _data.color.value,
        color,
        _channels[CHANNEL_WW1],
        _channels[CHANNEL_WW2],
        _channels[CHANNEL_CW1],
        _channels[CHANNEL_CW2],
        _ratio[0],
        _ratio[1]
    );
}

void Driver_4ChDimmer::_setLockChannels(bool value)
{
    _data.lockChannels.value = value;
    if (value) {
        _ratio[0] = 2;
        _ratio[1] = 2;
    }
}

void Driver_4ChDimmer::_calcRatios()
{
    _ratio[0] = _channels[CHANNEL_WW2] ? ((_channels[CHANNEL_WW1] + _channels[CHANNEL_WW2]) / (float)_channels[CHANNEL_WW2]) : (_channels[CHANNEL_WW1] ? INFINITY : 2);
    _ratio[1] = _channels[CHANNEL_CW2] ? ((_channels[CHANNEL_CW1] + _channels[CHANNEL_CW2]) / (float)_channels[CHANNEL_CW2]) : (_channels[CHANNEL_CW1] ? INFINITY : 2);
    _debug_printf_P(PSTR("Driver_4ChDimmer::_calcRatios(): %f,%f\n"), _ratio[0], _ratio[1]);
}

#if IOT_ATOMIC_SUN_CALC_POWER
float Driver_4ChDimmer::_calcPower(uint8_t channel)
{
    // this calculation is based on measurements of the bulbs being used
    static float adjust[4] PROGMEM = { 0.9945, 0.9671, .9863, 0.9041 };
    auto value = _channels[channel];
    if (value > 5400) {
        return 36.5 * adjust[channel];
    }
    else if (value <= 1000) {
        return 0;
    }
    else if (value <= 2300) {
        return 0.1;
    }
    else {
        return (41.2 + (-41.2 / (1 + pow((value - 2300.9) / 1240.9, 2.2)))) * adjust[channel];
    }
}

float Driver_4ChDimmer::_calcTotalPower()
{
    return _calcPower(0) + _calcPower(1) + _calcPower(2) + _calcPower(3) + 0.6;
}
#endif

int16_t Driver_4ChDimmer::getChannel(uint8_t channel) const
{
    return _channels[channel];
}

bool Driver_4ChDimmer::getChannelState(uint8_t channel) const
{
    return _channels[channel] > 0;
}

void Driver_4ChDimmer::setChannel(uint8_t channel, int16_t level, float time)
{
    _debug_printf_P(PSTR("Driver_4ChDimmer::setChannel(%u, %u, %f): locked=%u\n"), channel, level, time, _data.lockChannels.value);
    _channels[channel] = level;
    if (_data.lockChannels.value) {
        switch(channel) {
            case CHANNEL_WW1:
                _channels[CHANNEL_WW2] = level;
                break;
            case CHANNEL_WW2:
                _channels[CHANNEL_WW1] = level;
                break;
            case CHANNEL_CW1:
                _channels[CHANNEL_CW2] = level;
                break;
            case CHANNEL_CW2:
                _channels[CHANNEL_CW1] = level;
                break;
        }
    } else {
        _calcRatios();
    }
    _channelsToBrightness();
    _setChannels(time);
}

uint8_t Driver_4ChDimmer::getChannelCount() const
{
    return 4;
}

void Driver_4ChDimmer::_setChannels(float fadetime)
{
    if (fadetime == -1) {
        fadetime = getFadeTime();
    }
    _debug_printf_P(PSTR("Driver_4ChDimmer::_setChannels(): %u,%u,%u,%u state=%u\n"), _channels[0], _channels[1], _channels[2], _channels[3], _data.state.value);
    _fade(0, _channels[0], fadetime);
    _fade(1, _channels[1], fadetime);
    _fade(2, _channels[2], fadetime);
    _fade(3, _channels[3], fadetime);
    writeEEPROM();
}

// get brightness values from dimmer
void Driver_4ChDimmer::_getChannels()
{
    _debug_printf_P(PSTR("Driver_4ChDimmer::_getChannels()\n"));
    if (_lockWire()) {
        _wire.beginTransmission(DIMMER_I2C_ADDRESS);
        _wire.write(DIMMER_REGISTER_COMMAND);
        _wire.write(DIMMER_COMMAND_READ_CHANNELS);
        _wire.write((uint8_t)(sizeof(_channels) / sizeof(_channels[0])) << 4);
        if (_endTransmission() == 0) {
            if (_wire.requestFrom(DIMMER_I2C_ADDRESS, sizeof(_channels)) == sizeof(_channels)) {
                _wire.readBytes(reinterpret_cast<uint8_t *>(&_channels), sizeof(_channels));
                _debug_printf_P(PSTR("Driver_4ChDimmer::_getChannels(): %u,%u,%u,%u\n"), _channels[0], _channels[1], _channels[2], _channels[3]);
            }
        }
        _unlockWire();

#if IOT_SENSOR_HLW80xx_ADJUST_CURRENT
    Scheduler.addTimer(100, false, [this](EventScheduler::TimerPtr) {
        _setDimmingLevels();
    });
#endif
    }
}

AtomicSunPlugin dimmer_plugin;

void AtomicSunPlugin::setup(PluginSetupMode_t mode)
{
    _begin();
    setupWebServer();
}

void AtomicSunPlugin::reconfigure(PGM_P source)
{
    if (!source) {
        writeConfig();
    }
    else if (!strcmp_P_P(source, PSTR("http"))) {
        setupWebServer();
    }
}

bool AtomicSunPlugin::hasReconfigureDependecy(PluginComponent *plugin) const
{
    return plugin->nameEquals(F("http"));
}

void AtomicSunPlugin::createWebUI(WebUI &webUI)
{
    auto row = &webUI.addRow();
    row->setExtraClass(F("title"));
    row->addGroup(F("Atomic Sun"), false);

    row = &webUI.addRow();
    row->addSlider(F("dimmer_brightness"), F("Atomic Sun Brightness"), 0, IOT_ATOMIC_SUN_MAX_BRIGHTNESS * 4);

    row = &webUI.addRow();
    row->addColorSlider(F("dimmer_color"), F("Atomic Sun Color"));

    row = &webUI.addRow();
    row->addBadgeSensor(F("dimmer_vcc"), F("Atomic Sun VCC"), F("V"));
    row->addBadgeSensor(F("dimmer_frequency"), F("Frequency"), F("Hz"));
    row->addBadgeSensor(F("dimmer_int_temp"), F("ATmega"), F("\u00b0C"));
    row->addBadgeSensor(F("dimmer_ntc_temp"), F("NTC"), F("\u00b0C"));
#if IOT_ATOMIC_SUN_CALC_POWER
    row->addBadgeSensor(F("dimmer_power"), F("Power"), F("W"));
#endif
    row->addSwitch(F("dimmer_lock"), F("Lock Channels"), false, true);

    row->addGroup(F("Channels"), false);

    const uint8_t order[4] = { CHANNEL_WW1, CHANNEL_WW2, CHANNEL_CW1, CHANNEL_CW2 };
    for(uint8_t j = 0; j < 4; j++) {
        row = &webUI.addRow();
        row->addSlider(PrintString(F("dimmer_channel%u"), order[j]), PrintString(F("Channel %u"), j + 1), 0, IOT_ATOMIC_SUN_MAX_BRIGHTNESS);
    }
}

#endif
