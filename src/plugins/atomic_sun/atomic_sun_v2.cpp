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

#define COMMAND_PREFIX "~$"
#define DEBUG_4CH_DIMMER 0

#ifdef DEBUG_4CH_DIMMER
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#define DIMMER_COMPONENT SPGM(component_light)

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
    auto discovery = _debug_new MQTTAutoDiscovery();
    discovery->create(this, format);
    discovery->addParameter(FSPGM(mqtt_state_topic), _data.state.state);
    discovery->addParameter(FSPGM(mqtt_command_topic), _data.state.set);
    discovery->addParameter(FSPGM(mqtt_payload_on), FSPGM(1));
    discovery->addParameter(FSPGM(mqtt_payload_off), FSPGM(0));
    discovery->addParameter(FSPGM(mqtt_brightness_state_topic), _data.brightness.state);
    discovery->addParameter(FSPGM(mqtt_brightness_command_topic), _data.brightness.set);
    discovery->addParameter(FSPGM(mqtt_brightness_scale), String(MAX_BRIGHTNESS));
    discovery->addParameter(FSPGM(mqtt_color_temp_state_topic), _data.color.state);
    discovery->addParameter(FSPGM(mqtt_color_temp_command_topic), _data.color.set);
    discovery->finalize();
    return discovery;
}

void Driver_4ChDimmer::onConnect(MQTTClient *client) {

    _data.state.set = MQTTClient::formatTopic(getNumber(), F("/set"));
    _data.state.state = MQTTClient::formatTopic(getNumber(), F("/state"));
    _data.brightness.set = MQTTClient::formatTopic(getNumber(), F("/brightness/set"));
    _data.brightness.state = MQTTClient::formatTopic(getNumber(), F("/brightness/state"));
    _data.color.set = MQTTClient::formatTopic(getNumber(), F("/color/set"));
    _data.color.state = MQTTClient::formatTopic(getNumber(), F("/color/state"));

#if MQTT_AUTO_DISCOVERY
    if (MQTTAutoDiscovery::isEnabled()) {
        auto discovery = createAutoDiscovery(MQTTAutoDiscovery::FORMAT_JSON);
        client->publish(discovery->getTopic(), MQTTClient::getDefaultQos(), true, discovery->getPayload());
        delete discovery;
    }
#endif

    client->subscribe(this, _data.state.set, MQTTClient::getDefaultQos());
    client->subscribe(this, _data.brightness.set, MQTTClient::getDefaultQos());
    client->subscribe(this, _data.color.set, MQTTClient::getDefaultQos());

    _publishState(client);
}

void Driver_4ChDimmer::onMessage(MQTTClient *client, char *topic, char *payload, size_t len) {

    int value = atoi(payload);

    if (_data.state.set.equals(topic)) {

        // on/off only changes brightness if the state is different
        if (value) {
            on();
        } else {
            off();
        }
        _publishState(client);

    } else if (_data.brightness.set.equals(topic)) {

        float fadetime;

        // if brightness changes, also turn dimmer on or off
        if (value > 0) {
            fadetime = _data.state.value ? FADE_TIME_NORMAL : FADE_TIME_ON_OFF;
            _data.state.value = true;
            _data.brightness.value = std::min(value, MAX_BRIGHTNESS);
        } else {
            fadetime = _data.state.value ? FADE_TIME_ON_OFF : FADE_TIME_NORMAL;
            _data.state.value = false;
            _data.brightness.value = 0;
        }
        setLevel(fadetime);
        _publishState(client);

    } else if (_data.color.set.equals(topic)) {

        _data.color.value = value;
        setLevel();
        _publishState(client);

    }
}

void Driver_4ChDimmer::_publishState(MQTTClient *client) {

    _debug_printf_P(PSTR("Driver_4ChDimmer::_publishState()\n"));

    client->publish(_data.state.state, MQTTClient::getDefaultQos(), 1, String(_data.state.value));
    client->publish(_data.brightness.state, MQTTClient::getDefaultQos(), 1, String(_data.brightness.value));
    client->publish(_data.color.state, MQTTClient::getDefaultQos(), 1, String(_data.color.value));
}

