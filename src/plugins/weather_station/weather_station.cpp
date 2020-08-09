/**
 * Author: sascha_lammers@gmx.de
 */

#include "weather_station.h"
#include <PrintHtmlEntitiesString.h>
#include <LoopFunctions.h>
#include <WiFiCallbacks.h>
#include <EventTimer.h>
#include <MicrosTimer.h>
#include <HeapStream.h>
#include <ProgmemStream.h>
#include <serial_handler.h>
#include <WebUISocket.h>
#include <AsyncBitmapStreamResponse.h>
#include <GFXCanvasRLEStream.h>
#include <EspSaveCrash.h>
#include "RestAPI.h"
#include "web_server.h"
#include "async_web_response.h"
#include "blink_led_timer.h"
#include "./plugins/sensor/sensor.h"
#include "./plugins/sensor/Sensor_BME280.h"
#if IOT_WEATHER_STATION_COMP_RH
#include "./plugins/sensor/EnvComp.h"
#endif
#include "kfc_fw_config.h"

using KFCConfigurationClasses::Plugins;

// web accesss for screen capture
//
// screen capture
// http://192.168.0.95/images/screen_capture.bmp
//
//

#if DEBUG_IOT_WEATHER_STATION
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

PROGMEM_STRING_DEF(weather_station_webui_id, "weather_station");

static WeatherStationPlugin plugin;

