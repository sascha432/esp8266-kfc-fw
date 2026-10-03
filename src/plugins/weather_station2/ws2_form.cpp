/**
 * Author: sascha_lammers@gmx.de
 */

#include "weather_station2.h"

#include <KFCForms.h>
#include <kfc_fw_config.h>

// The logger enables __LDBG_printf() per file (see logger.h): without this block the traces of
// the form builder are compiled out (the pages are rendered by the web task, so this is the only
// way to see a page that never finishes)
#ifndef DEBUG_WEATHER_STATION2
#    define DEBUG_WEATHER_STATION2 1
#endif

#if DEBUG_WEATHER_STATION2
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

//
// Settings of the weather station 2.x: the OpenWeatherMap API access, the units and the sources
// of the indoor metrics.
//
// The values live in the weather configuration of the core - the same parameters the 1.x plugin
// uses - so both plugins share one configuration and WeatherStation2::WeatherDataSource can read
// them without a configuration of its own. The moon needs no configuration at all, it is
// calculated from the clock (see shared/moon_phase).
//
// The "Sensors" group selects the source of every indoor metric (none, the internal sensor of
// the sensor plugin or an MQTT topic). One string per metric is stored, the format is
//
//   none
//   internal
//   mqtt:<status topic>,<value topic>,<value|json_value:<key>>
//
// The four fields of a metric change one part of that string (see setSourcePart()), the value
// defaults are used while nothing is configured.
//
// The page is Resources/html/weather2.html (only built for envs that set IOT_WEATHER_STATION2).
// The field names match the ones of the 1.x page, so the "Update latitude/longitude" helper of
// the page works the same way (it asks the OpenWeatherMap geocoding API).
//
// The plugin also handles the "world-clock" form of the 1.x plugin (same fields, same page
// Resources/html/world-clock.html), it configures the clocks of the world clock screen.
//

using WeatherStation = KFCConfigurationClasses::Plugins::WeatherStationConfigNS::WeatherStation;
using WeatherStationConfig = WeatherStation::Config_t;

using namespace WeatherStation2;

// One source of an indoor metric: the type plus the topics. The fields are added for every
// metric of IndoorValues, the names are indexed (sst_0..sst_3 etc.)
static void _addSensorsGroup(FormUI::Form::BaseForm &form)
{
    auto &group = form.addCardGroup(F("ws2_sensors"), F("Sensors"), false);

    PROGMEM_DEF_LOCAL_VARNAMES(_VAR_, WEATHER_STATION2_NUM_INDOOR_METRICS, sst, stp, svt, svl);

    static_assert(WEATHER_STATION2_NUM_INDOOR_METRICS == IndoorValues::kNumMetrics, "update WEATHER_STATION2_NUM_INDOOR_METRICS");

    for (uint8_t i = 0; i < IndoorValues::kNumMetrics; i++) {
        const auto metric = static_cast<IndoorValues::Metric>(i);
        String title = getMetricTitle(metric);

        // source type
        form.addCallbackGetterSetter<uint8_t>(F_VAR(sst, i), [metric](uint8_t &value, FormUI::Field::BaseField &field, bool store) {
            if (store) {
                String type;
                switch (value) {
                case static_cast<uint8_t>(SensorType::INTERNAL):
                    type = F("internal");
                    break;
                case static_cast<uint8_t>(SensorType::MQTT):
                    type = F("mqtt");
                    break;
                default:
                    type = F("none");
                    break;
                }
                setSourcePart(metric, 0, type);
            }
            else {
                const auto name = getSourcePart(metric, 0);
                value = (F("mqtt") == name) ? static_cast<uint8_t>(SensorType::MQTT)
                    : ((F("internal") == name) ? static_cast<uint8_t>(SensorType::INTERNAL) : static_cast<uint8_t>(SensorType::NONE));
            }
            return true;
        });
        String label = title;
        label += F(" Source");
        form.addFormUI(FormUI::Label(label), FormUI::List(
            static_cast<uint8_t>(SensorType::NONE), F("None"),
            static_cast<uint8_t>(SensorType::INTERNAL), F("Internal sensor"),
            static_cast<uint8_t>(SensorType::MQTT), F("MQTT")
        ));

        // status topic (availability of the sensor)
        form.addCallbackSetter<String>(F_VAR(stp, i), getSourcePart(metric, 1), [metric](String &value, FormUI::Field::BaseField &field) {
            setSourcePart(metric, 1, value);
        });
        label = title;
        label += F(" Status Topic");
        form.addFormUI(FormUI::Label(label), FormUI::PlaceHolder(F("online/offline, optional")));
        form.addValidator(FormUI::Validator::Length(0, WeatherStation::kTemperatureSourceMaxSize, true));

        // value topic
        form.addCallbackSetter<String>(F_VAR(svt, i), getSourcePart(metric, 2), [metric](String &value, FormUI::Field::BaseField &field) {
            setSourcePart(metric, 2, value);
        });
        label = title;
        label += F(" Value Topic");
        form.addFormUI(FormUI::Label(label), FormUI::PlaceHolder(F("empty = MQTT disabled")));
        form.addValidator(FormUI::Validator::Length(0, WeatherStation::kTemperatureSourceMaxSize, true));

        // value inside the payload
        form.addCallbackSetter<String>(F_VAR(svl, i), getSourcePart(metric, 3), [metric](String &value, FormUI::Field::BaseField &field) {
            setSourcePart(metric, 3, value);
        });
        label = title;
        label += F(" Value");
        form.addFormUI(FormUI::Label(label), FormUI::PlaceHolder(F("value or json_value:key")));
        form.addValidator(FormUI::Validator::Length(0, WeatherStation::kTemperatureSourceMaxSize, true));
    }

    group.end();
}

