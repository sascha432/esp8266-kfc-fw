/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

// Data model of the weather station plugin 2.x.
//
// DataSource owns the values the screens display, WeatherDataSource fills them from the real
// sources:
//
//   - outdoor weather: shared/open_weather_map (One Call API 3.0, streamed into a fixed size
//                      parser and mapped into the model)
//   - moon:            shared/moon_phase (Meeus, no network, no configuration)
//   - system info:     KFCFramework configuration, WiFi, ESP
//   - indoor climate:  the internal sensor of the sensor plugin or an MQTT topic (see the
//                      "Sensors" group of the weather2 form), "offline"/"--" until a source
//                      delivers
//   - power/energy:    Sensor_INA219/Sensor_HLW80xx (screen not implemented yet, see the
//                      migration plan in docs/MIGRATION_LVGL.md)
//
// The request of the OpenWeatherMap client blocks while the response is transferred, so it runs
// in its own task and the main loop only applies a finished response (see _requestLoop()). The
// configuration (API key, location, units) is written by the "weather2" form of the plugin.
//
// No value is made up: getWeatherState()/getWeatherStatusText() report why the weather is
// missing and the screens show that instead of values that were never received.
//
// The screens only read through the DataSource interface and never touch a sensor or the network.

#include <Arduino_compat.h>
#include <Mutex.h>
#include <memory>
#include <time.h>

#include "global.h"

namespace WeatherStation2 {

// The HTTP client and the parsed response of the shared/open_weather_map module are only used by
// the request task of WeatherDataSource. The forward declarations keep the HTTP/TLS headers out
// of this header (open_weather_map_client.h includes this file)
namespace OpenWeatherMap {
    class Client;
    struct Data;
}

// TCP client of the remote power monitor server (shared/power_monitor)
namespace PowerMonitor {
    class Client;
    struct Sample;
}

// icon of a weather condition, the screens map it to LVGLUI::IconType
enum class WeatherIcon : uint8_t {
    SUN,
    PARTLY_CLOUDY,
    CLOUDY,
    RAIN,
    SNOW,
    STORM,
    FOG,
    UNKNOWN,
};

// state of the outdoor weather values. The screens show the values only for READY and the
// message of getWeatherStatusText() for everything else - they never display made up values
enum class WeatherState : uint8_t {
    NOT_CONFIGURED = 0, // no API key or coordinates configured
    WAITING,            // configured, no response received yet
    ERROR,              // configured, the last request failed
    READY,              // a response was parsed, the model holds real values
};

struct CurrentWeather {
    bool valid{false};
    WeatherIcon icon{WeatherIcon::UNKNOWN};
    String description;
    String location;
    float temperature{0};
    float feelsLike{0};
    float minTemperature{0};
    float maxTemperature{0};
    float humidity{0};
    float pressure{0};
    float windSpeed{0};
    float rain{0};
    float uvIndex{0};
    // sunrise/sunset as minutes since midnight, -1 if unknown
    int16_t sunRise{-1};
    int16_t sunSet{-1};
};

struct ForecastDay {
    bool valid{false};
    String day;
    WeatherIcon icon{WeatherIcon::UNKNOWN};
    float minTemperature{0};
    float maxTemperature{0};
    float rain{0};
};

// ------------------------------------------------------------------------------------------
// parts of the day of the "1 day" layout of the forecast screen
// ------------------------------------------------------------------------------------------

// The forecast screen shows one card per forecast day. A swipe up/down switches to the four
// parts of the day (morning, noon, afternoon, night) and back. The values and the icon of one
// part of the day are the hourly forecast of its hour of the day (local time of the location), so
// every part has an icon of its own. The hour is fixed, the card shows the name of the part of
// the day and the time the entry belongs to (with the weekday when an hour of the current day is
// already over)
//
// number of parts of the day (morning, noon, afternoon, night)
static constexpr uint8_t kNumDayParts = 4;

// local hour of the day of one part of the day
uint8_t getDayPartHour(uint8_t index);
// label of one part of the day ("Morning", "Noon", "Afternoon", "Night")
const __FlashStringHelper *getDayPartName(uint8_t index);

// one part of the day of the "1 day" layout
struct ForecastSlot {
    bool valid{false};
    // local time of the hourly entry the values were taken from, the weekday is added when it is
    // not the day of the current conditions ("09:00" or "Mon 09:00")
    String time;
    WeatherIcon icon{WeatherIcon::UNKNOWN};
    float temperature{0};
    float feelsLike{0};
    float rain{0};
    // probability of precipitation, 0..1
    float pop{0};
};

// ------------------------------------------------------------------------------------------
// indoor metrics and their sources
// ------------------------------------------------------------------------------------------

// type of the source of one indoor metric
enum class SensorType : uint8_t {
    NONE = 0,   // no source configured, the screens show "--"
    INTERNAL,   // the sensor of the sensor plugin (BME280/BME680/CCS811)
    MQTT,       // the value topic of an MQTT sensor
};

// how the value is extracted from the payload of the value topic
enum class MqttValueType : uint8_t {
    VALUE = 0,      // the payload is the value itself ("25.7")
    JSON_VALUE,     // the payload is a JSON object, the value is the member <key>
};

// state of one indoor metric, the screens map it to a text and a color. The values are prefixed
// on purpose: plain names like DISABLED are macros of the Arduino core / SDK headers
enum class MetricState : uint8_t {
    NO_SOURCE = 0,  // no source configured
    SOURCE_OFFLINE, // the source is configured but not reachable (or it did not deliver)
    NO_VALUE,       // the source is reachable but no value was received yet
    HAS_VALUE,      // value available
};

struct IndoorValue {
    bool configured{false};
    bool online{false};
    bool available{false};
    float value{0};