PROGMEM_DEFINE_PLUGIN_OPTIONS(
    WeatherStationPlugin,
    // name
    "weather",
    // friendly name
    "Weather Station",
    // web_templates
    "",
    // config_forms
    "weather",
    // reconfigure_dependencies
    "http",
    PluginComponent::PriorityType::WEATHER_STATION,
    PluginComponent::RTCMemoryId::NONE,
    static_cast<uint8_t>(PluginComponent::MenuType::AUTO),
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
    WSDraw(),
    // _updateTimer(0),
    _updateCounter(0),
    _backlightLevel(1023),
    _pollTimer(0),
    _httpClient(nullptr),
    _toggleScreenTimer(0),
    _redrawFlag(false)
{
#if DEBUG_IOT_WEATHER_STATION
    _debugDisplayCanvasBorder = false;
#endif
    REGISTER_PLUGIN(this, "WeatherStationPlugin");
#if IOT_WEATHER_STATION_HAS_TOUCHPAD
    _touchpadDebug = false;
#endif

#if SAVE_CRASH_HAVE_CALLBACKS && false
    auto nextCallback = EspSaveCrash::getCallback();
    EspSaveCrash::setCallback([this, nextCallback](const EspSaveCrash::ResetInfo_t &info) {
        if (_canvasPtr) {
            delete _canvasPtr;
            _canvasPtr = nullptr;
        }
        if (nextCallback) {
            nextCallback(info);
        }
    });
#endif
}

void WeatherStationPlugin::_sendScreenCaptureBMP(AsyncWebServerRequest *request)
{
    // __LDBG_printf("WeatherStationPlugin::_sendScreenCapture(): is_authenticated=%u", WebServerPlugin::getInstance().isAuthenticated(request) == true);

    if (WebServerPlugin::getInstance().isAuthenticated(request) == true) {
        //auto response = new AsyncClonedBitmapStreamResponse(plugin.getCanvas().clone());
        __LDBG_print("AsyncBitmapStreamResponse");
        auto response = new AsyncBitmapStreamResponse(plugin.getCanvas());
        HttpHeaders httpHeaders;
        httpHeaders.addNoCache();
        httpHeaders.setAsyncWebServerResponseHeaders(response);
        request->send(response);
    }
    else {
        request->send(403);
    }
}

void WeatherStationPlugin::_installWebhooks()
{
    __LDBG_printf("server=%p", WebServerPlugin::getWebServerObject());
    WebServerPlugin::addHandler(F("/images/screen_capture.bmp"), _sendScreenCaptureBMP);

    WebServerPlugin::addRestHandler(WebServerPlugin::RestHandler(F(KFC_RESTAPI_ENDPOINT "ws"), [this](AsyncWebServerRequest *request, WebServerPlugin::RestRequest &rest) -> AsyncWebServerResponse * {
        auto response = new AsyncJsonResponse();
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
                LoopFunctions::callOnce([this, title, message, timeout]() {
                    _displayMessage(title, message, ST77XX_YELLOW, ST77XX_WHITE, timeout);
                });
            }
        }
        else if (rest.isUriMatch(FSPGM(display))) {
            _setScreen(((uint8_t)reader.get(F("screen")).getValue().toInt()) % NUM_SCREENS);
            json.add(FSPGM(status), 200);
            json.add(FSPGM(message), PrintString(F("Screen set to #%u"), _currentScreen));
            _redraw();
        }
        else if (rest.isUriMatch(F("backlight"))) {
            auto levelVar = reader.get(F("level"));
            int level = levelVar.getValue().toInt();
            if (levelVar.getType() != JsonBaseReader::JSON_TYPE_INT || level < 0 || level > 1023) {
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
    }));
}

void WeatherStationPlugin::_readConfig()
{
    _config = Plugins::WeatherStation::getConfig();
    _backlightLevel = std::min(1024 * _config.backlight_level / 100, 1023);
}

void WeatherStationPlugin::setup(SetupModeType mode)
{
    _readConfig();
    _init();

    _weatherApi.setAPIKey(WeatherStation::getApiKey());
    _weatherApi.setQuery(WeatherStation::getApiQuery());

    _setScreen(MAIN); // TODO add config options for initial screen
    _draw();
    _fadeBacklight(0, _backlightLevel);
    int progressValue = -1;
    WebServerPlugin::getInstance().setUpdateFirmwareCallback([this, progressValue](size_t position, size_t size) mutable {
        int progress = position * 100 / size;
        if (progressValue != progress) {
            if (progressValue == -1) {
                _setScreen(TEXT_UPDATE);
            }
            setText(PrintString(F("Updating\n%d%%"), progress), FONTS_DEFAULT_MEDIUM);
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
#if IOT_WEATHER_STATION_HAS_TOUCHPAD
    // needs to be first callback to stop the event to be passed to other callbacks if the alarm is active
    _touchpad.addCallback(Mpr121Touchpad::EventType::RELEASED, 1, [this](const Mpr121Touchpad::Event &) {
        return _resetAlarm();
    });
#endif
#endif
#if IOT_WEATHER_STATION_HAS_TOUCHPAD
    _touchpad.addCallback(Mpr121Touchpad::EventType::RELEASED, 2, [this](const Mpr121Touchpad::Event &event) {
        // __LDBG_printf("event %u", event.getType());
        // if (event.isSwipeLeft() || event.isDoublePress()) {
        //     _debug_println(F("left"));
        //     _setScreen((_currentScreen + NUM_SCREENS - 1) % NUM_SCREENS);
        // }
        // else if (event.isSwipeRight() || event.isPress()) {
        //     _debug_println(F("right"));
        //     _setScreen((_currentScreen + 1) % NUM_SCREENS);
        // }
        // else {
        //     return false;
        // }
        _currentScreen = (_currentScreen + 1) % NUM_SCREENS;
        // check if screen is supposed to change automatically
        if (_config.screenTimer[_currentScreen]) {
            _toggleScreenTimer = millis() + 60000;  // leave screen on for 60 seconds
        }
        else {
            _toggleScreenTimer = 0;
        }
        _redraw();
        return true;
    });
#endif

    auto compensationCallback = [this](Sensor_BME280::SensorData_t &sensor) {
        sensor.humidity += _config.humidity_offset;
        sensor.pressure += _config.pressure_offset;
#if IOT_WEATHER_STATION_COMP_RH
        float temp = sensor.temperature + _config.temp_offset;
        sensor.humidity = EnvComp::getCompensatedRH(sensor.temperature, sensor.humidity, temp);
#else
        sensor.temperature += _config.temp_offset;
#endif
    };
    for(auto sensor: SensorPlugin::getSensors()) {
        if (sensor->getType() == MQTTSensorSensorType::ENUM::BME280) {
            reinterpret_cast<Sensor_BME280 *>(sensor)->setCompensationCallback(compensationCallback);
        }
    }

    LoopFunctions::add(loop);

#if IOT_WEATHER_STATION_WS2812_NUM
    WiFiCallbacks::add(WiFiCallbacks::EventType::CONNECTED, [this](WiFiCallbacks::EventType event, void *payload) {
        _fadeStatusLED();
    }, this);

    if (config.isWiFiUp()) {
        _fadeStatusLED();
    }
#endif

    // SerialHandler::getInstance().addHandler(serialHandler, SerialHandler::RECEIVE);

    _installWebhooks();

/*
    WsClient::addClientCallback([this](WsClient::ClientCallbackTypeEnum_t type, WsClient *client) {
        if (type == WsClient::ClientCallbackTypeEnum_t::AUTHENTICATED) {
            // draw sends the screen capture to the web socket, do it for each new client
            Scheduler.addTimer(100, false, [this](EventScheduler::TimerPtr timer) {
                _redraw();
            });
        }
    });
    */
}

void WeatherStationPlugin::reconfigure(const String &source)
{
    if (String_equals(source, SPGM(http))) {
        _installWebhooks();
    }
    else {
        auto oldLevel = _backlightLevel;
        _readConfig();

#if IOT_WEATHER_STATION_HAS_TOUCHPAD
        _touchpad.getMPR121().setThresholds(_config.touch_threshold, _config.released_threshold);
#endif
        _fadeBacklight(oldLevel, _backlightLevel);
    }
}

void WeatherStationPlugin::shutdown()
{
#if IOT_ALARM_PLUGIN_ENABLED
    _resetAlarm();
    AlarmPlugin::setCallback(nullptr);
#endif
#if IOT_WEATHER_STATION_WS2812_NUM
    _pixelTimer.remove();
#endif
#if IOT_WEATHER_STATION_HAS_TOUCHPAD
    _touchpad.end();
#endif
    _fadeTimer.remove();
    _drawText(F("Rebooting\nDevice"), FONTS_DEFAULT_BIG, COLORS_DEFAULT_TEXT, true);
    LoopFunctions::remove(loop);
    // SerialHandler::getInstance().removeHandler(serialHandler);
}

void WeatherStationPlugin::getStatus(Print &output)
{
    output.printf_P(PSTR("%ux%u TFT"), TFT_WIDTH, TFT_HEIGHT);
#if IOT_WEATHER_STATION_WS2812_NUM
    output.printf_P(PSTR(HTML_S(br) "%ux WS2812 RGB LED"), IOT_WEATHER_STATION_WS2812_NUM);
#endif
#if IOT_WEATHER_STATION_HAS_TOUCHPAD
    output.printf_P(PSTR(HTML_S(br) "MPR121 Touch Pad"));
#endif
}

void WeatherStationPlugin::createConfigureForm(FormCallbackType type, const String &formName, Form &form, AsyncWebServerRequest *request)
{
    if (!isCreateFormCallbackType(type)) {
        return;
    }

    auto &cfg = Plugins::WeatherStation::getWriteableConfig();

    auto &ui = form.getFormUIConfig();
    ui.setTitle(F("Weather Station Configuration"));
    ui.setContainerId(F("dimmer_settings"));
    ui.setStyle(FormUI::StyleType::ACCORDION);

    auto &mainGroup = form.addCardGroup(FSPGM(config), String());

    form.add(F("tf"), _H_W_STRUCT_VALUE(cfg, time_format_24h));
    form.addFormUI(F("Time Format"), FormUI::BoolItems(F("24h"), F("12h")));

    form.add(F("im"), _H_W_STRUCT_VALUE(cfg, is_metric));
    form.addFormUI(F("Units"), FormUI::BoolItems(F("Metric"), F("Imperial")));

    form.add(F("api"), _H_W_STRUCT_VALUE(cfg, weather_poll_interval));
    form.addFormUI(F("Weather Poll Interval"), FormUI::FPSuffix(FSPGM(minutes)));
    form.addValidator(FormRangeValidator(5, 240));

    form.add(F("ato"), _H_W_STRUCT_VALUE(cfg, api_timeout));
    form.addFormUI(F("API Timeout"), FormUI::FPSuffix(FSPGM(seconds)));
    form.addValidator(FormRangeValidator(30, 900));

    form.add(F("bll"), _H_W_STRUCT_VALUE(cfg, backlight_level));
    form.addFormUI(F("Backlight Level"), FormUI::Suffix('%'));
    form.addValidator(FormRangeValidator(0, 100));

    form.add(F("tth"), _H_W_STRUCT_VALUE(cfg, touch_threshold));
    form.addFormUI(F("Touch Threshold"));

    form.add(F("trt"), _H_W_STRUCT_VALUE(cfg, released_threshold));
    form.addFormUI(F("Release Threshold"));

    form.add(F("t_ofs"), _H_W_STRUCT_VALUE(cfg, temp_offset));
    form.addFormUI(F("BMP280 Temperature Offset"), FormUI::Suffix('%'));

    form.add(F("h_ofs"), _H_W_STRUCT_VALUE(cfg, humidity_offset));
    form.addFormUI(F("Humidity Offset"), FormUI::Suffix('%'));

    form.add(F("p_ofs"), _H_W_STRUCT_VALUE(cfg, pressure_offset));
    form.addFormUI(F("Pressure Offset"), FormUI::FPSuffix(FSPGM(hPa)));

    for(uint8_t i = 0; i < WeatherStationPlugin::ScreenEnum_t::NUM_SCREENS; i++) {
        PrintString str;
        if (i == 0) {
            str = F("Display screen for the specified time and switch to next one.<br>");
        }
        str.printf_P(PSTR("Screen #%u, %s:"), i + 1, WeatherStationPlugin::getScreenName(i));

        form.add(PrintString(F("st_%u"), i), _H_W_STRUCT_VALUE_TYPE(cfg, screenTimer[i], uint8_t, i));
        form.addFormUI(FormUI::Label(str, true), FormUI::FPSuffix(FSPGM(seconds)));
    }

    form.add(F("stft"), _H_W_STRUCT_VALUE(cfg, show_webui));
    form.addFormUI(F("Show TFT Contents In WebUI"), FormUI::BoolItems(FSPGM(Yes), FSPGM(No)));

    mainGroup.end();

    form.finalize();
}

void WeatherStationPlugin::loop()
{
    plugin._loop();
}

void WeatherStationPlugin::_init()
{
    pinMode(TFT_PIN_LED, OUTPUT);
    digitalWrite(TFT_PIN_LED, LOW);
    __LDBG_printf("tft: cs=%d, dc=%d, rst=%d", TFT_PIN_CS, TFT_PIN_DC, TFT_PIN_RST);
    _tft.initR(INITR_BLACKTAB);
    _tft.fillScreen(0);
    _tft.setRotation(0);
}


WeatherStationPlugin &WeatherStationPlugin::_getInstance()
{
    return plugin;
}

void WeatherStationPlugin::createWebUI(WebUI &webUI)
{
    auto row = &webUI.addRow();
    row->setExtraClass(JJ(title));
    row->addGroup(F("Weather Station"), false);

    row = &webUI.addRow();
    row->addSlider(F("bl_brightness"), F("Backlight Brightness"), 0, 1023);

    if (_config.show_webui) {
        row = &webUI.addRow();
        row->addScreen(FSPGM(weather_station_webui_id), _canvas.width(), _canvas.height());
   }
}

// void WeatherStationPlugin::serialHandler(uint8_t type, const uint8_t *buffer, size_t len)
// {
//     plugin._serialHandler(buffer, len);
// }

void WeatherStationPlugin::getValues(JsonArray &array)
{
    __LDBG_println();

    auto obj = &array.addObject(2);
    obj->add(JJ(id), F("bl_brightness"));
    obj->add(JJ(value), _backlightLevel);

    if (_config.show_webui) {
        // broadcast entire screen for each new client that connects
        LoopFunctions::callOnce([this]() {
            _broadcastCanvas(0, 0, TFT_WIDTH, TFT_HEIGHT);
        });
    }
}

void WeatherStationPlugin::setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState)
{
    __LDBG_println();

    if (String_equals(id, PSTR("bl_brightness"))) {
        if (hasValue) {
            int level =  value.toInt();
            _fadeBacklight(_backlightLevel, level);
            _backlightLevel = level;
        }
    }
}

#if AT_MODE_SUPPORTED

#include "at_mode.h"

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(WSSET, "WSSET", "<touchpad/timeformat24h/metrics/tft/scroll/stats>,<on/off/options>", "Enable/disable function");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(WSBL, "WSBL", "<level=0-1023>", "Set backlight level");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(WSU, "WSU", "<i/f>", "Update weather info/forecast");

ATModeCommandHelpArrayPtr WeatherStationPlugin::atModeCommandHelp(size_t &size) const
{
    static ATModeCommandHelpArray PROGMEM tmp = {
        PROGMEM_AT_MODE_HELP_COMMAND(WSSET),
        PROGMEM_AT_MODE_HELP_COMMAND(WSBL),
        PROGMEM_AT_MODE_HELP_COMMAND(WSU)
    };
    size = sizeof(tmp) / sizeof(tmp[0]);
    return tmp;
}

bool WeatherStationPlugin::atModeHandler(AtModeArgs &args)
{
#if IOT_WEATHER_STATION_HAS_TOUCHPAD
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(WSSET))) {
        if (args.requireArgs(1, 2)) {
            bool state = !args.isFalse(1);
            if (args.equalsIgnoreCase(0, F("touchpad"))) {
                _touchpadDebug = state;
                args.printf_P(PSTR("touchpad debug=%u"), state);
            }
            else if (args.equalsIgnoreCase(0, F("timeformat24h"))) {
                _config.time_format_24h = state;
                _draw();
                args.printf_P(PSTR("time format 24h=%u"), state);
            }
            else if (args.equalsIgnoreCase(0, FSPGM(metrics))) {
                _config.is_metric = state;
                _draw();
                args.printf_P(PSTR("metrics=%u"), state);
            }
            else if (args.equalsIgnoreCase(0, F("screen"))) {
                _setScreen((_currentScreen + 1) % NUM_SCREENS);
                _draw();
                args.printf_P(PSTR("screen=%u"), _currentScreen);
            }
            else if (args.equalsIgnoreCase(0, F("tft"))) {
                _tft.initR(INITR_BLACKTAB);
                _tft.fillScreen(state ? ST77XX_YELLOW : ST77XX_RED);
                _tft.setRotation(0);
                args.printf_P(PSTR("set color"));
            }
            else if (args.equalsIgnoreCase(0, F("scroll"))) {
                if (!_isScrolling()) {
                    _doScroll();
                    _drawEnvironmentalSensor(_scrollCanvas->getCanvas(), 0);
                    Scheduler.addTimer(20, true, [this](EventScheduler::TimerPtr timer) {
                        _scrollTimer(*this);
                    });
                }
            }
            else if (args.equalsIgnoreCase(0, F("text"))) {
                setText(args.get(1), FONTS_DEFAULT_MEDIUM);
                _setScreen(TEXT_CLEAR);
            }
            else if (args.equalsIgnoreCase(0, F("stats"))) {
                if (args.equalsIgnoreCase(1, F("print"))) {
                    for(const auto &item: _stats) {
                        PrintString str;
                        str.printf_P(PSTR("%s: "), item.first.c_str());
                        auto iterator = item.second.begin();
                        while(iterator != item.second.end()) {
                            str.printf_P(PSTR("%.2f "), *iterator);
                            ++iterator;
                        }
                        args.print(str.c_str());
                    }
                }
                else {
                    _debug_stats = state;
                }
            }
            else {
                args.print("Invalid type");
            }
        }
        return true;
    }
#endif
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(WSBL))) {
        if (args.requireArgs(1, 1)) {
            uint16_t level = args.toIntMinMax<uint16_t>(0, 0, 1023);
            _fadeBacklight(_backlightLevel, level);
            _backlightLevel = level;
            args.printf_P(PSTR("backlight=%d"), level);
        }
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(WSU))) {
        if (args.equals(0, 'f')) {
            args.print(F("Updating forecast..."));
            _getWeatherForecast([this, args](bool status) mutable {
                args.printf_P(SPGM(status__u), status);
                _redraw();
            });
        }
        else {
            args.print(F("Updating info..."));
            _getWeatherInfo([this, args](bool status) mutable {
                args.printf_P(SPGM(status__u), status);
                _redraw();
            });
        }
        return true;
    }
    return false;
}

