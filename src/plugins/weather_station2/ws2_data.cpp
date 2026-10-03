/**
 * Author: sascha_lammers@gmx.de
 */

#include "ws2_data.h"

#include <algorithm>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <WiFi.h>
#include <Mutex.h>
#include <StrView.h>
#include <kfc_fw_config.h>
#include "shared/moon_phase/moon_phase_model.h"
#include "shared/open_weather_map/open_weather_map_client.h"
#include "shared/power_monitor/power_monitor_client.h"

// The internal sensor is optional, the plugin uses the driver of the sensor plugin when the env
// compiles one (see the "internal" source type and the local power channel)
#if IOT_SENSOR_HAVE_BME280 || IOT_SENSOR_HAVE_BME680 || IOT_SENSOR_HAVE_CCS811 || IOT_SENSOR_HAVE_INA219
#    include "../sensor/sensor.h"
#    if IOT_SENSOR_HAVE_BME280
#        include "../sensor/Sensor_BME280.h"
#    endif
#    if IOT_SENSOR_HAVE_BME680
#        include "../sensor/Sensor_BME680.h"
#    endif
#    if IOT_SENSOR_HAVE_CCS811
#        include "../sensor/Sensor_CCS811.h"
#    endif
#    if IOT_SENSOR_HAVE_INA219
#        include "../sensor/Sensor_INA219.h"
#    endif
#endif

#ifndef DEBUG_WEATHER_STATION2
#    define DEBUG_WEATHER_STATION2 0
#endif

#if DEBUG_WEATHER_STATION2
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

