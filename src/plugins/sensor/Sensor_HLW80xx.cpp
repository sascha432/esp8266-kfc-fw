/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_SENSOR_HAVE_HLW8012 || IOT_SENSOR_HAVE_HLW8032

#include <EventTimer.h>
#include <StringKeyValueStore.h>
#include "Sensor_HLW80xx.h"
#include "sensor.h"
#include "MicrosTimer.h"
#include "Sensor_HLW8012.h"
#include "Sensor_HLW8032.h"
#include "plugins/http2serial/http2serial.h"

#if DEBUG_IOT_SENSOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

PROGMEM_STRING_DEF(iot_sensor_hlw80xx_state_file, "/.pvt/hlw80xx.state");

Sensor_HLW80xx::Sensor_HLW80xx(const String &name) : MQTTSensor(), _name(name)
{
#if DEBUG_MQTT_CLIENT
    debug_printf_P(PSTR("Sensor_HLW80xx(): component=%p\n"), this);
#endif
#if IOT_SENSOR_HLW80xx_ADJUST_CURRENT
    _dimmingLevel = -1;
#endif
    _loadEnergyCounter();
    _power = NAN;
    _voltage = NAN;
    _current = NAN;
    _calibrationI = 1;
    _calibrationP = 1;
    _calibrationU = 1;
    _extraDigits = 0;
    reconfigure(nullptr);

    setUpdateRate(IOT_SENSOR_HLW80xx_UPDATE_RATE);
    setMqttUpdateRate(IOT_SENSOR_HLW80xx_UPDATE_RATE_MQTT);

#if IOT_SENSOR_HLW80xx_DATA_PLOT
    _webSocketClient = nullptr;
    _webSocketPlotData = VOLTAGE;
    _plotDataTime = 0;
#endif
}

MQTTComponent::MQTTAutoDiscoveryPtr Sensor_HLW80xx::nextAutoDiscovery(MQTTAutoDiscovery::FormatType format, uint8_t num)
{
    if (num > 5) {
        return nullptr;
    }
    String topic = _getTopic();
    auto discovery = new MQTTAutoDiscovery();
    switch(num) {
        case 0:
            discovery->create(this, F("power"), format);
            discovery->addStateTopic(topic);
            discovery->addUnitOfMeasurement('W');
            discovery->addValueTemplate(F("power"));
            break;
        case 1:
            discovery->create(this, F("energy_total"), format);
            discovery->addStateTopic(topic);
            discovery->addUnitOfMeasurement(F("kWh"));
            discovery->addValueTemplate(F("energy_total"));
            break;
        case 2:
            discovery->create(this, F("energy"), format);
            discovery->addStateTopic(topic);
            discovery->addUnitOfMeasurement(F("kWh"));
            discovery->addValueTemplate(F("energy"));
            break;
        case 3:
            discovery->create(this, F("voltage"), format);
            discovery->addStateTopic(topic);
            discovery->addUnitOfMeasurement('V');
            discovery->addValueTemplate(F("voltage"));
            break;
        case 4:
            discovery->create(this, F("current"), format);
            discovery->addStateTopic(topic);
            discovery->addUnitOfMeasurement('A');
            discovery->addValueTemplate(F("current"));
            break;
        case 5:
            discovery->create(this, F("pf"), format);
            discovery->addStateTopic(topic);
            discovery->addUnitOfMeasurement(emptyString);
            discovery->addValueTemplate(F("pf"));
            discovery->finalize();
            break;
    }
    discovery->finalize();
    return discovery;
}

uint8_t Sensor_HLW80xx::getAutoDiscoveryCount() const
{
    return 6;
}

void Sensor_HLW80xx::getValues(JsonArray &array, bool timer)
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
    obj->add(JJ(value), JsonNumber(_voltage, 1 + _extraDigits));

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

    (*row)->addSensor(_getId(F("power")), _name + F(" Power"), 'W');
    (*row)->addSensor(_getId(F("energy_total")), _name + F(" Energy Total"), F("kWh"));
    (*row)->addSensor(_getId(F("energy")), _name + F(" Energy"), F("kWh"));
    (*row)->addSensor(_getId(F("voltage")), _name + F(" Voltage"), 'V');
    (*row)->addSensor(_getId(F("current")), _name + F(" Current"), 'A');
    (*row)->addSensor(_getId(F("pf")), _name + F(" Power Factor"), JsonString());
}

