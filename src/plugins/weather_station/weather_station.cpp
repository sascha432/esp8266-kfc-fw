/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_WEATHER_STATION

#include "weather_station.h"
#include <PrintHtmlEntitiesString.h>
#include <EventScheduler.h>
#include <MicrosTimer.h>
#include <HeapStream.h>
#include <serial_handler.h>
#include <WebUISocket.h>
#include <AsyncBitmapStreamResponse.h>
#include <GFXCanvasRLEStream.h>
#include <kfc_fw_config.h>
#include "RestAPI.h"
#include "web_server.h"
#include "async_web_response.h"
#include "blink_led_timer.h"
#include "./plugins/sensor/sensor.h"
#include "./plugins/sensor/Sensor_BME280.h"

#if IOT_WEATHER_STATION_WS2812_NUM
#define FASTLED_INTERNAL
#include <FastLED.h>
#endif

using Plugins = KFCConfigurationClasses::PluginsType;

// web access for screen capture
//
// screen capture
// http://192.168.0.99/images/screen_capture.bmp
// http://192.168.0.56/images/screen_capture.bmp
//
//

#if DEBUG_IOT_WEATHER_STATION
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#if IOT_WEATHER_STATION_WS2812_NUM && __LED_BUILTIN == -1
#error invalid built-in LED
#endif

WeatherStationPlugin ws_plugin;

PROGMEM_DEFINE_PLUGIN_OPTIONS(
    WeatherStationPlugin,
    // name
    "weather",
    // friendly name
    "Weather Station",
    // web_templates
    "",
    // config_forms
    "weather,world-clock",
    // reconfigure_dependencies
    "http",
    PluginComponent::PriorityType::WEATHER_STATION,
    PluginComponent::RTCMemoryId::NONE,
    static_cast<uint8_t>(PluginComponent::MenuType::CUSTOM),
    false,              // allow_safe_mode
    false,              // setup_after_deep_sleep
    true,               // has_get_status
    true,               // has_config_forms
    true,               // has_web_ui
    false,              // has_web_templates
    true,               // has_at_mode
    0                   // __reserved
);

WeatherStationPlugin::WeatherStationPlugin() :
    PluginComponent(PROGMEM_GET_PLUGIN_OPTIONS(WeatherStationPlugin)),
    // _pollTimer(0),
    _httpClient(nullptr)
    #if IOT_WEATHER_STATION_WS2812_NUM
        , _pixels(WS2812LEDTimer::_pixels)
    #endif
{
    REGISTER_PLUGIN(this, "WeatherStationPlugin");
    #if defined(E8266) && __LED_BUILTIN_WS2812_PIN != 16
        // the WS2812 were connected to port 16, but since framework 3.0.0 GPIO16 cannot be used anymore
        digitalWrite(16, LOW);
        pinMode(16, INPUT);
    #endif
}

#if WEATHER_STATION_HAVE_BMP_SCREENSHOT

void WeatherStationPlugin::_sendScreenCaptureBMP(AsyncWebServerRequest *request)
{
    // __LDBG_printf("WeatherStationPlugin::_sendScreenCapture(): is_authenticated=%u", WebServerPlugin::getInstance().isAuthenticated(request) == true);

    if (WebServer::Plugin::getInstance().isAuthenticated(request) == true) {
        if (WeatherStationPlugin::_getInstance().lock()) {
            #if 1
                auto response = new AsyncBitmapStreamResponse(*WeatherStationPlugin::_getInstance().getCanvas());
            #else
                __LDBG_IF(
                    auto mem = ESP.getFreeHeap()
                    );
                auto response = new AsyncClonedBitmapStreamResponse(lock.getCanvas().clone());
                __LDBG_IF(
                    auto usage = mem - ESP.getFreeHeap();
                    __DBG_printf("AsyncClonedBitmapStreamResponse memory usage %u", usage);
                );
                plugin.releaseCanvasLock();
            #endif
            HttpHeaders httpHeaders;
            httpHeaders.addNoCache();
            httpHeaders.setResponseHeaders(response);
            request->send(response);
        }
        else {
            request->send(503);
        }
    }
    else {
        request->send(403);
    }
}

