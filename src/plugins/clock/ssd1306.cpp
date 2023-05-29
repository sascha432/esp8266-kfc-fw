/**
 * Author: sascha_lammers@gmx.de
 */

#include "clock.h"

// ** work in progress **

#if IOT_LED_MATRIX_HAVE_SSD1306

//
// WiFI icons by Dryicons https://dryicons.com/
//
#include "graphics/wlan.h"

void ClockPlugin::ssd1306Begin()
{
    _ssd1306.begin(SSD1306_SWITCHCAPVCC, true, true);
    _ssd1306.cp437(true);

    // boot screen
    ssd1306Clear(false);
    _ssd1306.setCursor(0, 5);
    _ssd1306.println(F("KFC Firmware"));
    _ssd1306.print('v');
    _ssd1306.print(config.getShortFirmwareVersion_P());
    _ssd1306.display();

    // start updating after 15 seconds
    _Timer(_ssd1306Timer).add(Event::seconds(15), false, ssd1306InitTimer);
}

void ClockPlugin::ssd1306End()
{
    _Timer(_ssd1306Timer).remove();
    ssd1306Clear();
    WiFiCallbacks::remove(WiFiCallbacks::EventType::ANY, this);
}

void ClockPlugin::ssd1306Clear(bool display)
{
    _ssd1306.setTextColor(WHITE);
    _ssd1306.setFont(nullptr);
    _ssd1306.setTextSize(1);
    _ssd1306.clearDisplay();
    _ssd1306.setCursor(0, 0);
    if (display) {
        _ssd1306.display();
    }
}

void ClockPlugin::ssd1306Update()
{
    if (_ssd1306Blank) {
        return;
    }
    ssd1306Clear(false);
    if (WiFi.isConnected()) {
        uint8_t tmp[sizeof(wlan_icon)];
        memcpy_P(tmp, wlan_icon, sizeof(tmp));
        _ssd1306.drawBitmap(128 - 16, 0, tmp, 15, 14, WHITE);
    }
    PrintString timeStr;
    timeStr.strftime(F("%a %b %d %Y\n%H:%M:%S"), time(nullptr));
    _ssd1306.println(timeStr);

    for(const auto sensorPtr: SensorPlugin::getSensors()) {
        if (sensorPtr->getType() == SensorPlugin::SensorType::BME280) {
            auto &sensor = *reinterpret_cast<Sensor_BME280 *>(sensorPtr);
            auto data = sensor.readSensor();
            _ssd1306.printf_P(PSTR("%.1f"), data.temperature);
            _ssd1306.setCursor(_ssd1306.getCursorX(), _ssd1306.getCursorY() - 2);
            _ssd1306.print('\x09');
            _ssd1306.setCursor(_ssd1306.getCursorX(), _ssd1306.getCursorY() + 2);
            _ssd1306.printf_P(PSTR("C %.1f%% %.1fhPa\n"), data.humidity, data.pressure);
        }
        else if (sensorPtr->getType() == SensorPlugin::SensorType::CCS811) {
            auto &sensor = *reinterpret_cast<Sensor_CCS811 *>(sensorPtr);
            auto &data = sensor.readSensor();
            if (data.available) {
                _ssd1306.printf_P(PSTR("%u eCO2 %u TVOC\n"), data.eCO2, data.TVOC);
            }
        }
        else if (sensorPtr->getType() == SensorPlugin::SensorType::AMBIENT_LIGHT) {
            auto &sensor = *reinterpret_cast<Sensor_AmbientLight *>(sensorPtr);
            if (sensor.getId() == 1) {
                _ssd1306.printf_P(PSTR("%d lux\n"), sensor.getValue());
            }
        }
    }

    _ssd1306.display();
}

void ClockPlugin::ssd1306Blank(bool state)
{
    if (state == _ssd1306Blank) {
        return;
    }
    if (state) {
        _ssd1306Blank = true;
        ssd1306Clear(true);
    }
    else {
        _ssd1306Blank = false;
        ssd1306Update();
    }
}

void ClockPlugin::ssd1306InitTimer(Event::CallbackTimerPtr)
{
    auto &plugin = ClockPlugin::getInstance();

    // update wifi icon
    WiFiCallbacks::add(WiFiCallbacks::EventType::CONNECTION, ssd1306WiFiCallback, &plugin);

    for(const auto sensorPtr: SensorPlugin::getSensors()) {
        if (sensorPtr->getType() == SensorPlugin::SensorType::MOTION) {
            plugin.ssd1306Blank(!reinterpret_cast<Sensor_Motion *>(sensorPtr)->getMotionState());
            break;
        }
    }

    _Timer(plugin._ssd1306Timer).add(Event::seconds(1), true, ssd1306UpdateTimer);
}

void ClockPlugin::ssd1306UpdateTimer(Event::CallbackTimerPtr)
{
    ClockPlugin::getInstance().ssd1306Update();
}

void ClockPlugin::ssd1306WiFiCallback(WiFiCallbacks::EventType, void *)
{
    ClockPlugin::getInstance().ssd1306Update();
}

#endif
