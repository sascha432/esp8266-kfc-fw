/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_SENSOR_HAVE_INA219

#include <Arduino_compat.h>
#include "Sensor_INA219.h"
#include "sensor.h"
#include <EventScheduler.h>
#include <MicrosTimer.h>
#include "WebUIComponent.h"

#if DEBUG_IOT_SENSOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

Sensor_INA219::Sensor_INA219(const String &name, uint8_t address, TwoWire &wire) :
    MQTT::Sensor(MQTT::SensorType::INA219),
    _name(name),
    _address(address),
    _config(_readConfig()),
    _updateTimer(0),
    _holdPeakTimer(0),
    _data(500),
    _avgData(_config.averaging_period * 1000),
    _mqttData(_mqttUpdateRate * 1000 / 2),
    _Ipeak(NAN),
    _Ppeak(NAN),
    _ina219(address)
{
    REGISTER_SENSOR_CLIENT(this);
    _ina219.begin(&wire);
    _ina219.setCalibration(IOT_SENSOR_INA219_BUS_URANGE, IOT_SENSOR_INA219_GAIN, IOT_SENSOR_INA219_SHUNT_ADC_RES, IOT_SENSOR_INA219_R_SHUNT * _config.calibration, _config.offset);

    __LDBG_printf("address=%x voltage_range=%x gain=%x shunt_ADC_res=%x", _address, IOT_SENSOR_INA219_BUS_URANGE, IOT_SENSOR_INA219_GAIN, IOT_SENSOR_INA219_SHUNT_ADC_RES);

    setUpdateRate(_config.webui_update_rate);
    LoopFunctions::add([this]() {
        this->_loop();
    }, this);
}

Sensor_INA219::~Sensor_INA219()
{
    LoopFunctions::remove(this);
    UNREGISTER_SENSOR_CLIENT(this);
}

MQTT::AutoDiscovery::EntityPtr Sensor_INA219::getAutoDiscovery(FormatType format, uint8_t num)
{
    auto discovery = new MQTT::AutoDiscovery::Entity();
    __DBG_discovery_printf("num=%u/%u d=%p", num, getAutoDiscoveryCount(), discovery);
    auto baseTopic = MQTT::Client::getBaseTopicPrefix();
    switch(num) {
        case 0:
            if (discovery->create(this, _getId(SensorInputType::VOLTAGE), format)) {
                discovery->addStateTopic(MQTT::Client::formatTopic(_getId(SensorInputType::VOLTAGE)));
                discovery->addDeviceClass(F("voltage"),'V');
                discovery->addName(F("Voltage"));
                discovery->addObjectId(baseTopic + F("voltage"));
            }
            break;
        case 1:
            if (discovery->create(this, _getId(SensorInputType::CURRENT), format)) {
                discovery->addStateTopic(MQTT::Client::formatTopic(_getId(SensorInputType::CURRENT)));
                discovery->addDeviceClass(F("current"), _getCurrentUnit());
                discovery->addName(F("Current"));
                discovery->addObjectId(baseTopic + F("current"));
            }
            break;
        case 2:
            if (discovery->create(this, _getId(SensorInputType::POWER), format)) {
                discovery->addStateTopic(MQTT::Client::formatTopic(_getId(SensorInputType::POWER)));
                discovery->addDeviceClass(F("power"), _getPowerUnit());
                discovery->addName(F("Power"));
                discovery->addObjectId(baseTopic + F("power"));
            }
            break;
        case 3:
            if (discovery->create(this, _getId(SensorInputType::PEAK_CURRENT), format)) {
                discovery->addStateTopic(MQTT::Client::formatTopic(_getId(SensorInputType::PEAK_CURRENT)));
                discovery->addDeviceClass(F("current"), _getCurrentUnit());
                discovery->addName(F("Peak Current"));
                discovery->addObjectId(baseTopic + F("peak_current"));
            }
            break;
        case 4:
            if (discovery->create(this, _getId(SensorInputType::PEAK_POWER), format)) {
                discovery->addStateTopic(MQTT::Client::formatTopic(_getId(SensorInputType::PEAK_POWER)));
                discovery->addDeviceClass(F("power"), _getPowerUnit());
                discovery->addName(F("Peak Power"));
                discovery->addObjectId(baseTopic + F("peak_power"));
            }
            break;
    }
    return discovery;
}

