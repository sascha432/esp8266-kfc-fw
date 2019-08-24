/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_DIMMER_MODULE

#include "../include/templates.h"
#include "progmem_data.h"
#include "plugins.h"
#include "dimmer_module.h"
#include "WebUISocket.h"

#if DEBUG_IOT_DIMMER_MODULE
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#include "../../trailing_edge_dimmer/src/dimmer_protocol.h"

Driver_DimmerModule::Driver_DimmerModule() : Dimmer_Base() {
}

const String DimmerModulePlugin::getStatus() {
    PrintHtmlEntitiesString out;
    out.printf_P(PSTR("%u Channel MOSFET Dimmer enabled on "), IOT_DIMMER_MODULE_CHANNELS);
#if IOT_DIMMER_MODULE_INTERFACE_UART
    out.print(F("Serial Port"));
#else
    out.print(F("I2C"));
#endif
    _printStatus(out);
    return out;
}

void Driver_DimmerModule::_begin() {
    Dimmer_Base::_begin();
    auto mqttClient = MQTTClient::getClient();
    if (mqttClient) {
        mqttClient->setUseNodeId(true);
        for (uint8_t i = 0; i < IOT_DIMMER_MODULE_CHANNELS; i++) {
            _channels[i].setup(this, i);
            mqttClient->registerComponent(&_channels[i]);
        }
    }
    _getChannels();
}

void Driver_DimmerModule::_end() {
    auto mqttClient = MQTTClient::getClient();
    if (mqttClient) {
        for (uint8_t i = 0; i < IOT_DIMMER_MODULE_CHANNELS; i++) {
            mqttClient->unregisterComponent(&_channels[i]);
        }
    }
    _dimmer = nullptr;
    Dimmer_Base::_end();
}

void Driver_DimmerModule::createAutoDiscovery(MQTTAutoDiscovery::Format_t format, PrintHtmlEntitiesString &payload) {
    for (uint8_t i = 0; i < IOT_DIMMER_MODULE_CHANNELS; i++) {
        auto discovery = _channels[i].createAutoDiscovery(format);
        payload.print(discovery->getPayload());
        delete discovery;
    }
}

void Driver_DimmerModule::onConnect(MQTTClient *client) {

#if MQTT_AUTO_DISCOVERY
    if (MQTTAutoDiscovery::isEnabled()) {
        String topic = MQTTClient::formatTopic(-1, F("/metrics/"));
        MQTTComponentHelper component(MQTTComponent::SENSOR);

        component.setNumber(IOT_DIMMER_MODULE_CHANNELS);
        auto discovery = component.createAutoDiscovery();
        discovery->addStateTopic(topic + F("temperature"));
        discovery->addUnitOfMeasurement(F("°C"));
        discovery->finalize();
        client->publish(discovery->getTopic(), MQTTClient::getDefaultQos(), true, discovery->getPayload());
        delete discovery;

        component.setNumber(IOT_DIMMER_MODULE_CHANNELS + 1);
        discovery = component.createAutoDiscovery();
        discovery->addStateTopic(topic + F("vcc"));
        discovery->addUnitOfMeasurement(F("V"));
        discovery->finalize();
        client->publish(discovery->getTopic(), MQTTClient::getDefaultQos(), true, discovery->getPayload());
        delete discovery;

        component.setNumber(IOT_DIMMER_MODULE_CHANNELS + 2);
        discovery = component.createAutoDiscovery();
        discovery->addStateTopic(topic + F("frequency"));
        discovery->addUnitOfMeasurement(F("Hz"));
        discovery->finalize();
        client->publish(discovery->getTopic(), MQTTClient::getDefaultQos(), true, discovery->getPayload());
        delete discovery;
    }
#endif
}

void Driver_DimmerModule::_printStatus(PrintHtmlEntitiesString &out) {
    out.print(F(", Fading enabled" HTML_S(br)));
    for(uint8_t i = 0; i < IOT_DIMMER_MODULE_CHANNELS; i++) {
        out.printf_P(PSTR("Channel %u: "), i);
        if (_channels[i].getOnState()) {
            out.printf_P(PSTR("on - %.1f%%" HTML_S(br)), _channels[i].getLevel() / (float)IOT_DIMMER_MODULE_MAX_BRIGHTNESS * 100.0);
        } else {
            out.print(F("off" HTML_S(br)));
        }
    }
    Dimmer_Base::_printStatus(out);
}


bool Driver_DimmerModule::on(uint8_t channel) {
    return _channels[channel].on();
}