    MetricState getState() const {
        if (!configured) {
            return MetricState::NO_SOURCE;
        }
        if (!online) {
            return MetricState::SOURCE_OFFLINE;
        }
        return available ? MetricState::HAS_VALUE : MetricState::NO_VALUE;
    }
};

// number of indoor metrics as a preprocessor constant, PROGMEM_DEF_LOCAL_VARNAMES() of the
// configuration form needs an integer literal (the static_assert keeps both in sync)
#define WEATHER_STATION2_NUM_INDOOR_METRICS 4

struct IndoorValues {
    // the configuration strings, the model and the screens use the same order
    enum class Metric : uint8_t {
        TEMPERATURE = 0,
        HUMIDITY,
        PRESSURE,
        ECO2,
    };
    static constexpr uint8_t kNumMetrics = WEATHER_STATION2_NUM_INDOOR_METRICS;

    IndoorValue values[kNumMetrics];

    const IndoorValue &get(Metric metric) const {
        return values[static_cast<uint8_t>(metric)];
    }
    IndoorValue &get(Metric metric) {
        return values[static_cast<uint8_t>(metric)];
    }
};

// ------------------------------------------------------------------------------------------
// power monitor channels
// ------------------------------------------------------------------------------------------
//
// The power screen displays one channel at a time (RD6006 style) and a selector switches
// between the configured channels. A channel is either the local INA219 of the sensor plugin or
// one channel of a remote server that speaks the rpi-power-monitor TCP protocol
// (https://github.com/sascha432/rpi-power-monitor). The remote host/port is shared by all remote
// channels, the channel id (1..3 for the rails of an INA3221) selects which one is displayed.
//
// The model carries the total energy only: the local INA219 has no counter at all and the remote
// server's "energy since this run" counter is ignored, only its persistent total is used.

// type of the source of one power channel
enum class PowerSourceType : uint8_t {
    NONE = 0,   // not configured, the screen shows no channel
    LOCAL,      // the local INA219 sensor of the sensor plugin (1 channel)
    REMOTE,     // one channel of the TCP power monitor server
};

// number of power channels as a preprocessor constant, PROGMEM_DEF_LOCAL_VARNAMES() of the
// configuration form needs an integer literal (the static_assert keeps both in sync)
#define WEATHER_STATION2_NUM_POWER_CHANNELS 4

// latest sample of one power channel. The energy is the total (persistent) counter of the source,
// the local INA219 has none (hasEnergy stays false)
struct PowerValues {
    bool configured{false};
    bool online{false};     // the source is connected/available
    bool available{false};  // at least one sample was received
    PowerSourceType source{PowerSourceType::NONE};
    // remote channel id of the source (PowerSourceType::REMOTE only)
    uint32_t remoteChannelId{0};
    float voltage{0};       // V
    float current{0};       // A
    float power{0};         // W
    bool hasEnergy{false};  // the source has an energy counter
    double energy{0};       // kWh, total counter

