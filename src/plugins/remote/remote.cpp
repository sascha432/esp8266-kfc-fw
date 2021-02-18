/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
// #include <WiFiCallbacks.h>
#include <LoopFunctions.h>
#include <BitsToStr.h>
#include <ReadADC.h>
#include "kfc_fw_config.h"
#include "web_server.h"
#include "blink_led_timer.h"
#include "remote.h"
#include "remote_button.h"
#include "push_button.h"
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

String RemoteControlPlugin::PinState::toString()
{
    return PrintString(F("pin state=%s time=%u actve_low=%u active=%s"),
        BitsToStr<17, true>(_state).merge(_read),
        _time,
        activeLow(),
        BitsToStr<17, true>(activeLow() ? _state ^ _read : _state).merge(_read)
    );
}

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
    "general,events,combos,actions,buttons-1,buttons-2,buttons-3,buttons-4",
    "http,mqtt",             // reconfigure_dependencies
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
    Base()
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
            __DBG_printf("button#=%u action=%u,%u,%u,%u,%u,%u,%u,%u udp=%s mqtt=%s",
                button.getButtonNum(),
                cfg.down, cfg.up, cfg.press, cfg.single_click, cfg.double_click, cfg.long_press, cfg.hold, cfg.hold_released,
                BitsToStr<9, true>(cfg.udp.event_bits).c_str(), BitsToStr<9, true>(cfg.udp.event_bits).c_str()
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


    PrintString stateStr = F("Pin state: ");

#if PIN_MONITOR_BUTTON_GROUPS
    SingleClickGroupPtr group1;
    group1.reset(_config.click_time);
    SingleClickGroupPtr group2;
    group2.reset(_config.click_time);
#endif

    // disable interrupts during setup
    for(uint8_t n = 0; n < _buttonPins.size(); n++) {
        auto pin = _buttonPins[n];
#if PIN_MONITOR_BUTTON_GROUPS
        auto &button = pinMonitor.attach<Button>(pin, n, this);
        using EventNameType = Plugins::RemoteControlConfig::EventNameType;

        if (!_config.enabled[EventNameType::BUTTON_PRESS]) {
            if (n == 0 || n == 3) {
                button.setSingleClickGroup(group1);
            }
            else {
                button.setSingleClickGroup(group2);
            }
        }
        else {
            button.setSingleClickGroup(group1);
            group1.reset(_config.click_time);
        }
#else
        pinMonitor.attach<Button>(pin, n, this);
#endif
        pinMode(pin, INPUT);
    }

    pinMode(IOT_REMOTE_CONTROL_AWAKE_PIN, OUTPUT);
    digitalWrite(IOT_REMOTE_CONTROL_AWAKE_PIN, HIGH);
    _buttonsLocked = 0;
    _readConfig();
    _updateButtonConfig();

    // reset state and feed debouncer with initial values and state
    pinMonitor.feed(_pinState._time, _pinState._state, _pinState._read, _pinState.activeLow());
    pinMonitor.loop();

    LoopFunctions::callOnce([this]() {

        __LDBG_printf("delayed mqtt setup");
        _setup();

        // WiFiCallbacks::add(WiFiCallbacks::EventType::CONNECTED, wifiCallback);
        LoopFunctions::add(loop);

        __LDBG_printf("initial %s", _pinState.toString().c_str());

        // _actions.emplace_back(new ActionUDP(100, Payload::String(F("test1")), F("!acidpi1.local"), IPAddress(), 7712));
        // _actions.emplace_back(new ActionUDP(200, Payload::String(F("test2")), F("192.168.0.3"), IPAddress(), 7712));
        // _actions.emplace_back(new ActionUDP(300, Payload::String(F("test3")), F("acidpi1.local"), IPAddress(), 7712));
        // IPAddress addr;
        // addr.fromString(F("192.168.0.3"));
        // _actions.emplace_back(new ActionUDP(400, Payload::String(F("test4")), emptyString, addr, 7712));
        // _resolveActionHostnames();

    });
}

void RemoteControlPlugin::reconfigure(const String &source)
{
    _readConfig();
    if (String_equals(source, F("mqtt"))) {
        _reconfigure();
    }
    else if (String_equals(source, F("http"))) {
        _reconfigure();
    }
    else {
        _updateButtonConfig();
        _reconfigure();
    }
}

void RemoteControlPlugin::shutdown()
{
    _shutdown();
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
    auto root = bootstrapMenu.getMenuItem(navMenu.config);

    auto subMenu = root.addSubMenu(getFriendlyName());
    subMenu.addMenuItem(F("General"), F("remotectrl/general.html"));
    subMenu.addDivider();
    subMenu.addMenuItem(F("Button Events"), F("remotectrl/events.html"));
    subMenu.addMenuItem(F("Button 1"), F("remotectrl/buttons-1.html"));
    subMenu.addMenuItem(F("Button 2"), F("remotectrl/buttons-2.html"));
    subMenu.addMenuItem(F("Button 3"), F("remotectrl/buttons-3.html"));
    subMenu.addMenuItem(F("Button 4"), F("remotectrl/buttons-4.html"));
}


#if AT_MODE_SUPPORTED

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(RMNOSLP, "RMNOSLP", "Disable auto sleep");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(RMNPUB, "RMNPUB", "Publish auto discovery");

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
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(RMNPUB))) {
        publishAutoDiscovery();
        return true;
    }
    return false;
}