#endif

void WeatherStationPlugin::_drawEnvironmentalSensor(GFXCanvasCompressed& canvas, uint8_t top)
{
    // __LDBG_printf("WSDraw::_drawEnvironmentalSensor()");
    _offsetY = top;

    Sensor_BME280::SensorData_t values = { NAN, NAN, NAN };
    for(auto sensor: SensorPlugin::getSensors()) {
        if (sensor->getType() == MQTTSensorSensorType::ENUM::BME280) {
            auto &bme280 = *(reinterpret_cast<Sensor_BME280 *>(sensor));
            bme280.readSensor(values);
            break;
        }
    }

    canvas.setFont(FONTS_WEATHER_DESCR);
    canvas.setTextColor(COLORS_WEATHER_DESCR);
    PrintString str(F("\n\nTemperature %.2f°C\nHumidity %.2f%%\nPressure %.2fhPa"), values.temperature, values.humidity, values.pressure);
    canvas.drawTextAligned(0, _offsetY, str, AdafruitGFXExtension::LEFT);
}

void WeatherStationPlugin::_getIndoorValues(float *data)
{
    Sensor_BME280::SensorData_t values = { NAN, NAN, NAN };
    for(auto sensor: SensorPlugin::getSensors()) {
        if (sensor->getType() == MQTTSensorSensorType::ENUM::BME280) {
            auto &bme280 = *(reinterpret_cast<Sensor_BME280 *>(sensor));
            bme280.readSensor(values);
            break;
        }
    }
    data[0] = values.temperature;
    data[1] = values.humidity;
    data[2] = values.pressure;
}

