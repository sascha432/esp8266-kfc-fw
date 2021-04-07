/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_SENSOR_HAVE_HLW8012 || IOT_SENSOR_HAVE_HLW8032

#include <EventScheduler.h>
#include "Sensor_HLW80xx.h"
#include "sensor.h"
#include "MicrosTimer.h"
#include "Sensor_HLW8012.h"
#include "Sensor_HLW8032.h"
#if HTTP2SERIAL_SUPPORT
#include "plugins/http2serial/http2serial.h"
#endif

#if DEBUG_IOT_SENSOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#include <debug_helper_enable_mem.h>

using KFCConfigurationClasses::Plugins;

PROGMEM_STRING_DEF(iot_sensor_hlw80xx_state_file, "/.pvt/hlw80xx.state");

Sensor_HLW80xx::Sensor_HLW80xx(const String &name, MQTT::SensorType type) : MQTT::Sensor(type), _name(name), _power(NAN), _voltage(NAN), _current(NAN)
{
#if DEBUG_MQTT_CLIENT
    debug_printf_P(PSTR("Sensor_HLW80xx(): component=%p\n"), this);
#endif
#if IOT_SENSOR_HLW80xx_ADJUST_CURRENT
    _dimmingLevel = -1;
#endif
    reconfigure(nullptr); // read config
    _loadEnergyCounter();
    // _power = NAN;
    // _voltage = NAN;
    // _current = NAN;

    setUpdateRate(IOT_SENSOR_HLW80xx_UPDATE_RATE);
    setMqttUpdateRate(IOT_SENSOR_HLW80xx_UPDATE_RATE_MQTT);

#if IOT_SENSOR_HLW80xx_DATA_PLOT
    _webSocketClient = nullptr;
    _webSocketPlotData = VOLTAGE;
    _plotDataTime = 0;
#endif
}

MQTT::AutoDiscovery::EntityPtr Sensor_HLW80xx::getAutoDiscovery(MQTT::FormatType format, uint8_t num)
{
    String topic = _getTopic();
    auto discovery = __LDBG_new(MQTT::AutoDiscovery::Entity);
    switch(num) {
        case 0:
            discovery->create(this, FSPGM(power), format);
            discovery->addStateTopic(topic);
            discovery->addUnitOfMeasurement('W');
            discovery->addValueTemplate(FSPGM(power));
            discovery->addDeviceClass(F("power"));
            break;
        case 1:
            discovery->create(this, FSPGM(energy_total), format);
            discovery->addStateTopic(topic);
            discovery->addUnitOfMeasurement(FSPGM(kWh));
            discovery->addValueTemplate(FSPGM(energy_total));
            discovery->addDeviceClass(F("energy"));
            break;
        case 2:
            discovery->create(this, FSPGM(energy), format);
            discovery->addStateTopic(topic);
            discovery->addUnitOfMeasurement(FSPGM(kWh));
            discovery->addValueTemplate(FSPGM(energy));
            discovery->addDeviceClass(F("energy"));
            break;
        case 3:
            discovery->create(this, FSPGM(voltage), format);
            discovery->addStateTopic(topic);
            discovery->addUnitOfMeasurement('V');
            discovery->addValueTemplate(FSPGM(voltage));
            discovery->addDeviceClass(F("voltage"));
            break;
        case 4:
            discovery->create(this, FSPGM(current), format);
            discovery->addStateTopic(topic);
            discovery->addUnitOfMeasurement('A');
            discovery->addValueTemplate(FSPGM(current));
            discovery->addDeviceClass(F("current"));
            break;
        case 5:
            discovery->create(this, FSPGM(pf), format);
            discovery->addStateTopic(topic);
            discovery->addUnitOfMeasurement('%');
            discovery->addValueTemplate(FSPGM(pf));
            discovery->addDeviceClass(F("power_factor"));
            break;
    }
    return discovery;
}

uint8_t Sensor_HLW80xx::getAutoDiscoveryCount() const
{
    return 6;
}

void Sensor_HLW80xx::getValues(WebUINS::Events &array, bool timer)
{
    array.append(
        Values(_getId(FSPGM(power)), _powerToNumber(_power)),
    );
    auto obj = &array.addObject(3);
    obj->add(JJ(id), );
    obj->add(JJ(state), !isnan(_power));
    obj->add(JJ(value), );

    obj = &array.addObject(3);
    obj->add(JJ(id), _getId(FSPGM(energy_total)));
    auto energy = _getEnergy(0);
    obj->add(JJ(state), !isnan(energy));
    obj->add(JJ(value), _energyToNumber(energy));

    obj = &array.addObject(3);
    obj->add(JJ(id), _getId(FSPGM(energy)));
    energy = _getEnergy(1);
    obj->add(JJ(state), !isnan(energy));
    obj->add(JJ(value), _energyToNumber(energy));

    obj = &array.addObject(3);
    obj->add(JJ(id), _getId(FSPGM(voltage)));
    obj->add(JJ(state), !isnan(_voltage));
    obj->add(JJ(value), JsonNumber(_voltage, 1 + _extraDigits));

    obj = &array.addObject(3);
    obj->add(JJ(id), _getId(FSPGM(current)));
    obj->add(JJ(state), !isnan(_current));
    obj->add(JJ(value), _currentToNumber(_current));

    obj = &array.addObject(3);
    obj->add(JJ(id), _getId(FSPGM(pf)));
    auto pf = _getPowerFactor();
    obj->add(JJ(state), !isnan(pf));
    obj->add(JJ(value), String(pf, 2));
}