void Driver_4ChDimmer::begin() {
    _debug_printf_P(PSTR("Driver_4ChDimmer::begin()\n"));
#if AT_MODE_SUPPORTED
    disable_at_mode();
    if (DIMMER_BAUDRATE != 115200) {
        Serial.flush();
        Serial.begin(DIMMER_BAUDRATE);
    }
#endif
#if SERIAL_HANDLER
    serialHandler.addHandler(IF_DEBUG("4ChDimmer"), onData, SerialHandler::RECEIVE);//|SerialHandler::LOCAL_TX);
#endif
    _getChannels();
    // calculate color and brightness values from dimmer channels
    int32_t sum = _channels[0] + _channels[1] + _channels[2] + _channels[3];
    if (sum) {
        _data.color.value = round((_channels[0] + _channels[1]) / sum * (500 - 153)) + 153;
        _data.brightness.value = sum * MAX_BRIGHTNESS / 4;
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
    if (DIMMER_BAUDRATE != 115200) {
        Serial.flush();
        Serial.begin(115200);
    }
#if AT_MODE_SUPPORTED
    enable_at_mode();
#endif
}

void Driver_4ChDimmer::printStatus(PrintHtmlEntitiesString &out) {
    out.print(F(", Fading enabled" HTML_S(br) "Power "));
    if (_data.state.value) {
        out.print(F("on"));
    } else {
        out.print(F("off"));
    }
    out.printf_P(PSTR(", brightness %.1f%%"), _data.brightness.value / (float)MAX_BRIGHTNESS * 100);
    out.printf_P(PSTR(", color temperature %d K" HTML_S(br)), 1000000 / _data.color.value);
    if (_stored.temperature) {
        out.printf_P(PSTR("Temperature %d&deg;C"), _stored.temperature);
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
        _dimmer->printStatus(out);
    }
    return out;
}

bool Driver_4ChDimmer::on() {
    if (!_data.state.value) {
        _data.brightness.value = _stored.brightness;
        _data.state.value = true;
        setLevel(30.0);

        auto client = MQTTClient::getClient();
        if (client) {
            _publishState(client);
        }
        return true;
    }
    return false;
}

bool Driver_4ChDimmer::off() {
    if (_data.state.value) {
        memset(&_channels, 0, sizeof(_channels));

        _clearResponse();
        _serialWrite(PSTR("fade=-1,0,30.0,-1\n"));
        _writeState();
        _waitForResponse(2);

        _stored.brightness = _data.brightness.value;
        _data.state.value = false;
        _data.brightness.value = 0;
        auto client = MQTTClient::getClient();
        if (client) {
            _publishState(client);
        }
        return true;
    }
    return false;
}

void Driver_4ChDimmer::setLevel(float fadetime) {
    uint8_t color = (_data.color.value - 153) * 255 / (500 - 153);
    auto channel12 = _data.brightness.value * color * 2 / 255;
    auto channel34 = _data.brightness.value * (255 - color) * 2 / 255;
    _debug_printf_P(PSTR("Driver_4ChDimmer::setBrightness() _brightness %d _color %d = %d, %d\n"), _data.brightness.value, color, channel12, channel34);
    _setChannels(channel12, channel12, channel34, channel34, fadetime);
}

void Driver_4ChDimmer::_clearResponse() {
    _responseCount = 0;
    _response = String();
}

// store brightness in EEPROM
void Driver_4ChDimmer::_writeState() {
    _serialWrite(PSTR(COMMAND_PREFIX "write\n"));
}

// send command to set a single channel
void Driver_4ChDimmer::_setChannel(uint8_t channel, int16_t brightness, float fadetime) {
    if (channel < 0 || channel >= 4) {
        return;
    }
    _channels[channel] = brightness;
    _serialWrite(PSTR(COMMAND_PREFIX "fade=-1,%u,%.3f,%u\n"), brightness, fadetime, channel);
}

// set all 4 channels and store brightness in EEPROM
void Driver_4ChDimmer::_setChannels(int16_t ch1, int16_t ch2, int16_t ch3, int16_t ch4, float fadetime) {
    _clearResponse();
    _setChannel(0, ch1, fadetime);
    _setChannel(1, ch2, fadetime);
    _setChannel(2, ch3, fadetime);
    _setChannel(3, ch4, fadetime);
    _writeState();
    _waitForResponse(5);
}

// get brightness values from dimmer
void Driver_4ChDimmer::_getChannels() {
    _clearResponse();
    _serialWrite(PSTR(COMMAND_PREFIX "get\n"));
    _waitForResponse(4);
}

// parse serial response from dimmer and extract temperature, voltage and brightness levels
int Driver_4ChDimmer::_parseLine(const String &response) {
    // _debug_printf_P(PSTR("Driver_4ChDimmer::_parseLine(%s)\n"), response.c_str());
#if AT_MODE_SUPPORTED
    if (response.startsWith(F("~~~~"))) { // re-enable at mode for debugging purposes
        enable_at_mode();
    }
#endif
    uint8_t temperature = _stored.temperature;
    uint16_t vcc = _stored.vcc;
    int pos = response.indexOf(F("temp="));
    if (pos != -1) {
        temperature = response.substring(pos + 5).toInt();
    }
    pos = response.indexOf(F("vcc="));
    if (pos != -1) {
        vcc = response.substring(pos + 4).toInt();
    }
    if (_stored.temperature != temperature || _stored.vcc != vcc) { // publish temp. or voltage if values have changed
        _stored.temperature = temperature;
        _stored.vcc = vcc;
        auto client = MQTTClient::getClient();
        if (client) {
            String topic = MQTTClient::formatTopic(-1, F("/metrics/"));
            if (_stored.temperature) {
                client->publish(topic + F("temperature"), MQTTClient::getDefaultQos(), 1, String(_stored.temperature));
            }
            if (_stored.vcc) {
                client->publish(topic + F("vcc"), MQTTClient::getDefaultQos(), 1, String(_stored.vcc / 1000.0, 3));
            }
        }
    }

    // brightness
    pos = response.indexOf(F("ch="));
    if (pos != -1) {
        int channel = response.substring(pos + 3).toInt();
        pos = response.indexOf(F("lvl="));
        if (pos != -1) {
            int level = response.substring(pos + 4).toInt();
            if (channel >= 0 && channel < 4) {
                _channels[channel] = (int16_t)level;
                _debug_printf_P(PSTR("_parseLine(%s) channel %d value %d\n"), response.c_str(), channel, level);
                return level;
            }
        }
    }
    return -1;
}

#if SERIAL_HANDLER

void Driver_4ChDimmer::onData(uint8_t type, const uint8_t *buffer, size_t len) {
    // _debug_printf_P(PSTR("_onData(%p, %s, %u)\n"), _dimmer, printable_string(buffer, len).c_str(), len);
    if (_dimmer) {
        _dimmer->_onData(buffer, len);
    }
}

void Driver_4ChDimmer::_onData(const uint8_t *buffer, size_t len) {
    const char *ptr = (const char *)buffer;
    while(len--) {
        if (*ptr == '\n') {
            _parseLine(_response);
            if (!_response.startsWith(F("temp="))) { // do not count temperature report as response
                _responseCount++;
            }
            _response = String();
        } else if (*ptr == '\r') {
        } else {
            if (_response.length() < 64) { // discard additional data
                _response += (char)*ptr;
            }
        }
        ptr++;
    }
}

// wait for "count" responses or until a timeout occurs
int Driver_4ChDimmer::_waitForResponse(int count) {
    // _debug_printf_P(PSTR("Driver_4ChDimmer::_waitForResponse(%u)\n"), count);
    ulong timeout = millis() + 100;
    while(millis() < timeout && _responseCount < count) {
        delay(1);
    }
    // _debug_printf_P(PSTR("Driver_4ChDimmer::_waitForResponse(%u) = %d, time %d ms\n"), count, _responseCount, (int)(millis() - (timeout - 100)));
    return _responseCount;
}

#else

int Driver_4ChDimmer::_waitForResponse(int count) {
    int ch;
    _response = String();
    _responseCount = 0;
    ulong timeout = millis() + 100;
    while(millis() < timeout && _responseCount < count) {
        if (_serial.available()) {
            ch = _serial.read();
            if (ch == '\r' || ch == -1) {
            } else if (ch == '\n') {
                _responseCount++;
                _getValue(_response);
                _response = String();
            } else {
                _response += (char)ch;
            }
        }
        delay(1);
    }
    return _responseCount;
}

#endif

void Driver_4ChDimmer::_serialWrite(PGM_P format, ...) {
    char buf[128];
    va_list arg;
    va_start(arg, format);
    vsnprintf_P(buf, sizeof(buf), format, arg);
    va_end(arg);
    // _debug_printf_P(PSTR("Driver_4ChDimmer::_serialWrite: %s"), buf);
    _serial.print(buf);
}

void Driver_4ChDimmer::setup(bool isSafeMode) {
    if (isSafeMode) {
        return;
    }
    _dimmer = _debug_new Driver_4ChDimmer(Serial);
    _dimmer->begin();
}

// bool atomic_sun_v2_at_mode_command_handler(Stream &serial, const String &command, int8_t argc, char **argv) {
//     return false;
// }

PROGMEM_PLUGIN_CONFIG_DEF(
/* pluginName               */ atomicsun,
/* setupPriority            */ 127,
/* allowSafeMode            */ false,
/* autoSetupWakeUp          */ false,
/* rtcMemoryId              */ 0,
/* setupPlugin              */ Driver_4ChDimmer::setup,
/* statusTemplate           */ Driver_4ChDimmer::getStatus,
/* configureForm            */ nullptr,
/* reconfigurePlugin        */ nullptr,
/* reconfigure Dependencies */ nullptr,
/* prepareDeepSleep         */ nullptr,
/* atModeCommandHandler     */ atomic_sun_v2_at_mode_command_handler
);

#endif