void WeatherStationPlugin::_httpRequest(const String &url, int timeout, JsonBaseReader *jsonReader, Callback_t finishedCallback)
{
    auto rest = new ::WeatherStation::RestAPI(url);
    rest->setAutoDelete(true);
    rest->call(jsonReader, std::max(15, timeout), [this, url, finishedCallback](bool status, const String &error) {
        __LDBG_printf("status=%u error=%s url=%s", status, error.c_str(), url.c_str());
        if (!status) {
            Logger_notice(F("Failed to load %s, error %s"), url.c_str(), error.c_str());
            _weatherError = F("Failed to load data");
        }
        LoopFunctions::callOnce([finishedCallback, status]() {
            finishedCallback(status);
        });
    });
}


void WeatherStationPlugin::_getWeatherInfo(Callback_t finishedCallback)
{
#if DEBUG_IOT_WEATHER_STATION
    auto prev = finishedCallback;
    finishedCallback = [this, prev](bool status) {
        _weatherApi.dump(DEBUG_OUTPUT);
        prev(status);
    };

#if 0
    if (_weatherError.indexOf("avail") != -1) { // load dummy data
        _weatherError = F("Dummy data");
        PGM_P data = PSTR("{\"coord\":{\"lon\":-123.07,\"lat\":49.32},\"weather\":[{\"id\":500,\"main\":\"Rain\",\"description\":\"light rain\",\"icon\":\"10n\"},{\"id\":701,\"main\":\"Mist\",\"description\":\"mist\",\"icon\":\"50n\"}],\"base\":\"stations\",\"main\":{\"temp\":277.55,\"pressure\":1021,\"humidity\":100,\"temp_min\":275.37,\"temp_max\":279.26},\"visibility\":8047,\"wind\":{\"speed\":1.19,\"deg\":165},\"rain\":{\"1h\":0.93},\"clouds\":{\"all\":90},\"dt\":1575357173,\"sys\":{\"type\":1,\"id\":5232,\"country\":\"CA\",\"sunrise\":1575301656,\"sunset\":1575332168},\"timezone\":-28800,\"id\":6090785,\"name\":\"North Vancouver\",\"cod\":200}");
        ProgmemStream stream(data, strlen(data));
        _weatherApi.clear();
        if (!_weatherApi.parseWeatherData(stream)) {
            _weatherError = F("Invalid data");
        }
        finishedCallback(true);
        return;
    }
#endif

#endif

    _httpRequest(_weatherApi.getWeatherApiUrl(), WeatherStation::getConfig().api_timeout, _weatherApi.getWeatherInfoParser(), finishedCallback);
}

