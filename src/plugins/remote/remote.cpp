/**
  Author: sascha_lammers@gmx.de
*/

#include <WiFiCallbacks.h>
#include <LoopFunctions.h>
#include <KFCForms.h>
#include "kfc_fw_config.h"
#include "web_server.h"
#include "blink_led_timer.h"
#include "remote.h"
#include "./plugins/home_assistant/home_assistant.h"
#include "plugins_menu.h"

#if DEBUG_IOT_REMOTE_CONTROL
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using KFCConfigurationClasses::Plugins;

static RemoteControlPlugin plugin;

RemoteControlPlugin::RemoteControlPlugin() : _autoSleepTimeout(0), _buttonsLocked(~0), _longPress(0), _comboButton(-1), _hass(HassPlugin::getInstance())
{
    REGISTER_PLUGIN(this, "RemoteControlPlugin");
    _config.autoSleepTime = 15;
    for(auto pin : _buttonPins) {
        pinMode(_buttonPins[pin], INPUT);
    }
}

void RemoteControlPlugin::setup(SetupModeType mode)
{
    _readConfig();

    for(uint8_t n = 0; n < _buttons.size(); n++) {
        auto &button = *(_buttons[n] = new PushButton(_buttonPins[n], PRESSED_WHEN_HIGH));
        button.onPress(onButtonPressed);
        button.onHoldRepeat(_config.longpressTime, _config.repeatTime, onButtonHeld);
        button.onRelease(onButtonReleased);
        button.update();
        __LDBG_printf("btn=%d pressed=%d", n, button.isPressed());
    }
    WiFiCallbacks::add(WiFiCallbacks::EventType::CONNECTED, wifiCallback);
    LoopFunctions::add(loop);

    _installWebhooks();

    pinMode(IOT_REMOTE_CONTROL_AWAKE_PIN, OUTPUT);
    digitalWrite(IOT_REMOTE_CONTROL_AWAKE_PIN, HIGH);
    _buttonsLocked = 0;
}

void RemoteControlPlugin::reconfigure(PGM_P source)
{
    _readConfig();
    if (source != nullptr) {
        _installWebhooks();
    }
    else {
        for(const auto button : _buttons) {
            button->onPress(onButtonPressed);
            button->onHoldRepeat(_config.longpressTime, _config.repeatTime, onButtonHeld);
            button->onRelease(onButtonReleased);
        }
    }
}

void RemoteControlPlugin::shutdown()
{
    LoopFunctions::remove(loop);
    WiFiCallbacks::remove(WiFiCallbacks::ANY, wifiCallback);
}

void RemoteControlPlugin::prepareDeepSleep(uint32_t sleepTimeMillis)
{
    __LDBG_printf("time=%u", sleepTimeMillis);
    digitalWrite(IOT_REMOTE_CONTROL_AWAKE_PIN, LOW);
}

void RemoteControlPlugin::getStatus(Print &output)
{
    output.printf_P(PSTR("%u Button Remote Control"), _buttons.size());
    if (_autoSleepTimeout == 0 || _autoSleepTimeout == AutoSleepDisabled) {
        output.print(F(", auto sleep disabled"));
    }
}

void RemoteControlPlugin::createMenu()
{
    bootstrapMenu.addSubMenu(getFriendlyName(), F("remotectrl.html"), navMenu.config);
    bootstrapMenu.addSubMenu(F("Enter Deep Sleep"), F("remote_sleep.html"), navMenu.device);
    bootstrapMenu.addSubMenu(F("Disable Auto Sleep"), F("remote_nosleep.html"), navMenu.device);
}