void Sensor_INA219::getValues(WebUINS::Events &array, bool timer)
{
    array.append(WebUINS::Values(_getId(SensorInputType::VOLTAGE), WebUINS::TrimmedFloat(_data.U(), _config.webui_voltage_precision)));
    if (_config.webui_current) {
        array.append(WebUINS::Values(_getId(SensorInputType::CURRENT), WebUINS::TrimmedFloat(_convertCurrent(_data.I()), _config.webui_current_precision)));
        array.append(WebUINS::Values(_getId(SensorInputType::POWER), WebUINS::TrimmedFloat(_convertPower(_data.P()), _config.webui_power_precision)));
    }
    if (_config.webui_average) {
        array.append(WebUINS::Values(_getId(SensorInputType::AVG_CURRENT), WebUINS::TrimmedFloat(_convertCurrent(_avgData.I()), _config.webui_current_precision)));
        array.append(WebUINS::Values(_getId(SensorInputType::AVG_POWER), WebUINS::TrimmedFloat(_convertPower(_avgData.P()), _config.webui_power_precision)));
    }
    if (_config.webui_peak) {
        array.append(WebUINS::Values(_getId(SensorInputType::PEAK_CURRENT), WebUINS::TrimmedFloat(_convertCurrent(_Ipeak), _config.webui_current_precision)));
        array.append(WebUINS::Values(_getId(SensorInputType::PEAK_POWER), WebUINS::TrimmedFloat(_convertPower(_Ppeak), _config.webui_power_precision)));
    }
    // __LDBG_printf("U=%f I=%f P=%f %s", _data.U(), _data.I(), _data.P(), array.toString().c_str());
}

void Sensor_INA219::createWebUI(WebUINS::Root &webUI)
{
    WebUINS::Row row(WebUINS::Sensor(_getId(SensorInputType::VOLTAGE), _name, 'V').setConfig(_renderConfig));
    if (_config.webui_current) {
        row.append(WebUINS::Sensor(_getId(SensorInputType::CURRENT), F("Current"), _getCurrentUnit()).setConfig(_renderConfig));
        row.append(WebUINS::Sensor(_getId(SensorInputType::POWER), F("Power"), _getPowerUnit()).setConfig(_renderConfig));
    }
    if (_config.webui_average) {
        row.append(WebUINS::Sensor(_getId(SensorInputType::AVG_CURRENT), F("\xe2\x8c\x80 Current"), _getCurrentUnit()).setConfig(_renderConfig));
        row.append(WebUINS::Sensor(_getId(SensorInputType::AVG_POWER), F("\xe2\x8c\x80 Power"), _getPowerUnit()).setConfig(_renderConfig));
    }
    if (_config.webui_peak) {
        row.append(WebUINS::Sensor(_getId(SensorInputType::PEAK_CURRENT), F("Peak Current"), _getCurrentUnit()).setConfig(_renderConfig));
        row.append(WebUINS::Sensor(_getId(SensorInputType::PEAK_POWER), F("Peak Power"), _getPowerUnit()).setConfig(_renderConfig));
    }
    webUI.addRow(row);
}

void Sensor_INA219::publishState()
{
    if (isConnected()) {
        publish(MQTT::Client::formatTopic(_getId(SensorInputType::VOLTAGE)), true, String(_mqttData.U(), 3));
        publish(MQTT::Client::formatTopic(_getId(SensorInputType::CURRENT)), true, String(_convertCurrent(_mqttData.I()), _getCurrentPrecision()));
        publish(MQTT::Client::formatTopic(_getId(SensorInputType::POWER)), true, String(_convertPower(_mqttData.P()), _getPowerPrecision()));
        publish(MQTT::Client::formatTopic(_getId(SensorInputType::PEAK_CURRENT)), true, String(_convertCurrent(_Ipeak), _getCurrentPrecision()));
        publish(MQTT::Client::formatTopic(_getId(SensorInputType::PEAK_POWER)), true, String(_convertPower(_Ppeak), _getPowerPrecision()));
    }
}

void Sensor_INA219::getStatus(Print &output)
{
    output.printf_P(PSTR("INA219 @ I2C address 0x%02x, shunt %.3fm\xE2\x84\xA6" HTML_S(br)), _address, IOT_SENSOR_INA219_R_SHUNT * 1000.0);
}

String Sensor_INA219::_getId(SensorInputType type) const
{
    return PrintString(F("ina219_0x%02x_%c"), _address, type);
}