void WeatherStationPlugin::_getWeatherForecast(Callback_t finishedCallback)
{
#if DEBUG_IOT_WEATHER_STATION
    auto prev = finishedCallback;
    finishedCallback = [this, prev](bool status) {
        _weatherApi.dump(DEBUG_OUTPUT);
        prev(status);
    };
#endif

    _httpRequest(_weatherApi.getForecastApiUrl(), WeatherStation::getConfig().api_timeout, _weatherApi.getWeatherForecastParser(), finishedCallback);
}

void WeatherStationPlugin::_fadeBacklight(uint16_t fromLevel, uint16_t toLevel, int8_t step)
{
    int8_t direction = fromLevel > toLevel ? -step : step;
    analogWrite(TFT_PIN_LED, fromLevel);

    if (fromLevel != toLevel) {
        _fadeTimer.add(10, true, [fromLevel, toLevel, direction, step](EventScheduler::TimerPtr timer) mutable {
            if (abs(toLevel - fromLevel) > step) {
                fromLevel += direction;
            } else {
                fromLevel = toLevel;
                timer->detach();
            }
            analogWrite(TFT_PIN_LED, fromLevel);
        }, EventScheduler::PRIO_HIGH);
    }
}

void WeatherStationPlugin::_fadeStatusLED()
{
#if IOT_WEATHER_STATION_WS2812_NUM
    uint32_t color = 0x001500;
    int16_t dir = 0x100;
    NeoPixel_fillColor(_pixels, sizeof(_pixels), color);
    NeoPixel_espShow(IOT_WEATHER_STATION_WS2812_PIN, _pixels, sizeof(_pixels), true);
    _pixelTimer.add(50, true, [this, color, dir](EventScheduler::TimerPtr timer) mutable {
        color += dir;
        if (color >= 0x003000) {
            dir = -dir;
            color = 0x003000;
        }
        else if (color == 0) {
            timer->detach();
        }
        NeoPixel_fillColor(_pixels, sizeof(_pixels), color);
        NeoPixel_espShow(IOT_WEATHER_STATION_WS2812_PIN, _pixels, sizeof(_pixels), true);
    });
#endif
}