void Sensor_HLW80xx::reconfigure(PGM_P source)
{
    auto sensor = config._H_GET(Config().sensor).hlw80xx;
    if (sensor.calibrationU) {
        _calibrationU = sensor.calibrationU;
    }
    if (sensor.calibrationI) {
        _calibrationI = sensor.calibrationI;
    }
    if (sensor.calibrationP) {
        _calibrationP = sensor.calibrationP;
    }
    _extraDigits = sensor.extraDigits;
    _debug_printf_P(PSTR("Sensor_HLW80xx::Sensor_HLW80xx(): calibration U=%f, I=%f, P=%f\n"), _calibrationU, _calibrationI, _calibrationP);
}

void Sensor_HLW80xx::shutdown()
{
    _saveEnergyCounter();

    // update energy counter in EEPROM
    auto &sensor = config._H_W_GET(Config().sensor).hlw80xx;
    sensor.energyCounter = _energyCounter[0];
    config.write();
}

void Sensor_HLW80xx::createConfigureForm(AsyncWebServerRequest *request, Form &form)
{
    auto &sensor = config._H_W_GET(Config().sensor);
    auto kWh = String(F("kWh"));

    form.add<float>(F("hlw80xx_calibrationU"), _H_STRUCT_VALUE(Config().sensor, hlw80xx.calibrationU))->setFormUI(new FormUI(FormUI::TEXT, F("HLW8012 Voltage Calibration")));
    form.add<float>(F("hlw80xx_calibrationI"), _H_STRUCT_VALUE(Config().sensor, hlw80xx.calibrationI))->setFormUI(new FormUI(FormUI::TEXT, F("HLW8012 Current Calibration")));
    form.add<float>(F("hlw80xx_calibrationP"), _H_STRUCT_VALUE(Config().sensor, hlw80xx.calibrationP))->setFormUI(new FormUI(FormUI::TEXT, F("HLW8012 Power Calibration")));
    form.add<uint8_t>(F("hlw80xx_extra_digits"), _H_STRUCT_VALUE(Config().sensor, hlw80xx.extraDigits))->setFormUI(new FormUI(FormUI::TEXT, F("HLW8012 Extra Digits/Precision")));
    form.addValidator(new FormRangeValidator(F("Enter 0 to 4 for extra digits"), 0, 4));

    form.add(F("energyCounterPrimary"), String(), FormField::InputFieldType::TEXT)
        ->setFormUI((new FormUI(FormUI::TEXT, F("HLW8012 Total Energy")))->setSuffix(kWh)->setPlaceholder(String(IOT_SENSOR_HLW80xx_PULSE_TO_KWH(getEnergyPrimaryCounter()), 3)));

    form.addValidator(new FormCallbackValidator([&sensor, this](String value, FormField &field) {
        if (value.length()) {
            // gets copied to getEnergyPrimaryCounter() in configurationSaved()
            sensor.hlw80xx.energyCounter = IOT_SENSOR_HLW80xx_KWH_TO_PULSE(value.toFloat());
        }
        return true;
    }));
    form.add(F("energyCounterSecondary"), String(), FormField::InputFieldType::TEXT)
        ->setFormUI((new FormUI(FormUI::TEXT, F("HLW8012 Energy")))->setSuffix(kWh)->setPlaceholder(String(IOT_SENSOR_HLW80xx_PULSE_TO_KWH(getEnergySecondaryCounter()), 3)));

    form.addValidator(new FormCallbackValidator([this](String value, FormField &field) {
        if (value.length()) {
            getEnergySecondaryCounter() = IOT_SENSOR_HLW80xx_KWH_TO_PULSE(value.toFloat());

        }
        return true;
    }));

}

void Sensor_HLW80xx::configurationSaved(Form *form)
{
    using KeyValueStorage::Container;
    using KeyValueStorage::ContainerPtr;
    using KeyValueStorage::Item;

    auto &sensor = config._H_W_GET(Config().sensor);
    getEnergyPrimaryCounter() = sensor.hlw80xx.energyCounter;

    auto container = ContainerPtr(new Container());
    container->add(Item::create(F("hlw80xx_u"), sensor.hlw80xx.calibrationU));
    container->add(Item::create(F("hlw80xx_i"), sensor.hlw80xx.calibrationI));
    container->add(Item::create(F("hlw80xx_p"), sensor.hlw80xx.calibrationP));
    container->add(Item::create(F("hlw80xx_e1"), getEnergyPrimaryCounter()));
    container->add(Item::create(F("hlw80xx_e2"), getEnergySecondaryCounter()));
    config.callPersistantConfig(container);
}

