/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include <WiFiCallbacks.h>
#include <LoopFunctions.h>
#include "kfc_fw_config.h"
#include "web_server.h"
#include "blink_led_timer.h"
#include "remote.h"
#include "remote_button.h"
#if HOME_ASSISTANT_INTEGRATION
#include "../src/plugins/home_assistant/home_assistant.h"
#endif
#include "plugins_menu.h"

#if DEBUG_IOT_REMOTE_CONTROL
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using KFCConfigurationClasses::Plugins;
using namespace RemoteControl;

static RemoteControlPlugin plugin;

PROGMEM_DEFINE_PLUGIN_OPTIONS(
    RemoteControlPlugin,
    "remotectrl",       // name
    "Remote Control",   // friendly name
    "",                 // web_templates
    "remote",           // config_forms
    "http",             // reconfigure_dependencies
    PluginComponent::PriorityType::MAX,
    PluginComponent::RTCMemoryId::REMOTE,
    static_cast<uint8_t>(PluginComponent::MenuType::CUSTOM),
    false,              // allow_safe_mode
    true,               // setup_after_deep_sleep
    true,               // has_get_status
    true,               // has_config_forms
    false,              // has_web_ui
    false,              // has_web_templates
    true,               // has_at_mode
    0                   // __reserved
);

RemoteControlPlugin::RemoteControlPlugin() :
    PluginComponent(PROGMEM_GET_PLUGIN_OPTIONS(RemoteControlPlugin)),
    Base()
#if HOME_ASSISTANT_INTEGRATION
    ,
    _hass(HassPlugin::getInstance())
#endif
{
    REGISTER_PLUGIN(this, "RemoteControlPlugin");
    // for(auto pin : _buttonPins) {
    //     pinMode(_buttonPins[pin], INPUT);
    // }
    for(uint8_t n = 0; n < _buttons.size(); n++) {
        _buttons[n] = std::move(Button(_buttonPins[n], n, this));
    }
}

void RemoteControlPlugin::setup(SetupModeType mode)
{
    _readConfig();

    // for(uint8_t n = 0; n < _buttons.size(); n++) {
    //     _buttons[n] = std::move(Button(_buttonPins[n], n, this));
    //     // // auto &button = *(_buttons[n] = __LDBG_new(PushButton, _buttonPins[n], PRESSED_WHEN_HIGH));
    //     // button.onPress(onButtonPressed);
    //     // button.onHoldRepeat(_config.longpressTime, _config.repeatTime, onButtonHeld);
    //     // button.onRelease(onButtonReleased);
    //     // button.update();
    //     // __LDBG_printf("btn=%d pressed=%d", n, button.isPressed());
    // }
    WiFiCallbacks::add(WiFiCallbacks::EventType::CONNECTED, wifiCallback);
    LoopFunctions::add(loop);

    _installWebhooks();

    pinMode(IOT_REMOTE_CONTROL_AWAKE_PIN, OUTPUT);
    digitalWrite(IOT_REMOTE_CONTROL_AWAKE_PIN, HIGH);
    _buttonsLocked = 0;
}

void RemoteControlPlugin::reconfigure(const String &source)
{
    _readConfig();
    if (String_equals(source, F("http"))) {
        _installWebhooks();
    }
    else {
        // buttons have this attach with &_config
        // for(const auto button : _buttons) {
        //     button->onPress(onButtonPressed);
        //     button->onHoldRepeat(_config.longpressTime, _config.repeatTime, onButtonHeld);
        //     button->onRelease(onButtonReleased);
        // }
    }
}

void RemoteControlPlugin::shutdown()
{
    LoopFunctions::remove(loop);
    WiFiCallbacks::remove(WiFiCallbacks::EventType::ANY, wifiCallback);
}

void RemoteControlPlugin::prepareDeepSleep(uint32_t sleepTimeMillis)
{
    __LDBG_printf("time=%u", sleepTimeMillis);
    digitalWrite(IOT_REMOTE_CONTROL_AWAKE_PIN, LOW);
}

void RemoteControlPlugin::getStatus(Print &output)
{
    output.printf_P(PSTR("%u Button Remote Control"), _buttons.size());
    if (_autoSleepTimeout == 0 || _autoSleepTimeout == kAutoSleepDisabled) {
        output.print(F(", auto sleep disabled"));
    }
}