#endif

void RemoteControlPlugin::_onShortPress(Button &button)
{
    __LDBG_printf("%u SHORT PRESS", button.getButtonNum());
}

void RemoteControlPlugin::_onLongPress(Button &button)
{
    __LDBG_printf("%u LONG PRESS", button.getButtonNum());
}

void RemoteControlPlugin::_onRepeat(Button &button)
{
    __LDBG_printf("%u REPEAT", button.getButtonNum());
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
                BUILDIN_LED_SET(BlinkLEDTimer::BlinkType::FLICKER);
                _longPress = 1;
                __LDBG_printf("longpress=%d combo=%d", _longPress, _comboButton);
            }
            else if (duration > 8000 && _longPress == 1) {
                __LDBG_printf("2 buttons held >8 seconds: %u", buttonNum);
                _longPress = 2;
                __LDBG_printf("longpress=%d combo=%d", _longPress, _comboButton);
                BUILDIN_LED_SET(BlinkLEDTimer::BlinkType::SOLID);
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
            BUILDIN_LED_SET(config.isWiFiUp() ? BlinkLEDTimer::BlinkType::SOLID : BlinkLEDTimer::BlinkType::OFF);
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

// void RemoteControlPlugin::disableAutoSleepHandler(AsyncWebServerRequest *request)
// {
//     __LDBG_printf("is_authenticated=%u", WebServerPlugin::getInstance().isAuthenticated(request) == true);
//     if (WebServerPlugin::getInstance().isAuthenticated(request) == true) {
//         disableAutoSleep();
//         AsyncWebServerResponse *response = request->beginResponse(302);
//         HttpHeaders httpHeaders;
//         httpHeaders.addNoCache(true);
//         httpHeaders.add<HttpLocationHeader>(F("/status.html"));
//         httpHeaders.setAsyncWebServerResponseHeaders(response);
//         request->send(response);
//     }
//     else {
//         request->send(403);
//     }
// }

// void RemoteControlPlugin::deepSleepHandler(AsyncWebServerRequest *request)
// {
//     __LDBG_printf("is_authenticated=%u", WebServerPlugin::getInstance().isAuthenticated(request) == true);
//     if (WebServerPlugin::getInstance().isAuthenticated(request) == true) {
//         AsyncWebServerResponse *response = request->beginResponse(302);
//         HttpHeaders httpHeaders;
//         httpHeaders.addNoCache(true);
//         httpHeaders.add<HttpLocationHeader>(F("/status.html"));
//         httpHeaders.setAsyncWebServerResponseHeaders(response);
//         request->send(response);

//         BUILDIN_LED_SET(BlinkLEDTimer::BlinkType::FLICKER);
//         disableAutoSleep();

//         _Scheduler.add(2000, false, [](Event::CallbackTimerPtr timer) {
//             __LDBG_printf("deep sleep=%us", plugin._config.deep_sleep_time);
//             config.enterDeepSleep(KFCFWConfiguration::seconds(plugin._config.deep_sleep_time), RF_DEFAULT, 1);
//         });
//     }
//     else {
//         request->send(403);
//     }
// }

RemoteControlPlugin::PinState RemoteControlPlugin::readPinState()
{
    if (!_pinState.isValid()) {
        _pinState = PinState(micros());
        for(uint8_t i = 0; i < _buttonPins.size(); i++) {
            auto pin = _buttonPins[i];
            pinMode(pin, INPUT);
            _pinState.setPin(pin, digitalRead(pin));
        }
    }
    return _pinState;
}

void RemoteControlPlugin::_loop()
{
    if (_autoSleepTimeout != kAutoSleepDisabled && _config.auto_sleep_time != 0) {
        if (_isUsbPowered()) {
            if (_autoSleepTimeout) {
                __LDBG_printf("usb power detected, disabling auto sleep");
                _autoSleepTimeout = 0;
            }
        }
        else if (_autoSleepTimeout == 0) {
            uint16_t timeout = _config.auto_sleep_time;
            if (!config.isWiFiUp()) {
                timeout = std::max<uint16_t>(30, timeout); // increase auto sleep timeout to at least 30 seconds if wifi is down
            }
            _autoSleepTimeout = millis() + (timeout * 1000UL);
            __LDBG_printf("auto deep sleep=%u connected=%u", _autoSleepTimeout, config.isWiFiUp());
        }
        // do not sleep until all events have been processed
        else if (!_hasEvents() && _autoSleepTimeout && millis() >= _autoSleepTimeout && !MQTTClient::safeIsAutoDiscoveryRunning()) {
            __LDBG_printf("entering deep sleep (auto=%d, time=%us)", _config.deep_sleep_time, _config.deep_sleep_time);
            _autoSleepTimeout = kAutoSleepDisabled;
            config.enterDeepSleep(KFCFWConfiguration::seconds(_config.deep_sleep_time), RF_DEFAULT, 1);
        }
    }

    // buttons are interrupt driven and do not require fast polling
    // 5-10ms catches pretty much all events
    // delay(10);
}

bool RemoteControlPlugin::_isUsbPowered() const
{
    return IOT_SENSOR_BATTERY_ON_EXTERNAL;
}

void RemoteControlPlugin::_readConfig()
{
    _config = Plugins::RemoteControl::getConfig();
}


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