void Sensor_HLW80xx::publishState(MQTTClient *client)
{
    if (client && client->isConnected()) {
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
        auto _qos = MQTTClient::getDefaultQos();
        client->publish(_getTopic(), _qos, 1, str);
    }
}

void Sensor_HLW80xx::resetEnergyCounter()
{
    _energyCounter = {};
    _saveEnergyCounter();
}

void Sensor_HLW80xx::_incrEnergyCounters(uint32_t count)
{
    for(uint8_t i = 0; i < _energyCounter.size(); i++) {
        _energyCounter[i] += count;
    }
}

void Sensor_HLW80xx::_saveEnergyCounter()
{
    _debug_printf_P(PSTR("Sensor_HLW80xx::_saveEnergyCounter()\n"));
#if IOT_SENSOR_HLW80xx_SAVE_ENERGY_CNT
    auto file = SPIFFS.open(FSPGM(iot_sensor_hlw80xx_state_file), fs::FileOpenMode::write);
    if (file) {
        file.write(reinterpret_cast<const uint8_t *>(_energyCounter.data()), sizeof(_energyCounter));
        file.close();
    }
    _saveEnergyCounterTimeout = millis() + IOT_SENSOR_HLW80xx_SAVE_ENERGY_CNT;

    using KeyValueStorage::Container;
    using KeyValueStorage::ContainerPtr;
    using KeyValueStorage::Item;

    config.callPersistantConfig(ContainerPtr(new Container()), [this](Container &data) {
        data.replace(Item::create(F("hlw80xx_e1_a"), getEnergyPrimaryCounter()));
        data.replace(Item::create(F("hlw80xx_e2_a"), getEnergySecondaryCounter()));
    });
#else
    _saveEnergyCounterTimeout = ~0;
#endif
}

void Sensor_HLW80xx::_loadEnergyCounter()
{
    _debug_printf_P(PSTR("Sensor_HLW80xx::_loadEnergyCounter()\n"));
#if IOT_SENSOR_HLW80xx_SAVE_ENERGY_CNT
    auto file = SPIFFS.open(FSPGM(iot_sensor_hlw80xx_state_file), fs::FileOpenMode::read);
    if (!file || file.read(reinterpret_cast<uint8_t *>(_energyCounter.data()), sizeof(_energyCounter)) != sizeof(_energyCounter)) {
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
    uint8_t digits = 2;
    if (current < 1) {
        digits = 3;
    }
    return JsonNumber(current, digits + _extraDigits);
}

JsonNumber Sensor_HLW80xx::_energyToNumber(float energy) const
{
    auto tmp = energy;
    uint8_t digits = 0;
    while(tmp >= 1 && digits < 3) {
        digits++;
        tmp *= 0.1;
    }
    return JsonNumber(energy, 3 - digits + _extraDigits);
}

JsonNumber Sensor_HLW80xx::_powerToNumber(float power) const
{
    uint8_t digits = 1;
    if (power < 10) {
        digits = 2;
    }
    return JsonNumber(power, digits + _extraDigits);
}

float Sensor_HLW80xx::_getPowerFactor() const
{
    if (isnan(_power) || isnan(_voltage) || isnan(_current) || _current == 0) {
        return 0;
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
    return MQTTClient::formatTopic(_getId());
}

void Sensor_HLW80xx::dump(Print &output)
{
    output.printf_P(PSTR("U=% 9.4f I=% 8.4f P=% 9.4f U*I=% 9.4f E=% 9.4f (count=%012.0f), pf=%03.2f\n"),
        _voltage,
        _current,
        _power,
        _voltage * _current,
        _getEnergy(0),
        (double)_energyCounter[0],
        _getPowerFactor()
    );
}

#if IOT_SENSOR_HLW80xx_DATA_PLOT

AsyncWebSocketClient *Sensor_HLW80xx::_getWebSocketClient() const
{
    // if we have a pointer, we need to verify it still exists
    if (_webSocketClient) {
        auto wsSerialConsole = Http2Serial::getConsoleServer();
        if (wsSerialConsole) {
            for(auto client: wsSerialConsole->getClients()) {
                if (_webSocketClient == client && client->status() && client->_tempObject && reinterpret_cast<WsClient *>(client->_tempObject)->isAuthenticated()) {
                     return _webSocketClient;
                }
            }
        }
    }
    return nullptr;
}

#endif

#if AT_MODE_SUPPORTED

#include "at_mode.h"

#undef PROGMEM_AT_MODE_HELP_COMMAND_PREFIX
#define PROGMEM_AT_MODE_HELP_COMMAND_PREFIX         "SP_"

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(HLWXD, "HLWXD", "<count/0-4>", "Display extra digits");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(HLWDUMP, "HLWDUMP", "<0=off/1...=seconds/2=cycle>", "Dump sensor data");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(HLWPLOT, "HLWPLOT", "<ClientID>,<U/I/P/0=disable>[,<1/true=convert units>]", "Request data for plotting live graph");

void Sensor_HLW80xx::atModeHelpGenerator()
{
    auto name = SensorPlugin::getInstance().getName();
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(HLWXD), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(HLWDUMP), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(HLWPLOT), name);
}

bool Sensor_HLW80xx::atModeHandler(AtModeArgs &args)
{
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(HLWXD))) {
        if (args.requireArgs(1)) {
            uint8_t digits = args.toIntMinMax(AtModeArgs::FIRST, 0, 4);
            auto count = SensorPlugin::for_each<Sensor_HLW80xx>(this, Sensor_HLW80xx::_compareFunc, [digits](Sensor_HLW80xx &sensor) {
                sensor.setExtraDigits(digits);
            });
            args.printf_P(PSTR("Set extra digits to %d for %u sensors"), digits, count);
        }
        return true;
    }
