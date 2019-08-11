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
#include "atomic_sun_web_socket.h"
#include "../dimmer_module/dimmer_module_form.h"
#include "SerialTwoWire.h"

#ifdef DEBUG_4CH_DIMMER
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#define DIMMER_COMPONENT SPGM(component_light)

#include "../../trailing_edge_dimmer/src/dimmer_protocol.h"

SerialTwoWire SerialWire;
Driver_4ChDimmer *Driver_4ChDimmer::_dimmer = nullptr;

Driver_4ChDimmer::Driver_4ChDimmer(HardwareSerial &serial) : _serial(serial) {
    memset(&_channels, 0, sizeof(_channels));
    auto mqttClient = MQTTClient::getClient();
    if (mqttClient) {
        mqttClient->registerComponent(this);
    }
    _data.state.value = false;
    _data.brightness.value = 0;
    _data.color.value = 0;
    memset(&_stored, 0, sizeof(_stored));
    _qos = 0;

    auto dimmer = config._H_GET(Config().dimmer);
    _fadeTime = dimmer.fade_time;
    _onOffFadeTime = dimmer.on_fade_time;
}

Driver_4ChDimmer::~Driver_4ChDimmer() {
    end();
    auto mqttClient = MQTTClient::getClient();
    if (mqttClient) {
        mqttClient->unregisterComponent(this);
    }
    _dimmer = nullptr;
}

const String Driver_4ChDimmer::getName() {
    return F("Atomic Sun v2");
}

PGM_P Driver_4ChDimmer::getComponentName() {
    return SPGM(mqtt_component_light);
}

MQTTAutoDiscovery *Driver_4ChDimmer::createAutoDiscovery(MQTTAutoDiscovery::Format_t format) {
    if (_data.state.set.length() == 0) {
        _createTopics();
    }
    auto discovery = _debug_new MQTTAutoDiscovery();
    discovery->create(this, format);
    discovery->addParameter(FSPGM(mqtt_state_topic), _data.state.state);
    discovery->addParameter(FSPGM(mqtt_command_topic), _data.state.set);
    discovery->addParameter(FSPGM(mqtt_payload_on), FSPGM(1));
    discovery->addParameter(FSPGM(mqtt_payload_off), FSPGM(0));
    discovery->addParameter(FSPGM(mqtt_brightness_state_topic), _data.brightness.state);
    discovery->addParameter(FSPGM(mqtt_brightness_command_topic), _data.brightness.set);
    discovery->addParameter(FSPGM(mqtt_brightness_scale), String(IOT_ATOMIC_SUN_MAX_BRIGHTNESS));
    discovery->addParameter(FSPGM(mqtt_color_temp_state_topic), _data.color.state);
    discovery->addParameter(FSPGM(mqtt_color_temp_command_topic), _data.color.set);
    discovery->finalize();
    return discovery;
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
    
#if MQTT_AUTO_DISCOVERY
    if (MQTTAutoDiscovery::isEnabled()) {
        auto discovery = createAutoDiscovery(MQTTAutoDiscovery::FORMAT_JSON);
        client->publish(discovery->getTopic(), _qos, true, discovery->getPayload());
        delete discovery;
    }
#endif

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
            fadetime = _data.state.value ? _getFadeTime() : _getOnOffFadeTime();
            _data.state.value = true;
            _data.brightness.value = std::min(value, IOT_ATOMIC_SUN_MAX_BRIGHTNESS);
        } else {
            fadetime = _data.state.value ? _getOnOffFadeTime() : _getFadeTime();
            _data.state.value = false;
            _data.brightness.value = 0;
        }
        setLevel(fadetime);
        publishState(client);

    } else if (_data.color.set.equals(topic)) {

        _data.color.value = value;
        setLevel(_getFadeTime());
        publishState(client);

    }
}

void Driver_4ChDimmer::publishState(MQTTClient *client) {
    if (!client) {
        client = MQTTClient::getClient();
    } 
    if (client) {
        _publishState(client);
    }

    PrintString message(F("+SETBC %d %u"), getBrightness(), getColor());
    WsAtomicSunClient::broadcast(message);
}

void Driver_4ChDimmer::_publishState(MQTTClient *client) {

    _debug_printf_P(PSTR("Driver_4ChDimmer::_publishState()\n"));

    client->publish(_data.state.state, _qos, 1, String(_data.state.value));
    client->publish(_data.brightness.state, _qos, 1, String(_data.brightness.value));
    client->publish(_data.color.state, _qos, 1, String(_data.color.value));
}

