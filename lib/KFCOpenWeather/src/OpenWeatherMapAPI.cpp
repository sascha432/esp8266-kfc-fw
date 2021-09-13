/**
* Author: sascha_lammers@gmx.de
*/

#include "OpenWeatherMapAPI.h"
#include "OpenWeatherInfoJsonReader.h"
#include "OpenWeatherForecastJsonReader.h"
#include <misc.h>

// PROGMEM_STRING_DEF(openweathermap_api_url, "http://api.openweathermap.org/data/2.5/{api_type}?q={query}&appid={api_key}");

OpenWeatherMapAPI::OpenWeatherMapAPI()
{
}

OpenWeatherMapAPI::OpenWeatherMapAPI(const String &apiKey) : _apiKey(apiKey)
{
}

void OpenWeatherMapAPI::clear()
{
    _info.clear();
    _forecast.clear();
}

void OpenWeatherMapAPI::setAPIKey(const String &key)
{
    _apiKey = key;
}

void OpenWeatherMapAPI::setQuery(const String &query)
{
    _apiQuery = query;
}

String OpenWeatherMapAPI::getApiUrl(const String &apiType) const
{
    String url = FSPGM(openweathermap_api_url);
    url.replace(F("{query}"), urlEncode(_apiQuery));
    url.replace(F("{api_key}"), urlEncode(_apiKey));
    url.replace(F("{api_type}"), apiType);
    return url;
}

String OpenWeatherMapAPI::getWeatherApiUrl() const
{
    return getApiUrl(F("weather"));
}

String OpenWeatherMapAPI::getForecastApiUrl() const
{
    return getApiUrl(F("forecast"));
}

bool OpenWeatherMapAPI::parseWeatherData(const String &data)
{
    StreamString stream;
    stream.print(data);
    return parseWeatherData(*(Stream *)&stream);
}

bool OpenWeatherMapAPI::parseWeatherData(Stream &stream)
{
    return OpenWeatherInfoJsonReader(&stream, _info).parse();
}

bool OpenWeatherMapAPI::parseForecastData(const String &data)
{
    StreamString stream;
    stream.print(data);
    return parseForecastData(*(Stream *)&stream);
}

bool OpenWeatherMapAPI::parseForecastData(Stream &stream)
{
    return OpenWeatherForecastJsonReader(&stream, _forecast).parse();
}

JsonBaseReader *OpenWeatherMapAPI::getWeatherInfoParser()
{
    auto parser = new OpenWeatherInfoJsonReader(_info);
    parser->initParser();
    return parser;
}

JsonBaseReader *OpenWeatherMapAPI::getWeatherForecastParser()
{
    auto parser = new OpenWeatherForecastJsonReader(_forecast);
    parser->initParser();
    return parser;
}

OpenWeatherMapAPI::WeatherInfo &OpenWeatherMapAPI::getWeatherInfo()
{
    return _info;
}

OpenWeatherMapAPI::WeatherForecast &OpenWeatherMapAPI::getWeatherForecast()
{
    return _forecast;
}

time_t OpenWeatherMapAPI::WeatherInfo::getSunRiseAsGMT() const
{
    return val.sunrise + val.timezone;
}

time_t OpenWeatherMapAPI::WeatherInfo::getSunSetAsGMT() const
{
    return val.sunset + val.timezone;
}