#endif

void WeatherStationPlugin::_installWebhooks()
{
    __LDBG_printf("server=%p", WebServer::Plugin::getWebServerObject());

    #if WEATHER_STATION_HAVE_BMP_SCREENSHOT

        WebServer::Plugin::addHandler(F("/images/screen_capture.bmp"), _sendScreenCaptureBMP);

    #endif

    #if 0
    WebServer::Plugin::addRestHandler(std::move(WebServer::Plugin::RestHandler(F("/api/ws"), [this](AsyncWebServerRequest *request, WebServerPlugin::RestRequest &rest) -> AsyncWebServerResponse * {
        // TODO replace AsyncJsonResponse
        // add no cache no store headers
        // #error read TODO
        auto response = __DBG_new(AsyncJsonResponse);
        auto &json = response->getJsonObject();
        auto &reader = rest.getJsonReader();

        if (_currentScreen == TEXT_UPDATE) {
            json.add(FSPGM(status), 503);
            json.add(FSPGM(message), F("Firmware update in progress"));
        }
        else if (rest.isUriMatch(FSPGM(message))) {
            auto title = reader.get(FSPGM(title)).getValue();
            auto message = reader.get(FSPGM(message)).getValue();
            auto timeout = reader.get(F("timeout")).getValue().toInt();
            auto confirm = reader.get(F("confirm")).getBooleanValue();

            // default time if not set
            if (timeout == 0) {
                timeout = 60;
            }
            // display until message has been confirmed
            if (confirm == JsonVar::BooleanValueType::TRUE) {
                timeout = 0;
            }

            if (message.length() == 0) {
                json.add(FSPGM(status), 400);
                json.add(FSPGM(message), F("Empty message"));
            }
            else {
                json.add(FSPGM(status), 200);
                json.add(FSPGM(message), AsyncWebServerResponse::responseCodeToString(200));
                displayMessage(title, message, ST77XX_YELLOW, ST77XX_WHITE, timeout);
            }
        }
        else if (rest.isUriMatch(FSPGM(display))) {
            _setScreen(((uint8_t)reader.get(F("screen")).getValue().toInt()) % NUM_SCREENS);
            json.add(FSPGM(status), 200);
            json.add(FSPGM(message), PrintString(F("Screen set to #%u"), _currentScreen));
            redraw();
        }
        else if (rest.isUriMatch(F("backlight"))) {
            auto levelVar = reader.get(F("level"));
            int level = levelVar.getValue().toInt();
            if (levelVar.getType() != JsonBaseReader::JSON_TYPE_INT || level < 0 || level > PWMRANGE) {
                json.add(FSPGM(status), 400);
                json.add(FSPGM(message), F("Backlight level out of range (0-1023)"));
            }
            else {
                json.add(FSPGM(status), 200);
                json.add(FSPGM(message), PrintString(F("Backlight level set to %u"), level));
                _fadeBacklight(_backlightLevel, level);
                _backlightLevel = level;
            }
        }
        else if (rest.isUriMatch(FSPGM(sensors))) {
            json.add(FSPGM(status), 200);
            auto &sensors = json.addArray(FSPGM(sensors));
            for(const auto sensor: SensorPlugin::getSensors()) {
                String name;
                StringVector values;
                if (sensor->getSensorData(name, values)) {
                    auto &sensor = sensors.addObject(2);
                    sensor.add(FSPGM(name), name);
                    auto &vals = sensor.addArray(FSPGM(values));
                    for(const auto &value: values) {
                        vals.add(value);
                    }
                }
            }
        }
        else if (rest.isUriMatch(nullptr)) {
            json.add(FSPGM(status), 200);
            json.add(FSPGM(message), AsyncWebServerResponse::responseCodeToString(200));
        }
        else {
            json.add(FSPGM(status), 404);
            json.add(FSPGM(message), AsyncWebServerResponse::responseCodeToString(404));
        }

        return response->finalize();
    })));
    #endif
}

