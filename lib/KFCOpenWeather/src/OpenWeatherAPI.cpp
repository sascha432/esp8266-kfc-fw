/**
* Author: sascha_lammers@gmx.de
*/

#include "OpenWeatherAPI.h"
#include <JsonCallbackReader.h>
#include <misc.h>

static const char _OpenWeatherAPI_url[] PROGMEM = { "http://api.openweathermap.org/data/2.5/weather?q={query}&appid={api_key}" };

class OpenWeatherInfoJsonReader : public JsonBaseReader {
public:
    OpenWeatherInfoJsonReader(Stream &stream, OpenWeatherAPI::WeatherInfo &info) : JsonBaseReader(stream), _info(info) {
    }

    virtual bool processElement() {

        auto keyStr = getKey();
        auto key = keyStr.c_str();
        auto pathStr = getPath();
        auto path = pathStr.c_str();
        int16_t index = _stack.size() ? _stack.back().arrayIndex : -1;

        //Serial.printf("key %s value %s type %s path %s index %d\n", key, getValue().c_str(), jsonType2String(getType()).c_str(), getPath().c_str(), index);

        if (!strcmp_P(path, "name")) {
            _info.location = getValue();
        }
        else if (!strcmp_P(path, "visibility")) {
            _info.val.visibility = getValue().toInt();
        }
        else if (!strcmp_P(path, "timezone")) {
            _info.val.timezone = getValue().toInt();
        }
        else if (!strncmp_P(path, "main.", 5)) {
            if (!strcmp_P(key, "temp")) {
                _info.val.temperature = getValue().toFloat();
            }
            else if (!strcmp_P(key, "temp_min")) {
                _info.val.temperature_min = getValue().toFloat();
            }
            else if (!strcmp_P(key, "temp_max")) {
                _info.val.temperature_max = getValue().toFloat();
            }
            else if (!strcmp_P(key, "pressure")) {
                _info.val.pressure = getValue().toFloat();
            }
            else if (!strcmp_P(key, "humidity")) {
                _info.val.humidity = getValue().toFloat();
            }
        }
        else if (!strncmp_P(path, "rain.", 5)) {
            _info.rain_mm[keyStr] = getValue().toFloat();
        }
        else if (!strncmp_P(path, "weather[", 8) && index >= 0) {
            while((int16_t)_info.weather.size() <= index) {
                _info.weather.emplace_back(OpenWeatherAPI::WeatherInfo::Weather_t());
            }
            auto &item = _info.weather.at(index);
            if (!strcmp_P(key, "main")) {
                item.main = getValue();
            }
            else if (!strcmp_P(key, "description")) {
                item.descr = getValue();
            }
            else if (!strcmp_P(key, "icon")) {
                item.icon = getValue();
            }
            else if (!strcmp_P(key, "id")) {
                item.id = (uint16_t)getValue().toInt();
            }
        }
        else if (!strncmp_P(path, "sys.", 4)) {
            if (!strcmp_P(key, "country")) {
                _info.country = getValue();
            }
            else if (!strcmp_P(key, "sunrise")) {
                _info.val.sunrise = getValue().toInt();
            }
            else if (!strcmp_P(key, "sunset")) {
                _info.val.sunset = getValue().toInt();
            }
        }
        else if (!strncmp_P(path, "wind.", 5)) {
            if (!strcmp_P(key, "speed")) {
                _info.val.wind_speed = getValue().toFloat();
            }
            else if (!strcmp_P(key, "deg")) {
                _info.val.wind_deg = (uint16_t)getValue().toInt();
            }
        }

        return true;
    }

    virtual bool recoverableError(JsonErrorEnum_t errorType) {
        return true;
    }


private:
    OpenWeatherAPI::WeatherInfo &_info;
};

OpenWeatherAPI::OpenWeatherAPI()
{
}

OpenWeatherAPI::OpenWeatherAPI(const String &apiKey) : _apiKey(apiKey)
{
}

void OpenWeatherAPI::clear()
{
    _info.clear();
}

void OpenWeatherAPI::setAPIKey(const String &key)
{
    _apiKey = key;
}

void OpenWeatherAPI::setQuery(const String &query)
{
    _apiQuery = query;
}

String OpenWeatherAPI::getApiUrl() const
{
    String url = FPSTR(_OpenWeatherAPI_url);
    url.replace(F("{query}"), url_encode(_apiQuery));
    url.replace(F("{api_key}"), url_encode(_apiKey));
    return url;
}

bool OpenWeatherAPI::parseApiData(const String &data)
{
    StreamString stream;
    stream.print(data);
    return parseApiData(stream);
}

bool OpenWeatherAPI::parseApiData(StreamString & stream)
{
    return OpenWeatherInfoJsonReader(stream, _info).parse();
}

OpenWeatherAPI::WeatherInfo &OpenWeatherAPI::getWeatherInfo()
{
    return _info;
}

void OpenWeatherAPI::dump(Print & output) const
{
    if (_info.hasData()) {
        
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
}

float OpenWeatherAPI::kelvinToC(float temp) 
{
    return temp - 273.15f;
}

float OpenWeatherAPI::kelvinToF(float temp)
{
    return (temp - 273.15f) * (9.0f / 5.0f) + 32;
}

float OpenWeatherAPI::kmToMiles(float km)
{
    return km / 1.60934f;
}