void RemoteControlPlugin::createMenu()
{
    bootstrapMenu.addSubMenu(getFriendlyName(), F("remotectrl.html"), navMenu.config);
    bootstrapMenu.addSubMenu(F("Enter Deep Sleep"), F("remote-sleep.html"), navMenu.device);
    bootstrapMenu.addSubMenu(F("Disable Auto Sleep"), F("remote-nosleep.html"), navMenu.device);
}


#if AT_MODE_SUPPORTED

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(RMNOSLP, "RMNOSLP", "Disable auto sleep");

void RemoteControlPlugin::atModeHelpGenerator()
{
    auto name = getName_P();
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(RMNOSLP), name);
}

bool RemoteControlPlugin::atModeHandler(AtModeArgs &args)
{
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(RMNOSLP))) {
        args.printf_P(PSTR("Auto sleep disabled. timeout=%u"), _autoSleepTimeout);
        _autoSleepTimeout = kAutoSleepDisabled;
        return true;
    }
    return false;
}

#endif

// void Base::onButtonHeld(Button& btn, uint16_t duration, uint16_t repeatCount)
// {
//     __LDBG_printf("btn=%d duration=%d repeat=%d", plugin._getButtonNum(btn), duration, repeatCount);
//     plugin._onButtonHeld(btn, duration, repeatCount);
// }

// void RemoteControlPlugin::onButtonReleased(Button& btn, uint16_t duration)
// {
//     __LDBG_printf("btn=%d duration=%d", plugin._getButtonNum(btn), duration);
//     plugin._onButtonReleased(btn, duration);
// }

void RemoteControlPlugin::_onButtonHeld(Button& btn, uint16_t duration, uint16_t repeatCount)
{
    auto buttonNum = btn.getNum();
#if IOT_REMOTE_CONTROL_COMBO_BTN
    if (_anyButton(&btn)) { // is any button pressed except btn?
        _resetAutoSleep();
        if (duration < 5000 || _comboButton != 3) {
            if (_comboButton == -1) {
                _comboButton = buttonNum; // store first button held
                __LDBG_printf("longpress=%d combo=%d", _longPress, _comboButton);
            }
            if (buttonNum != _comboButton) { // fire event for any other button that is held except first one
                _addButtonEvent(ButtonEvent(buttonNum, EventType::COMBO_REPEAT, duration, repeatCount, _comboButton));
            }
        }
        else if (_comboButton == 3) { // button 4 has special functions
            if (duration > 5000 && _longPress == 0) {
                // indicate long press by setting LED to flicker
                __LDBG_printf("2 buttons held >5 seconds: %u", buttonNum);
                BlinkLEDTimer::setBlink(__LED_BUILTIN, BlinkLEDTimer::FLICKER);
                _longPress = 1;
                __LDBG_printf("longpress=%d combo=%d", _longPress, _comboButton);
            }
            else if (duration > 8000 && _longPress == 1) {
                __LDBG_printf("2 buttons held >8 seconds: %u", buttonNum);
                _longPress = 2;
                __LDBG_printf("longpress=%d combo=%d", _longPress, _comboButton);
                BlinkLEDTimer::setBlink(__LED_BUILTIN, BlinkLEDTimer::SOLID);
                if (buttonNum == 0) {
                    __LDBG_printf("disable auto sleep");
                    disableAutoSleep();
                }
                else if (buttonNum == 1) {
                    __LDBG_printf("restart");
                    shutdown();
                    _Scheduler.add(3000, false, [](Event::CallbackTimerPtr timer) {
                        config.restartDevice();
                    });
                }
                else if (buttonNum == 2) {
                    __LDBG_printf("restore factory settings");
                    shutdown();
                    _Scheduler.add(3000, false, [](Event::CallbackTimerPtr timer) {
                        config.restoreFactorySettings();
                        config.write();
                        config.restartDevice();
                    });
                }
            }
        }
    }
    else
#endif
    {
        _addButtonEvent(ButtonEvent(buttonNum, EventType::REPEAT, duration, repeatCount));
    }
}

