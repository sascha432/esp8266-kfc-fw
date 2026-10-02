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
#include <new>

#include "global.h"

#include "plugins.h"
#include "ws2_data.h"
#include "ws2_screens.h"

#if MQTT_SUPPORT
#    include "../mqtt/mqtt_client.h"
#endif

// ==========================================================================================
// PSRAM objects
// ==========================================================================================
// The plugin instance is a static object and the internal DRAM is the scarce resource of this
// board (8 MB of PSRAM are idle while ~100 KB of DRAM have to carry the network stack, the LVGL
// buffers, the task stacks and everything else). The big data of the Home Assistant dashboard
// (the parsed /hass.yaml, the value and history of every tile, the request buffers - about 47 KB)
// and of its screen (the widget tree of the current page - about 8 KB) are therefore allocated
// and constructed in the PSRAM once and kept for the lifetime of the firmware, exactly like the
// draw buffers of the display.
//
template<typename T, typename... Args>
static T &_ws2PsramObject(Args &&...args)
{
    constexpr size_t size = sizeof(T);
    void *ptr = nullptr;
#if ESP32
    // The plugin instance is a static object, so this code runs before the Arduino core reaches
    // initArduino() -> psramInit(), and ps_malloc() returns nullptr until the PSRAM is initialized
    // (it checks the spiramDetected flag). psramInit() is idempotent, the later call of the core
    // does nothing, and it runs the same sequence the core would run a few milliseconds later.
    psramInit();
    ptr = ps_malloc(size);
    if (!ptr) {
        // a failed PSRAM allocation must not break the dashboard - the internal RAM is the
        // next best thing (that is where these objects lived before they were moved)
        __DBG_printf_E("cannot allocate %u bytes in the PSRAM", static_cast<unsigned>(size));
        ptr = malloc(size);
    }
#else
    ptr = malloc(size);
#endif
    if (!ptr) {
        __DBG_printf_E("cannot allocate %u bytes", static_cast<unsigned>(size));
        abort();
    }
    return *new (ptr) T(std::forward<Args>(args)...);
}

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

#if IOT_HASS_DASHBOARD
    // Applies the configured orientation to the dashboard screen. Only the pending flag of the
    // screen is set here (the display itself is switched by the main loop), so this is safe to
    // call from the web task. The form save uses it to apply the change without waiting for the
    // deferred reconfigure() of the framework
    void applyHassOrientation();
#endif

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
    WeatherStation2::MainScreen _mainScreen{_data};
    WeatherStation2::IndoorScreen _indoorScreen{_data};
    WeatherStation2::ForecastScreen _forecastScreen{_data};
    WeatherStation2::WorldClockScreen _worldClockScreen{_data};
    WeatherStation2::MoonPhaseScreen _moonPhaseScreen{_data};
    WeatherStation2::PowerScreen _powerScreen{_data};
    WeatherStation2::InfoScreen _infoScreen{_data};
#if IOT_HASS_DASHBOARD
    // dashboard of a Home Assistant instance, configured by /hass.yaml (docs/hass_config.md)
    // and the screen that draws it. Both are large (about 47 KB and 8 KB) and are created in the
    // PSRAM by the constructor of the plugin - the screen is initialized with the dashboard that
    // is declared above it
    WeatherStation2::HomeAssistant::Dashboard &_hass;
    WeatherStation2::HassScreen &_hassScreen;
#endif
    bool _registered{false};
};