void Sensor_INA219::_loop()
{
    if (millis() > _updateTimer) {
        _updateTimer = millis() + IOT_SENSOR_INA219_READ_INTERVAL;
        auto microsTime = micros();
        float U = _ina219.getBusVoltage_V();
        float I = _ina219.getCurrent_mA();
        float P = U * I;
        if (I > _Ipeak || P > _Ppeak || millis() >= _holdPeakTimer) {
            _Ipeak = I;
            _Ppeak = P;
            _holdPeakTimer = millis() + _config.getHoldPeakTimeMillis();
        }
        _data.add(U, I, microsTime);
        _avgData.add(U, I, microsTime);
        _mqttData.add(U, I, microsTime);
    }
}

Sensor_INA219::ConfigType Sensor_INA219::_readConfig() const
{
    return Plugins::Sensor::getConfig().ina219;
}

const __FlashStringHelper *Sensor_INA219::_getCurrentUnit() const
{
    return _config.getDisplayCurrent() == CurrentDisplayType::MILLIAMPERE ? F("mA") : F("A");
}

const __FlashStringHelper *Sensor_INA219::_getPowerUnit() const
{
    return _config.getDisplayPower() == PowerDisplayType::MILLIWATT ? F("mW") : F("W");
}

uint8_t Sensor_INA219::_getCurrentPrecision() const
{
    return _config.getDisplayCurrent() == CurrentDisplayType::MILLIAMPERE ? 0 : 2;
}

uint8_t Sensor_INA219::_getPowerPrecision() const
{
    return _config.getDisplayPower() == PowerDisplayType::MILLIWATT ? 0 : 2;
}

float Sensor_INA219::_convertCurrent(float current) const
{
    return _config.getDisplayCurrent() == CurrentDisplayType::MILLIAMPERE ? current : current / 1000.0;
}

float Sensor_INA219::_convertPower(float power) const
{
    return _config.getDisplayPower() == PowerDisplayType::MILLIWATT ? power : power / 1000.0;
}

void Sensor_INA219::createConfigureForm(AsyncWebServerRequest *request, FormUI::Form::BaseForm &form)
{
    auto &cfg = Plugins::Sensor::getWriteableConfig();
    auto &group = form.addCardGroup(F("ina219cfg"), F(IOT_SENSOR_NAMES_INA219_FORM_TITLE), true);

    form.addObjectGetterSetter(F("ina219_iu"), cfg.ina219, cfg.ina219.get_int_display_current, cfg.ina219.set_int_display_current);
    form.addFormUI(F("Current Unit"), FormUI::List(CurrentDisplayType::MILLIAMPERE, F("mA"), CurrentDisplayType::AMPERE, F("A")));

    form.addObjectGetterSetter(F("ina219_pu"), cfg.ina219, cfg.ina219.get_int_display_power, cfg.ina219.set_int_display_power);
    form.addFormUI(F("Power Unit"), FormUI::List(PowerDisplayType::MILLIWATT, F("mW"), PowerDisplayType::WATT, F("W")));

    form.addObjectGetterSetter(F("ina219_ap"), cfg.ina219, cfg.ina219.get_bits_averaging_period, cfg.ina219.set_bits_averaging_period);
    form.addFormUI(F("Averaging Period"), FormUI::Suffix(F("seconds")));
    cfg.ina219.addRangeValidatorFor_averaging_period(form);

    form.addObjectGetterSetter(F("ina219_wr"), cfg.ina219, cfg.ina219.get_bits_webui_update_rate, cfg.ina219.set_bits_webui_update_rate);
    form.addFormUI(F("WebUI Update Rate"), FormUI::Suffix(F("seconds")));
    cfg.ina219.addRangeValidatorFor_webui_update_rate(form);

    form.addObjectGetterSetter(F("ina219_hp"), cfg.ina219, cfg.ina219.get_bits_hold_peak_time, cfg.ina219.set_bits_hold_peak_time);
    form.addFormUI(F("Hold Peak Time"), FormUI::Suffix(F("seconds")));
    cfg.ina219.addRangeValidatorFor_hold_peak_time(form);

    form.addObjectGetterSetter(F("ina219_dc"), cfg.ina219, cfg.ina219.get_bits_webui_current, cfg.ina219.set_bits_webui_current);
    form.addFormUI(F("Display Current Values"), FormUI::BoolItems());

    form.addObjectGetterSetter(F("ina219_da"), cfg.ina219, cfg.ina219.get_bits_webui_average, cfg.ina219.set_bits_webui_average);
    form.addFormUI(F("Display Average Values"), FormUI::BoolItems());

    form.addObjectGetterSetter(F("ina219_dp"), cfg.ina219, cfg.ina219.get_bits_webui_peak, cfg.ina219.set_bits_webui_peak);
    form.addFormUI(F("Display Peak Values"), FormUI::BoolItems());

    form.addObjectGetterSetter(F("ina219_up"), cfg.ina219, cfg.ina219.get_bits_webui_voltage_precision, cfg.ina219.set_bits_webui_voltage_precision);
    form.addFormUI(F("Voltage Display Precision"));
    cfg.ina219.addRangeValidatorFor_webui_voltage_precision(form);

    form.addObjectGetterSetter(F("ina219_ip"), cfg.ina219, cfg.ina219.get_bits_webui_current_precision, cfg.ina219.set_bits_webui_current_precision);
    form.addFormUI(F("Current Display Precision"));
    cfg.ina219.addRangeValidatorFor_webui_current_precision(form);

    form.addObjectGetterSetter(F("ina219_pp"), cfg.ina219, cfg.ina219.get_bits_webui_power_precision, cfg.ina219.set_bits_webui_power_precision);
    form.addFormUI(F("Power Display Precision"));
    cfg.ina219.addRangeValidatorFor_webui_power_precision(form);

    form.addObjectGetterSetter(F("ina219_cal"), FormGetterSetter(cfg.ina219, calibration));
    form.addFormUI(F("Shunt Calibration"));
    cfg.ina219.addRangeValidatorFor_calibration(form);

    form.addObjectGetterSetter(F("ina219_io"), FormGetterSetter(cfg.ina219, offset));
    form.addFormUI(F("Shunt Offset"), FormUI::Suffix(F("mA")));
    cfg.ina219.addRangeValidatorFor_offset(form);

    group.end();
}