namespace WeatherStation2 {

// ------------------------------------------------------------------------------------------
// formatting
// ------------------------------------------------------------------------------------------
// formats one value into the buffer of the caller (the String versions below delegate to it)
static void _formatValue(const char *format, double value, char *output, size_t size)
{
    snprintf(output, size, format, value);
}

// copies a text into the buffer of the caller, always NUL terminated
static void _copyText(char *output, size_t size, const char *text)
{
    if (!size) {
        return;
    }
    strncpy(output, text, size - 1);
    output[size - 1] = 0;
}

void DataSource::formatTemperature(float value, char *output, size_t size) const
{
    _formatValue(_metric ? "%.1f \xC2\xB0""C" : "%.1f \xC2\xB0""F",
                 _metric ? value : (value * 9.0f / 5.0f + 32.0f), output, size);
}

String DataSource::formatTemperature(float value) const
{
    char buffer[kFormatSize];
    formatTemperature(value, buffer, sizeof(buffer));
    return String(buffer);
}

void DataSource::formatHumidity(float value, char *output, size_t size) const
{
    _formatValue("%.1f %%", value, output, size);
}

String DataSource::formatHumidity(float value) const
{
    char buffer[kFormatSize];
    formatHumidity(value, buffer, sizeof(buffer));
    return String(buffer);
}

void DataSource::formatPressure(float value, char *output, size_t size) const
{
    _formatValue(_metric ? "%.1f hPa" : "%.2f inHg", _metric ? value : (value / 33.8639f), output, size);
}

String DataSource::formatPressure(float value) const
{
    char buffer[kFormatSize];
    formatPressure(value, buffer, sizeof(buffer));
    return String(buffer);
}

void DataSource::formatWind(float value, char *output, size_t size) const
{
    _formatValue(_metric ? "%.1f km/h" : "%.1f mph", _metric ? value : (value * 0.621371f), output, size);
}

String DataSource::formatWind(float value) const
{
    char buffer[kFormatSize];
    formatWind(value, buffer, sizeof(buffer));
    return String(buffer);
}

void DataSource::formatRain(float value, char *output, size_t size) const
{
    _formatValue(_metric ? "%.1f mm" : "%.2f in", _metric ? value : (value / 25.4f), output, size);
}

String DataSource::formatRain(float value) const
{
    char buffer[kFormatSize];
    formatRain(value, buffer, sizeof(buffer));
    return String(buffer);
}

void DataSource::formatEco2(float value, char *output, size_t size) const
{
    _formatValue("%.0f ppm", value, output, size);
}

String DataSource::formatEco2(float value) const
{
    char buffer[kFormatSize];
    formatEco2(value, buffer, sizeof(buffer));
    return String(buffer);
}

void DataSource::formatIllumination(float value, char *output, size_t size) const
{
    _formatValue("%.1f %%", value * 100.0f, output, size);
}

String DataSource::formatIllumination(float value) const
{
    char buffer[kFormatSize];
    formatIllumination(value, buffer, sizeof(buffer));
    return String(buffer);
}

void DataSource::formatAge(float value, char *output, size_t size) const
{
    _formatValue("%.1f days", value, output, size);
}

String DataSource::formatAge(float value) const
{
    char buffer[kFormatSize];
    formatAge(value, buffer, sizeof(buffer));
    return String(buffer);
}

void DataSource::formatUvIndex(float value, char *output, size_t size) const
{
    _formatValue("%.1f", value, output, size);
}

String DataSource::formatUvIndex(float value) const
{
    char buffer[kFormatSize];
    formatUvIndex(value, buffer, sizeof(buffer));
    return String(buffer);
}

void DataSource::formatIndoorValue(IndoorValues::Metric metric, const IndoorValue &value, char *output, size_t size) const
{
    switch (value.getState()) {
    case MetricState::HAS_VALUE:
        switch (metric) {
        case IndoorValues::Metric::TEMPERATURE:
            formatTemperature(value.value, output, size);
            return;
        case IndoorValues::Metric::HUMIDITY:
            formatHumidity(value.value, output, size);
            return;
        case IndoorValues::Metric::PRESSURE:
            formatPressure(value.value, output, size);
            return;
        default:
            formatEco2(value.value, output, size);
            return;
        }
    case MetricState::SOURCE_OFFLINE:
        // the source is configured but it does not deliver (no MQTT connection, availability
        // topic says offline or the sensor is not available)
        _copyText(output, size, "offline");
        return;
    default:
        // no source configured, or the source is online but sent no value yet
        _copyText(output, size, "--");
        return;
    }
}

String DataSource::formatIndoorValue(IndoorValues::Metric metric, const IndoorValue &value) const
{
    char buffer[kFormatSize];
    formatIndoorValue(metric, value, buffer, sizeof(buffer));
    return String(buffer);
}

// The readout cards of the power screen draw the unit separately, so these return the bare
// number. The precision matches the sensor resolution (INA219/INA3221 measure mV/mA). The buffer
// version is the one the power screen uses, the String version only the status output
void DataSource::formatVoltage(float value, char *output, size_t size) const
{
    snprintf(output, size, "%.2f", static_cast<double>(value));
}

String DataSource::formatVoltage(float value) const
{
    char buffer[24];
    formatVoltage(value, buffer, sizeof(buffer));
    return String(buffer);
}

void DataSource::formatCurrent(float value, char *output, size_t size) const
{
    snprintf(output, size, "%.3f", static_cast<double>(value));
}

String DataSource::formatCurrent(float value) const
{
    char buffer[24];
    formatCurrent(value, buffer, sizeof(buffer));
    return String(buffer);
}

void DataSource::formatPower(float value, char *output, size_t size) const
{
    snprintf(output, size, "%.2f", static_cast<double>(value));
}

String DataSource::formatPower(float value) const
{
    char buffer[24];
    formatPower(value, buffer, sizeof(buffer));
    return String(buffer);
}

void DataSource::formatEnergy(double value, char *output, size_t size) const
{
    // a double on purpose, the total counter of the server can become large
    snprintf(output, size, "%.4f", value);
}

String DataSource::formatEnergy(double value) const
{
    char buffer[40];
    formatEnergy(value, buffer, sizeof(buffer));
    return String(buffer);
}

const char *DataSource::getPowerStateText(const PowerValues &value) const
{
    switch (value.getState()) {
    case MetricState::SOURCE_OFFLINE:
        return "offline";
    case MetricState::NO_VALUE:
        return "no data";
    default:
        return "";
    }
}

void DataSource::formatTimeOfDay(int16_t minutes, char *output, size_t size) const
{
    if (minutes < 0) {
        _copyText(output, size, "--:--");
        return;
    }
    snprintf(output, size, "%02u:%02u", static_cast<unsigned>(minutes / 60), static_cast<unsigned>(minutes % 60));
}

String DataSource::formatTimeOfDay(int16_t minutes) const
{
    char buffer[kFormatSize];
    formatTimeOfDay(minutes, buffer, sizeof(buffer));
    return String(buffer);
}

void DataSource::formatUptime(uint32_t uptime, char *output, size_t size)
{
    auto seconds = uptime % 60;
    auto minutes = (uptime / 60) % 60;
    auto hours = (uptime / 3600) % 24;
    auto days = uptime / 86400;
    snprintf(output, size, "%ud %02u:%02u:%02u", static_cast<unsigned>(days), static_cast<unsigned>(hours),
             static_cast<unsigned>(minutes), static_cast<unsigned>(seconds));
}

String DataSource::formatUptime(uint32_t uptime)
{
    char buffer[kFormatSize];
    formatUptime(uptime, buffer, sizeof(buffer));
    return String(buffer);
}

String DataSource::getWeatherStatusText() const
{
    String text;
    getWeatherStatusText(text);
    return text;
}

void DataSource::getWeatherStatusText(String &output) const
{
    // The screens show this while getWeatherState() != READY, i.e. instead of values that were
    // never received. The line break keeps the reason separate from the error
    switch (getWeatherState()) {
    case WeatherState::NOT_CONFIGURED:
        output += "Weather station not configured\nthe API key or the location is missing";
        break;
    case WeatherState::ERROR: {
        output += "Weather data unavailable";
        const auto error = getWeatherError();
        if (error.length()) {
            output += "\n";
            output += error;
        }
        break;
    }
    case WeatherState::WAITING:
        output += "Waiting for weather data";
        break;
    default:
        break;
    }
}

// ------------------------------------------------------------------------------------------
// parts of the day of the "1 day" layout of the forecast screen
// ------------------------------------------------------------------------------------------
// The hours are local hours of the location, the screens display the actual time of the hourly
// entry the values were taken from (see OpenWeatherMap::applyDayParts)

uint8_t getDayPartHour(uint8_t index)
{
    switch (index) {
    case 0:
        return 6; // morning
    case 1:
        return 12; // noon
    case 2:
        return 15; // afternoon
    default:
        return 21; // night
    }
}

const __FlashStringHelper *getDayPartName(uint8_t index)
{
    switch (index) {
    case 0:
        return F("Morning");
    case 1:
        return F("Noon");
    case 2:
        return F("Afternoon");
    default:
        return F("Night");
    }
}

// ------------------------------------------------------------------------------------------
// indoor metric sources: configuration helpers
// ------------------------------------------------------------------------------------------
// The source of every indoor metric is one configuration string, see the "Sensors" group of
// the weather2 form and WeatherStationConfigNS::WeatherStation:
//
//   none
//   internal
//   mqtt:<status topic>,<value topic>,<value|json_value:<key>>

using Plugins = KFCConfigurationClasses::PluginsType;

const __FlashStringHelper *getMetricTitle(IndoorValues::Metric metric)
{
    switch (metric) {
    case IndoorValues::Metric::TEMPERATURE:
        return F("Temperature");
    case IndoorValues::Metric::HUMIDITY:
        return F("Humidity");
    case IndoorValues::Metric::PRESSURE:
        return F("Pressure");
    default:
        return F("eCO2");
    }
}

const __FlashStringHelper *getSensorTypeName(SensorType type)
{
    switch (type) {
    case SensorType::INTERNAL:
        return F("internal");
    case SensorType::MQTT:
        return F("mqtt");
    default:
        return F("none");
    }
}

const __FlashStringHelper *getDefaultSource(IndoorValues::Metric metric)
{
    // Home Assistant style sensors, the values are the reference installation the plugin was
    // developed with. They are used while the matching configuration string is empty
    switch (metric) {
    case IndoorValues::Metric::TEMPERATURE:
        return F("mqtt:home/bme280_living_room/status,home/bme280_living_room/bme280_0x76,json_value:temperature");
    case IndoorValues::Metric::HUMIDITY:
        return F("mqtt:home/bme280_living_room/status,home/bme280_living_room/bme280_0x76,json_value:humidity");
    case IndoorValues::Metric::PRESSURE:
        return F("mqtt:home/bme280_living_room/status,home/bme280_living_room/bme280_0x76,json_value:pressure");
    default:
        return F("mqtt:home/hexagon_wall_panel/status,home/hexagon_wall_panel/ccs811_0x5a,json_value:eCO2");
    }
}

// the stored source string of a metric, nullptr or empty while nothing was configured
static const char *_getConfiguredSource(IndoorValues::Metric metric)
{
    switch (metric) {
    case IndoorValues::Metric::TEMPERATURE:
        return Plugins::WeatherStation::getTemperatureSource();
    case IndoorValues::Metric::HUMIDITY:
        return Plugins::WeatherStation::getHumiditySource();
    case IndoorValues::Metric::PRESSURE:
        return Plugins::WeatherStation::getPressureSource();
    default:
        return Plugins::WeatherStation::getEco2Source();
    }
}

static void _setConfiguredSource(IndoorValues::Metric metric, const char *value)
{
    switch (metric) {
    case IndoorValues::Metric::TEMPERATURE:
        Plugins::WeatherStation::setTemperatureSource(value);
        break;
    case IndoorValues::Metric::HUMIDITY:
        Plugins::WeatherStation::setHumiditySource(value);
        break;
    case IndoorValues::Metric::PRESSURE:
        Plugins::WeatherStation::setPressureSource(value);
        break;
    default:
        Plugins::WeatherStation::setEco2Source(value);
        break;
    }
}

static void _setConfiguredSource(IndoorValues::Metric metric, const __FlashStringHelper *value)
{
    switch (metric) {
    case IndoorValues::Metric::TEMPERATURE:
        Plugins::WeatherStation::setTemperatureSource(value);
        break;
    case IndoorValues::Metric::HUMIDITY:
        Plugins::WeatherStation::setHumiditySource(value);
        break;
    case IndoorValues::Metric::PRESSURE:
        Plugins::WeatherStation::setPressureSource(value);
        break;
    default:
        Plugins::WeatherStation::setEco2Source(value);
        break;
    }
}

// the configured source string, the built-in default while it is empty
static String _getEffectiveSource(IndoorValues::Metric metric)
{
    const char *configured = _getConfiguredSource(metric);
    if (configured && *configured) {
        return String(configured);
    }
    return String(getDefaultSource(metric));
}

// splits "<status topic>,<value topic>,<value>" into three parts, missing parts stay empty
static void _splitMqttSource(const String &str, String parts[3])
{
    int start = 0;
    for (uint8_t i = 0; i < 3; i++) {
        const auto pos = (i < 2) ? str.indexOf(',', start) : -1;
        if (pos == -1) {
            parts[i] = str.substring(start);
            break;
        }
        parts[i] = str.substring(start, pos);
        start = pos + 1;
    }
}

String getSourcePart(IndoorValues::Metric metric, uint8_t part)
{
    const auto config = _getEffectiveSource(metric);
    if (part == 0) {
        if (config.startsWith(F("mqtt:"))) {
            return String(F("mqtt"));
        }
        return (F("internal") == config) ? String(F("internal")) : String(F("none"));
    }
    if (part > 3 || !config.startsWith(F("mqtt:"))) {
        return String();
    }
    String parts[3];
    _splitMqttSource(config.substring(5), parts);
    return parts[part - 1];
}

void setSourcePart(IndoorValues::Metric metric, uint8_t part, const String &value)
{
    if (part == 0) {
        if (F("mqtt") == value) {
            // keep the configured topics when the type does not change. Without a previous
            // configuration the defaults are used, so selecting MQTT pre-fills the form
            const char *configured = _getConfiguredSource(metric);
            if (configured && StrView(configured).startsWith(F("mqtt:"))) {
                return;
            }
            _setConfiguredSource(metric, getDefaultSource(metric));
            return;
        }
        _setConfiguredSource(metric, (F("internal") == value) ? F("internal") : F("none"));
        return;
    }

    // change one MQTT part, the type and the other parts are kept. The form posts the topic
    // fields even when the metric does not use MQTT (the select is processed first), those
    // changes are ignored
    const char *configured = _getConfiguredSource(metric);
    if (part > 3 || !configured || !StrView(configured).startsWith(F("mqtt:"))) {
        return;
    }
    auto config = String(configured);
    String parts[3];
    _splitMqttSource(config.substring(5), parts);
    parts[part - 1] = value;

    String result = F("mqtt:");
    result += parts[0];
    result += ',';
    result += parts[1];
    result += ',';
    result += parts[2].length() ? parts[2] : String(F("value"));
    _setConfiguredSource(metric, result.c_str());
}

// ------------------------------------------------------------------------------------------
// power channel sources: configuration helpers
// ------------------------------------------------------------------------------------------
// The source of every power channel is one configuration string, see the "Power Monitor" group
// of the weather2 form and WeatherStationConfigNS::WeatherStation:
//
//   none
//   local|<name>|<remote channel id>
//   remote|<name>|<remote channel id>
//
// The remote host and port are shared by all remote channels (getPowerRemoteHost()/Port()).

// the reference installation the plugin was developed with: an rpi-power-monitor server that
// reads an INA3221 with the three rails of the shipped config/server.yaml
static constexpr char kDefaultPowerRemoteHost[] = "192.168.0.4";
static constexpr uint16_t kDefaultPowerRemotePort = 7000;

const __FlashStringHelper *getPowerSourceTypeName(PowerSourceType type)
{
    switch (type) {
    case PowerSourceType::LOCAL:
        return F("local INA219");
    case PowerSourceType::REMOTE:
        return F("remote TCP");
    default:
        return F("none");
    }
}

const __FlashStringHelper *getDefaultPowerChannel(uint8_t index)
{
    switch (index) {
    case 0:
        return F("remote|12V Input|1");
    case 1:
        return F("remote|12V NAS|2");
    case 2:
        return F("remote|5V Output|3");
    default:
        return F("none|Channel 4|4");
    }
}

static const char *_getConfiguredPowerChannel(uint8_t index)
{
    switch (index) {
    case 0:
        return Plugins::WeatherStation::getPowerChannel0();
    case 1:
        return Plugins::WeatherStation::getPowerChannel1();
    case 2:
        return Plugins::WeatherStation::getPowerChannel2();
    default:
        return Plugins::WeatherStation::getPowerChannel3();
    }
}

static void _setConfiguredPowerChannel(uint8_t index, const char *value)
{
    switch (index) {
    case 0:
        Plugins::WeatherStation::setPowerChannel0(value);
        break;
    case 1:
        Plugins::WeatherStation::setPowerChannel1(value);
        break;
    case 2:
        Plugins::WeatherStation::setPowerChannel2(value);
        break;
    default:
        Plugins::WeatherStation::setPowerChannel3(value);
        break;
    }
}

// the configured channel string, the built-in default while it is empty
static String _getEffectivePowerChannel(uint8_t index)
{
    const char *configured = _getConfiguredPowerChannel(index);
    if (configured && *configured) {
        return String(configured);
    }
    return String(getDefaultPowerChannel(index));
}

// splits "<type>|<name>|<channel id>" into three parts, missing parts stay empty
static void _splitPowerChannel(const String &str, String parts[3])
{
    int start = 0;
    for (uint8_t i = 0; i < 3; i++) {
        const auto pos = (i < 2) ? str.indexOf('|', start) : -1;
        if (pos == -1) {
            parts[i] = str.substring(start);
            break;
        }
        parts[i] = str.substring(start, pos);
        start = pos + 1;
    }
}

static String _buildPowerChannel(const String parts[3])
{
    String result = parts[0];
    result += '|';
    result += parts[1];
    result += '|';
    result += parts[2].length() ? parts[2] : String(F("0"));
    return result;
}

String getPowerChannelPart(uint8_t index, uint8_t part)
{
    if (part > 2) {
        return String();
    }
    String parts[3];
    _splitPowerChannel(_getEffectivePowerChannel(index), parts);
    return parts[part];
}

void getPowerChannelPart(uint8_t index, uint8_t part, char *output, size_t size)
{
    if (size) {
        output[0] = 0;
    }
    if (!size || part > 2) {
        return;
    }
    // the configured string, or the built-in default while it is empty
    const char *text = _getConfiguredPowerChannel(index);
    if (!text || !*text) {
        text = flashStringToCStr(getDefaultPowerChannel(index));
    }
    // walk to the requested part (the parts are separated by '|', a missing one is empty)
    for (uint8_t i = 0; i < part; i++) {
        const auto separator = strchr(text, '|');
        if (!separator) {
            return;
        }
        text = separator + 1;
    }
    const auto separator = strchr(text, '|');
    const auto length = separator ? static_cast<size_t>(separator - text) : strlen(text);
    const auto copyLength = (length < size) ? length : size - 1;
    memcpy(output, text, copyLength);
    output[copyLength] = 0;
}

void setPowerChannelPart(uint8_t index, uint8_t part, const String &value)
{
    if (part > 2) {
        return;
    }
    String parts[3];
    _splitPowerChannel(_getEffectivePowerChannel(index), parts);

    if (part == 0) {
        // keep the configured name and channel id while the type does not change. Without a
        // previous configuration the defaults are used, so selecting a type pre-fills the form
        String type;
        if (F("local") == value) {
            type = F("local");
        }
        else if (F("remote") == value) {
            type = F("remote");
        }
        else {
            type = F("none");
        }
        if (type == parts[0]) {
            return;
        }
        parts[0] = type;
        if (parts[1].length() == 0) {
            String defaults[3];
            _splitPowerChannel(String(getDefaultPowerChannel(index)), defaults);
            parts[1] = defaults[1];
            parts[2] = defaults[2];
        }
    }
    else {
        // the form posts the name and the channel id even for a channel that does not use them
        // (the select is processed first), ignore those changes
        if (F("none") == parts[0]) {
            return;
        }
        parts[part] = value;
    }
    _setConfiguredPowerChannel(index, _buildPowerChannel(parts).c_str());
}

String getPowerRemoteHost()
{
    const char *host = Plugins::WeatherStation::getPowerRemoteHost();
    if (host && *host) {
        return String(host);
    }
    return String(kDefaultPowerRemoteHost);
}

uint16_t getPowerRemotePort()
{
    const char *port = Plugins::WeatherStation::getPowerRemotePort();
    const auto value = (port && *port) ? atoi(port) : 0;
    if (value <= 0 || value > 65535) {
        return kDefaultPowerRemotePort;
    }
    return static_cast<uint16_t>(value);
}

// the range the power screen allocates for has to match the configuration
static_assert(kMinPowerGraphMinutes == Plugins::WeatherStation::kPowerGraphMinutesMin, "update kMinPowerGraphMinutes");
static_assert(kMaxPowerGraphMinutes == Plugins::WeatherStation::kPowerGraphMinutesMax, "update kMaxPowerGraphMinutes");
static_assert(kDefaultPowerGraphMinutes == Plugins::WeatherStation::kPowerGraphMinutesDefault, "update kDefaultPowerGraphMinutes");

// ------------------------------------------------------------------------------------------
// payload parsing
// ------------------------------------------------------------------------------------------

// "online"/"on"/"1" = 1, "offline"/"off"/"0" = 0, anything else = -1 (unknown)
static int8_t _parseAvailability(const char *payload)
{
    if (!strcasecmp(payload, "online") || !strcasecmp(payload, "on") || !strcmp(payload, "1")) {
        return 1;
    }
    if (!strcasecmp(payload, "offline") || !strcasecmp(payload, "off") || !strcmp(payload, "0")) {
        return 0;
    }
    return -1;
}

// number at the beginning of the payload, values with a unit ("25.7 C") are accepted
static bool _parseNumber(const char *payload, size_t length, float &value)
{
    (void)length; // the payload of the MQTT client is NUL terminated
    char *end = nullptr;
    value = strtof(payload, &end);
    return end != payload;
}

// Extracts a numeric member from a flat JSON object, e.g. '{"temperature":25.7,"humidity":53.7}'.
// The payload is the buffer of the MQTT client, it is not modified. Nested objects and duplicate
// keys are not supported, the first match wins - the payloads of an MQTT sensor are flat
static bool _parseJsonValue(const char *payload, size_t length, const char *key, float &value)
{
    if (!key || !*key) {
        return false;
    }
    char needle[40];
    const auto keyLength = strlen(key);
    if (keyLength + 3 > sizeof(needle)) {
        return false;
    }
    needle[0] = '"';
    memcpy(needle + 1, key, keyLength);
    needle[keyLength + 1] = '"';
    needle[keyLength + 2] = 0;

    (void)length; // the payload of the MQTT client is NUL terminated
    const char *ptr = strstr(payload, needle);
    if (!ptr) {
        return false;
    }
    ptr += keyLength + 2;
    while (*ptr == ' ' || *ptr == '\t') {
        ptr++;
    }
    if (*ptr != ':') {
        return false;
    }
    ptr++;
    while (*ptr == ' ' || *ptr == '\t') {
        ptr++;
    }
    if (*ptr == '"') {
        ptr++; // a quoted number
    }
    return _parseNumber(ptr, strlen(ptr), value);
}

#if DEBUG_LVGL_SCREENSHOT

static WeatherIcon _iconFromName(const String &value)
{
    if (value.equalsIgnoreCase(F("sun"))) {
        return WeatherIcon::SUN;
    }
    if (value.equalsIgnoreCase(F("partly")) || value.equalsIgnoreCase(F("partly_cloudy"))) {
        return WeatherIcon::PARTLY_CLOUDY;
    }
    if (value.equalsIgnoreCase(F("cloudy"))) {
        return WeatherIcon::CLOUDY;
    }
    if (value.equalsIgnoreCase(F("rain"))) {
        return WeatherIcon::RAIN;
    }
    if (value.equalsIgnoreCase(F("snow"))) {
        return WeatherIcon::SNOW;
    }
    if (value.equalsIgnoreCase(F("storm"))) {
        return WeatherIcon::STORM;
    }
    if (value.equalsIgnoreCase(F("fog"))) {
        return WeatherIcon::FOG;
    }
    if (value.equalsIgnoreCase(F("unknown"))) {
        return WeatherIcon::UNKNOWN;
    }
    return WeatherIcon::UNKNOWN;
}

// The model implements the keys, so a pushed value works with every source - the debug feature
// is about how the screen looks, not about where the value came from.
static void _setDebugIndoorValue(IndoorValues &indoor, IndoorValues::Metric metric, float value)
{
    auto &metricValue = indoor.get(metric);
    metricValue.configured = true;
    metricValue.online = true;
    metricValue.available = true;
    metricValue.value = value;
}

#if DEBUG_LVGL_SCREENSHOT
String DataSource::getDebugSetInfo() const
{
    if (!_debugSetCount) {
        return String(F("none"));
    }
    char count[8];
    snprintf_P(count, sizeof(count), PSTR("%u"), static_cast<unsigned>(_debugSetCount));
    String info(count);
    info += F(": ");
    info += _debugLastSet;
    return info;
}
#endif

bool DataSource::debugSetValue(const String &key, const String &value)
{
#if DEBUG_LVGL_SCREENSHOT
    _debugSetCount++;
    _debugLastSet = key + '=' + value;
#endif
    if (key.equalsIgnoreCase(F("freeze"))) {
        _debugFrozen = (value.toInt() != 0) && !value.equalsIgnoreCase(F("false"));
        return true;
    }
    if (key.equalsIgnoreCase(F("hasspage"))) {
        // page of the Home Assistant dashboard (an area page of /hass.yaml, 0 = the main page)
        _debugHassPage = static_cast<uint32_t>(value.toInt());
        return true;
    }
    if (key.equalsIgnoreCase(F("hassfull"))) {
        // fullscreen image of a picture tile of the Home Assistant dashboard. False: the screen
        // applies the value without rebuilding the widget tree, the image that is open has to
        // stay open (a reload would close it)
        _debugHassFullscreen = static_cast<uint32_t>(value.toInt());
        return false;
    }
    if (key.equalsIgnoreCase(F("hasspanel"))) {
        // panel of a light, dimmer, climate or sensor tile of the Home Assistant dashboard (the same
        // effect as a tap on that tile). False: the screen applies the value without rebuilding the
        // widget tree, else the reload would close the panel again - the tile whose panel is open
        // closes it with the same key
        _debugHassPanel = static_cast<uint32_t>(value.toInt());
        return false;
    }
    if (key.equalsIgnoreCase(F("hassrange"))) {
        // range of the history graph of the sensor panel (12, 24 or 48 hours), the same effect as
        // a tap on one of the three buttons of the panel. False: the screen applies the value
        // without rebuilding the widget tree
        _debugHassRange = static_cast<uint8_t>(value.toInt());
        return false;
    }
    if (key.equalsIgnoreCase(F("hasssettings"))) {
        // quick settings sheet of the Home Assistant dashboard (0 = close it, 1 = the tiles,
        // 2..4 = the editor of the idle brightness, the idle timeout and the standby timeout).
        // False: the screen builds the sheet itself, a reload would close it again
        _debugHassSettings = static_cast<uint8_t>(value.toInt());
        return false;
    }
    if (key.equalsIgnoreCase(F("hassview"))) {
        // control the panel of an open tile shows (0 = the level slider of a light, the arc of a
        // climate, 1..3 = the color wheel, the colour temperature and the effect list / the three
        // option lists of a climate), the same effect as a tap on the button of that control.
        // False: the screen applies the value without rebuilding the widget tree
        _debugHassView = static_cast<uint8_t>(value.toInt());
        return false;
    }
    if (key.equalsIgnoreCase(F("hassrot"))) {
        // orientation of the Home Assistant dashboard (0 = landscape, 1 = portrait, 2/3 = both
        // turned 180 degrees). False: the screen applies it without rebuilding the widget tree
        // (it rebuilds the grid and the quick settings itself)
        _debugHassRotation = static_cast<uint8_t>(value.toInt());
        return false;
    }
    // pushing a value marks the current weather as valid, the screens only show real (or here:
    // pushed) values instead of the "no data" message
    if (key.equalsIgnoreCase(F("temp"))) {
        _current.valid = true;
        _current.temperature = value.toFloat();
        return true;
    }
    if (key.equalsIgnoreCase(F("feels"))) {
        _current.valid = true;
        _current.feelsLike = value.toFloat();
        return true;
    }
    if (key.equalsIgnoreCase(F("min"))) {
        _current.valid = true;
        _current.minTemperature = value.toFloat();
        return true;
    }
    if (key.equalsIgnoreCase(F("max"))) {
        _current.valid = true;
        _current.maxTemperature = value.toFloat();
        return true;
    }
    if (key.equalsIgnoreCase(F("hum"))) {
        _current.valid = true;
        _current.humidity = value.toFloat();
        return true;
    }
    if (key.equalsIgnoreCase(F("press"))) {
        _current.valid = true;
        _current.pressure = value.toFloat();
        return true;
    }
    if (key.equalsIgnoreCase(F("wind"))) {
        _current.valid = true;
        _current.windSpeed = value.toFloat();
        return true;
    }
    if (key.equalsIgnoreCase(F("rain"))) {
        _current.valid = true;
        _current.rain = value.toFloat();
        return true;
    }
    if (key.equalsIgnoreCase(F("uv"))) {
        _current.valid = true;
        _current.uvIndex = value.toFloat();
        return true;
    }
    if (key.equalsIgnoreCase(F("city"))) {
        _current.valid = true;
        _current.location = value;
        return true;
    }
    if (key.equalsIgnoreCase(F("descr"))) {
        _current.valid = true;
        _current.description = value;
        return true;
    }
    if (key.equalsIgnoreCase(F("icon"))) {
        _current.valid = true;
        _current.icon = _iconFromName(value);
        return true;
    }
    if (key.equalsIgnoreCase(F("days"))) {
        _forecastCount = static_cast<uint8_t>(constrain(value.toInt(), 1, 5));
        // The debug feature reproduces a screen from pushed values and the forecast needs a name,
        // an icon and temperatures to show the layout. This is the only place that fills the
        // cards without a response - the (dev-only) debug build never ships, the screens
        // themselves display real data or the status message
        static const WeatherIcon icons[5] = { WeatherIcon::PARTLY_CLOUDY, WeatherIcon::SUN, WeatherIcon::RAIN, WeatherIcon::CLOUDY, WeatherIcon::SNOW };
        for (uint8_t i = 0; i < _forecastCount; i++) {
            auto &day = _forecast[i];
            day.valid = true;
            day.icon = icons[i];
            day.minTemperature = 15.0f + i;
            day.maxTemperature = 24.0f - i;
            day.rain = (i == 2) ? 4.2f : 0.0f;
            day.day.clear();
            StrWrapper(day.day).printf("Day %u", static_cast<unsigned>(i + 1));
        }
        return true;
    }
    if (key.equalsIgnoreCase(F("dayparts"))) {
        // the four parts of the day of the "1 day" layout, same purpose as the "days" key
        static const WeatherIcon icons[kNumDayParts] = { WeatherIcon::PARTLY_CLOUDY, WeatherIcon::SUN, WeatherIcon::RAIN, WeatherIcon::SNOW };
        static const char *const times[kNumDayParts] = { "06:00", "12:00", "Mon 15:00", "Tue 21:00" };
        for (uint8_t i = 0; i < kNumDayParts; i++) {
            auto &slot = _dayParts[i];
            slot.valid = true;
            slot.time = times[i];
            slot.icon = icons[i];
            slot.temperature = 14.0f + i;
            slot.feelsLike = 13.0f + i;
            slot.rain = (i == 2) ? 1.4f : 0.0f;
            slot.pop = (i == 2) ? 0.62f : 0.05f;
        }
        return true;
    }
    if (key.equalsIgnoreCase(F("itmp"))) {
        _setDebugIndoorValue(_indoor, IndoorValues::Metric::TEMPERATURE, value.toFloat());
        return true;
    }
    if (key.equalsIgnoreCase(F("ihum"))) {
        _setDebugIndoorValue(_indoor, IndoorValues::Metric::HUMIDITY, value.toFloat());
        return true;
    }
    if (key.equalsIgnoreCase(F("ipress"))) {
        _setDebugIndoorValue(_indoor, IndoorValues::Metric::PRESSURE, value.toFloat());
        return true;
    }
    if (key.equalsIgnoreCase(F("igas"))) {
        _setDebugIndoorValue(_indoor, IndoorValues::Metric::ECO2, value.toFloat());
        return true;
    }
    if (key.equalsIgnoreCase(F("phase"))) {
        _moon.phase = value;
        return true;
    }
    if (key.equalsIgnoreCase(F("illum"))) {
        _moon.illumination = value.toFloat();
        return true;
    }
    if (key.equalsIgnoreCase(F("age"))) {
        _moon.age = value.toFloat();
        return true;
    }
    // power channels: "pwrch" selects the channel the following keys write to. With the freeze
    // active the periodic update does not overwrite the pushed values, so the power screen and
    // its chart can be checked without a sensor or the remote server
    if (key.equalsIgnoreCase(F("pwrch"))) {
        _debugPowerChannel = static_cast<uint8_t>(constrain(value.toInt(), 0, static_cast<int>(PowerChannels::kNumChannels) - 1));
        return true;
    }
    if (key.equalsIgnoreCase(F("pwrv")) || key.equalsIgnoreCase(F("pwra")) ||
        key.equalsIgnoreCase(F("pwrw")) || key.equalsIgnoreCase(F("pwre"))) {
        auto &power = _power.values[_debugPowerChannel];
        power.online = true;
        power.available = true;
        if (key.equalsIgnoreCase(F("pwrv"))) {
            power.voltage = value.toFloat();
        }
        else if (key.equalsIgnoreCase(F("pwra"))) {
            power.current = value.toFloat();
        }
        else if (key.equalsIgnoreCase(F("pwrw"))) {
            power.power = value.toFloat();
        }
        else {
            power.energy = value.toDouble();
            power.hasEnergy = true;
        }
        return true;
    }
    return false;
}

#endif

// ------------------------------------------------------------------------------------------
// DataSource: helpers shared by the screens and the sources
// ------------------------------------------------------------------------------------------

void DataSource::readSettings()
{
    // The 2.x plugin has no configuration of its own, it uses the weather configuration of the
    // core (the classes exist in every env, independent of whether the 1.x plugin is compiled
    // in). The env this plugin is built for has no form of the 1.x plugin, the "weather2" form
    // of this plugin writes the same parameters
    using Plugins = KFCConfigurationClasses::PluginsType;
    const auto config = Plugins::WeatherStation::getConfig();
    _metric = config.is_metric;
    _timeFormat24h = config.time_format_24h;
    const auto location = Plugins::WeatherStation::getLocation();
    if (location && *location) {
        // the configuration holds "City, Country (ISO 3166)"; the screens show the city only,
        // everything from the first comma on is dropped (same behavior as the 1.x plugin)
        _current.location = location;
        const auto pos = _current.location.indexOf(',');
        if (pos > 0) {
            _current.location.remove(pos);
        }
    }
}

void DataSource::updateSystemInfo()
{
    auto &system = _system;
    system.hostname = KFCConfigurationClasses::System::Device::getName();
    system.firmware = String(KFCFWConfiguration::getShortFirmwareVersion());
    system.uptime = millis() / 1000;
    system.freeHeap = ESP.getFreeHeap();
    system.freePsram = ESP.getFreePsram();

    // the network values are real while the WiFi is connected, otherwise only the SSID is known
    if (WiFi.isConnected()) {
        system.ssid = WiFi.SSID();
        system.ip = WiFi.localIP().toString();
        system.gateway = WiFi.gatewayIP().toString();
        system.dns1 = WiFi.dnsIP(0).toString();
        system.dns2 = WiFi.dnsIP(1).toString();
        system.rssi = static_cast<int16_t>(WiFi.RSSI());
    }
    else {
        auto ssid = WiFi.SSID();
        system.ssid = ssid.length() ? (String(F("connecting to ")) + ssid) : String(F("not connected"));
        system.ip = F("--");
        system.gateway = F("--");
        system.dns1 = F("--");
        system.dns2 = F("--");
        system.rssi = 0;
    }
}

void DataSource::updateMoonValues(time_t utc, int32_t timezoneOffset)
{
    // shared/moon_phase: Meeus chapter 47/48 for the state, chapter 49 for the four phases. The
    // calculation is pure math, it needs no network and no configuration - only a valid clock
    MoonPhase::applyTo(_moon, utc, timezoneOffset);
    __LDBG_printf("moon %.3f illum=%.3f age=%.1f %s (offset=%d)", _moon.illumination, _moon.illumination, _moon.age,
                  _moon.phase.c_str(), timezoneOffset);
}

bool DataSource::isDebugFrozen() const
{
#if DEBUG_LVGL_SCREENSHOT
    return _debugFrozen;
#else
    return false;
#endif
}

bool DataSource::hasValidTime(time_t utc)
{
    // 2020-09-13: anything before that means the clock was not set (NTP) yet
    return utc > 1600000000;
}

// ------------------------------------------------------------------------------------------
// WeatherDataSource: indoor metric sources
// ------------------------------------------------------------------------------------------

static void _addTopic(StringVector &topics, const String &topic)
{
    if (topic.length() && std::find(topics.begin(), topics.end(), topic) == topics.end()) {
        topics.push_back(topic);
    }
}

void WeatherDataSource::_readIndoorSources()
{
    {
        MUTEX_LOCK_BLOCK(_lock) {
            for (uint8_t i = 0; i < IndoorValues::kNumMetrics; i++) {
                auto &source = _mqtt[i];
                // the previous state is dropped, a metric that no longer uses MQTT must not
                // keep its value or availability
                source = MqttSource();

                const auto config = _getEffectiveSource(static_cast<IndoorValues::Metric>(i));
                if (config.startsWith(F("mqtt:"))) {
                    String parts[3];
                    _splitMqttSource(config.substring(5), parts);
                    if (parts[1].length() == 0) {
                        // without a value topic there is nothing to subscribe to
                        __LDBG_printf("metric %u: no value topic in '%s'", i, config.c_str());
                    }
                    else {
                        source.type = SensorType::MQTT;
                        source.statusTopic = parts[0];
                        source.valueTopic = parts[1];
                        if (parts[2].startsWith(F("json_value:"))) {
                            source.valueType = MqttValueType::JSON_VALUE;
                            source.key = parts[2].substring(11);
                        }
                    }
                }
                else if (F("internal") == config) {
                    source.type = SensorType::INTERNAL;
                }
                else {
                    __LDBG_printf("metric %u: no source ('%s')", i, config.c_str());
                }
            }

            // temperature/humidity/pressure usually share the same topics
            _mqttTopics.clear();
            for (const auto &source : _mqtt) {
                if (source.type != SensorType::MQTT) {
                    continue;
                }
                _addTopic(_mqttTopics, source.statusTopic);
                _addTopic(_mqttTopics, source.valueTopic);
            }

            // the configuration is known before the first update(), the screens need the
            // "configured" flag to decide whether a row exists
            for (uint8_t i = 0; i < IndoorValues::kNumMetrics; i++) {
                auto &metric = _indoor.values[i];
                metric = IndoorValue();
                metric.configured = _mqtt[i].type != SensorType::NONE;
            }
            __LDBG_printf("indoor sources: %u topic(s)", static_cast<unsigned>(_mqttTopics.size()));
        }
    }
    _system.sensors = _createSensorDescription();
}

String WeatherDataSource::_createSensorDescription() const
{
    uint8_t mqttCount = 0;
    uint8_t internalCount = 0;
    MUTEX_LOCK_BLOCK(_lock) {
        for (const auto &source : _mqtt) {
            if (source.type == SensorType::MQTT) {
                mqttCount++;
            }
            else if (source.type == SensorType::INTERNAL) {
                internalCount++;
            }
        }
    }
    if (mqttCount && internalCount) {
        return String(F("MQTT + internal sensor"));
    }
    if (mqttCount) {
        return String(F("MQTT sensors"));
    }
    if (internalCount) {
        return String(F("internal sensor"));
    }
    return String(F("no sensor configured"));
}

void WeatherDataSource::_applyIndoorValues()
{
    // The internal sensors are read without the lock, an I2C access takes a moment and the MQTT
    // task must not be blocked by it. The values are copied into the model below
    bool hasTemperature = false;
    bool hasHumidity = false;
    bool hasPressure = false;
    bool hasEco2 = false;
    float temperature = 0;
    float humidity = 0;
    float pressure = 0;
    float eco2 = 0;

#if IOT_SENSOR_HAVE_BME280 || IOT_SENSOR_HAVE_BME680
    {
        #if IOT_SENSOR_HAVE_BME680
            auto sensor = SensorPlugin::getSensor<Sensor_BME680>(MQTT::SensorType::BME680);
        #else
            auto sensor = SensorPlugin::getSensor<Sensor_BME280>(MQTT::SensorType::BME280);
        #endif
        if (sensor) {
            auto data = sensor->readSensor();
            temperature = data.temperature;
            humidity = data.humidity;
            pressure = data.pressure;
            hasTemperature = hasHumidity = hasPressure = true;
        }
    }
#endif

#if IOT_SENSOR_HAVE_CCS811
    {
        auto sensor = SensorPlugin::getSensor<Sensor_CCS811>(MQTT::SensorType::CCS811);
        if (sensor) {
            const auto &data = sensor->readSensor();
            if (data.available == 1) {
                eco2 = data.eCO2;
                hasEco2 = true;
            }
        }
    }
#endif

    MUTEX_LOCK_BLOCK(_lock) {
        for (uint8_t i = 0; i < IndoorValues::kNumMetrics; i++) {
            auto &metric = _indoor.values[i];
            const auto &source = _mqtt[i];
            metric.configured = source.type != SensorType::NONE;
            switch (source.type) {
            case SensorType::MQTT:
                // Online while the MQTT client is connected, the availability topic switches it
                // off. An unknown status (-1) counts as online, the retained status message
                // arrives right after subscribing
                metric.online = _mqttConnected && (source.statusTopic.length() == 0 || source.availability != 0);
                metric.available = source.valueReceived;
                if (source.valueReceived) {
                    metric.value = source.value;
                }
                break;
            case SensorType::INTERNAL: {
                bool available = false;
                float value = 0;
                switch (static_cast<IndoorValues::Metric>(i)) {
                case IndoorValues::Metric::TEMPERATURE:
                    available = hasTemperature;
                    value = temperature;
                    break;
                case IndoorValues::Metric::HUMIDITY:
                    available = hasHumidity;
                    value = humidity;
                    break;
                case IndoorValues::Metric::PRESSURE:
                    available = hasPressure;
                    value = pressure;
                    break;
                default:
                    available = hasEco2;
                    value = eco2;
                    break;
                }
                metric.online = available;
                metric.available = available;
                if (available) {
                    metric.value = value;
                }
                break;
            }
            default:
                metric.online = false;
                metric.available = false;
                break;
            }
        }
    }
}

void WeatherDataSource::_readPowerSources()
{
    // the model knows the configuration before the first sample - the screen needs it to decide
    // whether a channel selector has to be shown
    bool remote = false;
    for (uint8_t i = 0; i < PowerChannels::kNumChannels; i++) {
        auto &source = _powerSources[i];
        source = PowerSource();
        source.name = getPowerChannelPart(i, 1);

        const auto type = getPowerChannelPart(i, 0);
        if (F("remote") == type) {
            source.type = PowerSourceType::REMOTE;
            source.remoteChannelId = static_cast<uint32_t>(getPowerChannelPart(i, 2).toInt());
            remote = true;
        }
        else if (F("local") == type) {
            source.type = PowerSourceType::LOCAL;
        }
        else {
            __LDBG_printf("power channel %u: no source ('%s')", i, type.c_str());
        }

        auto &values = _power.values[i];
        values = PowerValues();
        values.configured = source.type != PowerSourceType::NONE;
        values.source = source.type;
        values.remoteChannelId = source.remoteChannelId;
    }

    if (_powerClient) {
        // no remote channel configured: an empty host keeps the reader task idle
        _powerClient->setTarget(remote ? getPowerRemoteHost() : String(), getPowerRemotePort());
    }
    // cached for the power screen, it refreshes at 5 fps and must not read the configuration
    _powerGraphMinutes = Plugins::WeatherStation::getPowerGraphMinutes();
    __LDBG_printf("power sources: %u channel(s) configured, remote=%u, graph=%umin", static_cast<unsigned>(_power.getCount()), static_cast<unsigned>(remote), static_cast<unsigned>(_powerGraphMinutes));
}

void WeatherDataSource::_applyPowerValues()
{
    // The local INA219 is read without the lock, an I2C access takes a moment and the reader task
    // must not be blocked by it. The values are copied into the model below
    bool localAvailable = false;
    float localVoltage = 0;
    float localCurrent = 0;
    float localPower = 0;

#if IOT_SENSOR_HAVE_INA219
    {
        auto sensor = SensorPlugin::getSensor<Sensor_INA219>(MQTT::SensorType::INA219);
        if (sensor) {
            localVoltage = sensor->getVoltage();
            localCurrent = sensor->getCurrent();
            localPower = sensor->getPower();
            localAvailable = true;
        }
    }
#endif

    const bool remoteConnected = _powerClient && _powerClient->isConnected();

    for (uint8_t i = 0; i < PowerChannels::kNumChannels; i++) {
        const auto &source = _powerSources[i];
        auto &values = _power.values[i];
        if (source.type == PowerSourceType::NONE) {
            values = PowerValues();
            continue;
        }
        values.configured = true;
        switch (source.type) {
        case PowerSourceType::LOCAL:
            values.online = localAvailable;
            values.available = localAvailable;
            // the INA219 has no energy counter and the total is never accumulated in software
            values.hasEnergy = false;
            if (localAvailable) {
                values.voltage = localVoltage;
                values.current = localCurrent;
                values.power = localPower;
            }
            break;
        case PowerSourceType::REMOTE: {
            values.online = remoteConnected;
            PowerMonitor::Sample sample;
            if (_powerClient && _powerClient->getSample(source.remoteChannelId, sample)) {
                values.available = true;
                values.voltage = sample.voltage;
                values.current = sample.current;
                values.power = sample.power;
                values.energy = sample.energy;
                values.hasEnergy = true;
            }
            break;
        }
        default:
            values.online = false;
            break;
        }
    }
}

void WeatherDataSource::stopPowerMonitor()
{
    if (_powerClient) {
        _powerClient->stop();
    }
}

void WeatherDataSource::appendPowerRemoteStatus(String &output) const
{
    if (!_powerClient || !_powerClient->isConfigured()) {
        return;
    }
    if (_powerClient->isConnected()) {
        StrWrapper(output).printf("connected, %u sample(s)", static_cast<unsigned>(_powerClient->getSampleCount()));
        const auto age = _powerClient->getLastSampleAge();
        if (age) {
            StrWrapper(output).printf(", %us ago", static_cast<unsigned>(age / 1000));
        }
        return;
    }
    output += "not connected";
    const auto error = _powerClient->getError();
    if (error.length()) {
        output += ": ";
        output += error;
    }
}

void WeatherDataSource::mqttConnected()
{
    MUTEX_LOCK_BLOCK(_lock) {
        _mqttConnected = true;
        for (auto &source : _mqtt) {
            if (source.type == SensorType::MQTT) {
                // the status topic is evaluated again, "unknown" counts as online
                source.availability = -1;
            }
        }
    }
    __LDBG_printf("mqtt connected, %u topic(s)", static_cast<unsigned>(_mqttTopics.size()));
}

void WeatherDataSource::mqttDisconnected()
{
    _mqttConnected = false;
    __LDBG_printf("mqtt disconnected");
}

void WeatherDataSource::mqttMessage(const char *topic, const char *payload, size_t len)
{
    MUTEX_LOCK_BLOCK(_lock) {
        for (auto &source : _mqtt) {
            if (source.type != SensorType::MQTT) {
                continue;
            }
            if (source.statusTopic.length() && source.statusTopic.equals(topic)) {
                const auto online = _parseAvailability(payload);
                if (online >= 0) {
                    source.availability = online;
                    __LDBG_printf("%s: availability=%s", source.statusTopic.c_str(), online ? "online" : "offline");
                }
            }
            else if (source.valueTopic.length() && source.valueTopic.equals(topic)) {
                float value = 0;
                const bool ok = (source.valueType == MqttValueType::JSON_VALUE)
                    ? _parseJsonValue(payload, len, source.key.c_str(), value)
                    : _parseNumber(payload, len, value);
                if (ok) {
                    source.value = value;
                    source.valueReceived = true;
                    __LDBG_printf("%s: %s=%.2f", source.valueTopic.c_str(), source.key.length() ? source.key.c_str() : "value", value);
                }
                else {
                    __LDBG_printf("%s: cannot parse '%s'", source.valueTopic.c_str(), payload);
                }
            }
        }
    }
}

void WeatherDataSource::getMqttTopics(StringVector &topics) const
{
    MUTEX_LOCK_BLOCK(_lock) {
        topics = _mqttTopics;
    }
}

// UTC offset of the device in seconds, derived from the TZ environment of the NTP plugin
static int32_t _deviceUtcOffset(time_t utc)
{
    struct tm local;
    struct tm utcTm;
    if (!localtime_r(&utc, &local) || !gmtime_r(&utc, &utcTm)) {
        return 0;
    }
    // the day difference keeps time zones up to +/-14h correct
    const int days = local.tm_yday - utcTm.tm_yday;
    return ((local.tm_hour - utcTm.tm_hour + days * 24) * 3600 +
            (local.tm_min - utcTm.tm_min) * 60 +
            (local.tm_sec - utcTm.tm_sec));
}

// ------------------------------------------------------------------------------------------
// WeatherDataSource: setup/loop and the request task
// ------------------------------------------------------------------------------------------
// The constructor only creates the HTTP client. The model stays empty until the first response
// was applied and the screens show the message of getWeatherStatusText() instead - plausible
// looking placeholder values are never displayed

WeatherDataSource::WeatherDataSource()
{
    _client.reset(new OpenWeatherMap::Client());
    _received.reset(new OpenWeatherMap::Data());

    // The outdoor weather, the forecast and the moon keep their empty defaults (getWeatherState()
    // reports WAITING/NOT_CONFIGURED/ERROR until a response arrives). The indoor metrics are read
    // from the configured sources by _readIndoorSources()/_applyIndoorValues() and show
    // "offline" or "--" until they deliver.
}

WeatherDataSource::~WeatherDataSource()
{
    // The object is a static member of the plugin, so it normally lives as long as the firmware
    // runs and the task never has to be stopped. Ask it to finish and give the current request a
    // moment, the flag is checked between two requests
    _stop = true;
    for (uint8_t i = 0; i < 20 && _task; i++) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void WeatherDataSource::begin()
{
    // this runs from the plugin setup, the constructor is too early (see ws2_data.h)
    readSettings();
    // the API key/coordinates and the poll interval are needed by the request task, without them
    // it never starts and isConfigured() reports "not configured"
    _readApiSettings();
    // the indoor sources drive the MQTT subscriptions, the plugin reads the topics afterwards
    _readIndoorSources();
    // the remote power channels are read by the TCP client, the local INA219 by update()
    _powerClient.reset(new PowerMonitor::Client());
    _readPowerSources();
    _powerClient->begin();
    updateSystemInfo();

    // the moon needs the clock only, the offset starts with the one of the device and is
    // replaced by the offset of the location as soon as a response was parsed
    const auto utc = ::time(nullptr);
    if (hasValidTime(utc)) {
        _timezoneOffset = _deviceUtcOffset(utc);
        _lastMoonUpdate = millis();
        updateMoonValues(utc, _timezoneOffset);
    }

    // the request blocks while the response is transferred, so it runs in its own task
    TaskHandle_t handle = nullptr;
    if (xTaskCreate(_taskEntry, "weather2", kTaskStack, this, 1, &handle) == pdPASS) {
        _task = handle;
        __LDBG_printf("request task started (stack=%u)", static_cast<unsigned>(kTaskStack));
    }
    else {
        __LDBG_printf("cannot start the request task");
    }
}

void WeatherDataSource::update()
{
    const auto now = millis();

    // The power channels are refreshed faster than the rest of the model - the power screen runs at
    // 5 fps and both sources deliver new samples at that rate. Only the samples are copied here,
    // the MQTT/indoor sensor reads, the system info and the moon stay at 1 Hz
    if (static_cast<uint32_t>(now - _lastPowerUpdate) >= kPowerUpdateInterval) {
        _lastPowerUpdate = now;
        if (!isDebugFrozen()) {
            _applyPowerValues();
        }
    }

    if (static_cast<uint32_t>(now - _lastUpdate) < 1000) {
        return;
    }
    _lastUpdate = now;

    // host name, uptime, heap, PSRAM and the WiFi state are real values in any case
    updateSystemInfo();

    if (isDebugFrozen()) {
        // values were pushed for a screenshot, keep them - the real system values stay live
        return;
    }

    // maps the received MQTT values and the internal sensor into the indoor model
    _applyIndoorValues();

    const auto applied = _applyResponse();

    // The illumination changes slowly, one calculation per minute is plenty. Only the moon is
    // available without the network, so it is updated even when no request was made yet. The
    // first calculation runs as soon as the clock is set (NTP) - without the extra check the
    // interval or a response would delay it and the screen would show 0 % until then
    if (!_moon.valid || applied || static_cast<uint32_t>(now - _lastMoonUpdate) >= kMoonInterval) {
        const auto utc = ::time(nullptr);
        if (hasValidTime(utc)) {
            _lastMoonUpdate = now;
            updateMoonValues(utc, _timezoneOffset);
        }
    }
}

void WeatherDataSource::reconfigure()
{
    // the form was saved, re-read everything and request immediately instead of waiting for the
    // poll interval. Called from the main loop (WebServer::executeDelayed), only the settings are
    // written here, the request task reads them under _lock
    readSettings();
    // the indoor sources may have changed, the plugin updates the subscriptions afterwards
    _readIndoorSources();
    // the power channels/remote target may have changed
    _readPowerSources();
    _readApiSettings();
    _requestNow = true;
    __LDBG_printf("reconfigured");
}

void WeatherDataSource::_taskEntry(void *arg)
{
    static_cast<WeatherDataSource *>(arg)->_requestLoop();
    vTaskDelete(nullptr);
}

void WeatherDataSource::_requestLoop()
{
    OpenWeatherMap::Settings settings;
    uint32_t nextRequest = 0;
    bool first = true;

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        if (_stop) {
            break;
        }
        if (!_client) {
            continue;
        }

        char apiKey[72];
        float latitude = 0;
        float longitude = 0;
        const auto interval = _readSettings(apiKey, sizeof(apiKey), latitude, longitude);
        if (interval == 0) {
            // no API key or no location configured, wait for the form
            first = true;
            continue;
        }
        if (!WiFi.isConnected()) {
            continue;
        }
        if (!first && !_requestNow && static_cast<int32_t>(millis() - nextRequest) < 0) {
            continue;
        }
        first = false;
        _requestNow = false;

        settings.apiKey = apiKey;
        settings.latitude = latitude;
        settings.longitude = longitude;
        settings.metric = _metric;
        settings.language = nullptr; // the API default (English), the screens are English too
        _client->setSettings(settings);

        OpenWeatherMap::Data data;
        const auto ok = _client->fetch(data);

        MUTEX_LOCK_BLOCK(_lock) {
            if (ok) {
                *_received = data;
                _receivedValid = true;
                _lastError = String();
            }
            else {
                _lastError = _client->getError();
            }
            _requestCount = _client->getRequestCount();
        }

        // a failed request is retried earlier than the poll interval
        nextRequest = millis() + (ok ? interval : kRetryInterval);
        __LDBG_printf("request %s after %ums, next in %us", ok ? "ok" : "failed",
                      static_cast<unsigned>(_client->getDuration()),
                      static_cast<unsigned>((nextRequest - millis()) / 1000));
    }
    __LDBG_printf("request task stopped");
    _task = nullptr;
}

uint32_t WeatherDataSource::_readSettings(char *apiKey, size_t apiKeySize, float &latitude, float &longitude)
{
    MUTEX_LOCK_BLOCK(_lock) {
        const auto length = _apiKey.length();
        const auto copy = (length < apiKeySize - 1) ? length : apiKeySize - 1;
        if (copy) {
            memcpy(apiKey, _apiKey.c_str(), copy);
        }
        apiKey[copy] = 0;
        latitude = _latitude;
        longitude = _longitude;
        if (copy == 0 || (latitude == 0 && longitude == 0) || _pollInterval == 0) {
            return 0;
        }
        return _pollInterval;
    }
    return 0;
}

void WeatherDataSource::_readApiSettings()
{
    // The request task reads _apiKey/_latitude/_longitude/_pollInterval, so they must be filled
    // before the task starts (begin()) and after every form save (reconfigure()). Without this
    // the task never issues a request and isConfigured() reports "not configured" although the
    // form has an API key and coordinates
    using Plugins = KFCConfigurationClasses::PluginsType;
    const auto config = Plugins::WeatherStation::getConfig();
    MUTEX_LOCK_BLOCK(_lock) {
        _apiKey = Plugins::WeatherStation::getApiKey();
        _latitude = config.latitude;
        _longitude = config.longitude;
    }
    _pollInterval = config.getPollIntervalMillis();
    __LDBG_printf("api settings: key=%s, %.4f/%.4f, interval=%ums", _apiKey.length() ? "set" : "missing",
                  _latitude, _longitude, static_cast<unsigned>(_pollInterval));
}

bool WeatherDataSource::_applyResponse()
{
    OpenWeatherMap::Data data;
    MUTEX_LOCK_BLOCK(_lock) {
        if (!_receivedValid) {
            return false;
        }
        data = *_received;
        _receivedValid = false;
    }

    // the response reports the offset of the coordinates, that is the one the moon phases and
    // the sunrise/sunset are displayed with
    _timezoneOffset = data.timezoneOffset;

    OpenWeatherMap::applyCurrent(data, _current);
    _forecastCount = OpenWeatherMap::applyForecast(data, _forecast, kMaxForecastDays);
    // a response with less days must not leave the values of the previous one behind
    for (uint8_t i = _forecastCount; i < kMaxForecastDays; i++) {
        _forecast[i] = ForecastDay();
    }
    // the four parts of the day of the "1 day" layout, all slots are reset by the mapping
    const auto dayParts = OpenWeatherMap::applyDayParts(data, _dayParts, kNumDayParts);
    __LDBG_printf("applied %u forecast days, %u parts of the day, %s, %.1f, %.1f%%", static_cast<unsigned>(_forecastCount),
                  static_cast<unsigned>(dayParts), _current.description.c_str(), _current.temperature, _current.humidity);
    return true;
}

WeatherState WeatherDataSource::getWeatherState() const
{
    // the values of a parsed response stay visible while a later request fails - the error is
    // only reported while there is nothing to show
    if (_current.valid) {
        return WeatherState::READY;
    }
    if (!isConfigured()) {
        return WeatherState::NOT_CONFIGURED;
    }
    return getLastError().length() ? WeatherState::ERROR : WeatherState::WAITING;
}

String WeatherDataSource::getWeatherError() const
{
    return getLastError();
}

String WeatherDataSource::getLastError() const
{
    MUTEX_LOCK_BLOCK(_lock) {
        return _lastError;
    }
    return String();
}

uint32_t WeatherDataSource::getRequestCount() const
{
    MUTEX_LOCK_BLOCK(_lock) {
        return _requestCount;
    }
    return 0;
}

bool WeatherDataSource::isConfigured() const
{
    MUTEX_LOCK_BLOCK(_lock) {
        return _apiKey.length() && (_latitude != 0 || _longitude != 0);
    }
    return false;
}

} // namespace WeatherStation2