void Driver_4ChDimmer::begin() {
    _debug_printf_P(PSTR("Driver_4ChDimmer::begin()\n"));
#if AT_MODE_SUPPORTED
    disable_at_mode(Serial);
    if (IOT_ATOMIC_SUN_BAUD_RATE != 115200) {
        Serial.flush();
        Serial.begin(IOT_ATOMIC_SUN_BAUD_RATE);
    }
#endif
#if SERIAL_HANDLER
    serialHandler.addHandler(onData, SerialHandler::RECEIVE);
#endif
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

void Driver_4ChDimmer::end() {
    _debug_printf_P(PSTR("Driver_4ChDimmer::end()\n"));
#if SERIAL_HANDLER
    serialHandler.removeHandler(onData);
#endif
    if (IOT_ATOMIC_SUN_BAUD_RATE != 115200) {
        Serial.flush();
        Serial.begin(115200);
    }
#if AT_MODE_SUPPORTED
    enable_at_mode(Serial);
#endif
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
    if (_stored.temperature) {
        out.printf_P(PSTR("Temperature %d" HTML_DEG "C"), _stored.temperature);
    }
    if (_stored.vcc) {
        if (_stored.temperature) {
            out.print(F(", v"));
        } else {
            out.print('V');
        }
        out.printf_P(PSTR("oltage %.3fV"), _stored.vcc / 1000.0);
    }
}

const String Driver_4ChDimmer::getStatus() {
    PrintHtmlEntitiesString out;
    out = F("4 Channel MOSFET Dimmer enabled on Serial Port");
    if (_dimmer)  {
        _dimmer->_printStatus(out);
    }
    return out;
}

bool Driver_4ChDimmer::on() {
    if (!_data.state.value) {
        _data.brightness.value = _stored.brightness;
        _data.state.value = true;
        setLevel(_getOnOffFadeTime());
        publishState();
        return true;
    }
    return false;
}

bool Driver_4ChDimmer::off() {
    if (_data.state.value) {
        memset(&_channels, 0, sizeof(_channels));

        _fade(0xff, 0, _getOnOffFadeTime());
        _writeState();

        _stored.brightness = _data.brightness.value;
        _data.state.value = false;
        _data.brightness.value = 0;
        publishState();
        return true;
    }
    return false;
}

void Driver_4ChDimmer::setLevel(float fadetime) {
    if (fadetime == -1) {
        fadetime = _getFadeTime();
    }
    uint8_t color = (_data.color.value - 153) * 255 / (500 - 153);
    auto channel12 = _data.brightness.value * color * 2 / 255;
    auto channel34 = _data.brightness.value * (255 - color) * 2 / 255;
    _debug_printf_P(PSTR("Driver_4ChDimmer::setBrightness() time=%.3f brightness=%d color=%d ch1/2=%d ch3/4=%d\n"), fadetime, _data.brightness.value, color, channel12, channel34);
    _setChannels(channel12, channel12, channel34, channel34, fadetime);
}

void Driver_4ChDimmer::_fade(uint8_t channel, int16_t toLevel, float fadeTime) {
    _debug_printf_P(PSTR("Driver_4ChDimmer::_fade(%u, %u, %f)\n"), channel, toLevel, fadeTime);

    SerialWire.beginTransmission(DIMMER_I2C_ADDRESS);
    SerialWire.write(DIMMER_REGISTER_CHANNEL);
    SerialWire.write(channel);
    SerialWire.write(reinterpret_cast<const uint8_t *>(&toLevel), sizeof(toLevel));
    SerialWire.write(reinterpret_cast<const uint8_t *>(&fadeTime), sizeof(fadeTime));
    SerialWire.write(DIMMER_COMMAND_FADE);
    SerialWire.endTransmission();
}

void Driver_4ChDimmer::onReceive(int length) {
    _dimmer->_onReceive(length);
}

void Driver_4ChDimmer::_onReceive(int length) {

    auto type = SerialWire.read();

    _debug_printf_P(PSTR("Driver_4ChDimmer::_onReceive(%u): type %02x\n"), length, type);

    if (type == DIMMER_TEMPERATURE_REPORT && length == 4) {
        uint8_t temperature = SerialWire.read();
        uint16_t vcc;
        SerialWire.readBytes(reinterpret_cast<uint8_t *>(&vcc), sizeof(vcc));
        if (_stored.temperature != temperature || _stored.vcc != vcc) { // publish temp. or voltage if values have changed
            _stored.temperature = temperature;
            _stored.vcc = vcc;
            auto client = MQTTClient::getClient();
            if (client) {
                if (_stored.temperature) {
                    client->publish(_metricsTopic + F("temperature"), _qos, 1, String(_stored.temperature));
                }
                if (_stored.vcc) {
                    client->publish(_metricsTopic + F("vcc"), _qos, 1, String(_stored.vcc / 1000.0, 3));
                }
            }
        }
    }
    else if (type == DIMMER_TEMPERATURE_ALERT && length == 3) {
        uint8_t temperature = SerialWire.read();
        uint8_t max_temperature = SerialWire.read();
        Logger_error(F("Atomic Sun: Temperature alarm triggered: %u > %u"), temperature, max_temperature);
    }
}

// store brightness in EEPROM
void Driver_4ChDimmer::_writeState() {
    _debug_printf_P(PSTR("Driver_4ChDimmer::_writeState()\n"));
    SerialWire.beginTransmission(DIMMER_I2C_ADDRESS);
    SerialWire.write(DIMMER_REGISTER_COMMAND);
    SerialWire.write(DIMMER_COMMAND_WRITE_EEPROM);
    SerialWire.endTransmission();
}

// set all 4 channels and store brightness in EEPROM
void Driver_4ChDimmer::_setChannels(int16_t ch1, int16_t ch2, int16_t ch3, int16_t ch4, float fadetime) {
    _fade(0, ch1, fadetime);
    _fade(1, ch2, fadetime);
    _fade(2, ch3, fadetime);
    _fade(3, ch4, fadetime);
    _writeState();
}

// get brightness values from dimmer
void Driver_4ChDimmer::_getChannels() {
    _debug_printf_P(PSTR("Driver_4ChDimmer::_getChannels()\n"));
    SerialWire.beginTransmission(DIMMER_I2C_ADDRESS);
    SerialWire.write(DIMMER_REGISTER_COMMAND);
    SerialWire.write(DIMMER_COMMAND_READ_CHANNELS);
    SerialWire.write((uint8_t)(sizeof(_channels) / sizeof(_channels[0])) << 4);
    if (SerialWire.endTransmission() == 0) {
        if (SerialWire.requestFrom(DIMMER_I2C_ADDRESS, sizeof(_channels)) == sizeof(_channels)) {
            SerialWire.readBytes(reinterpret_cast<uint8_t *>(&_channels), sizeof(_channels));
            _debug_printf_P(PSTR("Driver_4ChDimmer::_getChannels(): %u, %u, %u, %u\n"), _channels[0], _channels[1], _channels[2], _channels[3]);
        }
    }
}

#if DEBUG_4CH_DIMMER
uint8_t Driver_4ChDimmer::endTransmission() {
    auto result = SerialWire.endTransmission();
    _debug_printf_P(PSTR("Driver_4ChDimmer::endTransmission(): returned %u\n"), result);
    return result;
}
#else
inline uint8_t Driver_4ChDimmer::endTransmission() {
    return SerialWire.endTransmission();
}
#endif

void Driver_4ChDimmer::onData(uint8_t type, const uint8_t *buffer, size_t len) {
    _debug_printf_P(PSTR("Driver_4ChDimmer::onData(): length %u\n"), len);
    while(len--) {
        SerialWire.feed(*buffer++);
    }
}

void Driver_4ChDimmer::writeConfig() {
    auto dimmer = config._H_GET(Config().dimmer);
    auto _wire = &SerialWire;

    _wire->beginTransmission(DIMMER_I2C_ADDRESS);
    _wire->write(DIMMER_REGISTER_OPTIONS);
    if (endTransmission() == 0 && _wire->requestFrom(DIMMER_I2C_ADDRESS, 1) == 1) {
        uint8_t options = _wire->read();

        if (dimmer.restore_level) {
            options |= DIMMER_OPTIONS_RESTORE_LEVEL;
        } else {
            options &= ~DIMMER_OPTIONS_RESTORE_LEVEL;
        }

        _wire->beginTransmission(DIMMER_I2C_ADDRESS);
        _wire->write(DIMMER_REGISTER_OPTIONS);
        _wire->write(options);
        _wire->write(dimmer.max_temperature);
        _wire->write(reinterpret_cast<const uint8_t *>(&dimmer.on_fade_time), sizeof(dimmer.on_fade_time));
        _wire->write(dimmer.temp_check_int);
        _wire->write(reinterpret_cast<const uint8_t *>(&dimmer.linear_correction), sizeof(dimmer.linear_correction));
        if (endTransmission() == 0) {
            _writeState();
        }
    }

}

void Driver_4ChDimmer::setup() {
    SerialWire.begin(DIMMER_I2C_ADDRESS + 1);
    SerialWire.setSerial(Serial);
    SerialWire.onReadSerial(SerialHandler::serialLoop);
    SerialWire.onReceive(Driver_4ChDimmer::onReceive);
    _dimmer = _debug_new Driver_4ChDimmer(Serial);
    _dimmer->begin();
    WsAtomicSunClient::setup();
}


void dimmer_module_reconfigure(PGM_P source) {
    if (!source) {
        auto dimmer = Driver_4ChDimmer::getInstance();
        if (dimmer) {
            dimmer->writeConfig();
        }
    }
    else {
        WsAtomicSunClient::setup();
    }
}

PROGMEM_STRING_DECL(plugin_config_name_http);

PROGMEM_PLUGIN_CONFIG_DEF(
/* pluginName               */ atomicsun,
/* setupPriority            */ 127,
/* allowSafeMode            */ false,
/* autoSetupWakeUp          */ false,
/* rtcMemoryId              */ 0,
/* setupPlugin              */ Driver_4ChDimmer::setup,
/* statusTemplate           */ Driver_4ChDimmer::getStatus,
/* configureForm            */ dimmer_module_create_settings_form,
/* reconfigurePlugin        */ dimmer_module_reconfigure,
/* reconfigure Dependencies */ SPGM(plugin_config_name_http),
/* prepareDeepSleep         */ nullptr,
/* atModeCommandHandler     */ nullptr
);

#endif