void WeatherStationPlugin::_loop()
{
#if IOT_WEATHER_STATION_HAS_TOUCHPAD
    if (_touchpadDebug) {
        SpeedBooster speedBooster;

        const auto &coords = _touchpad.get();
        uint16_t colorTouched = ST77XX_RED;
        uint16_t colorPredict = ST77XX_YELLOW;
        uint16_t colorGrid = ST77XX_WHITE;
        _canvas.fillScreen(COLORS_BACKGROUND);
        for(int yy = 1; yy <= 8; yy++) {
            _canvas.drawLine(10, yy * 10, 8 * 14, yy * 10, yy == coords.y ? colorTouched : (yy == _touchpad._event._predict.y ? colorPredict : colorGrid));
        }
        for(int xx = 1; xx <= 13; xx++) {
            _canvas.drawLine(xx * 8, 10, xx * 8, 8 * 10, xx == coords.x ? colorTouched : (xx == _touchpad._event._predict.x ? colorPredict : colorGrid));
        }

        _displayScreen(0, 0, TFT_WIDTH, 85);
        return;
    }
#endif

    if (_config.weather_poll_interval && millis() > _pollTimer) {
        if (config.isWiFiUp()) {
            _debug_println(F("polling weather info"));
            _pollTimer = millis() + _config.getPollIntervalMillis();
            _getWeatherInfo([this](bool status) {
                if (!status) {
                    _pollTimer = millis() + 60000;
                }
                _redraw();
            });
        }
        else {
            _pollTimer = millis() + 1000;
        }
    }

    if (_redrawFlag) {
        _draw();
        _redrawFlag = false;
    }
    else if (_currentScreen == TEXT_CLEAR || _currentScreen == TEXT_UPDATE) {
        _draw();
    }
    else {

        if (_displayMessageTimer.active()) {
            return;
        }

        if (_toggleScreenTimer && millis() > _toggleScreenTimer) {
            // __LDBG_printf("_toggleScreenTimer %lu", _toggleScreenTimer);
            _setScreen((_currentScreen + 1) % NUM_SCREENS);
            _draw();
            return;
        }

        time_t _time = time(nullptr);
        if (_currentScreen < NUM_SCREENS && _lastTime != _time) {
            _updateCounter++;
            do {
                if (/*_currentScreen == ScreenEnum_t::MAIN &&*/ (_updateCounter % 60 == 0)) {
                    // redraw all screens once per minute
                    _draw();
                    break;
                }
                else if (_currentScreen == ScreenEnum_t::INDOOR && (_updateCounter % 5 == 0)) {
                    // update indoor section every 5 seconds
                    _updateScreenIndoor();
                }
                else if (_currentScreen == ScreenEnum_t::MAIN && (_updateCounter % 5 == 0)) {
                    // update indoor section every 5 seconds
                    _updateWeatherIndoor();
                }

                if (_time - _lastTime > 10) {
                    // time jumped, redraw everything
                    _draw();
                }
                else {
                    _updateTime();
                }
            } while(false);
        }

    }
}

