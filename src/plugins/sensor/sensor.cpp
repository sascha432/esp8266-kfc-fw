/**
 * Author: sascha_lammers@gmx.de
 */

#include "sensor.h"
#include <PrintHtmlEntitiesString.h>
#include <KFCJson.h>
#include "../src/plugins/mqtt/mqtt_json.h"
#include <plugins_menu.h>
#include <WebUISocket.h>

#if DEBUG_IOT_SENSOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

static SensorPlugin plugin;

PROGMEM_DEFINE_PLUGIN_OPTIONS(
    SensorPlugin,
    "sensor",           // name
    "Sensors",          // friendly name
    "",                 // web_templates
    "sensor",           // config_forms
    "",                 // reconfigure_dependencies
    PluginComponent::PriorityType::SENSOR,
    PluginComponent::RTCMemoryId::NONE,
    static_cast<uint8_t>(PluginComponent::MenuType::CUSTOM),
    false,              // allow_safe_mode
    true,               // setup_after_deep_sleep
    true,               // has_get_status
    true,               // has_config_forms - the menu is custom and does not add the sensor form link if none of the senors has a form
    true,               // has_web_ui
    false,              // has_web_templates
    true,               // has_at_mode
    0                   // __reserved
);


SensorPlugin::SensorPlugin() : PluginComponent(PROGMEM_GET_PLUGIN_OPTIONS(SensorPlugin))
{
    REGISTER_PLUGIN(this, "SensorPlugin");
}

void SensorPlugin::getValues(WebUINS::Events &array)
{
    for(const auto sensor: _sensors) {
        sensor->getValues(array, false);
    }
}

void SensorPlugin::setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState)
{
    __LDBG_printf("setValue(%s)", id.c_str());
}

void SensorPlugin::setup(SetupModeType mode, const PluginComponents::DependenciesPtr &dependencies)
{
#if IOT_SENSOR_HAVE_MOTION_SENSOR
    addSensor<Sensor_Motion>(F(IOT_SENSOR_NAMES_MOTION_SENSOR));
#endif
#if IOT_SENSOR_HAVE_LM75A
    addSensor<Sensor_LM75A>(F(IOT_SENSOR_NAMES_LM75A), config.initTwoWire(), IOT_SENSOR_HAVE_LM75A);
#endif
#if IOT_SENSOR_HAVE_LM75A_2
    addSensor<Sensor_LM75A>(F(IOT_SENSOR_NAMES_LM75A_2), config.initTwoWire(), IOT_SENSOR_HAVE_LM75A_2);
#endif
#if IOT_SENSOR_HAVE_LM75A_3
    addSensor<Sensor_LM75A>(F(IOT_SENSOR_NAMES_LM75A_3), config.initTwoWire(), IOT_SENSOR_HAVE_LM75A_3);
#endif
#if IOT_SENSOR_HAVE_LM75A_4
    addSensor<Sensor_LM75A>(F(IOT_SENSOR_NAMES_LM75A_4), config.initTwoWire(), IOT_SENSOR_HAVE_LM75A_4);
#endif
#if IOT_SENSOR_HAVE_BME280
    addSensor<Sensor_BME280>(F(IOT_SENSOR_NAMES_BME280), config.initTwoWire(), IOT_SENSOR_HAVE_BME280);
#endif
#if IOT_SENSOR_HAVE_BME680
    addSensor<Sensor_BME680>(F(IOT_SENSOR_NAMES_BME680), IOT_SENSOR_HAVE_BME680);
#endif
#if IOT_SENSOR_HAVE_CCS811
    addSensor<Sensor_CCS811>(F(IOT_SENSOR_NAMES_CCS811), IOT_SENSOR_HAVE_CCS811);
#endif
#if IOT_ATOMIC_SUN_V2 || IOT_DIMMER_MODULE
    addSensor<Sensor_DimmerMetrics>(F(IOT_SENSOR_NAMES_DIMMER_METRICS));
#endif
#if IOT_SENSOR_HAVE_HLW8012
    addSensor<Sensor_HLW8012>(F(IOT_SENSOR_NAMES_HLW8012), IOT_SENSOR_HLW8012_SEL, IOT_SENSOR_HLW8012_CF, IOT_SENSOR_HLW8012_CF1);
#endif
#if IOT_SENSOR_HAVE_HLW8032
    addSensor<Sensor_HLW8032>(F(IOT_SENSOR_NAMES_HLW8032), IOT_SENSOR_HLW8032_RX, IOT_SENSOR_HLW8032_TX, IOT_SENSOR_HLW8032_PF);
#endif
#if IOT_SENSOR_HAVE_BATTERY
    addSensor<Sensor_Battery>(F(IOT_SENSOR_NAMES_BATTERY));
#endif
#if IOT_SENSOR_HAVE_DS3231
    addSensor<Sensor_DS3231>(F(IOT_SENSOR_NAMES_DS3231));
#endif
#if IOT_SENSOR_HAVE_INA219
    addSensor<Sensor_INA219>(F(IOT_SENSOR_NAMES_INA219), config.initTwoWire(), IOT_SENSOR_HAVE_INA219);
#endif
#if IOT_SENSOR_HAVE_DHTxx
    addSensor<Sensor_DHTxx>(F(IOT_SENSOR_NAMES_DHTxx), IOT_SENSOR_HAVE_DHTxx_PIN);
#endif
#if IOT_SENSOR_HAVE_AMBIENT_LIGHT_SENSOR
    addSensor<Sensor_AmbientLight>(F(IOT_SENSOR_NAMES_AMBIENT_LIGHT_SENSOR), 0);
#endif
#if IOT_SENSOR_HAVE_AMBIENT_LIGHT_SENSOR2
    addSensor<Sensor_AmbientLight>(F(IOT_SENSOR_NAMES_AMBIENT_LIGHT_SENSOR), 1);
#endif
#if IOT_SENSOR_HAVE_SYSTEM_METRICS
    addSensor<Sensor_SystemMetrics>();
#endif

    _sortSensors();
    _Timer(_timer).add(Event::milliseconds(1000), true, SensorPlugin::timerEvent);
    for(const auto sensor: _sensors) {
        sensor->setup();
    }

}

