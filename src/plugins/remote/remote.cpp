/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
// #include <WiFiCallbacks.h>
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

#if ESP8266
#define SHORT_POINTER_FMT       "%05x"
#define SHORT_POINTER(ptr)      ((uint32_t)ptr & 0xfffff)
#else
#define SHORT_POINTER_FMT       "%08x"
#define SHORT_POINTER(ptr)      ptr
#endif

using KFCConfigurationClasses::Plugins;
using namespace RemoteControl;

#if DEBUG_IOT_REMOTE_CONTROL

const char *Base::_getPressedButtons() const
{
    static char buffer[_buttonPins.size() + 1];
    char *ptr = buffer;
    for(uint8_t n = 0; n < _buttonPins.size(); n++) {
        *ptr++ = _isPressed(n) ? '1' : '0';
    }
    *ptr = 0;
    return buffer;
}
#endif

static RemoteControlPlugin plugin;

PROGMEM_DEFINE_PLUGIN_OPTIONS(
    RemoteControlPlugin,
    "remotectrl",       // name
    "Remote Control",   // friendly name
    "",                 // web_templates
    // config_forms
    "general,buttons,combos,actions",
    "http",             // reconfigure_dependencies
    PluginComponent::PriorityType::REMOTE,
    PluginComponent::RTCMemoryId::DEEP_SLEEP,
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
    Base(),
#if HOME_ASSISTANT_INTEGRATION
    _hass(HassPlugin::getInstance()),
#endif
    _pressedKeys(0),
    _pressedKeysTime(0)
{
    REGISTER_PLUGIN(this, "RemoteControlPlugin");
}

RemoteControlPlugin &RemoteControlPlugin::getInstance()
{
    return plugin;
}

void RemoteControlPlugin::_updateButtonConfig()
{
    for(const auto &buttonPtr: pinMonitor.getHandlers()) {
        auto &button = static_cast<Button &>(*buttonPtr);
        __LDBG_printf("arg=%p btn=%p pin=%u digital_read=%u inverted=%u owner=%u", button.getArg(), &button, button.getPin(), digitalRead(button.getPin()), button.isInverted(), button.getBase() == this);
        if (button.getBase() == this) { // auto downcast of this
#if DEBUG_IOT_REMOTE_CONTROL
            auto &cfg = _getConfig().actions[button.getButtonNum()];
            __DBG_printf("button#=%u action=%u,%u,%u multi=%u,%u,%u,%u pressed/released=%u,%u",
                button.getButtonNum(),
                cfg.single_click, cfg.double_click, cfg.longpress,
                cfg.multi_click[0].action, cfg.multi_click[1].action, cfg.multi_click[2].action, cfg.multi_click[3].action,
                cfg.pressed, cfg.released
            );
#endif
            button.updateConfig();
        }
    }
}

void RemoteControlPlugin::setup(SetupModeType mode)
{
    __LDBG_printf("mode=%u", mode);
    pinMonitor.begin();

    for(uint8_t n = 0; n < _buttonPins.size(); n++) {
        auto pin = _buttonPins[n];
#if DEBUG_PIN_MONITOR_BUTTON_NAME
        auto &button = pinMonitor.attach<Button>(pin, n, this); // downcast of this is stored, which is a different pointer cause of the multiple inheritence
        button.setName(PrintString(F(SHORT_POINTER_FMT ":" SHORT_POINTER_FMT "#%u"), SHORT_POINTER(button.getArg()), SHORT_POINTER(&button), n));
        __DBG_printf("D%08lu %s pin=%u arg=%p btn=%p btn#%u", millis(), button.name(), pin, button.getArg(), &button, n);
#else
        pinMonitor.attach<Button>(pin, n, this);
#endif
        pinMode(pin, INPUT);

        // set state of the pin to the stored value at boot time
        uint8_t mask = (1 << n);
        if (_pressedKeys & mask) {
            _pressedKeys &= ~mask;
            uint8_t pinNum = button.getPin();
            auto iterator = std::find_if(pinMonitor.getPins().begin(), pinMonitor.getPins().end(), [pinNum](const HardwarePinPtr &pin) {
                return pin->getPin() == pinNum;
            });
            __LDBG_printf("updating pin=%u state=%u time=%u now=%u exists=%u", pinNum, true, _pressedKeysTime, (uint32_t)micros(), iterator != pinMonitor.getPins().end());
            if (iterator != pinMonitor.getPins().end()) {
                iterator->get()->updateState(_pressedKeysTime, HardwarePin::kIncrementCount, true);
            }
        }
    }

    pinMode(IOT_REMOTE_CONTROL_AWAKE_PIN, OUTPUT);
    digitalWrite(IOT_REMOTE_CONTROL_AWAKE_PIN, HIGH);
    _buttonsLocked = 0;
    _readConfig();
    _updateButtonConfig();

    LoopFunctions::callOnce([this]() {

        __LDBG_printf("delayed setup");

        // WiFiCallbacks::add(WiFiCallbacks::EventType::CONNECTED, wifiCallback);
        LoopFunctions::add(loop);

        _installWebhooks();

        _actions.emplace_back(new ActionUDP(100, Payload::String(F("test1")), F("!acidpi1.local"), IPAddress(), 7712));
        _actions.emplace_back(new ActionUDP(200, Payload::String(F("test2")), F("192.168.0.3"), IPAddress(), 7712));
        _actions.emplace_back(new ActionUDP(300, Payload::String(F("test3")), F("acidpi1.local"), IPAddress(), 7712));
        IPAddress addr;
        addr.fromString(F("192.168.0.3"));
        _actions.emplace_back(new ActionUDP(400, Payload::String(F("test4")), emptyString, addr, 7712));
        _resolveActionHostnames();

    });
}

void RemoteControlPlugin::reconfigure(const String &source)
{
    _readConfig();
    if (String_equals(source, F("http"))) {
        _installWebhooks();
    }
    else {
        _updateButtonConfig();
    }
}

void RemoteControlPlugin::shutdown()
{
    pinMonitor.detach(this);

    LoopFunctions::remove(loop);
    // WiFiCallbacks::remove(WiFiCallbacks::EventType::ANY, wifiCallback);
}

void RemoteControlPlugin::prepareDeepSleep(uint32_t sleepTimeMillis)
{
    __LDBG_printf("time=%u", sleepTimeMillis);
    digitalWrite(IOT_REMOTE_CONTROL_AWAKE_PIN, LOW);
}

void RemoteControlPlugin::getStatus(Print &output)
{
    output.printf_P(PSTR("%u Button Remote Control"), _buttonPins.size());
    if (_autoSleepTimeout == 0 || _autoSleepTimeout == kAutoSleepDisabled) {
        output.print(F(", auto sleep disabled"));
    }
}

void RemoteControlPlugin::createMenu()
{
    bootstrapMenu.addSubMenu(getFriendlyName(), F("remotectrl/general.html"), navMenu.config);
    bootstrapMenu.addSubMenu(F("Buttons Assignments"), F("remotectrl/buttons.html"), navMenu.config);
    bootstrapMenu.addSubMenu(F("Buttons Combinations"), F("remotectrl/combos.html"), navMenu.config);
    bootstrapMenu.addSubMenu(F("Define Actions"), F("remotectrl/actions.html"), navMenu.config);

    // bootstrapMenu.addSubMenu(F("Enter Deep Sleep"), F("remote-sleep.html"), navMenu.device);
    // bootstrapMenu.addSubMenu(F("Disable Auto Sleep"), F("remote-nosleep.html"), navMenu.device);
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

void RemoteControlPlugin::_onShortPress(Button &button)
{
    __LDBG_printf("%s SHORT PRESS", button.name());
}

void RemoteControlPlugin::_onLongPress(Button &button)
{
    __LDBG_printf("%s LONG PRESS", button.name());
}

void RemoteControlPlugin::_onRepeat(Button &button)
{
    __LDBG_printf("%s REPEAT", button.name());
}


#if 0

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
    if (anyButton(getPressedMask(buttonNum))) { // is any button pressed except btn?
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
        if (!anyButton()) {
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

#endif

void RemoteControlPlugin::loop()
{
    plugin._loop();
}

// void RemoteControlPlugin::wifiCallback(WiFiCallbacks::EventType event, void *payload)
// {
//     if (event == WiFiCallbacks::EventType::CONNECTED) {
//         plugin._wifiConnected();
//     }
// }

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

uint8_t RemoteControlPlugin::detectKeyPress()
{
    bool value;
    _pressedKeys = 0;
    _pressedKeysTime = micros();
    for(uint8_t i = 0; i < _buttonPins.size(); i++) {
        auto pin = _buttonPins[i];
        pinMode(pin, INPUT);
        if ((value = digitalRead(pin)) == (PIN_MONITOR_ACTIVE_STATE == PinMonitor::ActiveStateType::PRESSED_WHEN_HIGH)) {
            _pressedKeys |= (1 << i);
        }
    }
    return _pressedKeys;
}

void RemoteControlPlugin::_loop()
{
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
                timeout = std::max<uint8_t>(30, _config.autoSleepTime); // increase auto sleep timeout to at least 30 seconds if wifi is down
            }
            _autoSleepTimeout = millis() + (timeout * 1000UL);
            __LDBG_printf("auto deep sleep=%u connected=%u", _autoSleepTimeout, config.isWiFiUp());
        }
#if DEBUG_IOT_REMOTE_CONTROL
        else if (_hasEvents() && _autoSleepTimeout && millis() >= _autoSleepTimeout) {
            static unsigned counter = 0;
            if ((counter++ % 2) == ((millis() / 500) % 2)) { // display every 500ms
                __DBG_printf("sleep timeout reached but %u events queued. delaying sleep by %ums", _queue.size(), millis() - _autoSleepTimeout);
            }
        }
#endif
        // do not sleep until all events have been processed
        else if (!_hasEvents() && _autoSleepTimeout && millis() >= _autoSleepTimeout) {
            __LDBG_printf("entering deep sleep (auto=%d, time=%us)", _config.autoSleepTime, _config.deepSleepTime);
            _autoSleepTimeout = kAutoSleepDisabled;
            config.enterDeepSleep(KFCFWConfiguration::seconds(0), RF_DEFAULT, 1); // TODO implement deep sleep

            // config.enterDeepSleep(KFCFWConfiguration::seconds(_config.deepSleepTime), RF_DEFAULT, 1);
        }
    }

    // buttons are interrupt driven and do not require fast polling
    // call delay to safe energy
    // delay(25);
    delay(5);
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
    // WebServerPlugin::addHandler(F("/remote-sleep.html"), deepSleepHandler);
    // WebServerPlugin::addHandler(F("/remote-nosleep.html"), disableAutoSleepHandler);
}


// void RemoteControlPlugin::_addButtonEvent(ButtonEvent &&event)
// {
//     _resetAutoSleep();
// #if 0 // container has auto discard
//     // if (_events.size() > 32) { // discard events
//     //     _events.pop_front();
//     // }
// #endif
//     _events.emplace_back(std::move(event));
//     __LDBG_printf("event=%s", _events.back().toString().c_str());
//     _scheduleSendEvents();
// }

#if 0
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

            ActionIdType actionId = 0;

//             auto &actions = _config.actions[event.getButton()];
//             switch(event.getType()) {
//                 case EventType::REPEAT:
//                     actionId = actions.repeat;
//                     break;
//                 case EventType::RELEASE:
//                     if (event.getDuration() < _config.longpressTime) {
//                         actionId = actions.shortpress;
//                     }
//                     else {
//                         actionId = actions.longpress;
//                     }
//                     break;
// #if IOT_REMOTE_CONTROL_COMBO_BTN
//                 case EventType::COMBO_REPEAT:
//                     actionId = event.getCombo(actions).repeat;
//                     break;
//                 case EventType::COMBO_RELEASE:
//                     if (event.getDuration() < _config.longpressTime) {
//                         actionId = event.getCombo(actions).shortpress;
//                     }
//                     else {
//                         actionId = event.getCombo(actions).longpress;
//                     }
//                     break;
// #endif
//                 default:
//                     actionId = 0;
//                     break;
//             }

#if HOME_ASSISTANT_INTEGRATION
            auto action = Plugins::HomeAssistant::getAction(actionId);
            if (action.getId()) {
                _lockButton(event.getButton());
                auto button = event.getButton();
                event.remove();

                _hass.executeAction(action, [this, button](bool status) {
                    _unlockButton(button);
                    _scheduleSendEvents();
                });
                return;
            }
#else
            auto actionPtr = _getAction(actionId);
            if (actionPtr && *actionPtr) {
                auto button = event.getButton();
                _lockButton(button);
                event.remove();

                actionPtr->execute([this, button](bool status) {
                    _unlockButton(button);
                    _scheduleSendEvents();
                });
                return;
            }
#endif

            __LDBG_printf("removing event with no action");
            event.remove();
        }

        ButtonEventList tmp;
#if DEBUG_IOT_REMOTE_CONTROL
        uint32_t start = micros();
#endif
        noInterrupts();
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
#endif

Action *RemoteControlPlugin::_getAction(ActionIdType id) const
{
    auto iterator = std::find_if(_actions.begin(), _actions.end(), [id](const ActionPtr &action) {
        return action->getId() == id;
    });
    if (iterator == _actions.end()) {
        return nullptr;
    }
    return iterator->get();
}

void RemoteControlPlugin::_resolveActionHostnames()
{
    for(auto &action: _actions) {
        action->resolve();
    }
}