void WeatherStationPlugin::_broadcastCanvas(int16_t x, int16_t y, int16_t w, int16_t h)
{
    if (!_config.show_webui) {
        return;
    }
    auto webSocketUI = WsWebUISocket::getWsWebUI();
    //__LDBG_printf("x=%d y=%d w=%d h=%d ws=%p empty=%u", x, y, w, h, webSocketUI, webSocketUI->getClients().isEmpty());
    if (webSocketUI && !webSocketUI->getClients().isEmpty()) {
        Buffer buffer;

        WebSocketBinaryPacketUnqiueId_t packetIdentifier = RGB565_RLE_COMPRESSED_BITMAP;
        buffer.write(reinterpret_cast<uint8_t *>(&packetIdentifier), sizeof(packetIdentifier));

        const uint8_t len = strlen_P(SPGM(weather_station_webui_id));
        buffer.write(len);
        buffer.write_P(SPGM(weather_station_webui_id), len);

        GFXCanvasRLEStream stream(getCanvas(), x, y, w, h);
        while(stream.available()) {
            buffer.write(stream.read());
        }

        auto wsBuffer = webSocketUI->makeBuffer(buffer.get(), buffer.length());
        // __LDBG_printf("buf=%p len=%u", wsBuffer, buffer.length());
        if (wsBuffer) {
            wsBuffer->lock();
            for(auto socket: webSocketUI->getClients()) {
                if (socket->status() == WS_CONNECTED && socket->_tempObject && reinterpret_cast<WsClient *>(socket->_tempObject)->isAuthenticated()) {
                    socket->binary(wsBuffer);
                }
            }
            wsBuffer->unlock();
            webSocketUI->_cleanBuffers();
        }
    }
}