void OpenWeatherMapAPI::WeatherInfo::dump(Print &output) const
{
    output.printf_P(PSTR("--- Current Weather\n"));
    output.printf_P(PSTR("-------------------\n"));
    output.printf_P(PSTR("Location: %s\n"), location.c_str());
    output.printf_P(PSTR("Country: %s\n"), country.c_str());
    output.printf_P(PSTR("Timezone: %d\n"), val.timezone);
    output.printf_P(PSTR("Temperature: %.1f C (min/max %.1f/%.1f)\n"), kelvinToC(val.temperature), kelvinToC(val.temperature_min), kelvinToC(val.temperature_max));
    output.printf_P(PSTR("Temperature: %.1f F (min/max %.1f/%.1f)\n"), kelvinToF(val.temperature), kelvinToF(val.temperature_min), kelvinToF(val.temperature_max));
    output.printf_P(PSTR("Humidity: %d %%\n"), val.humidity);
    output.printf_P(PSTR("Pressure: %d hPa\n"), val.pressure);

    char buf[32];
    time_t time = getSunRiseAsGMT();
    struct tm *tm = gmtime(&time);
    strftime(buf, sizeof(buf), "%D %T", tm);
    output.printf_P(PSTR("Sunrise: %s\n"), buf);

    time = getSunSetAsGMT();
    tm = gmtime(&time);
    strftime(buf, sizeof(buf), "%D %T", tm);
    output.printf_P(PSTR("Sunset: %s\n"), buf);

    output.printf_P(PSTR("Wind speed: %.2f\n"), val.wind_speed);
    output.printf_P(PSTR("Wind deg: %d\n"), val.wind_deg);
    output.printf_P(PSTR("Visibility: %.1f km\n"), val.visibility / 1000.0f);
    output.printf_P(PSTR("Visibility: %.1f mi\n"), kmToMiles(val.visibility / 1000.0f));

    for (auto &w : weather) {
        output.printf_P(PSTR("Weather id/main/icon: %u - %s (%s)\n"), w.id, w.main.c_str(), w.icon.c_str());
        output.printf_P(PSTR("Weather description: %s\n"), w.descr.c_str());
    }

    for (auto &rain : rain_mm) {
        output.printf_P(PSTR("Rain: %s - %.2f mm\n"), rain.first.c_str(), rain.second);
    }
}

//void OpenWeatherMapAPI::WeatherForecast::updateKeys()
//{
//    char buf[32];
//    std::map<String, Forecast_t> tmp;
//
//    for(auto &forecast : forecast) {
//        forecast.second.val.time += val.timezone;
//        struct tm *tm = gmtime(&forecast.second.val.time);
//        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm);
//        tmp.emplace(buf, std::move(forecast.second));
//    }
//    std::swap(forecast, tmp);
//}

void OpenWeatherMapAPI::WeatherForecast::dump(Print & output) const
{
    output.printf_P(PSTR("\n--- Weather Forecast\n"));
    output.printf_P(PSTR("----------------------\n"));
    output.printf_P(PSTR("City: %s\n"), city.c_str());
    output.printf_P(PSTR("Country: %s\n"), country.c_str());
    output.printf_P(PSTR("Timezone: %d\n"), val.timezone);

    for (const auto &f : forecast) {
        auto &info = f.second;
        output.printf_P(PSTR("[%s] - Time: %d\n"), f.first.c_str(), (int)info.val.time);
        output.printf_P(PSTR("Temperature: %.1f C (min/max %.1f/%.1f)\n"), kelvinToC(info.val.temperature), kelvinToC(info.val.temperature_min), kelvinToC(info.val.temperature_max));
        output.printf_P(PSTR("Temperature: %.1f F (min/max %.1f/%.1f)\n"), kelvinToF(info.val.temperature), kelvinToF(info.val.temperature_min), kelvinToF(info.val.temperature_max));
        output.printf_P(PSTR("Humidity: %d %%\n"), info.val.humidity);
        output.printf_P(PSTR("Pressure: %d hPa\n"), info.val.pressure);
    }
}

void OpenWeatherMapAPI::dump(Print &output) const
{
    if (_info.hasData()) {
        _info.dump(output);
    }
    else {
        output.println(F("No weather data available"));
    }

    if (_forecast.hasData()) {
        _forecast.dump(output);
    }
    else {
        output.println(F("No weather forecast available"));
    }
}

float OpenWeatherMapAPI::kelvinToC(float temp)
{
    return temp - 273.15f;
}

float OpenWeatherMapAPI::kelvinToF(float temp)
{
    return (temp - 273.15f) * (9.0f / 5.0f) + 32;
}

float OpenWeatherMapAPI::kmToMiles(float km)
{
    return km / 1.60934f;
}