    // same states as an indoor metric, the screens map them to a text and a color
    MetricState getState() const {
        if (!configured) {
            return MetricState::NO_SOURCE;
        }
        if (!online) {
            return MetricState::SOURCE_OFFLINE;
        }
        return available ? MetricState::HAS_VALUE : MetricState::NO_VALUE;
    }
};

struct PowerChannels {
    static constexpr uint8_t kNumChannels = WEATHER_STATION2_NUM_POWER_CHANNELS;

    PowerValues values[kNumChannels];

    const PowerValues &get(uint8_t index) const {
        return values[index];
    }
    PowerValues &get(uint8_t index) {
        return values[index];
    }
    // number of configured channels (PowerSourceType::NONE is skipped)
    uint8_t getCount() const {
        uint8_t count = 0;
        for (const auto &value : values) {
            if (value.configured) {
                count++;
            }
        }
        return count;
    }
    // index of the n-th configured channel, -1 if there is none
    int8_t getConfiguredIndex(uint8_t number) const {
        uint8_t index = 0;
        for (uint8_t i = 0; i < kNumChannels; i++) {
            if (values[i].configured) {
                if (index == number) {
                    return static_cast<int8_t>(i);
                }
                index++;
            }
        }
        return -1;
    }
};

// ------------------------------------------------------------------------------------------
// power channel configuration helpers, shared by the data source and the "Power Monitor" group
// of the configuration form
// ------------------------------------------------------------------------------------------

// name of a source type ("none", "local INA219", "remote TCP")
const __FlashStringHelper *getPowerSourceTypeName(PowerSourceType type);
// built-in default of one power channel, used while the configuration is empty
const __FlashStringHelper *getDefaultPowerChannel(uint8_t index);
// One part of the configured channel string, the built-in default is used while the string is
// empty. The parts are
//   0 = source type ("none"|"local"|"remote")
//   1 = display name of the channel
//   2 = remote channel id (the wire id of the rpi-power-monitor server)
String getPowerChannelPart(uint8_t index, uint8_t part);
// replaces one part of the channel string and stores it (see getPowerChannelPart for the parts)
void setPowerChannelPart(uint8_t index, uint8_t part, const String &value);
// remote server of the remote channels, defaults to the reference installation 192.168.0.4:7000
// while the configuration is empty. The port defaults to 7000 for an invalid value
String getPowerRemoteHost();
uint16_t getPowerRemotePort();

// Window of the power graph in minutes, stored as a bitfield in the weather configuration
// (WeatherStationConfig::Config_t::graph_minutes). The constants here are the range the screen
// allocates for and are checked against the configuration with a static_assert
static constexpr uint8_t kMinPowerGraphMinutes = 1;
static constexpr uint8_t kMaxPowerGraphMinutes = 60;
static constexpr uint8_t kDefaultPowerGraphMinutes = 5;

// ------------------------------------------------------------------------------------------
// source helpers, shared by the data source and the "Sensors" group of the configuration form
// ------------------------------------------------------------------------------------------

// title of a metric ("Temperature", "Humidity", "Pressure", "eCO2")
const __FlashStringHelper *getMetricTitle(IndoorValues::Metric metric);
// name of a source type ("none", "internal", "mqtt")
const __FlashStringHelper *getSensorTypeName(SensorType type);
// the built-in default source of a metric, used while the configuration string is empty
const __FlashStringHelper *getDefaultSource(IndoorValues::Metric metric);
// One part of the configured source string of a metric, the built-in default is used while the
// string is empty. The parts are
//   0 = source type ("none"|"internal"|"mqtt")
//   1 = status topic (availability of a Home Assistant compatible sensor)
//   2 = value topic
//   3 = value ("value" or "json_value:<key>")
String getSourcePart(IndoorValues::Metric metric, uint8_t part);
// replaces one part of the source string and stores it (see getSourcePart for the part numbers)
void setSourcePart(IndoorValues::Metric metric, uint8_t part, const String &value);

struct MoonInfo {
    struct Phase {
        // the name is stored in flash (PROGMEM), it is not owned by the model. Read it with a
        // flash aware reader or copy it into a String where a C string is required
        const __FlashStringHelper *name{nullptr};
        String dateTime;
    };
    static constexpr uint8_t kNumPhases = 4;