void SensorPlugin::reconfigure(const String &source)
{
    _sortSensors();
    for(const auto sensor: _sensors) {
        sensor->reconfigure(source.c_str());
    }
}

SensorPlugin &SensorPlugin::getInstance()
{
    return plugin;
}

void SensorPlugin::timerEvent(Event::CallbackTimerPtr timer)
{
    plugin._timerEvent();
}

// low priority timer executed in main loop()
void SensorPlugin::_timerEvent()
{
    auto mqttIsConnected = MQTTClient::safeIsConnected();

    if (WebUISocket::hasAuthenticatedClients()) {
        using namespace MQTT::Json;
        WebUINS::Events events;
        {
            for(const auto sensor: _sensors) {
                __ASSERT_PTR(sensor);
                sensor->timerEvent(&events, mqttIsConnected);
            }
        }
        if (events.hasAny()) {
            WebUISocket::broadcast(WebUISocket::getSender(), WebUINS::UpdateEvents(events));
        }
    }
    else if (mqttIsConnected) {
        for(const auto sensor: _sensors) {
            __ASSERT_PTR(sensor);
            sensor->timerEvent(nullptr, true);
        }
    }
}

void SensorPlugin::_sortSensors()
{
    std::sort(_sensors.begin(), _sensors.end(), [](const MQTT::SensorPtr a, const MQTT::SensorPtr b) {
         return b->getOrderId() >= a->getOrderId();
    });
}

void SensorPlugin::shutdown()
{
    _timer.remove();
    for(const auto sensor: _sensors) {
        __LDBG_printf("type=%u", sensor->getType());
        sensor->shutdown();
        delete sensor;
    }
    _sensors.clear();
}

bool SensorPlugin::_hasConfigureForm() const
{
    for(const auto sensor: _sensors) {
        if (sensor->hasForm()) {
            return true;
        }
    }
    return false;
}

void SensorPlugin::createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request)
{
    if (type == FormCallbackType::SAVE) {
        for(const auto sensor: _sensors) {
            sensor->configurationSaved(&form);
        }
    }
    if (!isCreateFormCallbackType(type)) {
        return;
    }

    auto &ui = form.createWebUI();
    ui.setTitle(F("Sensor Configuration"));
    ui.setContainerId(F("sensors"));
    ui.setStyle(FormUI::WebUI::StyleType::ACCORDION);

    for(const auto sensor: _sensors) {
        sensor->createConfigureForm(request, form);
    }

    form.finalize();
}

void SensorPlugin::createMenu()
{
    if (_hasConfigureForm()) {
        bootstrapMenu.addMenuItem(getFriendlyName(), F("sensor.html"), navMenu.config);
    }
}

void SensorPlugin::createWebUI(WebUINS::Root &webUI)
{
    if (_addCustomSensors) {
        _addCustomSensors(webUI, SensorType::MIN);
    }
    if (_count()) {
        webUI.addRow(WebUINS::Row(WebUINS::Group(F("Sensors"), false)));
    }

    for(const auto sensor: _sensors) {
        __LDBG_printf("createWebUI type=%u", sensor->getType());
        if (_addCustomSensors) {
            _addCustomSensors(webUI, sensor->getType());
        }
        sensor->createWebUI(webUI);
    }
    if (_addCustomSensors) {
        if (!_count()) {
            webUI.addRow(WebUINS::Row(WebUINS::Group(F("Sensors"), false)));
        }
        _addCustomSensors(webUI, SensorType::MAX);
    }
}

void SensorPlugin::getStatus(Print &output)
{
    __LDBG_printf("sensor count %d", _sensors.size());
    if (_sensors.empty()) {
        output.print(F("All sensors disabled"));
    }
    else {
        PrintHtmlEntitiesString str;
        str.setMode(PrintHtmlEntities::Mode::RAW);
        for(auto sensor: _sensors) {
            sensor->getStatus(str);
        }
        output.print(str);
    }
}

#if AT_MODE_SUPPORTED

#include "at_mode.h"

void SensorPlugin::atModeHelpGenerator()
{
    for(const auto sensor: _sensors) {
        sensor->atModeHelpGenerator();
    }
}

bool SensorPlugin::atModeHandler(AtModeArgs &args)
{
    for(const auto sensor: _sensors) {
        if (sensor->atModeHandler(args)) {
            return true;
        }
    }
    return false;
}

#endif