// One power channel of the power screen: the source type, the display name and the remote
// channel id. The three fields change one part of the channel string (see setPowerChannelPart),
// they are added for every channel of PowerChannels (pct_0..pct_3 etc.). The remote host and port
// are shared by all remote channels.
//
//   none
//   local|<name>|<remote channel id>     the local INA219 of the sensor plugin
//   remote|<name>|<remote channel id>    one channel of the rpi-power-monitor TCP server
//
static void _addPowerGroup(FormUI::Form::BaseForm &form)
{
    auto &group = form.addCardGroup(F("ws2_power"), F("Power Monitor"), false);

    // remote server of the "remote" channels (rpi-power-monitor, default port 7000)
    form.addCallbackSetter<String>(F("pwr_h"), getPowerRemoteHost(), [](String &value, FormUI::Field::BaseField &field) {
        WeatherStation::setPowerRemoteHost(value.c_str());
    });
    form.addFormUI(F("Remote Host"), FormUI::PlaceHolder(F("192.168.0.4")));
    form.addValidator(FormUI::Validator::Length(0, WeatherStation::kPowerRemoteHostMaxSize, true));

    form.addCallbackSetter<String>(F("pwr_p"), String(getPowerRemotePort()), [](String &value, FormUI::Field::BaseField &field) {
        WeatherStation::setPowerRemotePort(value.c_str());
    });
    form.addFormUI(F("Remote Port"), FormUI::PlaceHolder(F("7000")));
    form.addValidator(FormUI::Validator::Length(0, WeatherStation::kPowerRemotePortMaxSize, true));

    // window of the graph (1..60 minutes). The history is stored per channel in PSRAM, one sample
    // per second - 60 minutes are 3600 samples per channel and series
    form.addCallbackGetterSetter<uint8_t>(F("pwr_g"), [](uint8_t &value, FormUI::Field::BaseField &field, bool store) {
        if (store) {
            WeatherStation::setPowerGraphMinutes(value);
        }
        else {
            value = WeatherStation::getPowerGraphMinutes();
        }
        return true;
    });
    form.addFormUI(F("Graph Time"),
                   FormUI::PlaceHolder(WeatherStation::kPowerGraphMinutesDefault),
                   FormUI::MinMax(WeatherStation::kPowerGraphMinutesMin, WeatherStation::kPowerGraphMinutesMax),
                   FormUI::Type::NUMBER_RANGE, FormUI::Suffix(F("minutes")));
    form.addValidator(FormUI::Validator::Range(WeatherStation::kPowerGraphMinutesMin, WeatherStation::kPowerGraphMinutesMax, false));

    PROGMEM_DEF_LOCAL_VARNAMES(_VAR_, WEATHER_STATION2_NUM_POWER_CHANNELS, pct, pcn, pci);

    static_assert(WEATHER_STATION2_NUM_POWER_CHANNELS == PowerChannels::kNumChannels, "update WEATHER_STATION2_NUM_POWER_CHANNELS");

    for (uint8_t i = 0; i < PowerChannels::kNumChannels; i++) {
        // source type
        form.addCallbackGetterSetter<uint8_t>(F_VAR(pct, i), [i](uint8_t &value, FormUI::Field::BaseField &field, bool store) {
            if (store) {
                String type;
                switch (value) {
                case static_cast<uint8_t>(PowerSourceType::LOCAL):
                    type = F("local");
                    break;
                case static_cast<uint8_t>(PowerSourceType::REMOTE):
                    type = F("remote");
                    break;
                default:
                    type = F("none");
                    break;
                }
                setPowerChannelPart(i, 0, type);
            }
            else {
                const auto name = getPowerChannelPart(i, 0);
                value = (F("remote") == name) ? static_cast<uint8_t>(PowerSourceType::REMOTE)
                    : ((F("local") == name) ? static_cast<uint8_t>(PowerSourceType::LOCAL) : static_cast<uint8_t>(PowerSourceType::NONE));
            }
            return true;
        });
        String label = F("Channel ");
        label += static_cast<unsigned>(i + 1);
        label += F(" Source");
        form.addFormUI(FormUI::Label(label), FormUI::List(
            static_cast<uint8_t>(PowerSourceType::NONE), F("None"),
            static_cast<uint8_t>(PowerSourceType::LOCAL), F("Local INA219"),
            static_cast<uint8_t>(PowerSourceType::REMOTE), F("Remote TCP")
        ));

        // display name of the channel (the wire protocol carries the channel id only, the server
        // does not send the names)
        form.addCallbackSetter<String>(F_VAR(pcn, i), getPowerChannelPart(i, 1), [i](String &value, FormUI::Field::BaseField &field) {
            setPowerChannelPart(i, 1, value);
        });
        label = F("Channel ");
        label += static_cast<unsigned>(i + 1);
        label += F(" Name");
        form.addFormUI(FormUI::Label(label), FormUI::PlaceHolder(F("12V Input")));
        form.addValidator(FormUI::Validator::Length(0, WeatherStation::kPowerChannel0MaxSize, true));

        // channel id on the remote server (1..3 = the rails of an INA3221, 100+idx = aggregates)
        form.addCallbackSetter<String>(F_VAR(pci, i), getPowerChannelPart(i, 2), [i](String &value, FormUI::Field::BaseField &field) {
            setPowerChannelPart(i, 2, value);
        });
        label = F("Channel ");
        label += static_cast<unsigned>(i + 1);
        label += F(" Remote Channel");
        form.addFormUI(FormUI::Label(label), FormUI::PlaceHolder(F("1")));
        form.addValidator(FormUI::Validator::Length(0, 8, true));
    }

    group.end();
}

