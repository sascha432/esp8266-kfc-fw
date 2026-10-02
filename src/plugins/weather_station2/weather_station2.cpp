/**
 * Author: sascha_lammers@gmx.de
 */

#include "weather_station2.h"

#include <algorithm>
#include <LoopFunctions.h>
#include <PrintHtmlEntitiesString.h>
#if ESP32
// allocator of the TLS/mbedtls heap, see _installTlsPsramAllocator() below
#    include <esp_heap_caps.h>
#    include <esp32-hal-psram.h>
#    include <mbedtls/platform.h>
#endif
#include "lvgl_plugin.h"
#include "plugins_menu.h"
#if DEBUG_LVGL_SCREENSHOT
#    include "lvgl_debug.h"
#endif

#ifndef DEBUG_WEATHER_STATION2
#    define DEBUG_WEATHER_STATION2 0
#endif

#if DEBUG_WEATHER_STATION2
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

using WeatherStation2::IndoorValues;
using WeatherStation2::getMetricTitle;
using WeatherStation2::PowerChannels;
using WeatherStation2::PowerSourceType;
using WeatherStation2::getPowerSourceTypeName;
using WeatherStation2::getPowerChannelPart;
using Plugins = KFCConfigurationClasses::PluginsType;

PROGMEM_DEFINE_PLUGIN_OPTIONS(
    WeatherStation2Plugin,
    "weather2",         // name
    "Weather Station",  // friendly name
    "",                 // web_templates
    // the weather configuration and the world clock form are shared with the 1.x plugin. The plugin
    // form is split into pages, every page is a form of its own (see ws2_form.cpp)
    "weather2,weather2-sensors,weather2-power,weather2-hass,world-clock", // config_forms
    "",                 // reconfigure_dependencies
    // the screens need the display, so the plugin is set up after the lvgl plugin
    PluginComponent::PriorityType::MIN,
    PluginComponent::RTCMemoryId::NONE,
    // the two forms need their own menu entries, the automatic menu would use the friendly
    // name for both of them
    static_cast<uint8_t>(PluginComponent::MenuType::CUSTOM),
    false,              // allow_safe_mode
    false,              // setup_after_deep_sleep
    true,               // has_get_status
    true,               // has_config_forms
    false,              // has_web_ui
    false,              // has_web_templates
    false,              // has_at_mode
    0                   // __reserved
);

static WeatherStation2Plugin plugin;

WeatherStation2Plugin &WeatherStation2Plugin::getInstance()
{
    return plugin;
}

#if IOT_HASS_DASHBOARD
void WeatherStation2Plugin::applyHassOrientation()
{
    // the orientation is its own configuration parameter, the setters of the form stored it before
    // SAVE was called (FormCallbackType::SAVE runs before config.write(), see PluginComponent.h)
    _hassScreen.setOrientation(Plugins::WeatherStation::getHassRotation());
}
#endif

WeatherStation2Plugin::WeatherStation2Plugin() :
    PluginComponent(PROGMEM_GET_PLUGIN_OPTIONS(WeatherStation2Plugin))
#if MQTT_SUPPORT
    , MQTTComponent(MQTT::ComponentType::SENSOR)
#endif
#if IOT_HASS_DASHBOARD
    // the dashboard and its screen are the two largest objects of the plugin (~47 KB and ~8 KB) and
    // they are created in the PSRAM (see _ws2PsramObject(), which initializes the PSRAM as well -
    // the instance is a static object and its constructor runs before initArduino()). The display
    // is only used by the screen methods, never from the constructor
    , _hass(_ws2PsramObject<WeatherStation2::HomeAssistant::Dashboard>())
    , _hassScreen(_ws2PsramObject<WeatherStation2::HassScreen>(_data, _hass))
#endif
{
    REGISTER_PLUGIN(this, "WeatherStation2Plugin");
}

