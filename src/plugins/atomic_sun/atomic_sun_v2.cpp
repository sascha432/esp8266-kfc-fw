/**
 * Author: sascha_lammers@gmx.de
 */

// v2 uses a 4 channel mosfet dimmer with a different serial protocol

#include <PrintHtmlEntitiesString.h>
#include <StreamString.h>
#include "../include/templates.h"
#include "LoopFunctions.h"
#include "plugins.h"
#include "atomic_sun_v2.h"
#include "../dimmer_module/dimmer_module_form.h"
#include "SerialTwoWire.h"
#include "WebUISocket.h"

#include "../dimmer_module/firmware_protocol.h"

#ifdef DEBUG_4CH_DIMMER
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

Driver_4ChDimmer::Driver_4ChDimmer() :
    MQTTComponent(ComponentTypeEnum_t::LIGHT),
    Dimmer_Base(),
    channel_ww1(IOT_ATOMIC_SUN_CHANNEL_WW1),
    channel_ww2(IOT_ATOMIC_SUN_CHANNEL_WW2),
    channel_cw1(IOT_ATOMIC_SUN_CHANNEL_CW1),
    channel_cw2(IOT_ATOMIC_SUN_CHANNEL_CW2)
{
#if DEBUG_MQTT_CLIENT
    debug_printf_P(PSTR("component=%p\n"), this);
#endif
}

void Driver_4ChDimmer::readConfig()
{
    Dimmer_Base::readConfig();
    auto &dimmer = config._H_GET(Config().dimmer);
    channel_ww1 = dimmer.channel_mapping[0];
    channel_ww2 = dimmer.channel_mapping[1];
    channel_cw1 = dimmer.channel_mapping[2];
    channel_cw2 = dimmer.channel_mapping[3];
}


void Driver_4ChDimmer::_begin()
{
    Dimmer_Base::_begin();
    _channels = ChannelsArray();
    _storedChannels = ChannelsArray();
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
        output.print(FSPGM(enabled));
#if IOT_DIMMER_MODULE_INTERFACE_UART
        output.print(F(" on Serial Port"));
#else
        output.print(F(" on I2C"));
#endif
        _printStatus(output);
    }
    else {
        output.print(FSPGM(disabled));
    }
}
void Driver_4ChDimmer::createAutoDiscovery(MQTTAutoDiscovery::Format_t format, MQTTAutoDiscoveryVector &vector)
{
    _createTopics();

    auto discovery = new MQTTAutoDiscovery();
    discovery->create(this, F("main"), format);
    discovery->addStateTopic(_data.state.state);
    discovery->addCommandTopic(_data.state.set);
    discovery->addPayloadOn(1);
    discovery->addPayloadOff(0);
    discovery->addBrightnessStateTopic(_data.brightness.state);
    discovery->addBrightnessCommandTopic(_data.brightness.set);
    discovery->addBrightnessScale(MAX_LEVEL_ALL_CHANNELS);
    discovery->addColorTempStateTopic(_data.color.state);
    discovery->addColorTempCommandTopic(_data.color.set);
    discovery->finalize();
    vector.emplace_back(discovery);

    discovery = new MQTTAutoDiscovery();
    discovery->create(this, F("lock_channels"), format);
    discovery->addStateTopic(_data.lockChannels.state);
    discovery->addCommandTopic(_data.lockChannels.set);
    discovery->addPayloadOn(1);
    discovery->addPayloadOff(0);
    discovery->finalize();
    vector.emplace_back(discovery);

    for(uint8_t i = 0; i < MAX_CHANNELS; i++) {
        discovery = new MQTTAutoDiscovery();
        discovery->create(this, PrintString(FSPGM(channel__u, "channel_%u"), i), format);
        discovery->addStateTopic(_data.channels[i].state);
        discovery->addCommandTopic(_data.channels[i].set);
        discovery->addBrightnessStateTopic(_data.channels[i].brightnessState);
        discovery->addBrightnessCommandTopic(_data.channels[i].brightnessSet);
        discovery->addBrightnessScale(MAX_LEVEL);
        discovery->addPayloadOn(1);
        discovery->addPayloadOff(0);
        discovery->finalize();
        vector.emplace_back(discovery);
    }

    MQTTComponentHelper component(MQTTComponent::ComponentTypeEnum_t::SENSOR);

    discovery = component.createAutoDiscovery(F("temp"), format);
    discovery->addStateTopic(_getMetricsTopics(0));
    discovery->addUnitOfMeasurement(F("\u00b0C"));
    discovery->finalize();
    vector.emplace_back(discovery);

    discovery = component.createAutoDiscovery(F("temp2"), format);
    discovery->addStateTopic(_getMetricsTopics(1));
    discovery->addUnitOfMeasurement(F("\u00b0C"));
    discovery->finalize();
    vector.emplace_back(discovery);

    discovery = component.createAutoDiscovery(F("vcc"), format);
    discovery->addStateTopic(_getMetricsTopics(2));
    discovery->addUnitOfMeasurement('V');
    discovery->finalize();
    vector.emplace_back(discovery);

    discovery = component.createAutoDiscovery(F("frequency"), format);
    discovery->addStateTopic(_getMetricsTopics(3));
    discovery->addUnitOfMeasurement(F("Hz"));
    discovery->finalize();
    vector.emplace_back(discovery);

#if IOT_ATOMIC_SUN_CALC_POWER
    discovery = component.createAutoDiscovery(F("power"), format);
    discovery->addStateTopic(_getMetricsTopics(4));
    discovery->addUnitOfMeasurement('W');
    discovery->finalize();
    vector.emplace_back(discovery);

#endif
}

