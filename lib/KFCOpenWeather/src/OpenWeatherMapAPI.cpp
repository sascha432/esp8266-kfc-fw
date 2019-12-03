/**
* Author: sascha_lammers@gmx.de
*/

#include "OpenWeatherMapAPI.h"
#include "OpenWeatherInfoJsonReader.h"
#include "OpenWeatherForecastJsonReader.h"
#include <misc.h>

PROGMEM_STRING_DEF(openweathermap_api_url, "http://api.openweathermap.org/data/2.5/{api_type}?q={query}&appid={api_key}");

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
    url.replace(F("{query}"), url_encode(_apiQuery));
    url.replace(F("{api_key}"), url_encode(_apiKey));
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
    return nullptr;
}

OpenWeatherMapAPI::WeatherInfo &OpenWeatherMapAPI::getWeatherInfo()
{
    return _info;
}

OpenWeatherMapAPI::WeatherForecast &OpenWeatherMapAPI::getWeatherForecast()
{
    return _forecast;
}

void OpenWeatherMapAPI::dump(Print &output) const
{
    if (_info.hasData()) {

        output.printf_P(PSTR("--- Current Weather\n"));
        output.printf_P(PSTR("-------------------\n"));
        output.printf_P(PSTR("Location: %s\n"), _info.location.c_str());
        output.printf_P(PSTR("Country: %s\n"), _info.country.c_str());
        output.printf_P(PSTR("Temperature: %.1f C (min/max %.1f/%.1f)\n"), kelvinToC(_info.val.temperature), kelvinToC(_info.val.temperature_min), kelvinToC(_info.val.temperature_max));
        output.printf_P(PSTR("Temperature: %.1f F (min/max %.1f/%.1f)\n"), kelvinToF(_info.val.temperature), kelvinToF(_info.val.temperature_min), kelvinToF(_info.val.temperature_max));
        output.printf_P(PSTR("Humidity: %.1f %%\n"), _info.val.humidity);
        output.printf_P(PSTR("Pressure: %.1f hPa\n"), _info.val.pressure);

        char buf[32];
        time_t time = _info.getSunRiseAsGMT();
        struct tm *tm = gmtime(&time);
        strftime(buf, sizeof(buf), "%D %T", tm);
        output.printf_P(PSTR("Sunrise: %s\n"), buf);

        time = _info.getSunSetAsGMT();
        tm = gmtime(&time);
        strftime(buf, sizeof(buf), "%D %T", tm);
        output.printf_P(PSTR("Sunset: %s\n"), buf);

        output.printf_P(PSTR("Wind speed: %.2f\n"), _info.val.wind_speed);
        output.printf_P(PSTR("Wind deg: %d\n"), _info.val.wind_deg);
        output.printf_P(PSTR("Visibility: %.1f km\n"), _info.val.visibility / 1000.0f);
        output.printf_P(PSTR("Visibility: %.1f mi\n"), kmToMiles(_info.val.visibility / 1000.0f));

        for (const auto &weather : _info.weather) {
            output.printf_P(PSTR("Weather id/main/icon: %u - %s (%s)\n"), weather.id, weather.main.c_str(), weather.icon.c_str());
            output.printf_P(PSTR("Weather description: %s\n"), weather.descr.c_str());
        }

        for (const auto &rain : _info.rain_mm) {
            output.printf_P(PSTR("Rain: %s - %.2f mm\n"), rain.first.c_str(), rain.second);
        }

    }
    else {
        output.println(F("No weather data available"));
    }

    if (_forecast.hasData()) {

        output.printf_P(PSTR("\n--- Weather Forecast\n"));
        output.printf_P(PSTR("----------------------\n"));

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
