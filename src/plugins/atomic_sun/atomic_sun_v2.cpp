/**
 * Author: sascha_lammers@gmx.de
 */

// v2 uses a 4 channel mosfet dimmer with a different serial protocol

#include <PrintHtmlEntitiesString.h>
#include <StreamString.h>
#include "../include/templates.h"
#include "LoopFunctions.h"
#include "plugins.h"
#include "plugins_menu.h"
#include "atomic_sun_v2.h"
#include "../dimmer_module/dimmer_module_form.h"
#include "SerialTwoWire.h"
#include "WebUISocket.h"

#include "../dimmer_module/firmware_protocol.h"

#if DEBUG_4CH_DIMMER
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
    channel_ww1 = _config.channel_mapping[0];
    channel_ww2 = _config.channel_mapping[1];
    channel_cw1 = _config.channel_mapping[2];
    channel_cw2 = _config.channel_mapping[3];
}

void Driver_4ChDimmer::_begin()
{
    Dimmer_Base::_begin();
    _channels = ChannelsArray();
    _storedChannels = ChannelsArray();
    MQTTClient::safeRegisterComponent(this);
    _data.state.value = false;
    _data.brightness.value = 0;
    _data.color.value = 0;
    _data.lockChannels.value = false;
    _getChannels();
    _channelsToBrightness();
}

