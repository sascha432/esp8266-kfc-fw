/**
  Author: sascha_lammers@gmx.de
*/

#if IOT_REMOTE_CONTROL

#include <WiFiCallbacks.h>
#include <LoopFunctions.h>
#include "kfc_fw_config.h"
#include "progmem_data.h"
#include "web_server.h"
#include "blink_led_timer.h"
#include "remote.h"

#if DEBUG_IOT_REMOTE_CONTROL
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

static RemoteControlPlugin plugin;

RemoteControlPlugin::RemoteControlPlugin() : MQTTComponent(SWITCH), _autoSleepTimeout(0)
{
    REGISTER_PLUGIN(this, "RemoteControlPlugin");
    _config.config.autoSleepTime = 15;
}

void RemoteControlPlugin::setup(PluginSetupMode_t mode)
{
    _readConfig();

    for(uint8_t n = 0; n < IOT_REMOTE_CONTROL_BUTTON_COUNT; n++) {
        auto &button = *(_buttons[n] = new PushButton(_buttonPins[n], PRESSED_WHEN_HIGH));
        button.onPress(onButtonPressed);
        button.onHoldRepeat(_config.config.longpressTime, _config.config.repeatTime, onButtonHeld);
        button.onRelease(onButtonReleased);
        button.update();
        _debug_printf_P(PSTR("RemoteControlPlugin::setup(): btn=%d, pressed=%d\n"), n, button.isPressed());
    }
    WiFiCallbacks::add(WiFiCallbacks::CONNECTED, wifiCallback);
    LoopFunctions::add(loop);

    _installWebhooks();

    pinMode(IOT_REMOTE_CONTROL_AWAKE_PIN, OUTPUT);
    digitalWrite(IOT_REMOTE_CONTROL_AWAKE_PIN, HIGH);

    Scheduler.addTimer(1000, true, [](EventScheduler::TimerPtr) { // debug, send in bulk
        plugin._sendEvents();
    });
}

void RemoteControlPlugin::reconfigure(PGM_P source)
{
    _readConfig();
    if (source != nullptr) {
        _installWebhooks();
    }
    else {
        for(uint8_t n = 0; n < IOT_REMOTE_CONTROL_BUTTON_COUNT; n++) {
            auto &button = *_buttons[n];
            button.onPress(onButtonPressed);
            button.onHoldRepeat(_config.config.longpressTime, _config.config.repeatTime, onButtonHeld);
            button.onRelease(onButtonReleased);
        }
    }
}

void RemoteControlPlugin::restart()
{
    LoopFunctions::remove(loop);
    WiFiCallbacks::remove(WiFiCallbacks::ANY, wifiCallback);
}

void RemoteControlPlugin::prepareDeepSleep(uint32_t sleepTimeMillis)
{
    _debug_printf_P(PSTR("RemoteControlPlugin::prepareDeepSleep()\n"));
    digitalWrite(IOT_REMOTE_CONTROL_AWAKE_PIN, LOW);
}

void RemoteControlPlugin::getStatus(Print &output)
{
    output.printf_P(PSTR("%u Button Remote Control"), IOT_REMOTE_CONTROL_BUTTON_COUNT);
    if (_autoSleepTimeout != 0 || _autoSleepTimeout == (uint32_t)~0) {
        output.print(F(", auto sleep disabled"));
    }
}

void RemoteControlPlugin::createMenu()
{
    bootstrapMenu.addSubMenu(F("Enter Deep Sleep"), F("remote_sleep.html"), navMenu.device);
    bootstrapMenu.addSubMenu(F("Disable Auto Sleep"), F("remote_nosleep.html"), navMenu.device);
}

#if AT_MODE_SUPPORTED

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(RMNOSLP, "RMNOSLP", "Disable auto sleep");

void RemoteControlPlugin::atModeHelpGenerator()
{
    auto name = getName();
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(RMNOSLP), name);
}

bool RemoteControlPlugin::atModeHandler(AtModeArgs &args)
{
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(RMNOSLP))) {
        args.printf_P(PSTR("Auto sleep disabled. timeout=%u"), _autoSleepTimeout);
        _autoSleepTimeout = ~0;
        return true;
    }
    return false;
}

#endif

void RemoteControlPlugin::createAutoDiscovery(MQTTAutoDiscovery::Format_t format, MQTTAutoDiscoveryVector &vector)
{
}

void RemoteControlPlugin::onConnect(MQTTClient *client)
{

}

void RemoteControlPlugin::onButtonPressed(Button& btn)
{
    _debug_printf_P(PSTR("RemoteControlPlugin::onButtonPressed(): btn=%d\n"), plugin._getButtonNum(btn));
    plugin._addButtonEvent(ButtonEvent(plugin._getButtonNum(btn), ButtonEvent::PRESS));
}

void RemoteControlPlugin::onButtonHeld(Button& btn, uint16_t duration, uint16_t repeatCount)
{
    _debug_printf_P(PSTR("RemoteControlPlugin::onButtonHeld(): btn=%d, duration=%u, repeatCount=%u\n"), plugin._getButtonNum(btn), duration, repeatCount);
    plugin._addButtonEvent(ButtonEvent(plugin._getButtonNum(btn), ButtonEvent::HELD, duration, repeatCount));
}

void RemoteControlPlugin::onButtonReleased(Button& btn, uint16_t duration)
{
    _debug_printf_P(PSTR("RemoteControlPlugin::onButtonReleased(): btn=%d, duration=%u\n"), plugin._getButtonNum(btn), duration);
    plugin._addButtonEvent(ButtonEvent(plugin._getButtonNum(btn), ButtonEvent::RELEASE, duration));
}