void Sensor_INA219::reconfigure(PGM_P source)
{
    _config = _readConfig();
    _ina219.setShunt(IOT_SENSOR_INA219_R_SHUNT * _config.calibration, _config.offset);
    setUpdateRate(_config.webui_update_rate);
    _updateTimer = 0;
    _holdPeakTimer = 0;
    _Ipeak = NAN;
    _Ppeak = NAN;
    _data.clear();
    _avgData = SensorData(_config.averaging_period * 1000);
    _mqttData.clear();
}


#if AT_MODE_SUPPORTED

#include "at_mode.h"

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(SENSORINA219, "SENSORINA219", "<interval in ms>", "Print INA219 sensor data");

#if AT_MODE_HELP_SUPPORTED

ATModeCommandHelpArrayPtr Sensor_INA219::atModeCommandHelp(size_t &size) const
{
    static ATModeCommandHelpArray tmp PROGMEM = {
        PROGMEM_AT_MODE_HELP_COMMAND(SENSORINA219)
    };
    size = sizeof(tmp) / sizeof(tmp[0]);
    return tmp;
}

#endif

bool Sensor_INA219::atModeHandler(AtModeArgs &args)
{
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(SENSORINA219))) {

        static Event::Timer *_timer;
        if (_timer) {
            delete _timer;
            _timer = nullptr;
            args.print(F("Timer stopped"));
        }
        else {
            _timer = new Event::Timer();
            Event::Timer &timer = *_timer;

            auto &serial = args.getStream();
            auto timerPrintFunc = [this, &serial](Event::CallbackTimerPtr timer) {
                return std::count_if(SensorPlugin::begin(), SensorPlugin::end(), [this, &serial](SensorPtr sensorPtr) {
                    if (sensorPtr->getType() == SensorType::INA219) {
                        auto &sensor = *reinterpret_cast<Sensor_INA219 *>(sensorPtr);
                        auto &ina219 = sensor._ina219;
                        serial.printf_P(PSTR("+SENSORINA219: raw: U=%d, Vshunt=%d, I=%d, current: P=%d: %.3fV, %.1fmA, %.1fmW, average: %.3fV, %.1fmA, %.1fmW\n"),
                            ina219.getBusVoltage_raw(),
                            ina219.getShuntVoltage_raw(),
                            ina219.getCurrent_raw(),
                            ina219.getPower_raw(),
                            ina219.getBusVoltage_V(),
                            ina219.getCurrent_mA(),
                            ina219.getBusVoltage_V() * ina219.getCurrent_mA(),
                            sensor.getVoltage(),
                            sensor.getCurrent(),
                            sensor.getPower()
                        );
                        return true;
                    }
                    return false;
                });
            };

            if (!timerPrintFunc(*timer)) {
                args.print(F("No sensor found"));
                delete _timer;
                _timer = nullptr;
            }
            else {
                auto repeat = args.toMillis(0, 500, ~0, 0, String('s'));
                if (repeat) {
                    _Timer(timer).add(repeat, true, timerPrintFunc);
                }
            }
        }
        return true;
    }
    return false;
}

#endif

#endif