uint8_t Driver_4ChDimmer::getAutoDiscoveryCount() const
{
#if IOT_ATOMIC_SUN_CALC_POWER
    return MAX_CHANNELS + 7;
#else
    return MAX_CHANNELS + 6;
#endif
}

void Driver_4ChDimmer::_createTopics()
{
    if (_data.state.set.length() == 0) {
        String main = F("main");
        String lockChannels = F("lock_channels");

        _data.state.set = MQTTClient::formatTopic(main, F("/set"));
        _data.state.state = MQTTClient::formatTopic(main, F("/state"));
        _data.brightness.set = MQTTClient::formatTopic(main, F("/brightness/set"));
        _data.brightness.state = MQTTClient::formatTopic(main, F("/brightness/state"));
        _data.color.set = MQTTClient::formatTopic(main, F("/color/set"));
        _data.color.state = MQTTClient::formatTopic(main, F("/color/state"));

        _data.lockChannels.set = MQTTClient::formatTopic(lockChannels, F("/lock/set"));
        _data.lockChannels.state = MQTTClient::formatTopic(lockChannels, F("/lock/state"));

        for(uint8_t i = 0; i < MAX_CHANNELS; i++) {
            auto channel = PrintString(FSPGM(channel__u), i);
            _data.channels[i].set = MQTTClient::formatTopic(channel, F("/set"));
            _data.channels[i].state = MQTTClient::formatTopic(channel, F("/state"));
            _data.channels[i].brightnessSet = MQTTClient::formatTopic(channel, F("/brightness/set"));
            _data.channels[i].brightnessState = MQTTClient::formatTopic(channel, F("/brightness/state"));
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
    for(uint8_t i = 0; i < MAX_CHANNELS; i++) {
        client->subscribe(this, _data.channels[i].set, _qos);
        client->subscribe(this, _data.channels[i].brightnessSet, _qos);
    }

    publishState(client);
}

void Driver_4ChDimmer::onMessage(MQTTClient *client, char *topic, char *payload, size_t len)
{
    int value = atoi(payload);
    _debug_printf_P(PSTR("topic=%s value=%d\n"), topic, value);

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
            _data.brightness.value = std::min(value, MAX_LEVEL_ALL_CHANNELS);
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

        for(uint8_t i = 0; i < MAX_CHANNELS; i++) {
            auto &channel =_data.channels[i];

            if (channel.brightnessSet.equals(topic)) {

                float fadetime = (_channels[i] && value > 0) ? getFadeTime() : getOnOffFadeTime();
                _channels[i] = value;
                _channelsToBrightness();
                _setChannels(fadetime);
                publishState(client);
                break;

            }
            else if (channel.set.equals(topic)) {

                if (value && _channels[i]) {
                    // already on
                    break;
                }
                else if (value == 0 && _channels[i] == 0) {
                    // already off
                    break;
                }
                else {
                    float fadetime = getOnOffFadeTime();
                    if (value) {
                        value = _storedChannels[i]; // restore last state
                        if (value <= MIN_LEVEL) {
                            value = DEFAULT_LEVEL;
                        }
                    }
                    else {
                        // value = 0;
                    }

                    _channels[i] = value;
                    _channelsToBrightness();
                    _setChannels(fadetime);
                    publishState(client);
                    break;
                }

            }

        }
    }

}

void Driver_4ChDimmer::setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState)
{
    _debug_printf_P(PSTR("id=%s value=%s hasValue=%u state=%u hasState=%u\n"), id.c_str(), value.c_str(), hasValue, state, hasState);

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
                    _data.brightness.value = std::min(val, MAX_LEVEL_ALL_CHANNELS);
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
    _debug_println();
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
    _debug_println();

#if IOT_ATOMIC_SUN_CALC_POWER
    auto power = _calcTotalPower();
#endif

    if (!client) {
        client = MQTTClient::getClient();
    }
    if (client && client->isConnected()) {
        client->publish(_data.state.state, _qos, 1, String(_data.state.value ? 1 : 0));
        client->publish(_data.brightness.state, _qos, 1, String(_data.brightness.value));
        client->publish(_data.color.state, _qos, 1, String((int)(_data.color.value / 100)));
        for(uint8_t i = 0; i < 4; i++) {
            client->publish(_data.channels[i].state, _qos, 1, String(_channels[i] ? 1 : 0));
            client->publish(_data.channels[i].brightnessState, _qos, 1, String(_channels[i]));
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

    for(uint8_t i = 0; i < MAX_CHANNELS; i++) {
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

    auto buffer = std::shared_ptr<StreamString>(new StreamString());
    json.printTo(*buffer);

    LoopFunctions::callOnce([this, buffer]() {
        WsClient::broadcast(WsWebUISocket::getWsWebUI(), WsWebUISocket::getSender(), buffer->c_str(), buffer->length());
    });
}

void Driver_4ChDimmer::_printStatus(Print &out)
{
    out.printf_P(PSTR(HTML_S(br) "Channel order WW1: %d WW2: %d CW1: %d CW2: %d" HTML_S(br) "Power "), channel_ww1, channel_ww2, channel_cw1, channel_cw2);
    if (_data.state.value) {
        out.print(FSPGM(on));
    } else {
        out.print(FSPGM(off));
    }
    out.printf_P(PSTR(", brightness %.1f%%"), _data.brightness.value * (100.0 / (MAX_LEVEL_ALL_CHANNELS)));
    out.printf_P(PSTR(", color temperature %d K" HTML_S(br)), (int)_data.color.value != 0 ? (((1000000 * 100) / (int)_data.color.value)) : 0);
    Dimmer_Base::_printStatus(out);
}

void Driver_4ChDimmer::_onReceive(size_t length)
{
    if (_wire.peek() == DIMMER_FADING_COMPLETE) {
        _wire.read();
        length--;

        bool update = false;
        dimmer_fading_complete_event_t event;
        while(length >= sizeof(event)) {
            length -= _wire.read(event);
            if (event.channel < MAX_CHANNELS) {
                if (_channels[event.channel] != event.level) {  // update level if out of sync
                    _channels[event.channel] = event.level;
                    if (event.level > MIN_LEVEL) {
                        _storedChannels[event.channel] = event.level;
                    }
                    update = true;
                }
            }
        }
        if (update) {
            publishState();
        }
    }
    else {
        Dimmer_Base::_onReceive(length);
    }
}

bool Driver_4ChDimmer::on(uint8_t channel)
{
    if (!_data.state.value) {
        _channels = _storedChannels;
        if (_channels.getSum(MIN_LEVEL)) {
            _channels.setAll(DEFAULT_LEVEL);
        }
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
        _data.state.value = false;
        _data.brightness.value = 0;
        _channels.setAll(0);
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
    int32_t sum = _channels.getSum();
    if (sum) {
        _data.color.value = ((_channels[channel_ww1] + _channels[channel_ww2]) * COLOR_RANGE) / (double)sum + COLOR_MIN;
        _data.brightness.value = sum;
        _data.state.value = true;
    } else {
        _data.brightness.value = 0;
        _data.color.value = COLOR_MIN + (COLOR_RANGE / 2);
        _data.state.value = false;
    }
    _calcRatios();
    _debug_printf_P(PSTR("ww=%u,%u cw=%u,%u = brightness=%u color=%f ratio=%f,%f\n"),
        _channels[channel_ww1],
        _channels[channel_ww2],
        _channels[channel_cw1],
        _channels[channel_cw2],
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
    _channels[channel_ww2] = ww / _ratio[0];
    _channels[channel_ww1] = ww - _channels[channel_ww2];
    _channels[channel_cw2] = cw / _ratio[1];
    _channels[channel_cw1] = cw - _channels[channel_cw2];
    _debug_printf_P(PSTR("brightness=%u color=%f(%f) = ww=%u,%u cw=%u,%u, ratio=%f,%f\n"),
        _data.brightness.value,
        _data.color.value,
        color,
        _channels[channel_ww1],
        _channels[channel_ww2],
        _channels[channel_cw1],
        _channels[channel_cw2],
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
    _ratio[0] = _channels[channel_ww2] ? ((_channels[channel_ww1] + _channels[channel_ww2]) / (float)_channels[channel_ww2]) : (_channels[channel_ww1] ? INFINITY : 2);
    _ratio[1] = _channels[channel_cw2] ? ((_channels[channel_cw1] + _channels[channel_cw2]) / (float)_channels[channel_cw2]) : (_channels[channel_cw1] ? INFINITY : 2);
    _debug_printf_P(PSTR("ratio=%f ratio=%f\n"), _ratio[0], _ratio[1]);
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
    _debug_printf_P(PSTR("channel=%u level=%u time=%f locked=%u\n"), channel, level, time, _data.lockChannels.value);
    _channels[channel] = level;
    if (_data.lockChannels.value) {
        if (channel == channel_ww1) {
            _channels[channel_ww2] = level;
        }
        else if (channel == channel_ww2) {
            _channels[channel_ww1] = level;
        }
        else if (channel == channel_cw1) {
            _channels[channel_cw2] = level;
        }
        else if (channel == channel_cw2) {
            _channels[channel_cw1] = level;
        }
    } else {
        _calcRatios();
    }
    _channelsToBrightness();
    _setChannels(time);
}

void Driver_4ChDimmer::_setChannels(float fadetime)
{
    if (fadetime == -1) {
        fadetime = getFadeTime();
    }

    _debug_printf_P(PSTR("channels=%s state=%u\n"), implode(',', _channels).c_str(), _data.state.value);
    for(uint8_t i = 0; i < MAX_CHANNELS; i++) {
        if (_channels[i] > MIN_LEVEL) {
            _storedChannels[i] = _channels[i];
        }
        _fade(i, _channels[i], fadetime);
    }
    writeEEPROM();
}

// get brightness values from dimmer
void Driver_4ChDimmer::_getChannels()
{
    _debug_println();
    if (_wire.lock()) {
        _wire.beginTransmission(DIMMER_I2C_ADDRESS);
        _wire.write(DIMMER_REGISTER_COMMAND);
        _wire.write(DIMMER_COMMAND_READ_CHANNELS);
        _wire.write(MAX_CHANNELS << 4);
        if (_wire.endTransmission() == 0) {
            if (_wire.requestFrom(DIMMER_I2C_ADDRESS, sizeof(_channels)) == sizeof(_channels)) {
                _wire.read(_channels);
                _debug_printf_P(PSTR("channels=%s\n"), implode(',', _channels).c_str());
            }
        }
        _wire.unlock();

#if IOT_SENSOR_HLW80xx_ADJUST_CURRENT
        LoopFunctions::callOnce([this]() {
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
        _end();
        _begin();
    }
    else if (!strcmp_P_P(source, SPGM(http))) {
        setupWebServer();
    }
}

void AtomicSunPlugin::restart()
{
    _end();
}

bool AtomicSunPlugin::hasReconfigureDependecy(PluginComponent *plugin) const
{
    return plugin->nameEquals(FSPGM(http));
}

void AtomicSunPlugin::createWebUI(WebUI &webUI)
{
    auto row = &webUI.addRow();
    row->setExtraClass(FSPGM(title));
    row->addGroup(F("Atomic Sun"), false);

    row = &webUI.addRow();
    row->addSlider(F("dimmer_brightness"), F("Atomic Sun Brightness"), 0, MAX_LEVEL_ALL_CHANNELS);

    row = &webUI.addRow();
    row->addColorSlider(F("dimmer_color"), F("Atomic Sun Color"));

    row = &webUI.addRow();
    row->addBadgeSensor(F("dimmer_vcc"), F("Atomic Sun VCC"), 'V');
    row->addBadgeSensor(F("dimmer_frequency"), F("Frequency"), F("Hz"));
    row->addBadgeSensor(F("dimmer_int_temp"), F("ATmega"), F("\u00b0C"));
    row->addBadgeSensor(F("dimmer_ntc_temp"), F("NTC"), F("\u00b0C"));
#if IOT_ATOMIC_SUN_CALC_POWER
    row->addBadgeSensor(F("dimmer_power"), F("Power"), 'W');
#endif
    row->addSwitch(F("dimmer_lock"), F("Lock Channels"), false, true);

    row->addGroup(F("Channels"), false);

    const int8_t order[4] = { channel_ww1, channel_ww2, channel_cw1, channel_cw2 };
    for(uint8_t j = 0; j < MAX_CHANNELS; j++) {
        row = &webUI.addRow();
        row->addSlider(PrintString(F("dimmer_channel%u"), order[j]), PrintString(F("Channel %u"), j + 1), 0, MAX_LEVEL);
    }
}

#if AT_MODE_SUPPORTED

#include "at_mode.h"

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(DIMG, "DIMG", "Get level");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(DIMS, "DIMS", "<channel>,<level>[,<time>]", "Set level");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(DIMW, "DIMW", "Write EEPROM");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(DIMR, "DIMR", "Reset ATmega");

bool AtomicSunPlugin::hasAtMode() const
{
    return true;
}

void AtomicSunPlugin::atModeHelpGenerator()
{
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(DIMG), getName());
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(DIMS), getName());
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(DIMW), getName());
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(DIMR), getName());
}

bool AtomicSunPlugin::atModeHandler(AtModeArgs &args)
{
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(DIMW))) {
        writeEEPROM();
        args.ok();
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(DIMG))) {
        for(uint8_t i = 0; i < MAX_CHANNELS; i++) {
            args.printf_P(PSTR("%u: %d"), i, getChannel(i));
        }
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(DIMR))) {
        args.printf_P(PSTR("Pulling GPIO%u low for 10ms"), STK500V1_RESET_PIN);
        dimmer_plugin.resetDimmerMCU();
        args.printf_P(PSTR("GPIO%u set to input"), STK500V1_RESET_PIN);
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(DIMS))) {
        if (args.requireArgs(2, 2)) {
            size_t channel = args.toInt(0);
            if (channel >= 0 && channel < MAX_CHANNELS) {
                int level = args.toIntMinMax(1, 0, (int)MAX_LEVEL);
                float time = args.toFloat(2, -1);
                args.printf_P(PSTR("Set %u: %d (time %.2f)"), channel, level, time);
                setChannel(channel, level, time);
            }
            else {
                args.print(F("Invalid channel"));
            }
        }
        return true;
    }
    return false;
}

#endif