void Sensor_HLW80xx::createWebUI(WebUINS::Root &webUI)
{
    __LDBG_println();

    // if ((*row)->size() > 1) {
    //     *row = &webUI.addRow();
    // }

    (*row)->addSensor(_getId(FSPGM(power)), _name + F(" Power"), 'W');
    (*row)->addSensor(_getId(FSPGM(energy_total)), _name + F(" Energy Total"), FSPGM(kWh));
    (*row)->addSensor(_getId(FSPGM(energy)), _name + F(" Energy"), FSPGM(kWh));
    (*row)->addSensor(_getId(FSPGM(voltage)), _name + F(" Voltage"), 'V');
    (*row)->addSensor(_getId(FSPGM(current)), _name + F(" Current"), 'A');
    (*row)->addSensor(_getId(FSPGM(pf)), _name + F(" Power Factor"), JsonString());
}

void Sensor_HLW80xx::reconfigure(PGM_P source)
{
    auto sensor = Plugins::Sensor::getConfig();
    _calibrationU = sensor.calibrationU;
    _calibrationI = sensor.calibrationI;
    _calibrationP = sensor.calibrationP;
    _extraDigits = sensor.extraDigits;
    _energyCounter[0] = sensor.energyCounter;
    __LDBG_printf("calibration U=%f I=%f P=%f x_digits=%u energy=%.0f", _calibrationU, _calibrationI, _calibrationP, _extraDigits, (double)_energyCounter[0]);
}

void Sensor_HLW80xx::shutdown()
{
    _saveEnergyCounter();

    // update energy counter in EEPROM
    auto &cfg = Plugins::Sensor::getWriteableConfig();
    cfg.energyCounter = _energyCounter[0];
    config.write();
}

void Sensor_HLW80xx::createConfigureForm(AsyncWebServerRequest *request, FormUI::Form::BaseForm &form)
{
    auto &cfg = Plugins::Sensor::getWriteableConfig();

    auto &group = form.addCardGroup(F("hlw80xx_cf"), F("HLW8012"), true);

    form.addPointerTriviallyCopyable(F("hlw80xx_u"), &cfg.calibrationU);
    form.addFormUI(F("Voltage Calibration"));

    form.addPointerTriviallyCopyable(F("hlw80xx_i"), &cfg.calibrationI);
    form.addFormUI(F("Current Calibration"));

    form.addPointerTriviallyCopyable(F("hlw80xx_p"), &cfg.calibrationP);
    form.addFormUI(F("Power Calibration"));

    form.addObjectGetterSetter(F("hlw80xx_xd"), cfg, cfg.get_bits_extraDigits, cfg.set_bits_extraDigits);
    form.addFormUI(F("Extra Digits/Precision"));
    cfg.addRangeValidatorFor_extraDigits(form);

    form.add(F("hlw80xx_e1"), String(), FormUI::Field::Type::TEXT);
    form.addFormUI(F("Total Energy"), FormUI::Suffix(FSPGM(kWh)), FormUI::PlaceHolder(IOT_SENSOR_HLW80xx_PULSE_TO_KWH(getEnergyPrimaryCounter()), 3));

    // form.addValidator(FormUI::Validator::Callback([&cfg, this](String value, FormField &field) {
    //     if (value.length()) {
    //         // gets copied to getEnergyPrimaryCounter() in configurationSaved()
    //         cfg.hlw80xx.energyCounter = IOT_SENSOR_HLW80xx_KWH_TO_PULSE(value.toFloat());
    //     }
    //     return true;
    // }));
    form.add(F("hlw80xx_2"), String(), FormUI::Field::Type::TEXT);
    form.addFormUI(F("Energy"), FormUI::Label(FSPGM(kWh)), FormUI::PlaceHolder(IOT_SENSOR_HLW80xx_PULSE_TO_KWH(getEnergySecondaryCounter()), 3));

    // form.addValidator(FormUI::Validator::Callback([this](String value, FormField &field) {
    //     if (value.length()) {
    //         getEnergySecondaryCounter() = IOT_SENSOR_HLW80xx_KWH_TO_PULSE(value.toFloat());
    //     }
    //     return true;
    // }));

    group.end();

}

void Sensor_HLW80xx::configurationSaved(FormUI::Form::BaseForm *form)
{
    auto &cfg = Plugins::Sensor::getWriteableConfig();
    getEnergyPrimaryCounter() = cfg.energyCounter;
}