bool Driver_DimmerModule::off(uint8_t channel) {
    return _channels[channel].off();
}

// get brightness values from dimmer
void Driver_DimmerModule::_getChannels() {
    _debug_printf_P(PSTR("Driver_DimmerModule::_getChannels()\n"));

    _wire.beginTransmission(DIMMER_I2C_ADDRESS);
    _wire.write(DIMMER_REGISTER_COMMAND);
    _wire.write(DIMMER_COMMAND_READ_CHANNELS);
    _wire.write(IOT_DIMMER_MODULE_CHANNELS << 4);
    int16_t level;
    const int len = IOT_DIMMER_MODULE_CHANNELS * sizeof(level);
    if (_endTransmission() == 0 && _wire.requestFrom(DIMMER_I2C_ADDRESS, len) == len) {
        for(uint8_t i = 0; i < IOT_DIMMER_MODULE_CHANNELS; i++) {
            _wire.readBytes(reinterpret_cast<uint8_t *>(&level), sizeof(level));
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

int16_t Driver_DimmerModule::getChannel(uint8_t channel) const {
    return _channels[channel].getLevel();
}

bool Driver_DimmerModule::getChannelState(uint8_t channel) const {
    return _channels[channel].getOnState();
}

void Driver_DimmerModule::setChannel(uint8_t channel, int16_t level, float time) {
    if (time == -1) {
        time = _fadeTime;
    }
    _channels[channel].setLevel(level);
    _fade(channel, level, time);
    writeEEPROM();
    _channels[channel].publishState();
}

uint8_t Driver_DimmerModule::getChannelCount() const {
    return IOT_DIMMER_MODULE_CHANNELS;
}

#if AT_MODE_SUPPORTED && !IOT_DIMMER_MODULE_INTERFACE_UART

#include "at_mode.h"

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(DIMG, "DIMG", "Get level");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(DIMS, "DIMS", "<channel>,<level>[,<time>]", "Set level");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(DIMW, "DIMW", "Write EEPROM");

#endif

static DimmerModulePlugin plugin;

PGM_P DimmerModulePlugin::getName() const {
    return PSTR("dimmer");
}

void DimmerModulePlugin::setup(PluginSetupMode_t mode) {
    _begin();
}

void DimmerModulePlugin::reconfigure(PGM_P source) {
    if (source == nullptr) {
        writeConfig();
    }
}

bool DimmerModulePlugin::hasWebUI() const {
    return true;
}

WebUIInterface *DimmerModulePlugin::getWebUIInterface() {
    return this;
}

void DimmerModulePlugin::createWebUI(WebUI &webUI) {

    auto row = &webUI.addRow();
    row->setExtraClass(JJ(title));
    row->addGroup(JF("Dimmer"), true);

    for (uint8_t i = 0; i < getChannelCount(); i++) {
        row = &webUI.addRow();
        row->addSlider(PrintString(F("dimmer_channel%u"), i), PrintString(F("dimmer_channel%u"), i), 0, IOT_DIMMER_MODULE_MAX_BRIGHTNESS, true);
    }

    row = &webUI.addRow();
    row->addBadgeSensor(F("dimmer_vcc"), JF("Dimmer VCC"), JF("V"));
    row->addBadgeSensor(F("dimmer_frequency"), JF("Dimmer Frequency"), JF("Hz"));
    row->addBadgeSensor(F("dimmer_temp"), JF("Dimmer Internal Temperature"), JF("°C"));
}


bool DimmerModulePlugin::hasStatus() const {
    return true;
}

#if AT_MODE_SUPPORTED && !IOT_DIMMER_MODULE_INTERFACE_UART

bool DimmerModulePlugin::hasAtMode() const {
    return true;
}

void DimmerModulePlugin::atModeHelpGenerator() {
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(DIMG));
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(DIMS));
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(DIMW));
}

bool DimmerModulePlugin::atModeHandler(Stream &serial, const String &command, int8_t argc, char **argv) {
    if (constexpr_String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(DIMW))) {
        writeEEPROM();
        at_mode_print_ok(serial);
        return true;
    }
    else if (constexpr_String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(DIMG))) {
        for(uint8_t i = 0; i < IOT_DIMMER_MODULE_CHANNELS; i++) {
            serial.printf_P(PSTR("+DIMG: %u: %d\n"), i, getChannel(i));
        }
        return true;
    }
    else if (constexpr_String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(DIMS))) {
        if (argc < 2) {
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
                setChannel(channel, level, time);
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

#endif