void RemoteControlPlugin::_onButtonReleased(Button& btn, uint16_t duration)
{
#if IOT_REMOTE_CONTROL_COMBO_BTN
    if (_longPress) {
        if (!_anyButton()) {
            // disable LED after all buttons have been released
            BlinkLEDTimer::setBlink(__LED_BUILTIN, config.isWiFiUp() ? BlinkLEDTimer::SOLID : BlinkLEDTimer::OFF);
            __LDBG_printf("longpress=%d combo=%d", _longPress, _comboButton);
            _longPress = 0;
            _comboButton = -1;
        }
    }
    else if (_comboButton != -1) {
        _addButtonEvent(ButtonEvent(btn.getNum(), EventType::COMBO_RELEASE, duration, _comboButton));
        _comboButton = -1;
    }
    else
#endif
    {
        _addButtonEvent(ButtonEvent(btn.getNum(), EventType::RELEASE, duration));
    }
}


void RemoteControlPlugin::loop()
{
    plugin._loop();
}

void RemoteControlPlugin::wifiCallback(WiFiCallbacks::EventType event, void *payload)
{
    if (event == WiFiCallbacks::EventType::CONNECTED) {
        plugin._wifiConnected();
    }
}

void RemoteControlPlugin::disableAutoSleep()
{
    __LDBG_printf("disabled");
    plugin._autoSleepTimeout = kAutoSleepDisabled;
}

void RemoteControlPlugin::disableAutoSleepHandler(AsyncWebServerRequest *request)
{
    __LDBG_printf("is_authenticated=%u", WebServerPlugin::getInstance().isAuthenticated(request) == true);
    if (WebServerPlugin::getInstance().isAuthenticated(request) == true) {
        disableAutoSleep();
        AsyncWebServerResponse *response = request->beginResponse(302);
        HttpHeaders httpHeaders;
        httpHeaders.addNoCache(true);
        httpHeaders.add<HttpLocationHeader>(F("/status.html"));
        httpHeaders.setAsyncWebServerResponseHeaders(response);
        request->send(response);
    }
    else {
        request->send(403);
    }
}

void RemoteControlPlugin::deepSleepHandler(AsyncWebServerRequest *request)
{
    __LDBG_printf("is_authenticated=%u", WebServerPlugin::getInstance().isAuthenticated(request) == true);
    if (WebServerPlugin::getInstance().isAuthenticated(request) == true) {
        AsyncWebServerResponse *response = request->beginResponse(302);
        HttpHeaders httpHeaders;
        httpHeaders.addNoCache(true);
        httpHeaders.add<HttpLocationHeader>(F("/status.html"));
        httpHeaders.setAsyncWebServerResponseHeaders(response);
        request->send(response);

        BlinkLEDTimer::setBlink(__LED_BUILTIN, BlinkLEDTimer::FLICKER);
        disableAutoSleep();

        _Scheduler.add(2000, false, [](Event::CallbackTimerPtr timer) {
            __LDBG_printf("deep sleep=%us", plugin._config.deepSleepTime);
            config.enterDeepSleep(KFCFWConfiguration::seconds(plugin._config.deepSleepTime), RF_DEFAULT, 1);
        });
    }
    else {
        request->send(403);
    }
}

void RemoteControlPlugin::_loop()
{
    // for(const auto button : _buttons) {
    //     button->update();
    // }
    if (_autoSleepTimeout != kAutoSleepDisabled) {
        if (_isUsbPowered()) {
            if (_autoSleepTimeout) {
                __LDBG_printf("usb power detected, disabling auto sleep");
                _autoSleepTimeout = 0;
            }
        }
        else if (_autoSleepTimeout == 0) {
            uint8_t timeout = 0;
            if (config.isWiFiUp()) {
                timeout = _config.autoSleepTime;
            }
            else {
                timeout = std::max<uint8_t>(30, _config.autoSleepTime);
            }
            _autoSleepTimeout = millis() + (timeout * 1000UL);
            __LDBG_printf("auto deep sleep=%u connected=%u", _autoSleepTimeout, config.isWiFiUp());
        }
        if (_autoSleepTimeout && millis() > _autoSleepTimeout) {
            __LDBG_printf("entering deep sleep (auto=%d, time=%us)", _config.autoSleepTime, _config.deepSleepTime);
            _autoSleepTimeout = kAutoSleepDisabled;
            config.enterDeepSleep(KFCFWConfiguration::seconds(_config.deepSleepTime), RF_DEFAULT, 1);
        }
    }
}

