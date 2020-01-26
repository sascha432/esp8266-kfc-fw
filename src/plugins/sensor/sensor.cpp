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
#include "Sensor_DS3231.h"
#include "Sensor_INA219.h"

#if DEBUG_IOT_SENSOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

static SensorPlugin plugin;

void SensorPlugin::getValues(JsonArray &array)
{
    _debug_println(F(""));
    for(auto sensor: _sensors) {
        sensor->getValues(array);
    }
}

void SensorPlugin::setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState)
{
    _debug_printf_P(PSTR("setValue(%s)\n"), id.c_str());
}


PGM_P SensorPlugin::getName() const
{
    return PSTR("sensor");
}

void SensorPlugin::setup(PluginSetupMode_t mode)
{
    _timer.add(1000, true, SensorPlugin::timerEvent);
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
#if IOT_SENSOR_HAVE_DS3231
    _sensors.push_back(new Sensor_DS3231(F(IOT_SENSOR_NAMES_DS3231)));
#endif
#if IOT_SENSOR_HAVE_INA219
    _sensors.push_back(new Sensor_INA219(F(IOT_SENSOR_NAMES_INA219), config.initTwoWire(), IOT_SENSOR_HAVE_INA219));
#endif
}

void SensorPlugin::reconfigure(PGM_P source)
{
    for(auto sensor: _sensors) {
        sensor->reconfigure();
    }
}

SensorPlugin::SensorVector &SensorPlugin::getSensors()
{
    return plugin._sensors;
}

size_t SensorPlugin::getSensorCount()
{
    return plugin._sensors.size();
}

SensorPlugin &SensorPlugin::getInstance()
{
    return plugin;
}

void SensorPlugin::timerEvent(EventScheduler::TimerPtr timer)
{
    plugin._timerEvent();
}

// low priority timer executed in main loop()
void SensorPlugin::_timerEvent()
{
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

void SensorPlugin::restart()
{
    _timer.remove();
    for(auto sensor: _sensors) {
        _debug_printf_P(PSTR("type=%u\n"), sensor->getType());
        sensor->restart();
        delete sensor;
    }
}

bool SensorPlugin::hasWebUI() const
{
    return true;
}

WebUIInterface *SensorPlugin::getWebUIInterface()
{
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

void SensorPlugin::createMenu()
{
    if (_hasConfigureForm()) {
        bootstrapMenu.addSubMenu(F("Sensors"), F("sensor.html"), navMenu.config);
    }
}

void SensorPlugin::createWebUI(WebUI &webUI)
{
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


void SensorPlugin::getStatus(Print &output)
{
    _debug_printf_P(PSTR("sensor count %d\n"), _sensors.size());
    if (_sensors.empty()) {
        output.print(F("All sensors disabled"));
    }
    else {
        PrintHtmlEntitiesString str;
        for(auto sensor: _sensors) {
            sensor->getStatus(str);
        }
        static_cast<PrintHtmlEntitiesString &>(output).setRawOutput(true);
        output.print(str);
        static_cast<PrintHtmlEntitiesString &>(output).setRawOutput(false);
    }
}

#if AT_MODE_SUPPORTED

#include "at_mode.h"

void SensorPlugin::atModeHelpGenerator()
{
    for(auto sensor: _sensors) {
        sensor->atModeHelpGenerator();
    }
}

bool SensorPlugin::atModeHandler(AtModeArgs &args)
{
    for(auto sensor: _sensors) {
        if (sensor->atModeHandler(args)) {
            return true;
        }
    }
    return false;
}

#endif

#endif
