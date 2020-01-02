/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_SENSOR && (IOT_SENSOR_HAVE_HLW8012 || IOT_SENSOR_HAVE_HLW8032)

#include "Sensor_HLW80xx.h"
#include "sensor.h"
#include "MicrosTimer.h"

#if DEBUG_IOT_SENSOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

PROGMEM_STRING_DEF(iot_sensor_hlw80xx_state_file, "/hlw80xx.state");

Sensor_HLW80xx::Sensor_HLW80xx(const String &name) : MQTTSensor(), _name(name)
{
#if DEBUG_MQTT_CLIENT
    debug_printf_P(PSTR("Sensor_HLW80xx(): component=%p\n"), this);
#endif
    _loadEnergyCounter();
    _power = NAN;
    _voltage = NAN;
    _current = NAN;
    _calibrationI = 1;
    _calibrationP = 1;
    _calibrationU = 1;
    reconfigure();

    setUpdateRate(IOT_SENSOR_HLW80xx_UPDATE_RATE);
    _nextMQTTUpdate = 0;
}

void Sensor_HLW80xx::createAutoDiscovery(MQTTAutoDiscovery::Format_t format, MQTTAutoDiscoveryVector &vector)
{
    String topic = _getTopic();
    auto discovery = _debug_new MQTTAutoDiscovery();
    discovery->create(this, 0, format);
    discovery->addStateTopic(topic);
    discovery->addUnitOfMeasurement(F("W"));
    discovery->addValueTemplate(F("power"));
    discovery->finalize();
    vector.emplace_back(MQTTAutoDiscoveryPtr(discovery));

    discovery = _debug_new MQTTAutoDiscovery();
    discovery->create(this, 1, format);
    discovery->addStateTopic(topic);
    discovery->addUnitOfMeasurement(F("kWh"));
    discovery->addValueTemplate(F("energy_total"));
    discovery->finalize();
    vector.emplace_back(MQTTAutoDiscoveryPtr(discovery));

    discovery = _debug_new MQTTAutoDiscovery();
    discovery->create(this, 2, format);
    discovery->addStateTopic(topic);
    discovery->addUnitOfMeasurement(F("kWh"));
    discovery->addValueTemplate(F("energy"));
    discovery->finalize();
    vector.emplace_back(MQTTAutoDiscoveryPtr(discovery));

    discovery = _debug_new MQTTAutoDiscovery();
    discovery->create(this, 3, format);
    discovery->addStateTopic(topic);
    discovery->addUnitOfMeasurement(F("V"));
    discovery->addValueTemplate(F("voltage"));
    discovery->finalize();
    vector.emplace_back(MQTTAutoDiscoveryPtr(discovery));

    discovery = _debug_new MQTTAutoDiscovery();
    discovery->create(this, 4, format);
    discovery->addStateTopic(topic);
    discovery->addUnitOfMeasurement(F("A"));
    discovery->addValueTemplate(F("current"));
    discovery->finalize();
    vector.emplace_back(MQTTAutoDiscoveryPtr(discovery));

    discovery = _debug_new MQTTAutoDiscovery();
    discovery->create(this, 5, format);
    discovery->addStateTopic(topic);
    discovery->addUnitOfMeasurement(F(""));
    discovery->addValueTemplate(F("pf"));
    discovery->finalize();
    vector.emplace_back(MQTTAutoDiscoveryPtr(discovery));
}

uint8_t Sensor_HLW80xx::getAutoDiscoveryCount() const
{
    return 6;
}