void WeatherStationPlugin::_readConfig()
{
    _config = Plugins::WeatherStation::getConfig();

    _backlightLevel = std::min(PWMRANGE * _config.backlight_level / 100, PWMRANGE);

    _weatherApi.setAPIKey(WSDraw::WSConfigType::getApiKey());
    _weatherApi.setQuery(WSDraw::WSConfigType::getApiQuery());
    _weatherApi.clear();

    _setScreen(_getScreen(ScreenType::MAIN));
}

void WeatherStationPlugin::setup(SetupModeType mode, const PluginComponents::DependenciesPtr &dependencies)
{
    __LDBG_printf("setup");
    #if IOT_WEATHER_STATION_WS2812_NUM
        _rainbowStatusLED();
    #endif
    _initTFT();
    _readConfig();

    #if ESP32
        analogWriteFreq(1000);
    #elif ESP8266
        analogWriteRange(PWMRANGE);
        analogWriteFreq(1000);
    #else
        not supported
    #endif

    _fadeBacklight(0, _backlightLevel);

    // show progress bar during firmware updates
    int progressValue = -1;
    WebServer::Plugin::getInstance().setUpdateFirmwareCallback([this, progressValue](size_t position, size_t size) mutable {
        int progress = position * 100 / size;
        if (progressValue != progress) {
            if (progressValue == -1) {
                _setBacklightLevel(PWMRANGE);
                _setScreen(ScreenType::TEXT);
                LoopFunctions::remove(loop);
                #if IOT_WEATHER_STATION_WS2812_NUM
                    _Timer(_pixelTimer).remove();
                    BUILTIN_LED_SET(BlinkLEDTimer::BlinkType::OFF);
                #endif
            }
            #if IOT_WEATHER_STATION_WS2812_NUM
                // _setRGBLeds(((((100 - progress) * 0x50) / 100) << 16) | ((((progress) * 0x50) / 100) << 8)); // brightness 80
                uint8_t color2 = (progress * progress * progress * progress) / 1000000;
                _setRGBLeds(((100 - progress) << 16) | (color2 << 8)); // brightness 100
            #endif
            _drawText(PrintString(F("Updating\n%d%%"), progress), FONTS_DEFAULT_MEDIUM, COLORS_DEFAULT_TEXT, true);
            progressValue = progress;
        }
    });

    #if IOT_WEATHER_STATION_HAS_TOUCHPAD
        if (!_touchpad.begin(MPR121_I2CADDR_DEFAULT, IOT_WEATHER_STATION_MPR121_PIN, &config.initTwoWire())) {
            Logger_error(F("Failed to initialize touch sensor"));
        }
        else {
            _touchpad.getMPR121().setThresholds(_config.touch_threshold, _config.released_threshold);
        }
    #endif

    #if IOT_ALARM_PLUGIN_ENABLED
        AlarmPlugin::setCallback(alarmCallback);
    #endif

    #if IOT_WEATHER_STATION_HAS_TOUCHPAD
        _touchpad.addCallback(Mpr121Touchpad::EventType::RELEASED, 2, [this](const Mpr121Touchpad::Event &event) {
            __LDBG_printf("event=%u types=%s timeout=%d", event.getType(), event.getGesturesString(), (int32_t)(millis() - _touchpadTimer));
            #if IOT_ALARM_PLUGIN_ENABLED
                if (_resetAlarm()) {
                    _touchpadTimer = millis();
                    return false;
                }
            #endif
            #if HAVE_WEATHER_STATION_CURATED_ART
                if (_currentScreen == ScreenType::CURATED_ART && event.isSwipeAny()) {
                    if (_pickGalleryPicture()) {
                        redraw();
                    }
                    _touchpadTimer = millis();
                    return false;
                }
            #endif
            // ignore all events after the last release for 250ms
            if (millis() - _touchpadTimer < 250) {
                return false;
            }
            if (event.isSwipeRight() || event.isTap() || event.isSwipeDown()) {
                this->_setScreen(_getNextScreen(_getCurrentScreen(), true));
                _touchpadTimer = millis();
                return false;
            }
            if (event.isSwipeLeft() || event.isSwipeUp()) {
                this->_setScreen(_getPrevScreen(_getCurrentScreen(), true));
                _touchpadTimer = millis();
                return false;
            }
            return true;
        });
    #endif

    LoopFunctions::add(loop);

    _weatherApi.clear();
    WiFiCallbacks::add(WiFiCallbacks::EventType::CONNECTION, wifiCallback);
    if (WiFi.isConnected()) {
        _wifiCallback(WiFiCallbacks::EventType::CONNECTED, nullptr);
    }

    _installWebhooks();
}

