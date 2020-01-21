/**
  Author: sascha_lammers@gmx.de
*/

#if IOT_REMOTE_CONTROL

#include <WiFiCallbacks.h>
#include <LoopFunctions.h>
#include "kfc_fw_config.h"
#include "progmem_data.h"
#include "web_server.h"
#include "remote.h"

#if DEBUG_IOT_REMOTE_CONTROL
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

static RemoteControlPlugin plugin;

RemoteControlPlugin::RemoteControlPlugin() : MQTTComponent(SWITCH), _autoSleepTime(10), _autoSleepTimeout(0)
{
    REGISTER_PLUGIN(this, "RemoteControlPlugin");
}

void RemoteControlPlugin::setup(PluginSetupMode_t mode)
{
    pinMode(IOT_REMOTE_CONTROL_AWAKE_PIN, OUTPUT);

    for(uint8_t n = 0; n < IOT_REMOTE_CONTROL_BUTTON_COUNT; n++) {
        auto &button = *(_buttons[n] = new PushButton(_buttonPins[n], PRESSED_WHEN_HIGH));
        button.onPress(onButtonPressed);
        button.onRelease(onButtonReleased);
        button.update();
        _debug_printf_P(PSTR("RemoteControlPlugin::setup(): btn=%d, pressed=%d\n"), n, button.isPressed());
    }
    WiFiCallbacks::add(WiFiCallbacks::CONNECTED, wifiCallback);
    LoopFunctions::add(loop);
    _timer.add(1000, true, timerCallback);

    _installWebhooks();

    digitalWrite(IOT_REMOTE_CONTROL_AWAKE_PIN, HIGH);
}

void RemoteControlPlugin::reconfigure(PGM_P source)
{
    if (source != nullptr) {
        _installWebhooks();
    }
}

void RemoteControlPlugin::restart()
{
    _timer.remove();
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
    bootstrapMenu.addSubMenu(F("Disable Auto Sleep"), F("remote_nosleep.html"), navMenu.device);
}

#if AT_MODE_SUPPORTED

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(RMNOSLP, "RMNOSLP", "Disable auto sleep");

void RemoteControlPlugin::atModeHelpGenerator()
{
    auto name = getName();
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(RMNOSLP), name);
}

bool RemoteControlPlugin::atModeHandler(Stream &serial, const String &command, AtModeArgs &args)
{
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(RMNOSLP))) {
        args.printf_P(PSTR("Auto sleep disabled. timeout=%u, max deep sleep=%lu"), _autoSleepTimeout, (unsigned long)ESP.deepSleepMax());
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
    plugin._resetAutoSleep();
    Logger_notice(F("button pressed btn=%d"), plugin._getButtonNum(btn));
}

void RemoteControlPlugin::onButtonHeld(Button& btn, uint16_t duration, uint16_t repeatCount)
{
    _debug_printf_P(PSTR("RemoteControlPlugin::onButtonHeld(): btn=%d, duration=%u, repeatCount=%u\n"), plugin._getButtonNum(btn), duration, repeatCount);
    plugin._resetAutoSleep();
}

void RemoteControlPlugin::onButtonReleased(Button& btn, uint16_t duration)
{
    _debug_printf_P(PSTR("RemoteControlPlugin::onButtonReleased(): btn=%d, duration=%u\n"), plugin._getButtonNum(btn), duration);
    plugin._resetAutoSleep();
    Logger_notice(F("button released btn=%d, duration=%u"), plugin._getButtonNum(btn), duration);
}

void RemoteControlPlugin::loop()
{
    plugin._loop();
}

void RemoteControlPlugin::timerCallback(EventScheduler::TimerPtr)
{
    plugin._timerCallback();
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

void RemoteControlPlugin::_loop()
{
    for(uint8_t n = 0; n < IOT_REMOTE_CONTROL_BUTTON_COUNT; n++) {
        _buttons[n]->update();
    }
    if (_autoSleepTimeout != (uint32_t)~0) {
        if (_isUsbPowered()) {
            if (_autoSleepTimeout) {
                _debug_printf_P(PSTR("RemoteControlPlugin::_loop(): usb power detected, disabling auto sleep\n"));
                _autoSleepTimeout = 0;
            }
        }
        else if (_autoSleepTimeout == 0) {
            _autoSleepTimeout = millis() + (_autoSleepTime * 1000UL);
            _debug_printf_P(PSTR("RemoteControlPlugin::_loop(): auto deep sleep set %u\n"), _autoSleepTimeout);
        }
        if (_autoSleepTimeout && millis() > _autoSleepTimeout) {
            _debug_printf_P(PSTR("RemoteControlPlugin::_loop(): entering deep sleep (auto=%d)\n"), _autoSleepTime);
            _autoSleepTimeout = ~0;
            config.enterDeepSleep(3600UL * 1000UL, RF_DEFAULT, 1);
        }
    }
}

void RemoteControlPlugin::_timerCallback()
{
    // _debug_printf_P(PSTR("RemoteControlPlugin::_timerCallback(): ADC=%d, GPIO5=%u\n"), analogRead(A0), _isUsbPowered());

}

void RemoteControlPlugin::_wifiConnected()
{
    _debug_println(F("RemoteControlPlugin::_wifiConnected(): WiFi connected event"));
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

void RemoteControlPlugin::_installWebhooks()
{
    _debug_printf_P(PSTR("RemoteControlPlugin::_installWebhooks(): Installing web handler, server=%p\n"), get_web_server_object());
    web_server_add_handler(F("/remote_nosleep.html"), disableAutoSleepHandler);
}

void RemoteControlPlugin::_resetAutoSleep()
{
    if (_autoSleepTimeout != 0 && _autoSleepTimeout != (uint32_t)~0) {
        _autoSleepTimeout = millis() + (_autoSleepTime * 1000UL);
        _debug_printf_P(PSTR("RemoteControlPlugin::_resetAutoSleep(): auto deep sleep set %u\n"), _autoSleepTimeout);
    }
}

#endif
