/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_SENSOR

#include "sensor.h"
#include <PrintHtmlEntitiesString.h>
#include <KFCJson.h>
#include "WebUISocket.h"
#include "Sensor_LM75A.h"
#include "Sensor_BME280.h"
#include "Sensor_BME680.h"
#include "Sensor_CCS811.h"
#include "Sensor_HLW80xx.h"
#include "Sensor_HLW8012.h"
#include "Sensor_HLW8032.h"
#include "Sensor_Battery.h"

#if DEBUG_IOT_SENSOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

static SensorPlugin plugin;


void SensorPlugin::getValues(JsonArray &array) {
    _debug_printf_P(PSTR("SensorPlugin::getValues()\n"));
    for(auto sensor: _sensors) {
        sensor->getValues(array);
    }
}

void SensorPlugin::setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState) {
    _debug_printf_P(PSTR("SensorPlugin::setValue(%s)\n"), id.c_str());
}


PGM_P SensorPlugin::getName() const {
    return PSTR("sensor");
}

void SensorPlugin::setup(PluginSetupMode_t mode) {
    Scheduler.addTimer(&_timer, 1e4, true, SensorPlugin::timerEvent);
#if IOT_SENSOR_HAVE_LM75A
    _sensors.push_back(new Sensor_LM75A(F(IOT_SENSOR_NAMES_LM75A), config.initTwoWire(), IOT_SENSOR_HAVE_LM75A));
#endif
#if IOT_SENSOR_HAVE_BME280
    _sensors.push_back(new Sensor_BME280(F(IOT_SENSOR_NAMES_BME280), config.initTwoWire(), IOT_SENSOR_HAVE_BME280));
#endif
#if IOT_SENSOR_HAVE_BME680
    _sensors.push_back(new Sensor_BME680(F(IOT_SENSOR_NAMES_BME680), IOT_SENSOR_HAVE_BME680));
#endif
#if IOT_SENSOR_HAVE_CCS811
    _sensors.push_back(new Sensor_CCS811(F(IOT_SENSOR_NAMES_CCS811), IOT_SENSOR_HAVE_CCS811));
#endif
#if IOT_SENSOR_HAVE_HLW8012
    _sensors.push_back(new Sensor_HLW8012(F(IOT_SENSOR_NAMES_HLW8012), IOT_SENSOR_HLW8012_SEL, IOT_SENSOR_HLW8012_CF, IOT_SENSOR_HLW8012_CF1));
#endif
#if IOT_SENSOR_HAVE_HLW8032
    _sensors.push_back(new Sensor_HLW8032(F(IOT_SENSOR_NAMES_HLW8032), IOT_SENSOR_HLW8032_RX, IOT_SENSOR_HLW8032_TX, IOT_SENSOR_HLW8032_PF));
#endif
#if IOT_SENSOR_HAVE_BATTERY
    _sensors.push_back(new Sensor_Battery(F(IOT_SENSOR_NAMES_BATTERY)));
#endif
}

void SensorPlugin::reconfigure(PGM_P source)
{
    for(auto sensor: _sensors) {
        sensor->reconfigure();
    }
}

SensorPlugin::SensorVector &SensorPlugin::getSensors() {
    return plugin._sensors;
}

size_t SensorPlugin::getSensorCount() {
    return plugin._sensors.size();
}


void SensorPlugin::timerEvent(EventScheduler::TimerPtr timer) {
    plugin._timerEvent();
}

void SensorPlugin::_timerEvent() {
    JsonUnnamedObject json(2);
    json.add(JJ(type), JJ(ue));
    auto &events = json.addArray(JJ(events));
    for(auto sensor: _sensors) {
        sensor->timerEvent(events);
    }
    if (events.size()) {
        WsWebUISocket::broadcast(WsWebUISocket::getSender(), json);
    }
}

void SensorPlugin::restart() {
    for(auto sensor: _sensors) {
        sensor->restart();
    }
}

bool SensorPlugin::hasWebUI() const {
    return true;
}

WebUIInterface *SensorPlugin::getWebUIInterface() {
    return this;
}

bool SensorPlugin::_hasConfigureForm() const
{
    for(auto sensor: _sensors) {
        if (sensor->hasForm()) {
            return true;
        }
    }
    return false;
}

PGM_P SensorPlugin::getConfigureForm() const
{
    return _hasConfigureForm() ? PSTR("sensor") : nullptr;
}

void SensorPlugin::createConfigureForm(AsyncWebServerRequest *request, Form &form)
{
    form.setFormUI(F("Sensor Configuration"));
    for(auto sensor: _sensors) {
        sensor->createConfigureForm(request, form);
    }
    form.finalize();
}

void SensorPlugin::createMenu() {
    if (_hasConfigureForm()) {
        bootstrapMenu.addSubMenu(F("Sensors"), F("sensor.html"), navMenu.config);
    }
}

void SensorPlugin::createWebUI(WebUI &webUI) {
    auto row = &webUI.addRow();
    row->setExtraClass(JJ(title));
    row->addGroup(F("Sensors"), false);

    if (_sensors.size()) {
        row = &webUI.addRow();
    }

    for(auto sensor: _sensors) {
        sensor->createWebUI(webUI, &row);
    }
}


bool SensorPlugin::hasStatus() const {
    return _sensors.size() != 0;
}