void Sensor_HLW80xx::getValues(JsonArray &array)
{
    _debug_printf_P(PSTR("Sensor_HLW8012::getValues()\n"));

    auto obj = &array.addObject(3);
    obj->add(JJ(id), _getId(F("power")));
    obj->add(JJ(state), !isnan(_power));
    obj->add(JJ(value), _powerToNumber(_power));

    obj = &array.addObject(3);
    obj->add(JJ(id), _getId(F("energy_total")));
    auto energy = _getEnergy(0);
    obj->add(JJ(state), !isnan(energy));
    obj->add(JJ(value), _energyToNumber(energy));

    obj = &array.addObject(3);
    obj->add(JJ(id), _getId(F("energy")));
    energy = _getEnergy(1);
    obj->add(JJ(state), !isnan(energy));
    obj->add(JJ(value), _energyToNumber(energy));

    obj = &array.addObject(3);
    obj->add(JJ(id), _getId(F("voltage")));
    obj->add(JJ(state), !isnan(_voltage));
    obj->add(JJ(value), JsonNumber(_voltage, 1));

    obj = &array.addObject(3);
    obj->add(JJ(id), _getId(F("current")));
    obj->add(JJ(state), !isnan(_current));
    obj->add(JJ(value), _currentToNumber(_current));

    obj = &array.addObject(3);
    obj->add(JJ(id), _getId(F("pf")));
    auto pf = _getPowerFactor();
    obj->add(JJ(state), !isnan(pf));
    obj->add(JJ(value), String(pf, 2));
}

void Sensor_HLW80xx::createWebUI(WebUI &webUI, WebUIRow **row)
{
    _debug_printf_P(PSTR("Sensor_HLW8012::createWebUI()\n"));

    // if ((*row)->size() > 1) {
    //     *row = &webUI.addRow();
    // }

    (*row)->addSensor(_getId(F("power")), _name + F(" Power"), F("W"));
    (*row)->addSensor(_getId(F("energy_total")), _name + F(" Energy Total"), F("kWh"));
    (*row)->addSensor(_getId(F("energy")), _name + F(" Energy"), F("kWh"));
    (*row)->addSensor(_getId(F("voltage")), _name + F(" Voltage"), F("V"));
    (*row)->addSensor(_getId(F("current")), _name + F(" Current"), F("A"));
    (*row)->addSensor(_getId(F("pf")), _name + F(" Power Factor"), F(""));
}

void Sensor_HLW80xx::reconfigure()
{
    auto sensor = config._H_GET(Config().sensor).hlw80xx;
    if (sensor.calibrationI) {
        _calibrationI = sensor.calibrationI;
    }
    if (sensor.calibrationU) {
        _calibrationU = sensor.calibrationU;
    }
    if (sensor.calibrationP) {
        _calibrationP = sensor.calibrationP;
    }
    _debug_printf_P(PSTR("Sensor_HLW80xx::Sensor_HLW80xx(): calibration U=%f, I=%f, P=%f\n"), _calibrationU, _calibrationI, _calibrationP);
}

void Sensor_HLW80xx::restart()
{
    _saveEnergyCounter();

    // discard changes
    config.discard();
    // update energy counter in EEPROM
    auto &sensor = config._H_W_GET(Config().sensor).hlw80xx;
    sensor.energyCounter = _energyCounter[0];
    config.write();
}

void Sensor_HLW80xx::createConfigureForm(AsyncWebServerRequest *request, Form &form)
{
    auto *sensor = &config._H_W_GET(Config().sensor); // must be a pointer

    form.add<float>(F("hlw80xx_calibrationU"), &sensor->hlw80xx.calibrationU)->setFormUI(new FormUI(FormUI::TEXT, F("HLW8012 Voltage Calibration")));
    form.add<float>(F("hlw80xx_calibrationI"), &sensor->hlw80xx.calibrationI)->setFormUI(new FormUI(FormUI::TEXT, F("HLW8012 Current Calibration")));
    form.add<float>(F("hlw80xx_calibrationP"), &sensor->hlw80xx.calibrationP)->setFormUI(new FormUI(FormUI::TEXT, F("HLW8012 Power Calibration")));
}

void Sensor_HLW80xx::publishState(MQTTClient *client)
{
    auto currentTime = time(nullptr);
    if (currentTime > _nextMQTTUpdate) {
        _nextMQTTUpdate = currentTime + IOT_SENSOR_HLW80xx_UPDATE_RATE_MQTT;
    }
    else {
        return; // we skip this round
    }

    if (client) {
        PrintString str;
        JsonUnnamedObject json;
        json.add(F("power"), _powerToNumber(_power));
        json.add(F("energy_total"), _energyToNumber(_getEnergy(0)));
        json.add(F("energy"), _energyToNumber(_getEnergy(1)));
        json.add(F("voltage"), JsonNumber(_voltage, 1));
        json.add(F("current"), _currentToNumber(_current));
        auto pf = _getPowerFactor();
        json.add(F("pf"), String(pf, 2));
        json.printTo(str);
        client->publish(_getTopic(), _qos, 1, str);
    }
}