    bool valid{false};
    // illumination 0 = new moon, 0.5 = half, 1 = full
    float illumination{0};
    bool waxing{true};
    // moon age in days (0..29.53)
    float age{0};
    String phase;
    Phase phases[kNumPhases];
};

struct SystemInfo {
    String hostname;
    String ssid;
    String ip;
    String gateway;
    String dns1;
    String dns2;
    String firmware;
    String sensors;
    uint32_t uptime{0};
    uint32_t freeHeap{0};
    uint32_t freePsram{0};
    int16_t rssi{0};
};

// values for the screens, updated once per second from the main loop
class DataSource {
public:
    // number of forecast days the model can hold
    static constexpr uint8_t kMaxForecastDays = 5;

    virtual ~DataSource() = default;

    // refreshes the values, called once per second
    virtual void update() = 0;

    // called from the plugin setup, after the core and the configuration are initialized.
    // The constructor must not read the configuration, the ESP or the WiFi: the plugin
    // instance is a global object, so its constructor runs from the global constructors,
    // long before setup() - reading the configuration there crashes with LoadProhibited
    // in Configuration::_findParam
    virtual void begin() {}

    // re-reads the configuration, called after the settings form was saved
    virtual void reconfigure() {}

    // name of the source, shown in the status output and on the info screen
    virtual const __FlashStringHelper *getName() const = 0;

    const CurrentWeather &getCurrent() const {
        return _current;
    }
    const ForecastDay *getForecast() const {
        return _forecast;
    }
    uint8_t getForecastCount() const {
        return _forecastCount;
    }
    // the four parts of the day of the "1 day" layout of the forecast screen. A slot without
    // hourly data has valid == false and the screen shows no value for it
    const ForecastSlot *getDayParts() const {
        return _dayParts;
    }
    const IndoorValues &getIndoor() const {
        return _indoor;
    }
    const PowerChannels &getPower() const {
        return _power;
    }
    // Window of the power graph in minutes (1..60). The value is cached by the data source -
    // reading the configuration on every screen refresh (5 fps) would hit the flash/NVS
    uint8_t getPowerGraphMinutes() const {
        return _powerGraphMinutes;
    }
    const MoonInfo &getMoon() const {
        return _moon;
    }
    const SystemInfo &getSystem() const {
        return _system;
    }

    bool isMetric() const {
        return _metric;
    }
    bool isTimeFormat24h() const {
        return _timeFormat24h;
    }

    // state of the outdoor weather (main and forecast screens). READY means _current holds real
    // values, every other state means "show an explanation instead of values"
    virtual WeatherState getWeatherState() const {
        return _current.valid ? WeatherState::READY : WeatherState::WAITING;
    }
    // error of the last request, empty while it succeeded or no request was made
    virtual String getWeatherError() const {
        return String();
    }
    // message for the user while getWeatherState() != READY, empty for READY
    String getWeatherStatusText() const;
    // state of the remote power monitor connection for the status line of the power screen
    // ("connected, 120 samples" / "not connected: connect failed"), empty without a remote source
    virtual String getPowerRemoteStatus() const {
        return String();
    }