void WeatherStationPlugin::reconfigure(const String &source)
{
    __LDBG_printf("reconfigure src=%s", source.c_str());
    if (source.equals(FSPGM(http))) {
        _installWebhooks();
    }
    else {
        auto oldLevel = _backlightLevel;
        _readConfig();
        #if IOT_WEATHER_STATION_HAS_TOUCHPAD
            _touchpad.getMPR121().setThresholds(_config.touch_threshold, _config.released_threshold);
        #endif
        _fadeBacklight(oldLevel, _backlightLevel);
        _setScreen(_getScreen(_currentScreen));
    }
}

void WeatherStationPlugin::shutdown()
{
    _setBacklightLevel(PWMRANGE);
    _setScreen(ScreenType::TEXT);
    LoopFunctions::remove(loop);
    _drawText(F("Rebooting\nDevice"), FONTS_DEFAULT_BIG, COLORS_DEFAULT_TEXT, true);

    #if IOT_ALARM_PLUGIN_ENABLED
        _resetAlarm();
        AlarmPlugin::setCallback(nullptr);
    #endif
    #if IOT_WEATHER_STATION_WS2812_NUM
        _Timer(_pixelTimer).remove();
    #endif
    #if IOT_WEATHER_STATION_HAS_TOUCHPAD
        _touchpad.end();
    #endif
    _Timer(_fadeTimer).remove();
    _Timer(_pollDataTimer).remove();

    stdex::reset(_canvas);
}

void WeatherStationPlugin::getStatus(Print &output)
{
    output.printf_P(PSTR("%ux%u TFT " IOT_WEATHER_STATION_DRIVER), TFT_WIDTH, TFT_HEIGHT);
    #if IOT_WEATHER_STATION_WS2812_NUM && __LED_BUILTIN == NEOPIXEL_PIN_ID
        output.printf_P(PSTR(HTML_S(br) "%ux WS2812 RGB LED"), IOT_WEATHER_STATION_WS2812_NUM);
    #endif
    #if IOT_WEATHER_STATION_HAS_TOUCHPAD
        output.printf_P(PSTR(HTML_S(br) "MPR121 Touch Pad"));
    #endif
}

void WeatherStationPlugin::_initTFT()
{
    __LDBG_printf("spi0 clk %u, spi1 clk %u", static_cast<uint32_t>(SPI0CLK) / 1000000, static_cast<uint32_t>(SPI1CLK) / 1000000);

    #if ILI9341_DRIVER
        __LDBG_printf("cs=%d dc=%d rst=%d spi=%u", TFT_PIN_CS, TFT_PIN_DC, TFT_PIN_RST, SPI_FREQUENCY);
        _tft.begin(SPI_FREQUENCY);
    #else
        #if SPI_FREQUENCY
            __LDBG_printf("cs=%d dc=%d rst=%d spi=%uMHz", TFT_PIN_CS, TFT_PIN_DC, TFT_PIN_RST, SPI_FREQUENCY / 1000000);
            _tft.setSPISpeed(SPI_FREQUENCY);
        #else
            __LDBG_printf("cs=%d dc=%d rst=%d", TFT_PIN_CS, TFT_PIN_DC, TFT_PIN_RST);
        #endif
        _tft.initR(INITR_BLACKTAB);
    #endif
    _tft.fillScreen(0);
    _tft.setRotation(0);
}

