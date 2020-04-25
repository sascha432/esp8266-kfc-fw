#pragma once

namespace KFCConfigurationClasses {

    struct Plugins {

        class WeatherStation
        {
        public:
            class WeatherStationConfig_t {
            public:
                WeatherStationConfig_t() {
                    reset();
                }

                struct __attribute__packed__ {
                    uint16_t weather_poll_interval;
                    uint16_t api_timeout;
                    uint8_t backlight_level;
                    uint8_t touch_threshold;
                    uint8_t released_threshold;
                    uint8_t is_metric : 1;
                    uint8_t time_format_24h : 1;
                    float temp_offset;
                };

                void reset() {
                    weather_poll_interval = 15;
                    api_timeout = 30;
                    backlight_level = 100;
                    released_threshold = 53;
                    is_metric = 1;
                    time_format_24h = 1;
                    temp_offset = 0;
                }

                void validate() {
                    if (weather_poll_interval == 0 || api_timeout == 0 || touch_threshold == 0 || released_threshold == 0) {
                        reset();
                    }
                    if (backlight_level < 10) {
                        backlight_level = 10;
                    }
                }
                uint32_t getPollIntervalMillis() {
                    return weather_poll_interval * 60000UL;
                }
            };

            WeatherStation() {
                config.reset();
            }

            static void defaults();
            static const char *getApiKey();
            static const char *getQueryString();
            static WeatherStationConfig_t &getWriteableConfig();
            static WeatherStationConfig_t getConfig();

            char openweather_api_key[65];
            char openweather_api_query[65];
            WeatherStationConfig_t config;
        };

    };

};