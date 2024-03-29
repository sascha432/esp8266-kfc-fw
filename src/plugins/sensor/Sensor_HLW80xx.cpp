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
#include "plugins/http2serial/http2serial.h"

#if DEBUG_IOT_SENSOR
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

using Plugins = KFCConfigurationClasses::PluginsType;

#define STATE_FILENAME "/.pvt/hlw80xx.state"
#define STATE_NVS_KEY "hlw80xx_state"

Sensor_HLW80xx::Sensor_HLW80xx(const String &name, MQTT::SensorType type) :
    MQTT::Sensor(type),
    _name(name),
    _power(NAN),
    _voltage(NAN),
    _current(NAN)
{
    #if DEBUG_MQTT_CLIENT
        debug_printf_P(PSTR("Sensor_HLW80xx(): component=%p\n"), this);
    #endif
    #if IOT_SENSOR_HLW80xx_ADJUST_CURRENT
        _dimmingLevel = -1;
    #endif
    reconfigure(nullptr); // read config
    __loadEnergyCounter();

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
    auto discovery = new MQTT::AutoDiscovery::Entity();
    __DBG_discovery_printf("num=%u/%u d=%p", num, getAutoDiscoveryCount(), discovery);
    String topic = _getTopic();
    auto baseTopic = MQTT::Client::getBaseTopicPrefix();
    switch(num) {
        case 0:
            if (discovery->create(this, FSPGM(power), format)) {
                discovery->addStateTopic(topic);
                discovery->addValueTemplate(FSPGM(power));
                discovery->addDeviceClass(F("power"), 'W');
                discovery->addName(F("Power"));
                discovery->addObjectId(baseTopic + F("power"));
            }
            break;
        case 1:
            if (discovery->create(this, FSPGM(energy_total), format)) {
                discovery->addStateTopic(topic);
                discovery->addValueTemplate(FSPGM(energy_total));
                discovery->addDeviceClass(F("energy"), FSPGM(kWh));
                discovery->addName(F("Energy Total"));
                discovery->addObjectId(baseTopic + F("energy_total"));
            }
            break;
        case 2:
            if (discovery->create(this, FSPGM(energy), format)) {
                discovery->addStateTopic(topic);
                discovery->addValueTemplate(FSPGM(energy));
                discovery->addDeviceClass(F("energy"), FSPGM(kWh));
                discovery->addName(F("Energy"));
                discovery->addObjectId(baseTopic + F("energy"));
            }
            break;
        case 3:
            if (discovery->create(this, FSPGM(voltage), format)) {
                discovery->addStateTopic(topic);
                discovery->addValueTemplate(FSPGM(voltage));
                discovery->addDeviceClass(F("voltage"), 'V');
                discovery->addName(F("Voltage"));
                discovery->addObjectId(baseTopic + F("voltage"));
            }
            break;
        case 4:
            if (discovery->create(this, FSPGM(current), format)) {
                discovery->addStateTopic(topic);
                discovery->addValueTemplate(FSPGM(current));
                discovery->addDeviceClass(F("current"), 'A');
                discovery->addName(F("Current"));
                discovery->addObjectId(baseTopic + F("current"));
            }
            break;
        case 5:
            if (discovery->create(this, FSPGM(pf), format)) {
                discovery->addStateTopic(topic);
                discovery->addValueTemplate(FSPGM(pf));
                discovery->addDeviceClass(F("power_factor"));
                discovery->addName(F("Power Factor"));
                discovery->addObjectId(baseTopic + F("power_factor"));
            }
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
    auto energyTotal = _getEnergy(0);
    auto energy = _getEnergy(1);
    auto pf = _getPowerFactor();
    array.append(
        WebUINS::Values(_getId(FSPGM(power)), _powerToNumber(_power), !isnan(_power)),
        WebUINS::Values(_getId(FSPGM(energy_total)), _energyToNumber(energyTotal), !isnan(energyTotal)),
        WebUINS::Values(_getId(FSPGM(energy)), _energyToNumber(energy), !isnan(energy)),
        WebUINS::Values(_getId(FSPGM(voltage)), String(_voltage, 1 + _extraDigits), !isnan(_voltage)),
        WebUINS::Values(_getId(FSPGM(current)), _currentToNumber(_current), !isnan(_current)),
        WebUINS::Values(_getId(FSPGM(pf)), String(pf, 2), !isnan(pf))
    );
}

void Sensor_HLW80xx::createWebUI(WebUINS::Root &webUI)
{
    webUI.addRow(WebUINS::Row(
        WebUINS::Sensor(_getId(FSPGM(power)), _name + F(" Power"), 'W').setConfig(_renderConfig),
        WebUINS::Sensor(_getId(FSPGM(energy_total)), _name + F(" Energy Total"), FSPGM(kWh)).setConfig(_renderConfig),
        WebUINS::Sensor(_getId(FSPGM(energy)), _name + F(" Energy"), FSPGM(kWh)).setConfig(_renderConfig),
        WebUINS::Sensor(_getId(FSPGM(voltage)), _name + F(" Voltage"), 'V').setConfig(_renderConfig),
        WebUINS::Sensor(_getId(FSPGM(current)), _name + F(" Current"), 'A').setConfig(_renderConfig),
        WebUINS::Sensor(_getId(FSPGM(pf)), _name + F(" Power Factor"), emptyString).setConfig(_renderConfig)
    ));
}

void Sensor_HLW80xx::reconfigure(PGM_P source)
{
    auto sensor = Plugins::Sensor::getConfig().hlw80xx;
    _calibrationU = sensor.calibrationU;
    _calibrationI = sensor.calibrationI;
    _calibrationP = sensor.calibrationP;
    _extraDigits = sensor.extraDigits;
    _energyCounter[0] = sensor.energyCounter;
    __LDBG_printf("calibration U=%f I=%f P=%f x_digits=%u energy=%.0f", _calibrationU, _calibrationI, _calibrationP, _extraDigits, (double)_energyCounter[0]);
}

void Sensor_HLW80xx::shutdown()
{
    #if IOT_SENSOR_HLW80xx_SAVE_ENERGY_CNT
        _saveEnergyCounter(true);
    #endif

    // update energy counter in EEPROM
    auto &cfg = Plugins::Sensor::getWriteableConfig().hlw80xx;
    if (cfg.energyCounter != _energyCounter[0]) {
        cfg.energyCounter = _energyCounter[0];
        config.write();
    }
}

void Sensor_HLW80xx::createConfigureForm(AsyncWebServerRequest *request, FormUI::Form::BaseForm &form)
{
    auto &cfg = Plugins::Sensor::getWriteableConfig().hlw80xx;

    auto &group = form.addCardGroup(F("hlw80xx_cf"), F("HLW8012"), true);

    form.addObjectGetterSetter(F("hlw80xx_u"), FormGetterSetter(cfg, calibrationU));
    form.addFormUI(F("Voltage Calibration"));
    cfg.addRangeValidatorFor_calibrationU(form);

    form.addObjectGetterSetter(F("hlw80xx_i"), FormGetterSetter(cfg, calibrationI));
    form.addFormUI(F("Current Calibration"));
    cfg.addRangeValidatorFor_calibrationI(form);

    form.addObjectGetterSetter(F("hlw80xx_p"), FormGetterSetter(cfg, calibrationP));
    form.addFormUI(F("Power Calibration"));
    cfg.addRangeValidatorFor_calibrationP(form);

    form.addObjectGetterSetter(F("hlw80xx_xd"), FormGetterSetter(cfg, extraDigits));
    form.addFormUI(F("Extra Digits/Precision"));
    cfg.addRangeValidatorFor_extraDigits(form);

    form.add(F("hlw80xx_e1"), String(), FormUI::Field::Type::TEXT);
    form.addFormUI(F("Total Energy"), FormUI::Suffix(FSPGM(kWh)), FormUI::PlaceHolder(IOT_SENSOR_HLW80xx_PULSE_TO_KWH(getEnergyPrimaryCounter()), 3));

    form.addValidator(FormUI::Validator::CallbackTemplate<String>([&cfg, this](const String &value, FormField &field) {
        if (value.length()) {
            // gets copied to getEnergyPrimaryCounter() in configurationSaved()
            cfg.energyCounter = IOT_SENSOR_HLW80xx_KWH_TO_PULSE(value.toFloat());
            _resetSaveEnergyTimer();
        }
        return true;
    }));

    form.add(F("hlw80xx_2"), String(), FormUI::Field::Type::TEXT);
    form.addFormUI(F("Energy"), FormUI::Suffix(FSPGM(kWh)), FormUI::PlaceHolder(IOT_SENSOR_HLW80xx_PULSE_TO_KWH(getEnergySecondaryCounter()), 3));

    form.addValidator(FormUI::Validator::CallbackTemplate<String>([this](const String &value, FormField &field) {
        if (value.length()) {
            getEnergySecondaryCounter() = IOT_SENSOR_HLW80xx_KWH_TO_PULSE(value.toFloat());
            _resetSaveEnergyTimer();
        }
        return true;
    }));

    group.end();

}

void Sensor_HLW80xx::configurationSaved(FormUI::Form::BaseForm *form)
{
    auto &cfg = Plugins::Sensor::getWriteableConfig().hlw80xx;
    getEnergyPrimaryCounter() = cfg.energyCounter;
}

void Sensor_HLW80xx::publishState()
{
    if (isConnected()) {
        publish(_getTopic(), true, MQTT::Json::UnnamedObject(
            MQTT::Json::NamedDouble(FSPGM(power), _power, 1),
            MQTT::Json::NamedDouble(FSPGM(energy_total), _getEnergy(0), 1),
            MQTT::Json::NamedDouble(FSPGM(energy), _getEnergy(1), 1),
            MQTT::Json::NamedDouble(FSPGM(voltage), _voltage, 1 + _extraDigits),
            MQTT::Json::NamedDouble(FSPGM(current), _current, (_current < 1 ? 3 : 2) + _extraDigits),
            MQTT::Json::NamedDouble(FSPGM(pf), _getPowerFactor(), 2)
        ).toString());
    }
}

void Sensor_HLW80xx::__saveEnergyCounterToFile()
{
    __LDBG_printf("file=%s", PSTR(STATE_FILENAME));
    auto file = KFCFS.open(F(STATE_FILENAME), fs::FileOpenMode::write);
    if (file) {
        file.write(reinterpret_cast<const uint8_t *>(_energyCounter.data()), sizeof(_energyCounter));
        file.close();
    }
}

bool Sensor_HLW80xx::__loadEnergyCounterFromFile(EnergyCounterArray &energy)
{
    __LDBG_printf("file=%s", PSTR(STATE_FILENAME));
    auto file = KFCFS.open(F(STATE_FILENAME), fs::FileOpenMode::read);
    EnergyCounterArray tmp;
    if (file && file.read(reinterpret_cast<uint8_t *>(tmp.data()), sizeof(tmp)) == sizeof(tmp)) {
        energy = tmp;
    } else {
        __LDBG_printf("read error");
        energy = EnergyCounterArray({Plugins::Sensor::getConfig().hlw80xx.energyCounter, 0}); // restore data from config in case FS was updated
        return false;
    }
    return true;
}

void Sensor_HLW80xx::_saveEnergyCounter(bool shutdown)
{
    #if IOT_SENSOR_HLW80xx_SAVE_ENERGY_CNT
        __LDBG_printf("counter1=%llu counter2=%llu shutdown=%d", _energyCounter[0], _energyCounter[1], shutdown);

        uint32_t ms = millis();

        // check for changes
        if (!shutdown) {
            EnergyCounterArray energy;
            if (__loadEnergyCounter(energy)) {
                if (energy == _energyCounter) {
                    __LDBG_printf("no changes");
                    _saveEnergyCounterTimer = ms;
                    return;
                }
            }
        }

        #if IOT_SENSOR_HLW80xx_SAVE_ENERGY_TYPE == IOT_SENSOR_HLW80xx_SAVE_ENERGY_TYPE_FS

            __saveEnergyCounterToFile();
            _saveEnergyCounterTimer = ms;

        #elif IOT_SENSOR_HLW80xx_SAVE_ENERGY_TYPE == IOT_SENSOR_HLW80xx_SAVE_ENERGY_TYPE_NVS

            #if IOT_SENSOR_HLW80xx_SAVE_ENERGY_CNT_FS
                __LDBG_printf("save fs_cnt=%d/%d", _saveEnergyCounterFSCounter + 1, IOT_SENSOR_HLW80xx_SAVE_ENERGY_CNT_FS);
            #else
                __LDBG_printf("save");
            #endif

            auto err = config._nvs_open(true);
            if (err == ESP_OK) {
                err = config._nvs_set_blob(STATE_NVS_KEY, _energyCounter.data(), sizeof(_energyCounter));
                config._nvs_close();
            }
            if (err != ESP_OK) {
                __LDBG_printf("cannot write '%s' err=%x", PSTR(STATE_NVS_KEY), err);
            }
            _saveEnergyCounterTimer = ms;

            #if IOT_SENSOR_HLW80xx_SAVE_ENERGY_CNT_FS
                // fallback
                if (shutdown || ++_saveEnergyCounterFSCounter >= IOT_SENSOR_HLW80xx_SAVE_ENERGY_CNT_FS) {
                    __saveEnergyCounterToFile();
                    _saveEnergyCounterFSCounter = 0;
                }
            #endif

        #else

            #error not supported

        #endif
    #endif
}

bool Sensor_HLW80xx::__loadEnergyCounter(EnergyCounterArray &energy)
{
    #if IOT_SENSOR_HLW80xx_SAVE_ENERGY_CNT
        #if IOT_SENSOR_HLW80xx_SAVE_ENERGY_TYPE == IOT_SENSOR_HLW80xx_SAVE_ENERGY_TYPE_FS

            return __loadEnergyCounterFromFile(energy);

        #elif IOT_SENSOR_HLW80xx_SAVE_ENERGY_TYPE == IOT_SENSOR_HLW80xx_SAVE_ENERGY_TYPE_NVS

            EnergyCounterArray tmp;
            size_t length = sizeof(tmp);
            auto err = config._nvs_get_blob_with_open(STATE_NVS_KEY, tmp.data(), &length);
            if (err == ESP_OK && length == sizeof(tmp)) {
                energy = tmp;
                return true;
            }
            else {
                __LDBG_printf("failed to get '%s' len=%u size=%u err=%x", PSTR(STATE_NVS_KEY), length, sizeof(tmp), err);
            }
            #if IOT_SENSOR_HLW80xx_SAVE_ENERGY_CNT_FS
                return __loadEnergyCounterFromFile(energy);
            #endif
        #endif
        // __LDBG_printf("counter1=%.0f counter2=%.0f res=%d", float(_energyCounter[0]), float(_energyCounter[1]), result);
    #endif
    return false;
}

void Sensor_HLW80xx::__loadEnergyCounter()
{
    __loadEnergyCounter(_energyCounter);
    #if IOT_SENSOR_HLW80xx_SAVE_ENERGY_CNT
        _saveEnergyCounterTimer = millis();
        #if IOT_SENSOR_HLW80xx_SAVE_ENERGY_CNT_FS
            _saveEnergyCounterFSCounter = 0;
        #endif
    #endif
}

void Sensor_HLW80xx::_resetSaveEnergyTimer()
{
    #if IOT_SENSOR_HLW80xx_SAVE_ENERGY_CNT
        _saveEnergyCounterTimer = millis() - (IOT_SENSOR_HLW80xx_SAVE_ENERGY_CNT + 1);
        #if IOT_SENSOR_HLW80xx_SAVE_ENERGY_CNT_FS
            _saveEnergyCounterFSCounter = IOT_SENSOR_HLW80xx_SAVE_ENERGY_CNT_FS;
        #endif
    #endif
}

void Sensor_HLW80xx::setExtraDigits(uint8_t digits)
{
    Plugins::Sensor::getWriteableConfig().hlw80xx.extraDigits = std::min((uint8_t)6, digits);
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
#define PROGMEM_AT_MODE_HELP_COMMAND_PREFIX "SP_"

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(HLWXD, "HLWXD", "<count/0-4>", "Display extra digits");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(HLWDUMP, "HLWDUMP", "<0=off/1...=seconds/2=cycle>", "Dump sensor data");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(HLWPLOT, "HLWPLOT", "<ClientID>,<U/I/P/0=disable>[,<1/true=convert units>]", "Request data for plotting live graph");

#if AT_MODE_HELP_SUPPORTED

ATModeCommandHelpArrayPtr Sensor_HLW80xx::atModeCommandHelp(size_t &size) const
{
    static ATModeCommandHelpArray tmp PROGMEM = {
        PROGMEM_AT_MODE_HELP_COMMAND(HLWXD),
        PROGMEM_AT_MODE_HELP_COMMAND(HLWDUMP),
        PROGMEM_AT_MODE_HELP_COMMAND(HLWPLOT)
    };
    size = sizeof(tmp) / sizeof(tmp[0]);
    return tmp;
}

#endif

bool Sensor_HLW80xx::atModeHandler(AtModeArgs &args)
{
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(HLWXD))) {
        if (args.requireArgs(1)) {
            uint8_t digits = args.toIntMinMax(0, 0, 4);

            auto count = std::count_if(SensorPlugin::begin(), SensorPlugin::end(), [digits](MQTT::Sensor *sensor) {
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
        _Timer(_dumpTimer).remove();

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