void WeatherStationPlugin::createWebUI(WebUINS::Root &webUI)
{
    webUI.addRow(WebUINS::Group(F("Weather Station"), false));
    webUI.addRow(WebUINS::Slider(F("bl_br"), F("Backlight Brightness"), false).append(WebUINS::NamedInt32(J(range_max), 1023)));
}

void WeatherStationPlugin::getValues(WebUINS::Events &array)
{
    array.append(WebUINS::Values(F("bl_br"), _backlightLevel, true));
}

void WeatherStationPlugin::setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState)
{
    __LDBG_printf("id=%s value=%s has=%u", __S(id), __S(value), hasValue);
    if (hasValue && id.equals(F("bl_br"))) {
        int level =  value.toInt();
        _fadeBacklight(_backlightLevel, level);
        _backlightLevel = level;
    }
}

void WeatherStationPlugin::_drawEnvironmentalSensor(GFXCanvasCompressed& canvas, int16_t _offsetY)
{
    auto sensor = SensorPlugin::getSensor<Sensor_BME280>(MQTT::SensorType::BME280);
    if (!sensor) {
        return;
    }
    auto values = sensor->readSensor();

    canvas.setFont(FONTS_WEATHER_DESCR);
    canvas.setTextColor(COLORS_WEATHER_DESCR);
    PrintString str(F("\n\nTemperature %.2f°C\nHumidity %.2f%%\nPressure %.2fhPa"), values.temperature, values.humidity, values.pressure);
    canvas.drawTextAligned(0, _offsetY, str, AdafruitGFXExtension::LEFT);
}

WeatherStationPlugin::IndoorValues WeatherStationPlugin::_getIndoorValues()
{
    auto sensor = SensorPlugin::getSensor<Sensor_BME280>(MQTT::SensorType::BME280);
    if (!sensor) {
        return IndoorValues(0, 0, 0);
    }
    auto values = sensor->readSensor();
    return IndoorValues(values.temperature, values.humidity, values.pressure);
}

void WeatherStationPlugin::_fadeBacklight(uint16_t fromLevel, uint16_t toLevel, uint8_t step)
{
    _setBacklightLevel(fromLevel);
    int8_t direction = fromLevel > toLevel ? -step : step;

    //TODO sometimes fading does not work...
    //probably when called multiple times before its done

    _Timer(_fadeTimer).add(Event::milliseconds(30), true, [fromLevel, toLevel, step, direction, this](Event::CallbackTimerPtr timer) mutable {

        if (abs(toLevel - fromLevel) > step) {
            fromLevel += direction;
        }
        else {
            fromLevel = toLevel;
            timer->disarm();
        }
        _setBacklightLevel(fromLevel);

    }, Event::PriorityType::TIMER);
}

void WeatherStationPlugin::_setRGBLeds(uint32_t color)
{
    #if IOT_WEATHER_STATION_WS2812_NUM
        #if HAVE_FASTLED
            fill_solid(WS2812LEDTimer::_pixels, sizeof(WS2812LEDTimer::_pixels) / 3, CRGB(color));
            FastLED.show();
        #else
            _pixels.fill(color);
            _pixels.show();
        #endif
    #endif
}

void WeatherStationPlugin::_fadeStatusLED()
{
    #if IOT_WEATHER_STATION_WS2812_NUM
        __LDBG_printf("pin=%u px_timer=%u", IOT_WEATHER_STATION_WS2812_PIN, (bool)_pixelTimer);
        if (config.isSafeMode()) {
            return;
        }

        if (_pixelTimer) {
            return;
        }

        int32_t color = 0x001500;
        int16_t dir = 0x000100;

        BUILTIN_LED_SET(BlinkLEDTimer::BlinkType::OFF);
        _setRGBLeds(color);

        _Timer(_pixelTimer).add(Event::milliseconds(50), true, [this, color, dir](::Event::CallbackTimerPtr timer) mutable {
            color += dir;
            if (color >= 0x003000) {
                dir = -dir;
                color = 0x003000;
            }
            else if (color <= 0) {
                color = 0;
                timer->disarm();
            }
            _setRGBLeds(color);

        }, ::Event::PriorityType::TIMER);

    #endif
}