void RemoteControlPlugin::createConfigureForm(AsyncWebServerRequest *request, Form &form)
{
    using KFCConfigurationClasses::MainConfig;

    form.setFormUI(F("Remote Control Configuration"));

    form.add<uint8_t>(F("auto_sleep_time"), _H_STRUCT_VALUE(MainConfig().plugins.remotecontrol, autoSleepTime))
        ->setFormUI(new FormUI::UI(FormUI::Type::TEXT, F("Auto Sleep Time")))->setSuffix(FSPGM(seconds, "seconds")));
    form.add<uint16_t>(F("deep_sleep_time"), _H_STRUCT_VALUE(MainConfig().plugins.remotecontrol, deepSleepTime))
        ->setFormUI(new FormUI::UI(FormUI::Type::TEXT, F("Deep Sleep Time")))->setSuffix(F("seconds (0 = indefinitely)")));

    form.add<uint16_t>(F("long_press_time"), _H_STRUCT_VALUE(MainConfig().plugins.remotecontrol, longpressTime))
        ->setFormUI(new FormUI::UI(FormUI::Type::TEXT, F("Long Press Time")))->setSuffix(FSPGM(milliseconds, "milliseconds")));
    form.add<uint16_t>(F("repeat_time"), _H_STRUCT_VALUE(MainConfig().plugins.remotecontrol, repeatTime))
        ->setFormUI(new FormUI::UI(FormUI::Type::TEXT, F("Repeat Time")))->setSuffix(FSPGM(milliseconds)));

    FormUI::ItemsList actions;
    Plugins::HomeAssistant::ActionVector vector;
    Plugins::HomeAssistant::getActions(vector);

    actions.emplace_back(String(0), String(F("None")));
    for(const auto &action: vector) {
        auto str = PrintString(F("%s: %s"), action.getEntityId().c_str(), action.getActionFStr());
        if (action.getNumValues()) {
            str.printf_P(PSTR(" %d"), action.getValue(0));
        }
        actions.emplace_back(String(action.getId()), str);
    }

    for(uint8_t i = 0; i < _buttons.size(); i++) {

        auto &group = form.addGroup(PrintString(F("remote_%u"), i), PrintString(F("Button %u Action"), i + 1),
            (_config.actions[i].shortpress || _config.actions[i].longpress || _config.actions[i].repeat)
        );

        form.add<uint16_t>(PrintString(F("shortpress_action_%u"), i), _H_STRUCT_VALUE(MainConfig().plugins.remotecontrol, actions[i].shortpress, i))
            ->setFormUI(new FormUI::UI(FormUI::Type::SELECT, F("Short Press")))->addItems(actions));

        form.add<uint16_t>(PrintString(F("longpress_action_%u"), i), _H_STRUCT_VALUE(MainConfig().plugins.remotecontrol, actions[i].longpress, i))
            ->setFormUI(new FormUI::UI(FormUI::Type::SELECT, F("Long Press")))->addItems(actions));

        form.add<uint16_t>(PrintString(F("repeat_action_%u"), i), _H_STRUCT_VALUE(MainConfig().plugins.remotecontrol, actions[i].repeat, i))
            ->setFormUI(new FormUI::UI(FormUI::Type::SELECT, F("Repeat")))->addItems(actions));

        group.end();
    }

    form.finalize();
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
        _autoSleepTimeout = AutoSleepDisabled;
        return true;
    }
    return false;
}

#endif

void RemoteControlPlugin::onButtonPressed(Button& btn)
{
    __LDBG_printf("btn=%d", plugin._getButtonNum(btn));
    //plugin._addButtonEvent(ButtonEvent(plugin._getButtonNum(btn), ButtonEvent::PRESS));
}

void RemoteControlPlugin::onButtonHeld(Button& btn, uint16_t duration, uint16_t repeatCount)
{
    __LDBG_printf("btn=%d duration=%d repeat=%d", plugin._getButtonNum(btn), duration, repeatCount);
    plugin._onButtonHeld(btn, duration, repeatCount);
}

void RemoteControlPlugin::onButtonReleased(Button& btn, uint16_t duration)
{
    __LDBG_printf("btn=%d duration=%d", plugin._getButtonNum(btn), duration);
    plugin._onButtonReleased(btn, duration);
}