const String SensorPlugin::getStatus() {
    _debug_printf_P(PSTR("SensorPlugin::getStatus(): sensor count %d\n"), _sensors.size());
    PrintHtmlEntitiesString str;
    for(auto sensor: _sensors) {
        sensor->getStatus(str);
        if (!str.endsWith(HTML_TAG_E)) {
            str.print(F(HTML_S(br)));
        }
    }
    return str;
}

#if AT_MODE_SUPPORTED

#include "at_mode.h"

typedef enum {
    NONE = 0,
    FLOAT,
    UINT64,
} SensorVarsTypeEnum_t;

typedef struct {
    PGM_P name;
    SensorVarsTypeEnum_t type;
    void *ptr;
} SensorVars_t;

static String uint64ToString(uint64_t input) {
    String result;
    do {
        char c = (input % 10) + '0';
        input /= 10;
        result = c + result;
    } while (input);
    return result;
}

static String get_var_value(SensorVars_t &var) {
    switch(var.type) {
        case FLOAT:
            return String(*(float *)var.ptr, 6);
        case UINT64:
            return uint64ToString(*(uint64_t *)var.ptr);
        default:
            return String();
    }
}

static void set_var_value(const char *value, SensorVars_t &var) {
    switch(var.type) {
        case FLOAT:
            *(float *)var.ptr = atof(value);
            break;
        case UINT64:
            *(uint64_t *)var.ptr = atoll(value);
            break;
        default:
            break;
    }
}

PROGMEM_AT_MODE_HELP_COMMAND_DEF(SENSORC, "SENSORC", "<variable-name>,<value>", "Configure sensors", "Display configuration");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(SENSORR, "SENSORR", "Reconfigure sensor plugin");
#if IOT_SENSOR_HAVE_BATTERY
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(SENSORPBV, "SENSORPBV", "<repeat every n seconds>", "Print battery voltage");
#endif

void SensorPlugin::atModeHelpGenerator() {
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(SENSORC));
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(SENSORR));
#if IOT_SENSOR_HAVE_BATTERY
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(SENSORPBV));
#endif
}

bool SensorPlugin::atModeHandler(Stream &serial, const String &command, int8_t argc, char **argv) {
    if (constexpr_String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(SENSORR))) {
        serial.println(F("+SENSORR: Reconfiguring plugin..."));
        reconfigure(nullptr);
        return true;
    }
    else if (constexpr_String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(SENSORC))) {
#if IOT_SENSOR_HAVE_BATTERY || IOT_SENSOR_HAVE_HLW8012 || IOT_SENSOR_HAVE_HLW8032
        auto &_config = config._H_W_GET(Config().sensor);
#endif
        SensorVars_t sensor_variables[] = {
#if IOT_SENSOR_HAVE_BATTERY
            { PSTR("battery.calibration"), FLOAT, &_config.battery.calibration },
#endif
#if IOT_SENSOR_HAVE_HLW8012 || IOT_SENSOR_HAVE_HLW8032
            { PSTR("hlw80xx.calibrationU"), FLOAT, &_config.hlw80xx.calibrationU },
            { PSTR("hlw80xx.calibrationI"), FLOAT, &_config.hlw80xx.calibrationI },
            { PSTR("hlw80xx.calibrationP"), FLOAT, &_config.hlw80xx.calibrationP },
            { PSTR("hlw80xx.energyCounter"), UINT64, &_config.hlw80xx.energyCounter },
#endif
            { nullptr, SensorVarsTypeEnum_t::NONE, 0 }
        };

        if (argc != 2) {
            for(uint8_t i = 0; sensor_variables[i].name; i++) {
                serial.printf_P("+SENSORC: %s=%s\n", sensor_variables[i].name, get_var_value(sensor_variables[i]).c_str());
            }
        }
        else {
            for(uint8_t i = 0; sensor_variables[i].name; i++) {
                if (!strcmp_P(argv[0], sensor_variables[i].name)) {
                    set_var_value(argv[1], sensor_variables[i]);
                    serial.printf_P("+SENSORC: set %s=%s\n", sensor_variables[i].name, get_var_value(sensor_variables[i]).c_str());
                }
            }
        }
        return true;
    }
#if IOT_SENSOR_HAVE_BATTERY
    else if (constexpr_String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(SENSORPBV))) {
        static EventScheduler::TimerPtr timer = nullptr;
        if (timer) {
            Scheduler.removeTimer(timer);
            timer = nullptr;
        }
        double averageSum = 0;
        int averageCount = 0;
        auto printVoltage = [&serial, averageSum, averageCount](EventScheduler::TimerPtr timer) mutable {
            Sensor_Battery::SensorDataEx_t data;
            auto value = Sensor_Battery::readSensor(&data);
            averageSum += value;
            averageCount++;
            serial.printf_P(PSTR("+SENSORPBV: %.4fV - avg %.4f (calibration %.6f, #%d, adc sum %d, adc avg %.6f)\n"),
                value,
                averageSum / (double)averageCount,
                config._H_GET(Config().sensor).battery.calibration,
                data.adcReadCount,
                data.adcSum,
                data.adcSum / (double)data.adcReadCount
            );
        };
        printVoltage(nullptr);
        if (argc >= 1) {
            int repeat = atoi(argv[0]);
            if (repeat) {
                Scheduler.addTimer(&timer, repeat * 1000, true, printVoltage);
            }
        }
        return true;
    }
#endif
    return false;
}

#endif


#endif