// Orientation of the Home Assistant dashboard screen. Only the dashboard is rotated, the other
// screens of the plugin keep the landscape layout (the screen rotates the display back when it is
// left). It is its own binary parameter, so the weather configuration is not touched by the change
// (see WeatherStation::getHassRotation())
#if IOT_HASS_DASHBOARD
static void _addHassGroup(FormUI::Form::BaseForm &form)
{
    auto &group = form.addCardGroup(F("ws2_hass"), F("Home Assistant"), false);

    form.addCallbackGetterSetter<uint8_t>(F("hro"), [](uint8_t &value, FormUI::Field::BaseField &field, bool store) {
        if (store) {
            WeatherStation::setHassRotation(value);
        }
        else {
            value = WeatherStation::getHassRotation();
        }
        return true;
    });
    form.addFormUI(F("Dashboard Orientation"), FormUI::List(
        static_cast<uint8_t>(WeatherStation::HassRotation::LANDSCAPE), F("Landscape (480x320)"),
        static_cast<uint8_t>(WeatherStation::HassRotation::PORTRAIT), F("Portrait (320x480)"),
        static_cast<uint8_t>(WeatherStation::HassRotation::LANDSCAPE_FLIPPED), F("Landscape, turned 180 degrees"),
        static_cast<uint8_t>(WeatherStation::HassRotation::PORTRAIT_FLIPPED), F("Portrait, turned 180 degrees")
    ));

    // The rotation is also cycled and locked with the quick settings of the dashboard (a swipe left
    // or right on the dashboard), this is the same parameter
    form.addCallbackGetterSetter<uint8_t>(F("hro_lock"), [](uint8_t &value, FormUI::Field::BaseField &field, bool store) {
        if (store) {
            WeatherStation::setHassRotationLock(value != 0);
        }
        else {
            value = WeatherStation::getHassRotationLock() ? 1 : 0;
        }
        return true;
    });
    form.addFormUI(F("Rotation Lock"), FormUI::List(
        static_cast<uint8_t>(0), F("Off"),
        static_cast<uint8_t>(1), F("On")
    ));

    group.end();
}
#endif