    // formatting, the units belong to the source (configuration), not to the screens
    String formatTemperature(float value) const;
    String formatHumidity(float value) const;
    String formatPressure(float value) const;
    String formatWind(float value) const;
    String formatRain(float value) const;
    String formatEco2(float value) const;
    // text of one indoor metric: the formatted value, "offline" or "--"
    String formatIndoorValue(IndoorValues::Metric metric, const IndoorValue &value) const;
    // Power values. The readout cards draw the unit separately, so these return the bare number.
    // A channel without available data returns "--" (the screens show it in a muted color)
    String formatVoltage(float value) const;   // 2 decimals, V
    String formatCurrent(float value) const;   // 3 decimals, A
    String formatPower(float value) const;     // 2 decimals, W
    String formatEnergy(double value) const;   // 4 decimals, kWh
    // state text of a power channel for the status line ("--", "offline", "no data", "")
    String getPowerStateText(const PowerValues &value) const;
    String formatIllumination(float value) const;
    String formatAge(float value) const;
    // UV index has no unit, one decimal is enough
    String formatUvIndex(float value) const;
    // minutes since midnight as HH:MM
    String formatTimeOfDay(int16_t minutes) const;
    static String formatUptime(uint32_t uptime);

#if DEBUG_LVGL_SCREENSHOT
    // applies one "key:value" pair of the debug screenshot feature. The model implements the
    // keys, so pushed values work with any source. Returns false for unknown keys
    virtual bool debugSetValue(const String &key, const String &value);
    // the last "key:value" that arrived and how many arrived at all (diagnostics for the status
    // page of the plugin)
    String getDebugSetInfo() const;
    // The debug screen of the lvgl plugin can only switch screens, so the page of the Home
    // Assistant dashboard is pushed with "set=hasspage:10" (0xff = nothing requested). The
    // dashboard reads the value and clears it
    uint8_t getDebugHassPage() const {
        return _debugHassPage;
    }
    void clearDebugHassPage() {
        _debugHassPage = 0xff;
    }
    // The fullscreen image of a picture tile is opened by a tap on the tile, which cannot be
    // reached over the network: "set=hassfull:46" opens the image of tile 46 (the same key of a
    // tile whose image is open closes it again), 0xff = nothing requested
    uint8_t getDebugHassFullscreen() const {
        return _debugHassFullscreen;
    }
    void clearDebugHassFullscreen() {
        _debugHassFullscreen = 0xff;
    }
    // The panel of a light, dimmer or climate tile is opened by a tap on the tile: "hasspanel:3"
    // opens the panel of tile 3 (the same effect as the tap, the same tile closes it again),
    // 0xff = nothing requested
    uint8_t getDebugHassPanel() const {
        return _debugHassPanel;
    }
    void clearDebugHassPanel() {
        _debugHassPanel = 0xff;
    }
    // The range of the history graph is selected with the buttons of the sensor panel:
    // "hassrange:48" does the same, 0xff = nothing requested
    uint8_t getDebugHassRange() const {
        return _debugHassRange;
    }
    void clearDebugHassRange() {
        _debugHassRange = 0xff;
    }
    // The quick settings sheet of the dashboard is opened with a swipe, which cannot be reached
    // over the network: "hasssettings:1" opens it (0 = close it again, 1 = the tiles and 2..4 = the
    // editor of a setting), 0xff = nothing requested
    uint8_t getDebugHassSettings() const {
        return _debugHassSettings;
    }
    void clearDebugHassSettings() {
        _debugHassSettings = 0xff;
    }
    // control of the panel of an open tile ("hassview"), 0xff = nothing requested
    uint8_t getDebugHassView() const {
        return _debugHassView;
    }
    void clearDebugHassView() {
        _debugHassView = 0xff;
    }
    // The orientation of the dashboard needs a reboot when it is changed in the web form:
    // "hassrot:<0..3>" applies it without one (the value is not stored, see HassScreen)
    uint8_t getDebugHassRotation() const {
        return _debugHassRotation;
    }
    void clearDebugHassRotation() {
        _debugHassRotation = 0xff;
    }
#endif

protected:
    // host name, firmware, uptime, heap, PSRAM and the WiFi state (real values when available)
    void updateSystemInfo();
    // reads the settings of the weather configuration (units, time format, location)
    void readSettings();
    // fills the moon values with the real calculation (Meeus), see shared/moon_phase. The
    // timestamp is UTC, the offset is the one of the location
    void updateMoonValues(time_t utc, int32_t timezoneOffset);
    // true while the debug screenshot feature holds the pushed values
    bool isDebugFrozen() const;
    // true once the clock was set (NTP), the moon and the weekday names need it
    static bool hasValidTime(time_t utc);

protected:
    CurrentWeather _current;
    ForecastDay _forecast[kMaxForecastDays];
    uint8_t _forecastCount{0};
    // parts of the day of the "1 day" layout of the forecast screen
    ForecastSlot _dayParts[kNumDayParts];
    IndoorValues _indoor;
#if DEBUG_LVGL_SCREENSHOT
    uint16_t _debugSetCount{0};
    String _debugLastSet;
#endif
    PowerChannels _power;
    // cached by _readPowerSources(), see getPowerGraphMinutes()
    uint8_t _powerGraphMinutes{kDefaultPowerGraphMinutes};
    MoonInfo _moon;
    SystemInfo _system;
    // UTC offset of the location in seconds, used to format the moon phases. It starts with the
    // offset of the device and is replaced by the offset the OpenWeatherMap response reports
    // (that one belongs to the coordinates, not to the device)
    int32_t _timezoneOffset{0};
    bool _metric{true};
    bool _timeFormat24h{true};
#if DEBUG_LVGL_SCREENSHOT
    // stops the periodic update so pushed values stay on screen while tuning
    bool _debugFrozen{false};
    // channel the debug keys pwrv/pwra/pwrw/pwre write to, "pwrch" selects it
    uint8_t _debugPowerChannel{0};
    // page the Home Assistant dashboard has to open ("hasspage"), 0xff = none
    uint8_t _debugHassPage{0xff};
    // tile whose fullscreen image is opened or closed ("hassfull"), 0xff = none
    uint8_t _debugHassFullscreen{0xff};
    // tile whose panel is opened ("hasspanel"), 0xff = none
    uint8_t _debugHassPanel{0xff};
    // control of the panel of an open tile ("hassview"), 0xff = none
    uint8_t _debugHassView{0xff};
    // range of the history graph of the sensor panel ("hassrange", 12, 24 or 48 hours), 0xff = none
    uint8_t _debugHassRange{0xff};
    uint8_t _debugHassSettings{0xff};
    uint8_t _debugHassRotation{0xff};
#endif
};

// The real source: OpenWeatherMap for the outdoor weather, the Meeus calculation for the moon,
// the WiFi/ESP/system configuration for the info screen.
//
// The model starts empty and is only filled from real data. The screens ask getWeatherState()
// whether the values exist and show getWeatherStatusText() (not configured, waiting, error)
// while they do not. The moon needs no configuration at all, it is real as soon as the clock
// was set.
class WeatherDataSource : public DataSource {
public:
    // interval of the moon calculation (the phase instants are displayed with minutes)
    static constexpr uint32_t kMoonInterval = 60 * 1000;
    // wait before a failed request is retried
    static constexpr uint32_t kRetryInterval = 60 * 1000;
    // the power channels are copied into the model at this rate (5 fps, the power screen reads at
    // the same rate). Both sources deliver that fast: the local INA219 samples every
    // IOT_SENSOR_INA219_READ_INTERVAL (68 ms) and the rpi-power-monitor server sends a sample every
    // averaging * (bus + shunt conversion) = ~133 ms with the shipped configuration
    static constexpr uint32_t kPowerUpdateInterval = 200;
    // stack of the request task, the TLS handshake runs on it
    static constexpr uint32_t kTaskStack = 12288;