// ------------------------------------------------------------------------------------------
// mbedtls allocator (TLS)
// ------------------------------------------------------------------------------------------
// The TLS handshake of the weather/Home Assistant clients needs about 40 KB of heap and the small
// X.509 allocations are the first thing that fails when the internal DRAM runs out (the handshake
// reports "(-10368) X509 - Allocation of memory failed"). mbedtls is built with
// MBEDTLS_PLATFORM_MEMORY (mbedtls/esp_config.h) and its default allocator is the plain
// esp_mbedtls_mem_calloc()/calloc(), which the core serves from the internal RAM unless the
// allocation is larger than CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL (4 KB on this SDK variant).
// Installing an allocator
// that uses the PSRAM first moves every mbedtls allocation of the firmware there (TLS handshake,
// X.509, record buffers and the SHA contexts of the session/WebSocket code) - the 8 MB PSRAM is
// otherwise idle while the internal DRAM is the scarce resource of this board.
//
// The switch is global (mbedtls keeps one pair of function pointers), so it is installed once
// during the plugin setup. Blocks allocated before the switch are not a problem, ESP-IDF's free()
// and heap_caps_free() are the same implementation.
#if ESP32
static void *_psramCalloc(size_t count, size_t size)
{
    // PSRAM first, internal RAM as fallback if the PSRAM is exhausted
    auto ptr = heap_caps_calloc(count, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return ptr ? ptr : heap_caps_calloc(count, size, MALLOC_CAP_DEFAULT);
}

static void _psramFree(void *ptr)
{
    heap_caps_free(ptr);
}

static void _installTlsPsramAllocator()
{
    static bool installed = false;
    if (installed) {
        return;
    }
    installed = true;
    if (!psramFound()) {
        return;
    }
    mbedtls_platform_set_calloc_free(_psramCalloc, _psramFree);
    __LDBG_printf("mbedtls allocator uses the PSRAM (heap=%u, psram=%u)", ESP.getFreeHeap(), ESP.getFreePsram());
}
#else
static void _installTlsPsramAllocator()
{
}
#endif

void WeatherStation2Plugin::setup(SetupModeType mode, const PluginComponents::DependenciesPtr &dependencies)
{
    __LDBG_printf("mode=%u", static_cast<unsigned>(mode));
    // the TLS handshake of the request tasks must not run out of internal DRAM, move the mbedtls
    // allocations to the PSRAM before any of them starts
    _installTlsPsramAllocator();
    // reads the configuration and the system/WiFi values, cannot run in the constructor
    _data.begin();
#if IOT_HASS_DASHBOARD
    // reads /hass.yaml and starts the request task, a missing file is not an error
    _hass.begin();
    __LDBG_printf("dashboard at %p (%s), %u bytes", static_cast<const void *>(&_hass),
                  esp_ptr_external_ram(&_hass) ? "PSRAM" : "internal RAM", static_cast<unsigned>(sizeof(_hass)));
#endif
#if MQTT_SUPPORT
    // the indoor metrics can read MQTT topics, the client calls onConnect()/onMessage()
    MQTT::Client::registerComponent(this);
#endif
    _registerScreens();
#if IOT_HASS_DASHBOARD
    // Orientation of the dashboard screen (the other screens keep the landscape layout). The
    // display is rotated while the dashboard is shown, see HassScreen::setOrientation()
    _hassScreen.setOrientation(Plugins::WeatherStation::getHassRotation());
#endif
#if DEBUG_LVGL_SCREENSHOT
    // the debug screenshot feature can push values into the screens, see lvgl_debug.h
    LVGLDebug::setValueCallback([](const String &key, const String &value) {
        return WeatherStation2Plugin::getInstance()._data.debugSetValue(key, value);
    });
    LVGLDebug::setHelpCallback([]() -> const char * {
        return "temp, feels, min, max, hum, press, wind, rain, uv, city, descr, icon(sun|partly|cloudy|rain|snow|storm|fog), "
               "days(1-5), dayparts(1), itmp, ihum, ipress, igas, phase, illum(0-1), age, freeze(0|1), "
               "pwrch(0-3), pwrv, pwra, pwrw, pwre, hasspage(page of /hass.yaml), hassfull(tile of a picture tile), hasspanel(tile of a light/dimmer/climate/sensor tile), hassrange(12, 24 or 48 hours of the history graph)";
    });
#endif
    // refresh the values once per second, the data source rate limits the update itself
    LOOP_FUNCTION_ADD_ARG([this]() {
        _data.update();
#if IOT_HASS_DASHBOARD
        // applies the response of the request task and notices a new /hass.yaml
        _hass.update();
#endif
    }, this);
}

void WeatherStation2Plugin::createMenu()
{
    // "Weather Station" (the OpenWeatherMap access and the units) and the other pages of the split
    // form + "World Clock" below one sub menu, like the 1.x plugin
    auto configMenu = bootstrapMenu.getMenuItem(navMenu.config);
    auto subMenu = configMenu.addSubMenu(getFriendlyName());
    subMenu.addMenuItem(getFriendlyName(), F("weather2.html"));
    subMenu.addMenuItem(F("Sensors"), F("weather2-sensors.html"));
    subMenu.addMenuItem(F("Power Monitor"), F("weather2-power.html"));
    subMenu.addMenuItem(F("Home Assistant"), F("weather2-hass.html"));
    subMenu.addMenuItem(F("World Clock"), F("world-clock.html"));
}

void WeatherStation2Plugin::shutdown()
{
#if MQTT_SUPPORT
    MQTT::Client::unregisterComponent(this);
    _mqttSubscribed.clear();
#endif
    LoopFunctions::remove(this);
    _removeScreens();
    // the reader task of the remote power monitor is not needed after the shutdown
    _data.stopPowerMonitor();
#if IOT_HASS_DASHBOARD
    // stops the request task of the dashboard
    _hass.stop();
#endif
}

void WeatherStation2Plugin::reconfigure(const String &source)
{
    // called from the main loop after the settings form was saved (WebServer::executeDelayed),
    // the data source re-reads the configuration and requests immediately
    __LDBG_printf("source=%s", source.c_str());
    _data.reconfigure();
#if IOT_HASS_DASHBOARD
    // re-reads /hass.yaml
    _hass.reconfigure();
#endif
    // the clock in the top bar of the screen overview follows the same setting
    LVGLPlugin::screens().setTimeFormat24h(_data.isTimeFormat24h());
#if IOT_HASS_DASHBOARD
    // the "Home Assistant" group of the form can change the orientation of the dashboard
    _hassScreen.setOrientation(Plugins::WeatherStation::getHassRotation());
#endif
#if MQTT_SUPPORT
    // the "Sensors" group of the form may have changed the topics
    _updateMqttSubscriptions();
#endif
}

#if MQTT_SUPPORT

void WeatherStation2Plugin::onConnect()
{
    // a new connection has no subscriptions, the client discards them on disconnect
    _mqttSubscribed.clear();
    _data.mqttConnected();
    _updateMqttSubscriptions();
}

void WeatherStation2Plugin::onDisconnect(AsyncMqttClientDisconnectReason reason)
{
    __LDBG_printf("reason=%d", static_cast<int>(reason));
    _mqttSubscribed.clear();
    _data.mqttDisconnected();
}

void WeatherStation2Plugin::onShutdown()
{
    // the client is destroyed (MQTT disabled in the configuration), the metrics fall back to
    // "offline"
    _mqttSubscribed.clear();
    _data.mqttDisconnected();
}

void WeatherStation2Plugin::onMessage(const char *topic, const char *payload, size_t len)
{
    // routes the payload of a subscribed topic into the indoor model
    _data.mqttMessage(topic, payload, len);
}

void WeatherStation2Plugin::_updateMqttSubscriptions()
{
    std::vector<String> topics;
    _data.getMqttTopics(topics);

    if (!isConnected()) {
        // onConnect() subscribes when the connection is up
        _mqttSubscribed.clear();
        return;
    }

    // unsubscribe the topics that are no longer used
    for (const auto &topic : _mqttSubscribed) {
        if (std::find(topics.begin(), topics.end(), topic) == topics.end()) {
            unsubscribe(topic);
        }
    }
    // subscribe the new ones
    for (const auto &topic : topics) {
        if (std::find(_mqttSubscribed.begin(), _mqttSubscribed.end(), topic) == _mqttSubscribed.end()) {
            subscribe(topic);
        }
    }
    _mqttSubscribed = topics;
    __LDBG_printf("%u topic(s) subscribed", static_cast<unsigned>(_mqttSubscribed.size()));
}

#endif

void WeatherStation2Plugin::_registerScreens()
{
    if (_registered) {
        return;
    }
    auto &screens = LVGLPlugin::screens();
    screens.add(&_mainScreen);
    screens.add(&_indoorScreen);
    screens.add(&_forecastScreen);
    screens.add(&_worldClockScreen);
    screens.add(&_moonPhaseScreen);
    screens.add(&_powerScreen);
    screens.add(&_infoScreen);
#if IOT_HASS_DASHBOARD
    screens.add(&_hassScreen);
#endif
    screens.setRotationTime(kRotationTime);
    // the clock in the top bar of the screen overview uses the same format as the screens
    screens.setTimeFormat24h(_data.isTimeFormat24h());
    _registered = true;
    __LDBG_printf("registered %u screens", static_cast<unsigned>(screens.count()));
}

void WeatherStation2Plugin::_removeScreens()
{
    if (!_registered) {
        return;
    }
    auto &screens = LVGLPlugin::screens();
    screens.remove(&_mainScreen);
    screens.remove(&_indoorScreen);
    screens.remove(&_forecastScreen);
    screens.remove(&_worldClockScreen);
    screens.remove(&_moonPhaseScreen);
    screens.remove(&_powerScreen);
    screens.remove(&_infoScreen);
#if IOT_HASS_DASHBOARD
    screens.remove(&_hassScreen);
#endif
    _registered = false;
}

void WeatherStation2Plugin::getStatus(Print &output)
{
    const auto &current = _data.getCurrent();
    const auto &indoor = _data.getIndoor();
    const auto &moon = _data.getMoon();

    output.printf_P(PSTR("Data source: %s, metric=%u, 24h clock=%u" HTML_S(br)),
        _data.getName(), static_cast<unsigned>(_data.isMetric()), static_cast<unsigned>(_data.isTimeFormat24h()));
    if (current.valid) {
        output.printf_P(PSTR("Weather: %s, %s in %s" HTML_S(br)),
            current.description.c_str(), _data.formatTemperature(current.temperature).c_str(), current.location.c_str());
    }
    else {
        output.printf_P(PSTR("Weather: no data yet (%s)" HTML_S(br)), _data.isConfigured() ? "request pending" : "not configured");
    }
    output.printf_P(PSTR("Indoor: %s, %s, %s" HTML_S(br)),
        _data.formatTemperature(indoor.get(IndoorValues::Metric::TEMPERATURE).value).c_str(),
        _data.formatHumidity(indoor.get(IndoorValues::Metric::HUMIDITY).value).c_str(),
        _data.formatPressure(indoor.get(IndoorValues::Metric::PRESSURE).value).c_str());

    // the source and the state of every indoor metric
    {
        String sensors;
        for (uint8_t i = 0; i < IndoorValues::kNumMetrics; i++) {
            const auto metric = static_cast<IndoorValues::Metric>(i);
            if (i) {
                sensors += F(", ");
            }
            sensors += getMetricTitle(metric);
            sensors += '=';
            sensors += _data.formatIndoorValue(metric, indoor.get(metric));
        }
        output.printf_P(PSTR("Indoor values: %s" HTML_S(br)), sensors.c_str());
    }
#if MQTT_SUPPORT
    {
        StringVector topics;
        _data.getMqttTopics(topics);
        output.printf_P(PSTR("MQTT sensors: %u topic(s), %s, %u subscription(s)" HTML_S(br)),
            static_cast<unsigned>(topics.size()), _data.isMqttConnected() ? "connected" : "not connected",
            static_cast<unsigned>(_mqttSubscribed.size()));
    }
#endif

    // power monitor channels: the local INA219 or a channel of the remote server
    {
        const auto &power = _data.getPower();
        output.printf_P(PSTR("Power: %u channel(s) configured" HTML_S(br)), static_cast<unsigned>(power.getCount()));
        PrintString channelId;
        for (uint8_t i = 0; i < PowerChannels::kNumChannels; i++) {
            const auto &value = power.values[i];
            if (!value.configured) {
                continue;
            }
            PrintString reading;
            if (value.available) {
                reading.printf_P(PSTR("%s V, %s A, %s W"), _data.formatVoltage(value.voltage).c_str(),
                                 _data.formatCurrent(value.current).c_str(), _data.formatPower(value.power).c_str());
            }
            else {
                reading = F("no data");
            }
            channelId = String();
            if (value.source == PowerSourceType::REMOTE) {
                channelId.printf_P(PSTR(" #%u"), static_cast<unsigned>(value.remoteChannelId));
            }
            output.printf_P(PSTR("Power %u: %s '%s'%s, %s" HTML_S(br)), static_cast<unsigned>(i + 1),
                getPowerSourceTypeName(value.source), getPowerChannelPart(i, 1).c_str(), channelId.c_str(),
                reading.c_str());
        }
        const auto remote = _data.getPowerRemoteStatus();
        if (remote.length()) {
            output.printf_P(PSTR("Power monitor: %s" HTML_S(br)), remote.c_str());
        }
    }

    if (_data.getForecastCount()) {
        output.printf_P(PSTR("Forecast: %u day(s), first %s %s" HTML_S(br)), static_cast<unsigned>(_data.getForecastCount()),
            _data.getForecast()[0].day.c_str(), _data.formatTemperature(_data.getForecast()[0].maxTemperature).c_str());
    }
    else {
        output.printf_P(PSTR("Forecast: no data yet" HTML_S(br)));
    }
    if (moon.valid) {
        output.printf_P(PSTR("Moon: %s, %s, %s" HTML_S(br)), moon.phase.c_str(), _data.formatIllumination(moon.illumination).c_str(), _data.formatAge(moon.age).c_str());
    }
    else {
        output.printf_P(PSTR("Moon: waiting for the clock (NTP)" HTML_S(br)));
    }

    if (!_data.isConfigured()) {
        output.printf_P(PSTR("OpenWeatherMap: no API key or location configured (see the Weather Station form)" HTML_S(br)));
    }
    else if (!_data.isRunning()) {
        output.printf_P(PSTR("OpenWeatherMap: request task is not running" HTML_S(br)));
    }
    else {
        const auto error = _data.getLastError();
        if (error.length()) {
            output.printf_P(PSTR("OpenWeatherMap: %u request(s), last error: %s" HTML_S(br)),
                static_cast<unsigned>(_data.getRequestCount()), error.c_str());
        }
        else if (!current.valid) {
            output.printf_P(PSTR("OpenWeatherMap: %u request(s), waiting for data" HTML_S(br)),
                static_cast<unsigned>(_data.getRequestCount()));
        }
        else {
            output.printf_P(PSTR("OpenWeatherMap: %u request(s), no error" HTML_S(br)),
                static_cast<unsigned>(_data.getRequestCount()));
        }
    }

    auto &screens = LVGLPlugin::screens();
    output.printf_P(PSTR("Screens: %u registered, active: %s, auto rotation: %s" HTML_S(br)), static_cast<unsigned>(screens.count()),
        screens.getActiveScreen() ? screens.getActiveScreen()->getName() : "none",
        screens.isRotationPaused() ? "paused (touch)" : "enabled");
#if IOT_HASS_DASHBOARD
    output.printf_P(PSTR("Dashboard orientation: %s" HTML_S(br)), _hassScreen.getOrientationName());
    output.printf_P(PSTR("Dashboard page: %u" HTML_S(br)), static_cast<unsigned>(_hassScreen.getPage()));
#if DEBUG_LVGL_SCREENSHOT
    output.printf_P(PSTR("Debug sets: %s" HTML_S(br)), _data.getDebugSetInfo().c_str());
#endif
    _hass.getStatus(output);
#endif
}

#if DEBUG_WEATHER_STATION2
#    include <debug_helper_disable.h>
#endif