void Sensor_HLW80xx::resetEnergyCounter()
{
    memset(&_energyCounter, 0, sizeof(_energyCounter));
    _saveEnergyCounter();
}

void Sensor_HLW80xx::_incrEnergyCounters(uint32_t count)
{
    for(uint8_t i = 0; i < IOT_SENSOR_HLW80xx_NUM_ENERGY_COUNTERS; i++) {
        _energyCounter[i] += count;
    }
}

void Sensor_HLW80xx::_saveEnergyCounter()
{
    _debug_printf_P(PSTR("Sensor_HLW80xx::_saveEnergyCounter()\n"));
#if IOT_SENSOR_HLW80xx_SAVE_ENERGY_CNT
    auto file = SPIFFS.open(FSPGM(iot_sensor_hlw80xx_state_file), fs::FileOpenMode::write);
    if (file) {
        file.write(reinterpret_cast<uint8_t *>(&_energyCounter), sizeof(_energyCounter));
        file.close();
    }
    _saveEnergyCounterTimeout = millis() + IOT_SENSOR_HLW80xx_SAVE_ENERGY_CNT;
#else
    _saveEnergyCounterTimeout = ~0;
#endif
}

void Sensor_HLW80xx::_loadEnergyCounter()
{
    _debug_printf_P(PSTR("Sensor_HLW80xx::_loadEnergyCounter()\n"));
#if IOT_SENSOR_HLW80xx_SAVE_ENERGY_CNT
    auto file = SPIFFS.open(FSPGM(iot_sensor_hlw80xx_state_file), fs::FileOpenMode::read);
    if (!file || file.read(reinterpret_cast<uint8_t *>(&_energyCounter), sizeof(_energyCounter)) != sizeof(_energyCounter)) {
        resetEnergyCounter();
        _energyCounter[0] = config._H_GET(Config().sensor).hlw80xx.energyCounter; // restore data from EEPROM in case SPIFFS was updated
    }
    _saveEnergyCounterTimeout = millis() + IOT_SENSOR_HLW80xx_SAVE_ENERGY_CNT;
#else
    resetEnergyCounter();
    _saveEnergyCounterTimeout = ~0;
#endif
}

JsonNumber Sensor_HLW80xx::_currentToNumber(float current) const
{
    if (current < 0.2) {
        return JsonNumber(current, 3);
    }
    return JsonNumber(current, 2);
}

JsonNumber Sensor_HLW80xx::_energyToNumber(float energy) const
{
    auto tmp = energy;
    uint8_t digits = 0;
    while(tmp >= 1 && digits < 3) {
        digits++;
        tmp *= 0.1;
    }
    return JsonNumber(energy, 3 - digits);
}

JsonNumber Sensor_HLW80xx::_powerToNumber(float power) const
{
    if (power < 10) {
        return JsonNumber(power, 2);
    }
    return JsonNumber(power, 1);
}

float Sensor_HLW80xx::_getPowerFactor() const
{
    if (isnan(_power) || isnan(_voltage) || isnan(_current)) {
        return NAN;
    }
    return std::min(_power / (_voltage * _current), 1.0f);
}

float Sensor_HLW80xx::_getEnergy(uint8_t num) const
{
    if (num >= IOT_SENSOR_HLW80xx_NUM_ENERGY_COUNTERS) {
        return NAN;
    }
    return IOT_SENSOR_HLW80xx_PULSE_TO_KWH(_energyCounter[num]);
}

String Sensor_HLW80xx::_getTopic()
{
    return MQTTClient::formatTopic(-1, F("/%s/"), _getId().c_str());
}

#endif