    WeatherDataSource();
    virtual ~WeatherDataSource();

    virtual void begin() override;
    virtual void update() override;
    virtual void reconfigure() override;
    virtual const __FlashStringHelper *getName() const override {
        return F("OpenWeatherMap");
    }
    virtual WeatherState getWeatherState() const override;
    virtual String getWeatherError() const override;
    virtual String getPowerRemoteStatus() const override;

    // error of the last request, empty while it succeeded (status output)
    String getLastError() const;
    // number of requests since the boot
    uint32_t getRequestCount() const;
    // true while an API key and coordinates are configured
    bool isConfigured() const;
    // true while the request task is running
    bool isRunning() const {
        return _task != nullptr;
    }

    // --- MQTT sensors ---------------------------------------------------------------------
    // The plugin registers itself as an MQTT::Component and forwards the events to the data
    // source, which owns the topics and the parsed values:
    //
    //   onConnect()       -> mqttConnected() + getMqttTopics() -> subscribe()
    //   onMessage()       -> mqttMessage()
    //   onDisconnect()    -> mqttDisconnected()
    //
    // The MQTT callbacks run in the WiFi/TCP task, so the data only stores the payloads there
    // (under _lock) and the main loop maps them into the model.

    // the connection is up, all metrics lose their "offline" state until the status topic
    // reports otherwise. The plugin has to subscribe to getMqttTopics() afterwards
    void mqttConnected();
    // the connection is down, every MQTT metric is offline
    void mqttDisconnected();
    // one message of a subscribed topic
    void mqttMessage(const char *topic, const char *payload, size_t len);
    // topics of the configured MQTT sources, without duplicates (called by the plugin)
    void getMqttTopics(StringVector &topics) const;
    // true while the MQTT client is connected
    bool isMqttConnected() const {
        return _mqttConnected;
    }

