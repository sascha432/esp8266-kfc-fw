/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_SENSOR

#include "sensor.h"
#include <KFCJson.h>
#include "WebUISocket.h"

#if DEBUG_IOT_SENSOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

void SensorPlugin::getValues(JsonArray &array) {
    _debug_printf_P(PSTR("Sensor::getValues()\n"));
}

void SensorPlugin::setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState) {
    _debug_printf_P(PSTR("Sensor::setValue(%s)\n"), id.c_str());
}


static SensorPlugin plugin;

PGM_P SensorPlugin::getName() const {
    return PSTR("sensor");
}

void SensorPlugin::setup(PluginSetupMode_t mode) {
}

void SensorPlugin::reconfigure(PGM_P source) {
}

bool SensorPlugin::hasWebUI() const {
    return true;
}

WebUIInterface *SensorPlugin::getWebUIInterface() {
    return this;
}

void SensorPlugin::createWebUI(WebUI &webUI) {
    auto row = &webUI.addRow();
    row->setExtraClass(JJ(title));
    row->addGroup(JF("Sensors"), false);
}


bool SensorPlugin::hasStatus() const {
    return true;
}

const String SensorPlugin::getStatus() {
    return String();
}

#endif