void WeatherStationPlugin::_setScreen(uint8_t screen)
{
    if (screen < NUM_SCREENS) {
        auto time = _config.screenTimer[screen];
        auto next = _getNextScreen(screen);
        __LDBG_printf("set screen=%u time=%u next=%u", screen, time, next);
        if (time && next != screen) {
            _currentScreen = screen;
            _toggleScreenTimer = millis() + (time * 1000UL);
            return;
        }
        // else ... current screen has no timer or no other screens are available
    }
    // else ... no screen active

    __LDBG_printf("timer removed screen=%u current=%u", screen, _currentScreen);
    _currentScreen = screen;
    _toggleScreenTimer = 0;
}

uint8_t WeatherStationPlugin::_getNextScreen(uint8_t screen)
{
    for(uint8_t i = screen + 1; i < screen + NUM_SCREENS; i++) {
        if (_config.screenTimer[i % NUM_SCREENS]) {
            return i % NUM_SCREENS;
        }
    }
    return screen;
}

#if IOT_ALARM_PLUGIN_ENABLED

void WeatherStationPlugin::alarmCallback(Alarm::AlarmModeType mode, uint16_t maxDuration)
{
    plugin._alarmCallback(mode, maxDuration);
}

void WeatherStationPlugin::_alarmCallback(Alarm::AlarmModeType mode, uint16_t maxDuration)
{
    if (maxDuration == Alarm::STOP_ALARM) {
        _resetAlarm();
        return;
    }
    if (!_resetAlarmFunc) {
        _resetAlarmFunc = [this](EventScheduler::TimerPtr) {
#if IOT_WEATHER_STATION_WS2812_NUM
            BlinkLEDTimer::setBlink(__LED_BUILTIN, BlinkLEDTimer::OFF);
#endif
            _alarmTimer.remove(); // make sure the scheduler is not calling a dangling pointer.. not using the TimerPtr in case it is not called from the scheduler
            _resetAlarmFunc = nullptr;
        };
    }

    // check if an alarm is already active
    if (!_alarmTimer.active()) {
#if IOT_WEATHER_STATION_WS2812_NUM
        BlinkLEDTimer::setBlink(__LED_BUILTIN, BlinkLEDTimer::FAST, 0xff0000);
#endif
    }

    if (maxDuration == 0) {
        maxDuration = 300; // limit time
    }
    // reset time if alarms overlap
    __LDBG_printf("alarm duration %u", maxDuration);
    _alarmTimer.add(maxDuration * 1000UL, false, _resetAlarmFunc);
}

bool WeatherStationPlugin::_resetAlarm()
{
    debug_printf_P(PSTR("alarm_func=%u alarm_state=%u"), _resetAlarmFunc ? 1 : 0, AlarmPlugin::getAlarmState());
    if (_resetAlarmFunc) {
        _resetAlarmFunc(nullptr);
        AlarmPlugin::resetAlarm();
        return true;
    }
    return false;
}

#endif