    // --- power monitor --------------------------------------------------------------------
    // The remote channels are read by a TCP client that runs in its own task (see
    // shared/power_monitor), the local INA219 is read by update(). The task is started by
    // begin() and stopped by stopPowerMonitor() (the plugin shutdown)
    void stopPowerMonitor();

private:
    // entry point of the request task
    static void _taskEntry(void *arg);
    // waits for the poll interval, performs one request and stores the parsed response
    void _requestLoop();
    // takes a finished response and maps it into the model (main loop)
    bool _applyResponse();
    // copies the settings into caller supplied buffers, returns the poll interval in milliseconds
    // or 0 while no API key/coordinates are configured (request task)
    uint32_t _readSettings(char *apiKey, size_t apiKeySize, float &latitude, float &longitude);
    // reads the API key, the coordinates and the poll interval from the configuration (main
    // loop), without this the request task never starts and isConfigured() stays false
    void _readApiSettings();
    // parses the source strings of the indoor metrics, called when the configuration is read
    // (main loop). Rebuilds _mqttTopics, the plugin subscribes to them
    void _readIndoorSources();
    // maps the received MQTT values and the internal sensor into the model (main loop)
    void _applyIndoorValues();
    // short description of the configured sources, shown by the indoor screen
    String _createSensorDescription() const;
    // parses the power channel configuration (main loop), sets the target of the remote client
    void _readPowerSources();
    // maps the samples of the remote client and the local INA219 into the model (main loop)
    void _applyPowerValues();

private:
    // one source of an indoor metric
    struct MqttSource {
        SensorType type{SensorType::NONE};
        String statusTopic;                     // empty = no availability topic
        String valueTopic;
        MqttValueType valueType{MqttValueType::VALUE};
        String key;                             // JSON_VALUE only
        // written by the MQTT task, read by the main loop, both under _lock
        int8_t availability{-1};                // -1 unknown, 0 offline, 1 online
        bool valueReceived{false};
        float value{0};
    };
    MqttSource _mqtt[IndoorValues::kNumMetrics];
    // topics of all MQTT sources, without duplicates, rebuilt by _readIndoorSources()
    StringVector _mqttTopics;
    // set by mqttConnected()/mqttDisconnected(), read by the main loop
    volatile bool _mqttConnected{false};

    // one configured power channel, rebuilt by _readPowerSources() (main loop only)
    struct PowerSource {
        PowerSourceType type{PowerSourceType::NONE};
        String name;
        uint32_t remoteChannelId{0};
    };
    PowerSource _powerSources[PowerChannels::kNumChannels];
    // the reader task of the remote channels, created by begin()
    std::unique_ptr<PowerMonitor::Client> _powerClient;

    std::unique_ptr<OpenWeatherMap::Client> _client;
    // written by the request task, applied by the main loop, both under _lock
    std::unique_ptr<OpenWeatherMap::Data> _received;
    mutable SemaphoreMutex _lock;
    volatile bool _receivedValid{false};
    // set by reconfigure() to request immediately instead of waiting for the poll interval
    volatile bool _requestNow{false};
    // request of the task to end (checked between two requests)
    volatile bool _stop{false};
    void *_task{nullptr};
    // settings, written by the main loop and read by the request task through _readSettings()
    String _apiKey;
    float _latitude{0};
    float _longitude{0};
    uint32_t _pollInterval{0};
    // updated by the request task, read by getLastError()/getStatus()
    String _lastError;
    uint32_t _requestCount{0};
    // main loop only
    uint32_t _lastUpdate{0};
    uint32_t _lastPowerUpdate{0};
    uint32_t _lastMoonUpdate{0};
};

} // namespace WeatherStation2