void Sensor_HLW80xx::publishState(MQTTClient *client)
{
    if (client && client->isConnected()) {
        PrintString str;
        JsonUnnamedObject json;
        json.add(FSPGM(power), _powerToNumber(_power));
        json.add(FSPGM(energy_total), _energyToNumber(_getEnergy(0)));
        json.add(FSPGM(energy), _energyToNumber(_getEnergy(1)));
        json.add(FSPGM(voltage), JsonNumber(_voltage, 1 + _extraDigits));
        json.add(FSPGM(current), _currentToNumber(_current));
        auto pf = _getPowerFactor();
        json.add(FSPGM(pf), String(pf, 2));
        json.printTo(str);
        client->publish(_getTopic(), true, str);
    }
}

void Sensor_HLW80xx::_saveEnergyCounter()
{
#if IOT_SENSOR_HLW80xx_SAVE_ENERGY_CNT
    __LDBG_printf("file=%s", FSPGM(iot_sensor_hlw80xx_state_file));
    auto file = KFCFS.open(FSPGM(iot_sensor_hlw80xx_state_file), fs::FileOpenMode::write);
    if (file) {
        file.write(reinterpret_cast<const uint8_t *>(_energyCounter.data()), sizeof(_energyCounter));
        file.close();
    }
    _saveEnergyCounterTimeout = millis() + IOT_SENSOR_HLW80xx_SAVE_ENERGY_CNT;
#else
    _saveEnergyCounterTimeout = ~0;
#endif
}

void Sensor_HLW80xx::_loadEnergyCounter()
{
#if IOT_SENSOR_HLW80xx_SAVE_ENERGY_CNT
    __LDBG_printf("file=%s", FSPGM(iot_sensor_hlw80xx_state_file));
    auto file = KFCFS.open(FSPGM(iot_sensor_hlw80xx_state_file), fs::FileOpenMode::read);
    if (!file || file.read(reinterpret_cast<uint8_t *>(_energyCounter.data()), sizeof(_energyCounter)) != sizeof(_energyCounter)) {
        resetEnergyCounter();
        _energyCounter[0] = Plugins::Sensor::getConfig().energyCounter; // restore data from EEPROM in case SPIFFS was updated
    }
    _saveEnergyCounterTimeout = millis() + IOT_SENSOR_HLW80xx_SAVE_ENERGY_CNT;
#else
    resetEnergyCounter();
    _saveEnergyCounterTimeout = ~0;
#endif
}

void Sensor_HLW80xx::setExtraDigits(uint8_t digits)
{
    Plugins::Sensor::getWriteableConfig().extraDigits = std::min((uint8_t)6, digits);
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

#if AT_MODE_SUPPORTED

#include "at_mode.h"

#undef PROGMEM_AT_MODE_HELP_COMMAND_PREFIX
#define PROGMEM_AT_MODE_HELP_COMMAND_PREFIX         "SP_"

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(HLWXD, "HLWXD", "<count/0-4>", "Display extra digits");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(HLWDUMP, "HLWDUMP", "<0=off/1...=seconds/2=cycle>", "Dump sensor data");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(HLWPLOT, "HLWPLOT", "<ClientID>,<U/I/P/0=disable>[,<1/true=convert units>]", "Request data for plotting live graph");

void Sensor_HLW80xx::atModeHelpGenerator()
{
    auto name = SensorPlugin::getInstance().getName_P();
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(HLWXD), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(HLWDUMP), name);
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(HLWPLOT), name);
}

bool Sensor_HLW80xx::atModeHandler(AtModeArgs &args)
{
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(HLWXD))) {
        if (args.requireArgs(1)) {
            uint8_t digits = args.toIntMinMax(AtModeArgs::FIRST, 0, 4);

            auto count = std::count_if(SensorPlugin::begin(), SensorPlugin::end(), [digits](MQTTSensor *sensor) {
                if (sensor->getType() == MQTT::SensorType::HLW8012 || sensor->getType() == MQTT::SensorType::HLW8032) {
                    reinterpret_cast<Sensor_HLW80xx *>(sensor)->setExtraDigits(digits);
                    return true;
                }
                return false;
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

            _webSocketClient = Http2Serial::getClientById(clientId);
            // auto wsSerialConsole = Http2Serial::getServerSocket();
            // if (wsSerialConsole) {
            //     for(auto client: wsSerialConsole->getClients()) {
            //         if (reinterpret_cast<void *>(client) == clientId && client->status() && client->_tempObject && reinterpret_cast<WsClient *>(client->_tempObject)->isAuthenticated()) {
            //             _webSocketClient = client;
            //             break;
            //         }
            //     }
            // }

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
            _Timer(_dumpTimer).add(interval, true, [this, &serial](Event::CallbackTimerPtr timer) {
                for(auto sensor: SensorPlugin::getSensors()) {
                    if (sensor->getType() == MQTT::SensorType::HLW8012 || sensor->getType() == MQTT::SensorType::HLW8032) {
                        reinterpret_cast<Sensor_HLW80xx *>(sensor)->dump(serial);
                    }
                }
            });
        }
        return true;
    }
    return false;
}

#endif

#endif