void RemoteControlPlugin::loop()
{
    plugin._loop();
}

void RemoteControlPlugin::wifiCallback(uint8_t event, void *payload)
{
    if (event == WiFiCallbacks::CONNECTED) {
        plugin._wifiConnected();
    }
}

void RemoteControlPlugin::disableAutoSleep()
{
    _debug_printf_P(PSTR("RemoteControlPlugin::disableAutoSleep(): disabled\n"));
    plugin._autoSleepTimeout = ~0;
}

void RemoteControlPlugin::disableAutoSleepHandler(AsyncWebServerRequest *request)
{
    _debug_printf_P(PSTR("RemoteControlPlugin::disableAutoSleepHandler(): is_authenticated=%u\n"), web_server_is_authenticated(request));
    if (web_server_is_authenticated(request)) {
        disableAutoSleep();
        AsyncWebServerResponse *response = request->beginResponse(302);
        HttpHeaders httpHeaders;
        httpHeaders.addNoCache(true);
        httpHeaders.add(new HttpLocationHeader(F("/status.html")));
        httpHeaders.setWebServerResponseHeaders(response);
        request->send(response);
    }
    else {
        request->send(403);
    }
}

void RemoteControlPlugin::deepSleepHandler(AsyncWebServerRequest *request)
{
    _debug_printf_P(PSTR("RemoteControlPlugin::deepSleepHandler(): is_authenticated=%u\n"), web_server_is_authenticated(request));
    if (web_server_is_authenticated(request)) {
        AsyncWebServerResponse *response = request->beginResponse(302);
        HttpHeaders httpHeaders;
        httpHeaders.addNoCache(true);
        httpHeaders.add(new HttpLocationHeader(F("/status.html")));
        httpHeaders.setWebServerResponseHeaders(response);
        request->send(response);

        BlinkLEDTimer::setBlink(BlinkLEDTimer::FLICKER);
        disableAutoSleep();

        Scheduler.addTimer(2000, false, [](EventScheduler::TimerPtr) {
            config.enterDeepSleep(plugin._config.config.deepSleepTime * 1000000ULL, RF_DEFAULT, 1);
        });
    }
    else {
        request->send(403);
    }
}

void RemoteControlPlugin::_loop()
{
    for(uint8_t n = 0; n < IOT_REMOTE_CONTROL_BUTTON_COUNT; n++) {
        // _buttons[n]->update();
    }
    if (_autoSleepTimeout != (uint32_t)~0) {
        if (_isUsbPowered()) {
            if (_autoSleepTimeout) {
                _debug_printf_P(PSTR("RemoteControlPlugin::_loop(): usb power detected, disabling auto sleep\n"));
                _autoSleepTimeout = 0;
            }
        }
        else if (_autoSleepTimeout == 0) {
            _autoSleepTimeout = millis() + (_config.config.autoSleepTime * 1000UL);
            _debug_printf_P(PSTR("RemoteControlPlugin::_loop(): auto deep sleep set %u\n"), _autoSleepTimeout);
        }
        if (_autoSleepTimeout && millis() > _autoSleepTimeout) {
            _debug_printf_P(PSTR("RemoteControlPlugin::_loop(): entering deep sleep (auto=%d, time=%us)\n"), _config.config.autoSleepTime, _config.config.deepSleepTime);
            _autoSleepTimeout = ~0;
            config.enterDeepSleep(_config.config.deepSleepTime * 1000000ULL, RF_DEFAULT, 1);
        }
    }
}

void RemoteControlPlugin::_wifiConnected()
{
    _debug_println(F("RemoteControlPlugin::_wifiConnected(): WiFi connected event"));
    _sendEvents();
}

int8_t RemoteControlPlugin::_getButtonNum(Button &btn) const
{
    for(int8_t n = 0; n < IOT_REMOTE_CONTROL_BUTTON_COUNT; n++) {
        if (_buttons[n] == &btn) {
            return n;
        }
    }
    return -1;
}

bool RemoteControlPlugin::_isUsbPowered() const
{
    return digitalRead(IOT_SENSOR_BATTERY_CHARGE_DETECTION);
}

void RemoteControlPlugin::_readConfig()
{
    _config = config._H_GET(Config().remote_control);
    _config.validate();
}

void RemoteControlPlugin::_installWebhooks()
{
    _debug_printf_P(PSTR("RemoteControlPlugin::_installWebhooks(): Installing web handlers, server=%p\n"), get_web_server_object());
    web_server_add_handler(F("/remote_sleep.html"), deepSleepHandler);
    web_server_add_handler(F("/remote_nosleep.html"), disableAutoSleepHandler);
}

void RemoteControlPlugin::_resetAutoSleep()
{
    if (_autoSleepTimeout != 0 && _autoSleepTimeout != (uint32_t)~0) {
        _autoSleepTimeout = millis() + (_config.config.autoSleepTime * 1000UL);
        _debug_printf_P(PSTR("RemoteControlPlugin::_resetAutoSleep(): auto deep sleep set %u\n"), _autoSleepTimeout);
    }
}

void RemoteControlPlugin::_addButtonEvent(ButtonEvent &&event)
{
    _resetAutoSleep();
    if (_events.size() > 32) {
        _events.pop_front();
    }
    _events.emplace_back(event);
    // _sendEvents();
}

void RemoteControlPlugin::_sendEvents()
{
    if (config.isWiFiUp() && !_events.empty()) {
        PrintString str;
        int n = 0;
        for(auto event: _events) {
            if (n++ != 0) {
                str.print(F("; "));
            }
            event.printTo(str);
        }
        _events.clear();

        Logger_notice(F("events: %s"), str.c_str());
    }
}

#endif
