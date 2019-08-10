/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_DIMMER_MODULE

#include "../include/templates.h"
#include "progmem_data.h"
#include "plugins.h"
#include "dimmer_module.h"
#include "dimmer_web_socket.h"
#include "dimmer_module_form.h"

#if DEBUG_IOT_DIMMER_MODULE
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#define DIMMER_COMPONENT SPGM(component_light)

#include "../../trailing_edge_dimmer/src/dimmer_protocol.h"

Driver_DimmerModule *Driver_DimmerModule::_dimmer = nullptr;

#if IOT_DIMMER_MODULE_INTERFACE_UART
Driver_DimmerModule::Driver_DimmerModule(HardwareSerial &serial) : _serial(serial) {
    _wire = new SerialTwoWire(serial);
    _wire->onReadSerial(SerialHandler::serialLoop);
#else
Driver_DimmerModule::Driver_DimmerModule() {
    _wire = &Wire;
    _wire->begin(IOT_DIMMER_MODULE_INTERFACE_SDA, IOT_DIMMER_MODULE_INTERFACE_SCL);
    _wire->setClockStretchLimit(45000);
#endif
    _wire->begin(DIMMER_I2C_ADDRESS + 1);
    _wire->onReceive(Driver_DimmerModule::onReceive);

#if defined(IOT_DIMMER_MODULE_LVL_SHIFTER_ENABLE)
    pinMode(IOT_DIMMER_MODULE_LVL_SHIFTER_ENABLE, OUTPUT);
    digitalWrite(IOT_DIMMER_MODULE_LVL_SHIFTER_ENABLE, HIGH);
    _debug_printf_P(PSTR("Driver_DimmerModule::Driver_DimmerModule(): Setting PIN %u HIGH\n"), IOT_DIMMER_MODULE_LVL_SHIFTER_ENABLE);
#endif

    // memset(&_channels, 0, sizeof(_channels));
    auto mqttClient = MQTTClient::getClient();
    if (mqttClient) {
        for (uint8_t i = 0; i < IOT_DIMMER_MODULE_CHANNELS; i++) {
            _channels[i].setup(this, i);
            mqttClient->registerComponent(&_channels[i]);
        }
    }
    // memset(&_data, 0, sizeof(_data));
    // memset(&_stored, 0, sizeof(_stored));
    _temperature = 0;
    _vcc = 0;
    auto dimmer = config._H_GET(Config().dimmer);
    _fadeTime = dimmer.fade_time;
    _onOffFadeTime = dimmer.on_fade_time;
}

Driver_DimmerModule::~Driver_DimmerModule() {
    end();
    auto mqttClient = MQTTClient::getClient();
    if (mqttClient) {
        for (uint8_t i = 0; i < IOT_DIMMER_MODULE_CHANNELS; i++) {
            mqttClient->unregisterComponent(&_channels[i]);
        }
    }
    _dimmer = nullptr;
#if IOT_DIMMER_MODULE_INTERFACE_UART
    delete _wire;
#endif
#if defined(IOT_DIMMER_MODULE_LVL_SHIFTER_ENABLE)
    digitalWrite(IOT_DIMMER_MODULE_LVL_SHIFTER_ENABLE, LOW);
    pinMode(IOT_DIMMER_MODULE_LVL_SHIFTER_ENABLE, INPUT);
#endif

}

void Driver_DimmerModule::begin() {
    _debug_printf_P(PSTR("Driver_DimmerModule::begin()\n"));
#if IOT_DIMMER_MODULE_INTERFACE_UART
    #if AT_MODE_SUPPORTED
        disable_at_mode(Serial);
        if (IOT_DIMMER_MODULE_BAUD_RATE != 115200) {
            Serial.flush();
            Serial.begin(IOT_DIMMER_MODULE_BAUD_RATE);
        }
    #endif
    #if SERIAL_HANDLER
        serialHandler.addHandler(onData, SerialHandler::RECEIVE);
    #endif
#endif
    _getChannels();
}

void Driver_DimmerModule::end() {
    _debug_printf_P(PSTR("Driver_DimmerModule::end()\n"));
#if IOT_DIMMER_MODULE_INTERFACE_UART
    #if SERIAL_HANDLER
        serialHandler.removeHandler(onData);
    #endif
        if (IOT_DIMMER_MODULE_BAUD_RATE != 115200) {
            Serial.flush();
            Serial.begin(115200);
        }
    #if AT_MODE_SUPPORTED
        enable_at_mode(Serial);
    #endif
#endif
}

void Driver_DimmerModule::printStatus(PrintHtmlEntitiesString &out) {
    out.print(F(", Fading enabled" HTML_S(br)));
    for(uint8_t i = 0; i < IOT_DIMMER_MODULE_CHANNELS; i++) {
        out.printf_P(PSTR("Channel %u: "), i);
        if (_channels[i].getOnState()) {
            out.printf_P(PSTR("on - %.1f%%" HTML_S(br)), _channels[i].getLevel() / (float)IOT_DIMMER_MODULE_MAX_BRIGHTNESS * 100.0);
        } else {
            out.print(F("off" HTML_S(br)));
        }
    }
    if (_temperature) {
        out.printf_P(PSTR("Temperature %d" HTML_DEG "C"), _temperature);
    }
    if (_vcc) {
        if (_temperature) {
            out.print(F(", v"));
        } else {
            out.print('V');
        }
        out.printf_P(PSTR("oltage %.3fV"), _vcc / 1000.0);
    }
}

const String Driver_DimmerModule::getStatus() {
    PrintHtmlEntitiesString out;
    out.printf_P(PSTR("%u Channel MOSFET Dimmer enabled on "), IOT_DIMMER_MODULE_CHANNELS);
#if IOT_DIMMER_MODULE_INTERFACE_UART
    out.print(F("Serial Port"));
#else
    out.print(F("I2C"));
#endif
    if (_dimmer)  {
        _dimmer->printStatus(out);
    }
    return out;
}

bool Driver_DimmerModule::on(uint8_t channel) {
    if (channel >= IOT_DIMMER_MODULE_CHANNELS) {
        return false;
    }
    return _channels[channel].on();
}

bool Driver_DimmerModule::off(uint8_t channel) {
    if (channel >= IOT_DIMMER_MODULE_CHANNELS) {
        return false;
    }
    return _channels[channel].off();
}

#if DEBUG_IOT_DIMMER_MODULE
uint8_t Driver_DimmerModule::endTransmission() {
    auto result = _wire->endTransmission();
    _debug_printf_P(PSTR("Driver_DimmerModule::endTransmission(): returned %u\n"), result);
    return result;
}
#endif

// void Driver_DimmerModule::setLevel(uint8_t channel, float fadetime) {
//     _debug_printf_P(PSTR("Driver_DimmerModule::setLevel() channel %u brightness %d\n"), channel, _data[channel].brightness.value);
//     _fade(channel, _data[channel].brightness.value, fadetime);
// }

void Driver_DimmerModule::_fade(uint8_t channel, int16_t toLevel, float fadeTime) {
    _debug_printf_P(PSTR("Driver_DimmerModule::_fade(%u, %u, %.3f)\n"), channel, toLevel, fadeTime);

    _wire->beginTransmission(DIMMER_I2C_ADDRESS);
    _wire->write(DIMMER_REGISTER_CHANNEL);
    _wire->write(channel);
    _wire->write(reinterpret_cast<const uint8_t *>(&toLevel), sizeof(toLevel));
    _wire->write(reinterpret_cast<const uint8_t *>(&fadeTime), sizeof(fadeTime));
    _wire->write(DIMMER_COMMAND_FADE);
    endTransmission();
}

void Driver_DimmerModule::onReceive(int length) {
    // _debug_printf_P(PSTR("Driver_DimmerModule::onReceive(%u)\n"), length);
    _dimmer->_onReceive(length);
}

void Driver_DimmerModule::_onReceive(int length) {
    auto type = _wire->read();
    _debug_printf_P(PSTR("Driver_DimmerModule::_onReceive(%u): type %02x\n"), length, type);

    if (type == DIMMER_TEMPERATURE_REPORT && length == 4) {
        uint8_t temperature = _wire->read();
        uint16_t vcc;
        _wire->readBytes(reinterpret_cast<uint8_t *>(&vcc), sizeof(vcc));
        if (_temperature != temperature || _vcc != vcc) { // publish temp. or voltage if values have changed
            _temperature = temperature;
            _vcc = vcc;
            auto client = MQTTClient::getClient();
            if (client) {
                String topic = MQTTClient::formatTopic(-1, F("/metrics/"));
                if (_temperature) {
                    client->publish(topic + F("temperature"), MQTTClient::getDefaultQos(), 1, String(_temperature));
                }
                if (_vcc) {
                    client->publish(topic + F("vcc"), MQTTClient::getDefaultQos(), 1, String(_vcc / 1000.0, 3));
                }
            }
        }
    }
}

// store brightness in EEPROM
void Driver_DimmerModule::_writeState() {
    _debug_printf_P(PSTR("Driver_DimmerModule::_writeState()\n"));
    _wire->beginTransmission(DIMMER_I2C_ADDRESS);
    _wire->write(DIMMER_REGISTER_COMMAND);
    _wire->write(DIMMER_COMMAND_WRITE_EEPROM);
    endTransmission();
}

// get brightness values from dimmer
void Driver_DimmerModule::_getChannels() {
    int16_t level;
    _debug_printf_P(PSTR("Driver_DimmerModule::_getChannels()\n"));
    _wire->beginTransmission(DIMMER_I2C_ADDRESS);
    _wire->write(DIMMER_REGISTER_COMMAND);
    _wire->write(DIMMER_COMMAND_READ_CHANNELS);
    _wire->write(IOT_DIMMER_MODULE_CHANNELS << 4);
    if (endTransmission() == 0 && _wire->requestFrom(DIMMER_I2C_ADDRESS, IOT_DIMMER_MODULE_CHANNELS * sizeof(level)) == IOT_DIMMER_MODULE_CHANNELS * sizeof(level)) {
        for(uint8_t i = 0; i < IOT_DIMMER_MODULE_CHANNELS; i++) {
            _wire->readBytes(reinterpret_cast<uint8_t *>(&level), sizeof(level));
            _channels[i].setLevel(level);
        }
#if DEBUG_IOT_DIMMER_MODULE
        String str;
        for(uint8_t i = 0; i < IOT_DIMMER_MODULE_CHANNELS; i++) {
            str += String(_channels[i].getLevel());
            str += ' ';
        }
        _debug_printf_P(PSTR("Driver_DimmerModule::_getChannels(): %s\n"), str.c_str());
#endif
    }
}

#if IOT_DIMMER_MODULE_INTERFACE_UART

void Driver_DimmerModule::onData(uint8_t type, const uint8_t *buffer, size_t len) {
    _debug_printf_P(PSTR("Driver_DimmerModule::onData(): length %u\n"), len);
    while(len--) {
        _dimmer->_wire->feed(*buffer++);
    }
}

#endif

void Driver_DimmerModule::setup() {
#if IOT_DIMMER_MODULE_INTERFACE_UART
    _dimmer = _debug_new Driver_DimmerModule(Serial);
#else
    _dimmer = _debug_new Driver_DimmerModule();
#endif
    _dimmer->begin();
    WsDimmerClient::setup();
}

void Driver_DimmerModule::writeConfig() {
    auto dimmer = config._H_GET(Config().dimmer);

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

int16_t Driver_DimmerModule::getChannel(uint8_t channel) {
    return _channels[channel].getLevel();
}

void Driver_DimmerModule::Driver_DimmerModule::setChannel(uint8_t channel, int16_t level, float time) {
    if (time == -1) {
        time = _fadeTime;
    }
    _channels[channel].setLevel(level);
    _setChannel(channel, level, time);
    _writeState();
    _channels[channel].publishState();
}

void Driver_DimmerModule::writeEEPROM() {
    _writeState();
}

Driver_DimmerModule *Driver_DimmerModule::getInstance() {
    return _dimmer;
}

void dimmer_module_reconfigure(PGM_P source) {
    auto dimmer = Driver_DimmerModule::getInstance();
    if (dimmer) {
        if (source == nullptr) {
            dimmer->writeConfig();
        } 
        else {
            WsDimmerClient::setup();
        }
    }
}

#if AT_MODE_SUPPORTED && !IOT_DIMMER_MODULE_INTERFACE_UART

#include "at_mode.h"

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(DIMG, "DIMG", "Get level");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(DIMS, "DIMS", "<channel>,<level>[,<time>]", "Set level");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(DIMW, "DIMW", "Write EEPROM");

void at_mode_dimmer_not_initialized(Stream &serial) {
    serial.println(F("Dimmer not initizialized"));
}

bool dimmer_module_at_mode_command_handler(Stream &serial, const String &command, int8_t argc, char **argv) {
    if (command.length() == 0) {

        at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(DIMG));
        at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(DIMS));
        at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(DIMW));

    } 
    else if (constexpr_String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(DIMW))) {
        auto dimmer = Driver_DimmerModule::getInstance();
        if (!dimmer) {
            at_mode_dimmer_not_initialized(serial);
        }
        else {
            dimmer->writeEEPROM();
            at_mode_print_ok(serial);
        }
        return true;
    }
    else if (constexpr_String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(DIMG))) {
        auto dimmer = Driver_DimmerModule::getInstance();
        if (!dimmer) {
            at_mode_dimmer_not_initialized(serial);
        }
        else {
            for(uint8_t i = 0; i < IOT_DIMMER_MODULE_CHANNELS; i++) {
                serial.printf_P(PSTR("+DIMG: %u: %d\n"), i, dimmer->getChannel(i));
            }
        }
        return true;
    }
    else if (constexpr_String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(DIMS))) {
        auto dimmer = Driver_DimmerModule::getInstance();
        if (!dimmer) {
            at_mode_dimmer_not_initialized(serial);
        }
        else if (argc < 2) {
            at_mode_print_invalid_arguments(serial);
        } 
        else {
            float time = -1;
            int channel = atoi(argv[0]);
            if (channel >= 0 && channel < IOT_DIMMER_MODULE_CHANNELS) {
                int level = std::min(IOT_DIMMER_MODULE_MAX_BRIGHTNESS, std::max(0, atoi(argv[1])));
                if (argc >= 3) {
                    time = atof(argv[2]);
                }
                serial.printf_P(PSTR("+DIMS: Set %u: %d (time %.2f)\n"), channel, level, time);
                dimmer->setChannel(channel, level, time);
            }
            else {
                serial.println(F("+DIMS: Invalid channel"));
            }
        }
        return true;
    }
    return false;
}

#endif


PROGMEM_STRING_DECL(plugin_config_name_http);

PROGMEM_PLUGIN_CONFIG_DEF(
/* pluginName               */ dimmer,
/* setupPriority            */ 127,
/* allowSafeMode            */ false,
/* autoSetupWakeUp          */ false,
/* rtcMemoryId              */ 0,
/* setupPlugin              */ Driver_DimmerModule::setup,
/* statusTemplate           */ Driver_DimmerModule::getStatus,
/* configureForm            */ dimmer_module_create_settings_form,
/* reconfigurePlugin        */ dimmer_module_reconfigure,
/* reconfigure Dependencies */ SPGM(plugin_config_name_http),
/* prepareDeepSleep         */ nullptr,
#if IOT_DIMMER_MODULE_INTERFACE_UART
/* atModeCommandHandler     */ nullptr
#else
/* atModeCommandHandler     */ dimmer_module_at_mode_command_handler
#endif
);

#endif