void Driver_4ChDimmer::_end()
{
    MQTTClient::safeUnregisterComponent(this);
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

void AtomicSunPlugin::createConfigureForm(PluginComponent::FormCallbackType type, const String &formName, Form &form, AsyncWebServerRequest *request)
{
    if (isCreateFormCallbackType(type)) {
        readConfig();
        DimmerModuleForm::_createConfigureForm(type, formName, form, request);
    }
}


MQTTComponent::MQTTAutoDiscoveryPtr Driver_4ChDimmer::nextAutoDiscovery(MQTTAutoDiscovery::FormatType format, uint8_t num)
{
    if (num >= getAutoDiscoveryCount()) {
        return nullptr;
    }
    auto discovery = new MQTTAutoDiscovery();
    switch(num) {
        case 0:
            _createTopics();
            discovery->create(this, FSPGM(main), format);
            discovery->addStateTopic(_data.state.state);
            discovery->addCommandTopic(_data.state.set);
            discovery->addPayloadOn(1);
            discovery->addPayloadOff(0);
            discovery->addBrightnessStateTopic(_data.brightness.state);
            discovery->addBrightnessCommandTopic(_data.brightness.set);
            discovery->addBrightnessScale(MAX_LEVEL_ALL_CHANNELS);
            discovery->addColorTempStateTopic(_data.color.state);
            discovery->addColorTempCommandTopic(_data.color.set);
            break;
        case 1:
            discovery->create(this, FSPGM(lock_channels, "lock_channels"), format);
            discovery->addStateTopic(_data.lockChannels.state);
            discovery->addCommandTopic(_data.lockChannels.set);
            discovery->addPayloadOn(1);
            discovery->addPayloadOff(0);
            break;
        case 2:
        case 3:
        case 4:
        case 5: {
            uint8_t i = num - 2;
            discovery->create(this, PrintString(FSPGM(channel__u, "channel_%u"), i), format);
            discovery->addStateTopic(_data.channels[i].state);
            discovery->addCommandTopic(_data.channels[i].set);
            discovery->addBrightnessStateTopic(_data.channels[i].brightnessState);
            discovery->addBrightnessCommandTopic(_data.channels[i].brightnessSet);
            discovery->addBrightnessScale(MAX_LEVEL);
            discovery->addPayloadOn(1);
            discovery->addPayloadOff(0);
        }
        break;
    }
    discovery->finalize();
    return discovery;
}

uint8_t Driver_4ChDimmer::getAutoDiscoveryCount() const
{
    return 6;
}

void Driver_4ChDimmer::_createTopics()
{
    if (_data.state.set.length() == 0) {
        String main = FSPGM(main);
        String lockChannels = FSPGM(lock_channels);

        _data.state.set = MQTTClient::formatTopic(main, FSPGM(_set));
        _data.state.state = MQTTClient::formatTopic(main, FSPGM(_state));
        _data.brightness.set = MQTTClient::formatTopic(main, FSPGM(_brightness_set, "/brightness/set"));
        _data.brightness.state = MQTTClient::formatTopic(main, FSPGM(_brightness_state, "/brightness/state"));
        _data.color.set = MQTTClient::formatTopic(main, FSPGM(_color_set, "/color/set"));
        _data.color.state = MQTTClient::formatTopic(main, FSPGM(_color_state, "/color/state"));

        _data.lockChannels.set = MQTTClient::formatTopic(lockChannels, F("/lock/set"));
        _data.lockChannels.state = MQTTClient::formatTopic(lockChannels, F("/lock/state"));

        for(uint8_t i = 0; i < _channels.size(); i++) {
            auto channel = PrintString(FSPGM(channel__u), i);
            _data.channels[i].set = MQTTClient::formatTopic(channel, FSPGM(_set));
            _data.channels[i].state = MQTTClient::formatTopic(channel, FSPGM(_state));
            _data.channels[i].brightnessSet = MQTTClient::formatTopic(channel, FSPGM(_brightness_set));
            _data.channels[i].brightnessState = MQTTClient::formatTopic(channel, FSPGM(_brightness_state));
        }
    }
}

void Driver_4ChDimmer::onConnect(MQTTClient *client)
{
    _createTopics();

    client->subscribe(this, _data.state.set);
    client->subscribe(this, _data.brightness.set);
    client->subscribe(this, _data.color.set);
    client->subscribe(this, _data.lockChannels.set);
    for(uint8_t i = 0; i < _channels.size(); i++) {
        client->subscribe(this, _data.channels[i].set);
        client->subscribe(this, _data.channels[i].brightnessSet);
    }

    publishState(client);
}

void Driver_4ChDimmer::onMessage(MQTTClient *client, char *topic, char *payload, size_t len)
{
    int value = atoi(payload);
    __LDBG_printf("topic=%s value=%d", topic, value);

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

        for(uint8_t i = 0; i < _channels.size(); i++) {
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

void Driver_4ChDimmer::_setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState)
{
    __LDBG_printf("id=%s value=%s hasValue=%u state=%u hasState=%u", id.c_str(), value.c_str(), hasValue, state, hasState);

    auto ptr = id.c_str();
    if (!strncmp_P(ptr, PSTR("dimmer_"), 7)) {
        ptr += 7;
        if (!strcmp_P(ptr, SPGM(brightness))) {
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
            Dimmer_Base::_setValue(id, value, hasValue, state, hasState);
            publishState();
        }
    }
}

void Driver_4ChDimmer::_getValues(JsonArray &array)
{
    _debug_println();
    Dimmer_Base::_getValues(array);

    auto obj = &array.addObject(2);
    obj->add(JJ(id), F("dimmer_brightness"));
    obj->add(JJ(value), _data.brightness.value);

    obj = &array.addObject(2);
    obj->add(JJ(id), F("dimmer_color"));
    obj->add(JJ(value), _data.color.value);

    obj = &array.addObject(2);
    obj->add(JJ(id), F("dimmer_lock"));
    obj->add(JJ(value), (int)_data.lockChannels.value);
}


void Driver_4ChDimmer::publishState(MQTTClient *client)
{
    _debug_println();

    if (!client) {
        client = MQTTClient::getClient();
    }
    if (client && client->isConnected()) {
        client->publish(_data.state.state, true, String(_data.state.value ? 1 : 0));
        client->publish(_data.brightness.state, true, String(_data.brightness.value));
        client->publish(_data.color.state, true, String((int)(_data.color.value / 100)));
        for(uint8_t i = 0; i < 4; i++) {
            client->publish(_data.channels[i].state, true, String(_channels[i] ? 1 : 0));
            client->publish(_data.channels[i].brightnessState, true, String(_channels[i]));
        }
        client->publish(_data.lockChannels.state, true, String(_data.lockChannels.value));
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

        // update MQTT
        _forceMetricsUpdate(5);
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
    __LDBG_printf("ww=%u,%u cw=%u,%u = brightness=%u color=%f ratio=%f,%f",
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
    __LDBG_printf("brightness=%u color=%f(%f) = ww=%u,%u cw=%u,%u, ratio=%f,%f",
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
    __LDBG_printf("ratio=%f ratio=%f", _ratio[0], _ratio[1]);
}

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
    __LDBG_printf("channel=%u level=%u time=%f locked=%u", channel, level, time, _data.lockChannels.value);
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

    __LDBG_printf("channels=%s state=%u", implode(',', _channels).c_str(), _data.state.value);
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
                __LDBG_printf("channels=%s", implode(',', _channels).c_str());
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

PROGMEM_DEFINE_PLUGIN_OPTIONS(
    AtomicSunPlugin,
    "atomicsun",        // name
    "Atomic Sun v2",    // friendly name
    "",                 // web_templates
    "dimmer_cfg",       // config_forms
    "mqtt,http",        // reconfigure_dependencies
    PluginComponent::PriorityType::ATOMIC_SUN,
    PluginComponent::RTCMemoryId::NONE,
    static_cast<uint8_t>(PluginComponent::MenuType::CUSTOM),
    false,              // allow_safe_mode
    false,              // setup_after_deep_sleep
    true,               // has_get_status
    true,               // has_config_forms
    true,               // has_web_ui
    false,              // has_web_templates
    true,               // has_at_mode
    0                   // __reserved
);

AtomicSunPlugin::AtomicSunPlugin() : PluginComponent(PROGMEM_GET_PLUGIN_OPTIONS(AtomicSunPlugin)), Driver_4ChDimmer()
{
    REGISTER_PLUGIN(this, "AtomicSunPlugin");
}


void AtomicSunPlugin::setup(SetupModeType mode)
{
    _begin();
    setupWebServer();
}

void AtomicSunPlugin::reconfigure(const String &source)
{
    if (String_equals(source, SPGM(http))) {
        setupWebServer();
    }
    else if (String_equals(source, SPGM(mqtt))) {
        MQTTClient::safeReRegisterComponent(this);
    }
    else {
        writeConfig();
    }
}

void AtomicSunPlugin::shutdown()
{
    _end();
}

void AtomicSunPlugin::createMenu()
{
    bootstrapMenu.addSubMenu(getFriendlyName(), F("dimmer_cfg.html"), navMenu.config);
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
    auto sensor = getMetricsSensor();
    if (sensor) {
        sensor->_createWebUI(webUI, &row);
    }
    // row->addBadgeSensor(F("dimmer_vcc"), F("Atomic Sun VCC"), 'V');
    // row->addBadgeSensor(F("dimmer_frequency"), F("Frequency"), FSPGM(Hz));
    // row->addBadgeSensor(F("dimmer_int_temp"), F("ATmega"), FSPGM(_degreeC));
    // row->addBadgeSensor(F("dimmer_ntc_temp"), F("NTC"), FSPGM(_degreeC));
    row->addSwitch(F("dimmer_lock"), F("Lock Channels"), false, true);

    row->addGroup(F("Channels"), false);

    const int8_t order[4] = { channel_ww1, channel_ww2, channel_cw1, channel_cw2 };
    for(uint8_t j = 0; j < MAX_CHANNELS; j++) {
        row = &webUI.addRow();
        row->addSlider(PrintString(F("dimmer_channel%u"), order[j]), PrintString(F("Channel %u"), j + 1), 0, MAX_LEVEL);
    }
}