void RemoteControlPlugin::_onButtonHeld(Button& btn, uint16_t duration, uint16_t repeatCount)
{
    auto buttonNum = _getButtonNum(btn);
#if IOT_REMOTE_CONTROL_COMBO_BTN
    if (_anyButton(&btn)) { // is any button pressed except btn?
        _resetAutoSleep();
        if (duration < 5000 || _comboButton != 3) {
            if (_comboButton == -1) {
                _comboButton = buttonNum; // store first button held
                __LDBG_printf("longpress=%d combo=%d", _longPress, _comboButton);
            }
            if (buttonNum != _comboButton) { // fire event for any other button that is held except first one
                _addButtonEvent(ButtonEvent(buttonNum, ButtonEvent::COMBO_REPEAT, duration, repeatCount, _comboButton));
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
        _addButtonEvent(ButtonEvent(buttonNum, ButtonEvent::REPEAT, duration, repeatCount));
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
        _addButtonEvent(ButtonEvent(_getButtonNum(btn), ButtonEvent::COMBO_RELEASE, duration, _comboButton));
        _comboButton = -1;
    }
    else
#endif
    {
        _addButtonEvent(ButtonEvent(_getButtonNum(btn), ButtonEvent::RELEASE, duration));
    }
}


void RemoteControlPlugin::loop()
{
    plugin._loop();
}

void RemoteControlPlugin::wifiCallback(uint8_t event, void *payload)
{
    if (event == WiFiCallbacks::EventType::CONNECTED) {
        plugin._wifiConnected();
    }
}

void RemoteControlPlugin::disableAutoSleep()
{
    __LDBG_printf("disabled");
    plugin._autoSleepTimeout = AutoSleepDisabled;
}

void RemoteControlPlugin::disableAutoSleepHandler(AsyncWebServerRequest *request)
{
    __LDBG_printf("is_authenticated=%u", WebServerPlugin::getInstance().isAuthenticated(request) == true);
    if (WebServerPlugin::getInstance().isAuthenticated(request) == true) {
        disableAutoSleep();
        AsyncWebServerResponse *response = request->beginResponse(302);
        HttpHeaders httpHeaders;
        httpHeaders.addNoCache(true);
        httpHeaders.add(new HttpLocationHeader(F("/status.html")));
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
        httpHeaders.add(new HttpLocationHeader(F("/status.html")));
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
    for(const auto button : _buttons) {
        button->update();
    }
    if (_autoSleepTimeout != AutoSleepDisabled) {
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
                timeout = std::max((uint8_t)30, _config.autoSleepTime);
            }
            _autoSleepTimeout = millis() + (timeout * 1000UL);
            __LDBG_printf("auto deep sleep=%u connected=%u", _autoSleepTimeout, config.isWiFiUp());
        }
        if (_autoSleepTimeout && millis() > _autoSleepTimeout) {
            __LDBG_printf("entering deep sleep (auto=%d, time=%us)", _config.autoSleepTime, _config.deepSleepTime);
            _autoSleepTimeout = AutoSleepDisabled;
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

int8_t RemoteControlPlugin::_getButtonNum(const Button &btn) const
{
    for(uint8_t n = 0; n < _buttons.size(); n++) {
        if (_buttons[n] == &btn) {
            return n;
        }
    }
    return -1;
}

bool RemoteControlPlugin::_anyButton(const Button *btn) const
{
    for(const auto button : _buttons) {
        if (button != btn && button->isPressed()) {
            return true;
        }
    }
    return false;
}

bool RemoteControlPlugin::_isUsbPowered() const
{
    return digitalRead(IOT_SENSOR_BATTERY_CHARGE_DETECTION);
}

void RemoteControlPlugin::_readConfig()
{
    _config = Plugins::RemoteControl::get();
    _config.validate();
}

void RemoteControlPlugin::_installWebhooks()
{
    __LDBG_printf("server=%p", WebServerPlugin::getWebServerObject());
    WebServerPlugin::addHandler(F("/remote_sleep.html"), deepSleepHandler);
    WebServerPlugin::addHandler(F("/remote_nosleep.html"), disableAutoSleepHandler);
}

void RemoteControlPlugin::_resetAutoSleep()
{
    if (_autoSleepTimeout && _autoSleepTimeout != AutoSleepDisabled) {
        _autoSleepTimeout = millis() + (_config.autoSleepTime * 1000UL);
        __LDBG_printf("auto deep sleep set %u", _autoSleepTimeout);
    }
}

void RemoteControlPlugin::_addButtonEvent(ButtonEvent &&event)
{
    _resetAutoSleep();
    if (_events.size() > 32) { // discard events
        _events.pop_front();
    }
    _events.emplace_back(event);
    __LDBG_printf("event=%s", _events.back().toString().c_str());
    _scheduleSendEvents();
}

void RemoteControlPlugin::_sendEvents()
{
    __LDBG_printf("wifi=%u events=%u locked=%s", config.isWiFiUp(), _events.size(), String(_buttonsLocked & 0b1111, 2).c_str());

    if (config.isWiFiUp() && _events.size()) {
        for(auto iterator = _events.begin(); iterator != _events.end(); ++iterator) {
            if (iterator->getType() == ButtonEvent::NONE) {
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
                case ButtonEvent::REPEAT:
                    actionId = actions.repeat;
                    break;
                case ButtonEvent::RELEASE:
                    if (event.getDuration() < _config.longpressTime) {
                        actionId = actions.shortpress;
                    }
                    else {
                        actionId = actions.longpress;
                    }
                    break;
#if IOT_REMOTE_CONTROL_COMBO_BTN
                case ButtonEvent::COMBO_REPEAT:
                    actionId = event.getCombo(actions).repeat;
                    break;
                case ButtonEvent::COMBO_RELEASE:
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

            auto action = Plugins::HomeAssistant::getAction(actionId);
            if (action.getId()) {
                // Logger_notice(F("firing event: %s"), event.toString().c_str());

                _lockButton(event.getButton());
                auto button = event.getButton();
                //_events.erase(iterator);
                event.remove();

                _hass.executeAction(action, [this, button](bool status) {
                    _unlockButton(button);
                    _scheduleSendEvents();
                });
                return;
            }

            __LDBG_printf("removing event with no action");
            event.remove();
        }

        __LDBG_printf("cleaning events");
        _events.erase(std::remove_if(_events.begin(), _events.end(), [](const ButtonEvent &event) {
            return event.getType() == ButtonEvent::NONE;
        }), _events.end());
    }
}

void RemoteControlPlugin::_scheduleSendEvents() {
    if (_events.size()) {
        LoopFunctions::callOnce([this]() {
            _sendEvents();
        });
    }
}
