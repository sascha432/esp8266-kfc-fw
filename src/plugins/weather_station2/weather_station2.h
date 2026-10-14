/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

// Weather station 2.x: the LVGL UI of the weather station for the WT32-SC01.
//
// The plugin owns the data source and one object per screen, it registers them with the screen
// manager of the lvgl plugin (src/plugins/lvgl) which takes care of the widget lifetime, the
// automatic rotation and the touch gestures. All widgets come from LVGLUI, the screens only
// read from WeatherStation2::DataSource.
//
//   MAIN          local weather + indoor climate in the footer
//   INDOOR        indoor climate
//   FORECAST      one card per forecast day
//   WORLD_CLOCK   the local clock and up to 4 additional clocks
//   MOON_PHASE    moon disc, phase and the next four phases
//   POWER         power monitor channels (voltage/current/power, total energy, 5 min chart)
//   INFO          network and system values
//
// The values come from WeatherStation2::WeatherDataSource: the OpenWeatherMap One Call API for
// the outdoor weather (API key/coordinates are configured by the "weather2" form of this
// plugin, see ws2_form.cpp), the Meeus calculation for the moon and the indoor metrics from the
// configured sensor sources (the "Sensors" group of the same form: none, an internal sensor or
// an MQTT topic). The plugin is an MQTT::Component for the MQTT sources, it subscribes to the
// configured topics and forwards the messages to the data source. The world clocks are the only
// values that come from the weather configuration directly (the "world-clock" form, the same
// configuration the 1.x plugin uses).
//
// The migration plan (docs/MIGRATION_LVGL.md) has the remaining screens (analog clock,
// power/energy monitor, curated art, messages).

#include <Arduino_compat.h>
#include <vector>

#include "global.h"

#include "plugins.h"
#include "ws2_data.h"
#include "ws2_screens.h"

#if MQTT_SUPPORT
#    include "../mqtt/mqtt_client.h"
#endif

#if MQTT_SUPPORT
class WeatherStation2Plugin : public PluginComponent, public MQTTComponent {
#else
class WeatherStation2Plugin : public PluginComponent {
#endif
public:
    // screen rotation time in seconds, a screen can override it
    static constexpr uint32_t kRotationTime = 10;

    WeatherStation2Plugin();

    virtual void setup(SetupModeType mode, const PluginComponents::DependenciesPtr &dependencies) override;
    virtual void shutdown() override;
    virtual void reconfigure(const String &source) override;
    virtual void getStatus(Print &output) override;
    virtual void createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request) override;
    // one sub menu with the "Weather Station" and "World Clock" forms (the automatic menu can
    // only use the friendly name for every form of a plugin)
    virtual void createMenu() override;

    // the single instance of the plugin
    static WeatherStation2Plugin &getInstance();

private:
    // registers the screens with the screen manager of the lvgl plugin
    void _registerScreens();
    void _removeScreens();

#if MQTT_SUPPORT
    // MQTTComponent: the topics belong to the data source, the plugin only subscribes to them
    virtual void onConnect() override;
    virtual void onDisconnect(AsyncMqttClientDisconnectReason reason) override;
    // the client is destroyed (MQTT disabled in the configuration or reconfigured)
    virtual void onShutdown() override;
    virtual void onMessage(const char *topic, const char *payload, size_t len) override;
    // subscribes to the topics of the configured MQTT sources and removes the obsolete ones
    void _updateMqttSubscriptions();

private:
    // topics the plugin is currently subscribed to
    std::vector<String> _mqttSubscribed;
#endif

private:
    WeatherStation2::WeatherDataSource _data;
    WeatherStation2::MainScreen _mainScreen;
    WeatherStation2::IndoorScreen _indoorScreen;
    WeatherStation2::ForecastScreen _forecastScreen;
    WeatherStation2::WorldClockScreen _worldClockScreen;
    WeatherStation2::MoonPhaseScreen _moonPhaseScreen;
    WeatherStation2::PowerScreen _powerScreen;
    WeatherStation2::InfoScreen _infoScreen;
    bool _registered;
};