void RemoteControlPlugin::_wifiConnected()
{
    __LDBG_printf("events=%u", _events.size());
    _resetAutoSleep();
    _scheduleSendEvents();
}


bool RemoteControlPlugin::_isUsbPowered() const
{
    return digitalRead(IOT_SENSOR_BATTERY_CHARGE_DETECTION);
}

void RemoteControlPlugin::_readConfig()
{
    _config = Plugins::RemoteControl::getConfig();
}

void RemoteControlPlugin::_installWebhooks()
{
    __LDBG_printf("server=%p", WebServerPlugin::getWebServerObject());
    WebServerPlugin::addHandler(F("/remote-sleep.html"), deepSleepHandler);
    WebServerPlugin::addHandler(F("/remote-nosleep.html"), disableAutoSleepHandler);
}


void RemoteControlPlugin::_addButtonEvent(ButtonEvent &&event)
{
    _resetAutoSleep();
    // if (_events.size() > 32) { // discard events
    //     _events.pop_front();
    // }
    _events.emplace_back(std::move(event));
    __LDBG_printf("event=%s", _events.back().toString().c_str());
    _scheduleSendEvents();
}

void RemoteControlPlugin::_sendEvents()
{
    __LDBG_printf("wifi=%u events=%u locked=%s", config.isWiFiUp(), _events.size(), String(_buttonsLocked & 0b1111, 2).c_str());

    if (config.isWiFiUp() && _events.size()) {
        for(auto iterator = _events.begin(); iterator != _events.end(); ++iterator) {
            if (iterator->getType() == EventType::NONE) {
                continue;
            }
            auto &event = *iterator;
            __LDBG_printf("event=%s locked=%u", event.toString().c_str(), _isButtonLocked(event.getButton()));
            if (_isButtonLocked(event.getButton())) {
                continue;
            }

            uint16_t actionId;
            auto &actions = _config.actions[event.getButton()];
            switch(event.getType()) {
                case EventType::REPEAT:
                    actionId = actions.repeat;
                    break;
                case EventType::RELEASE:
                    if (event.getDuration() < _config.longpressTime) {
                        actionId = actions.shortpress;
                    }
                    else {
                        actionId = actions.longpress;
                    }
                    break;
#if IOT_REMOTE_CONTROL_COMBO_BTN
                case EventType::COMBO_REPEAT:
                    actionId = event.getCombo(actions).repeat;
                    break;
                case EventType::COMBO_RELEASE:
                    if (event.getDuration() < _config.longpressTime) {
                        actionId = event.getCombo(actions).shortpress;
                    }
                    else {
                        actionId = event.getCombo(actions).longpress;
                    }
                    break;
#endif
                default:
                    actionId = 0;
                    break;
            }

#if HOME_ASSISTANT_INTEGRATION
            auto action = Plugins::HomeAssistant::getAction(actionId);
            if (action.getId()) {
                // Logger_notice(F("firing event: %s"), event.toString().c_str());

                _lockButton(event.getButton());
                // auto button = event.getButton();
                //_events.erase(iterator);
                event.remove();

                _hass.executeAction(action, [this, button](bool status) {
                    _unlockButton(button);
                    _scheduleSendEvents();
                });
                return;
            }
#endif

            __LDBG_printf("removing event with no action");
            event.remove();
        }

#if DEBUG_IOT_REMOTE_CONTROL
        uint32_t start = micros();
#endif
        noInterrupts();
        ButtonEventList tmp;
        for(auto &&item: _events) {
            if (item.getType() != EventType::NONE) {
                tmp.emplace_back(std::move(item));
            }
        }
        _events = std::move(tmp);
        interrupts();

#if DEBUG_IOT_REMOTE_CONTROL
        auto dur = micros() - start;
        __LDBG_printf("cleaning events time=%uus (_events=%u bytes)", dur, sizeof(_events));
#endif

        // __LDBG_printf("cleaning events");
        // _events.erase(std::remove_if(_events.begin(), _events.end(), [](const ButtonEvent &event) {
        //     return event.getType() == EventType::NONE;
        // }), _events.end());
    }
}

void RemoteControlPlugin::_scheduleSendEvents()
{
    if (_events.size()) {
        LoopFunctions::callOnce([this]() {
            _sendEvents();
        });
    }
}