void WeatherStationPlugin::_rainbowStatusLED(bool stop)
{
    #if IOT_WEATHER_STATION_WS2812_NUM
        if (config.isSafeMode()) {
            return;
        }

        if (stop) {
            _rainbowBrightness--; // reduce below kMaxBrightness to initial fade out
        }
        else {
            if (_pixelTimer) {
                return;
            }

            // max brightness and fading speed
            constexpr int16_t kMaxBrightness = 0x200;
            constexpr uint8_t kReduceBrightness = 8;
            constexpr int16_t kMaxBrightnessValue = kMaxBrightness / kReduceBrightness;
            static_assert(kMaxBrightnessValue <= 255, "brightness is limited to 255");

            // rainbow speed and colors
            constexpr uint8_t kInitialHue = 30;
            constexpr accum88 kAnimationSpeed = 7;

            _rainbowBrightness = kMaxBrightness;

            BUILTIN_LED_SET(BlinkLEDTimer::BlinkType::OFF);
            _Timer(_pixelTimer).add(Event::milliseconds(20), true, [this](::Event::CallbackTimerPtr timer) {

                if (_rainbowBrightness < kMaxBrightness) {
                    _rainbowBrightness--;
                    if (_rainbowBrightness <= 0) {
                        timer->disarm();
                        _setRGBLeds(0);
                        return;
                    }
                }

                fill_rainbow(reinterpret_cast<CRGB *>(_pixels.ptr()), _pixels.getNumPixels(), beat8(kAnimationSpeed, 255), kInitialHue);
                #if HAVE_FASTLED
                    FastLED.show(_rainbowBrightness / kReduceBrightness);
                #else
                    _pixels.show(_rainbowBrightness / kReduceBrightness);
                #endif

            }, ::Event::PriorityType::TIMER);
        }
    #endif
}

#if IOT_ALARM_PLUGIN_ENABLED

    void WeatherStationPlugin::alarmCallback(ModeType mode, uint16_t maxDuration)
    {
        _getInstance()._alarmCallback(mode, maxDuration);
    }

    void WeatherStationPlugin::_alarmCallback(ModeType mode, uint16_t maxDuration)
    {
        if (maxDuration == STOP_ALARM) {
            _resetAlarm();
            return;
        }
        if (!_resetAlarmFunc) {
            _resetAlarmFunc = [this](Event::CallbackTimerPtr timer) {
                #if IOT_WEATHER_STATION_WS2812_NUM
                    BUILTIN_LED_SET(BlinkLEDTimer::BlinkType::OFF);
                #endif
                _alarmTimer.remove(); // make sure the scheduler is not calling a dangling pointer.. not using the TimerPtr in case it is not called from the scheduler
                _resetAlarmFunc = nullptr;
            };
        }

        // check if an alarm is already active
        if (!_alarmTimer) {
            #if IOT_WEATHER_STATION_WS2812_NUM
                BlinkLEDTimer::setBlink(__LED_BUILTIN, BlinkLEDTimer::BlinkType::FAST, 0xff0000);
            #endif
        }

        if (maxDuration == 0) {
            maxDuration = 300; // limit time
        }
        // reset time if alarms overlap
        __LDBG_printf("alarm duration %u", maxDuration);
        _Timer(_alarmTimer).add(Event::seconds(maxDuration), false, _resetAlarmFunc);
    }

    bool WeatherStationPlugin::_resetAlarm()
    {
        __LDBG_printf("reset=%u state=%u", _resetAlarmFunc ? 1 : 0, AlarmPlugin::getAlarmState());
        if (_resetAlarmFunc) {
            _resetAlarmFunc(*_alarmTimer);
            AlarmPlugin::resetAlarm();
            return true;
        }
        return false;
    }

#endif

extern "C" void WeatherStationPlugin_unlock();

void WeatherStationPlugin_unlock()
{
    WeatherStationPlugin::_getInstance().unlock();
}

#endif