// title, container id and style of one page of the plugin form (see createConfigureForm())
static void _setPage(FormUI::WebUI::Config &ui, const __FlashStringHelper *title, const __FlashStringHelper *containerId)
{
    ui.setTitle(title);
    ui.setContainerId(containerId);
    ui.setStyle(FormUI::WebUI::StyleType::ACCORDION);
}

void WeatherStation2Plugin::createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request)
{
    if (!isCreateFormCallbackType(type)) {
#if IOT_HASS_DASHBOARD
        // The dashboard orientation is applied right away instead of waiting for the deferred
        // reconfigure() of the framework (the config write is delayed, the user would save the
        // form and nothing would happen for a few seconds). The setters of the form stored the
        // value before SAVE was called
        if (type == FormCallbackType::SAVE && F("weather2-hass") == formName) {
            WeatherStation2Plugin::getInstance().applyHassOrientation();
        }
#endif
        // the configuration is written by the framework and re-read by reconfigure()
        return;
    }
    __LDBG_printf("form: '%s' (heap=%u)", formName.c_str(), (unsigned)ESP.getFreeHeap());

    auto &cfg = WeatherStation::getWriteableConfig();

    // The writeable configuration is the raw storage: an all zero blob means it was never saved
    // by a form (the defaults of the constructor are not stored). Without this the first save
    // would switch the units to imperial and the clock to 12h
    if (cfg.weather_poll_interval == 0 && cfg.latitude == 0 && cfg.longitude == 0 && !cfg.is_metric && !cfg.time_format_24h && !*WeatherStation::getApiKey()) {
        __LDBG_printf("initializing the uninitialized weather configuration with its defaults");
        cfg = WeatherStationConfig();
    }

    auto &ui = form.createWebUI();

    // The "world-clock" form writes the same configuration the 1.x plugin uses
    // (WeatherStation::getName/TZ/TZName and the flags of additionalClocks). The fields have the
    // same names, so Resources/html/world-clock.html and Resources/js/forms/world-clock.js work
    // unchanged
    if (F("world-clock") == formName) {

        ui.setTitle(F("World Clock Configuration"));
        ui.setContainerId(F("wc_settings"));
        ui.setStyle(FormUI::WebUI::StyleType::ACCORDION);

        auto &group = form.addCardGroup(F("w_clock"));

        PROGMEM_DEF_LOCAL_VARNAMES(_VAR_, WEATHER_STATION_MAX_CLOCKS, nm, tf, tz, tn);

        for (uint8_t i = 0; i < WEATHER_STATION_MAX_CLOCKS; i++) {

            // a clock is enabled while it has a display name
            form.addCallbackSetter(F_VAR(nm, i), WeatherStation::getName(i), [&cfg, i](const String &value, FormUI::Field::BaseField &field) {
                cfg.additionalClocks[i]._set__enabled(value.length() ? 1 : 0);
                WeatherStation::setName(i, value.c_str());
            });
            form.addFormUI(F("Display Name"), FormUI::PlaceHolder(F("Disabled")));
            form.addValidator(FormUI::Validator::Length(4, 16, true));

            form.addCallbackSetter(F_VAR(tz, i), WeatherStation::getTZ(i), [i](const String &value, FormUI::Field::BaseField &field) {
                WeatherStation::setTZ(i, value.c_str());
            }).setOptional(true);
            form.addFormUI(FormUI::Type::SELECT, F("Time Zone"));

            form.addCallbackSetter(F_VAR(tn, i), WeatherStation::getTZName(i), [i](const String &value, FormUI::Field::BaseField &field) {
                WeatherStation::setTZName(i, value.c_str());
            });
            form.addFormUI(FormUI::Type::HIDDEN);

            form.addObjectGetterSetter(F_VAR(tf, i), FormGetterSetter(cfg.additionalClocks[i], _time_format_24h));
            form.addFormUI(F("Time Format"), FormUI::BoolItems(F("24h"), F("12h")));

        }

        group.end();
        form.finalize();
        return;
    }

    // The form is split into pages, one form per page: the whole form is created before the response
    // is streamed, so a page that renders ~40 fields blocks the request for a long time (and a
    // single field that goes wrong takes the complete page down). The page is selected by the name
    // of the form, which is the name of the .html file in the file system (see createMenu() and
    // Resources/html/weather2*.html)
    if (F("weather2-sensors") == formName) {
        _setPage(ui, F("Sensor Configuration"), F("weather2_sensors"));
        _addSensorsGroup(form);
        form.finalize();
        return;
    }
    if (F("weather2-power") == formName) {
        _setPage(ui, F("Power Monitor Configuration"), F("weather2_power"));
        _addPowerGroup(form);
        form.finalize();
        return;
    }
    if (F("weather2-hass") == formName) {
        _setPage(ui, F("Home Assistant Configuration"), F("weather2_hass"));
#if IOT_HASS_DASHBOARD
        _addHassGroup(form);
#endif
        form.finalize();
        return;
    }

    // the main page: the OpenWeatherMap API access and the units
    _setPage(ui, F("Weather Station Configuration"), F("weather2_settings"));

    auto &apiGroup = form.addCardGroup(F("ws_api"), F("OpenWeatherMap"), true);

    form.addStringGetterSetter(F("apk"), WeatherStation::getApiKey, WeatherStation::setApiKey);
    WeatherStation::addApiKeyLengthValidator(form);
    form.addFormUI(F("API Key"));

    form.addStringGetterSetter(F("apq"), WeatherStation::getLocation, WeatherStation::setLocation);
    WeatherStation::addLocationLengthValidator(form);
    form.addFormUI(F("API Location"), FormUI::Suffix(F("City, Country (ISO 3166)")));

    // no range validator: 0 is a valid coordinate and the validator of a range would reject it
    form.addObjectGetterSetter(F("aqla"), FormGetterSetter(cfg, latitude));
    form.addFormUI(F("API Latitude"));

    form.addObjectGetterSetter(F("aqlo"), FormGetterSetter(cfg, longitude));
    form.addFormUI(F("API Longitude"));

    form.addObjectGetterSetter(F("ato"), FormGetterSetter(cfg, api_timeout));
    form.addFormUI(F("API Timeout"), FormUI::Suffix(F("seconds")));
    cfg.addRangeValidatorFor_api_timeout(form);

    form.addObjectGetterSetter(F("api"), FormGetterSetter(cfg, weather_poll_interval));
    form.addFormUI(F("Weather Poll Interval"), FormUI::Suffix(F("minutes")));
    cfg.addRangeValidatorFor_weather_poll_interval(form);

    apiGroup.end();

    auto &group = form.addCardGroup(F("config"));

    form.addObjectGetterSetter(F("im"), FormGetterSetter(cfg, is_metric));
    form.addFormUI(F("Units"), FormUI::BoolItems(F("Metric"), F("Imperial")));

    form.addObjectGetterSetter(F("tf"), FormGetterSetter(cfg, time_format_24h));
    form.addFormUI(F("Time Format"), FormUI::BoolItems(F("24h"), F("12h")));

    group.end();
}
