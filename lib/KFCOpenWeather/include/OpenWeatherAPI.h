/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <StreamString.h>
#include <map>

class OpenWeatherAPI {
public:
    class WeatherInfo {
    public:
        class Weather_t {
        public:
            Weather_t() : id(0) {
            }
            String main;
            String descr;
            String icon;
            uint16_t id;
        };

        WeatherInfo() {
            clear();
        }

        void clear() {
            location = String();
            country = String();
            memset(&val, 0, sizeof(val));
            weather.clear();
            rain_mm.clear();
        }

        bool hasData() const {
            return location.length() && weather.size();
        }

        time_t getSunRiseAsGMT() const {
            return val.sunrise + val.timezone;
        }

        time_t getSunSetAsGMT() const {
            return val.sunset + val.timezone;
        }

    public:
        String location;
        String country;
        struct {
            int32_t timezone;
            float temperature;
            float temperature_min;
            float temperature_max;
            float humidity;
            float pressure;
            time_t sunrise;
            time_t sunset;
            float wind_speed;
            uint16_t wind_deg;
            uint32_t visibility;
        } val;
        std::vector<Weather_t> weather;
        std::map<String, float> rain_mm;
    };

    OpenWeatherAPI();
    OpenWeatherAPI(const String &apiKey);

    void clear();

    void setAPIKey(const String &key);

    void setQuery(const String &query);
    String getApiUrl() const;
    bool parseApiData(const String &data);
    bool parseApiData(StreamString &stream);

    WeatherInfo &getWeatherInfo();

    void dump(Print &output) const;

    static float kelvinToC(float temp);
    static float kelvinToF(float temp);
    static float kmToMiles(float km);

private:
    String _apiQuery;
    String _apiKey;
    WeatherInfo _info;
};