#if IOT_SENSOR_HLW80xx_DATA_PLOT
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(HLWPLOT))) {
        if (args.requireArgs(2, 3)) {
            void *clientId = reinterpret_cast<void *>(args.toNumber(0));
            auto ch = args.toLowerChar(1);
            if (ch == 'u') {
                _webSocketPlotData = WebSocketDataTypeEnum_t::VOLTAGE;
            }
            else if (ch == 'i') {
                _webSocketPlotData = WebSocketDataTypeEnum_t::CURRENT;
            }
            else if (ch == 'p') {
                _webSocketPlotData = WebSocketDataTypeEnum_t::POWER;
            }
            else {
                args.print(F("Disabling plot data"));
                return true;
            }
            if (args.isTrue(2)) {
                _webSocketPlotData = (WebSocketDataTypeEnum_t)(_webSocketPlotData | WebSocketDataTypeEnum_t::CONVERT_UNIT);
            }
            _plotDataTime = 0;
            _plotData.clear();

            _webSocketClient = nullptr;
            auto wsSerialConsole = Http2Serial::getConsoleServer();
            if (wsSerialConsole) {
                for(auto client: wsSerialConsole->getClients()) {
                    if (reinterpret_cast<void *>(client) == clientId && client->status() && client->_tempObject && reinterpret_cast<WsClient *>(client->_tempObject)->isAuthenticated()) {
                        _webSocketClient = client;
                        break;
                    }
                }
            }

            if (!_webSocketClient) {
                args.printf_P(PSTR("Cannot find ClientID %p"), clientId);
            }
            else {
                args.printf_P(PSTR("{\"Imin\":%f,\"Imax\":%f,\"Ipmin\":%u,\"Ipmax\":%u,\"Rs\":%f,\"UIPc\":[%f,%f,%f]}"),
                    IOT_SENSOR_HLW80xx_MIN_CURRENT, IOT_SENSOR_HLW80xx_MAX_CURRENT,
                    IOT_SENSOR_HLW80xx_CURRENT_MIN_PULSE, IOT_SENSOR_HLW80xx_CURRENT_MAX_PULSE,
                    IOT_SENSOR_HLW80xx_SHUNT, _calibrationU, _calibrationI, _calibrationP);
            }
        }
        return true;
    }
#endif
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(HLWDUMP))) {
        _dumpTimer.remove();

        auto interval = args.toMillis(0, 500, ~0, 0, String('s'));
        if (interval) {
            auto &serial = args.getStream();
            _dumpTimer.add(interval, true, [this, &serial](EventScheduler::TimerPtr) {
                SensorPlugin::for_each<Sensor_HLW80xx>(this, Sensor_HLW80xx::_compareFunc, [&serial](Sensor_HLW80xx &sensor) {
                    sensor.dump(serial);
                });
            });
        }
        return true;
    }
    return false;
}

#endif

#endif
