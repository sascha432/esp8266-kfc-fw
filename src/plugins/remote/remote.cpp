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
#include "WebUISocket.h"
#if HTTP2SERIAL_SUPPORT
#include "../src/plugins/http2serial/http2serial.h"
#define IF_HTTP2SERIAL_SUPPORT(...) __VA_ARGS__
#else
#define IF_HTTP2SERIAL_SUPPORT(...)
#endif
#if SYSLOG_SUPPORT
#include "../src/plugins/syslog/syslog_plugin.h"
#endif

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

String PinState::toString()
{
    return PrintString(F("pin state=%s time=%u actve_low=%u active=%s"),
        BitsToStr<17, true>(_state).merge(_read),
        _time,
        activeLow(),
        BitsToStr<17, true>(activeLow() ? _state ^ _read : _state).merge(_read)
    );
 }

static RemoteControlPlugin plugin;

PROGMEM_DEFINE_PLUGIN_OPTIONS(
    RemoteControlPlugin,
    "remotectrl",       // name
    "Remote Control",   // friendly name
    "",                 // web_templates
    // config_forms
    "general,events,combos,actions,buttons-1,buttons-2,buttons-3,buttons-4",
    "",                 // reconfigure_dependencies
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

PinState _pinState;

extern "C" void preinit (void)
{
    // store states of buttons
    // all pins are reset to input before
    _pinState._readStates();
    // settings the awake pin will clear all buffers and key presses might be lost
    pinMode(IOT_REMOTE_CONTROL_AWAKE_PIN, OUTPUT);
    digitalWrite(IOT_REMOTE_CONTROL_AWAKE_PIN, HIGH);
}

RemoteControlPlugin::RemoteControlPlugin() :
    PluginComponent(PROGMEM_GET_PLUGIN_OPTIONS(RemoteControlPlugin)),
    Base(),
    _readUsbPinTimeout(0)
{
    REGISTER_PLUGIN(this, "RemoteControlPlugin");
    // pinMode(IOT_REMOTE_CONTROL_AWAKE_PIN, OUTPUT);
    // digitalWrite(IOT_REMOTE_CONTROL_AWAKE_PIN, HIGH);
}

RemoteControlPlugin &RemoteControlPlugin::getInstance()
{
    return plugin;
}

void RemoteControlPlugin::_updateButtonConfig()
{
    for(const auto &buttonPtr: pinMonitor.getHandlers()) {
        auto &button = static_cast<Button &>(*buttonPtr);
        // __LDBG_printf("arg=%p btn=%p pin=%u digital_read=%u inverted=%u owner=%u", button.getArg(), &button, button.getPin(), digitalRead(button.getPin()), button.isInverted(), button.getBase() == this);
        if (button.getBase() == this) { // auto downcast of this
#if DEBUG_IOT_REMOTE_CONTROL && 0
            auto &cfg = _getConfig().actions[button.getButtonNum()];
            __LDBG_printf("button#=%u action=%u,%u,%u,%u,%u,%u,%u,%u udp=%s mqtt=%s",
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
    // __LDBG_printf("mode=%u", mode);
    pinMonitor.begin();

#if PIN_MONITOR_BUTTON_GROUPS
    SingleClickGroupPtr group1;
    group1.reset(_config.click_time);
    SingleClickGroupPtr group2;
    group2.reset(_config.click_time);
#endif

    // get handlers ...
    // for(const auto &handler: pinMonitor.getHandlers()) {
    //     switch(handler->getPin()) {
    //         case kButtonSystemComboPins[0]:
    //         case kButtonSystemComboPins[1]:
    //             if (handler->getArg() == this) {
    //                 handler->setDisabled();

    //             }
    //             break;
    //     }
    // }

    // disable interrupts during setup
    for(uint8_t n = 0; n < kButtonPins.size(); n++) {
        auto pin = kButtonPins[n];
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

    _buttonsLocked = 0;
    _readConfig();
    _updateButtonConfig();

    // uint8_t count = 0;
    // do {
    //     bool reset = (digitalRead(_buttonSystemCombo[0]) == PinState::activeHigh()) && (digitalRead(_buttonSystemCombo[1]) == PinState::activeHigh()); // check if button 0 und 3 are down
    //     if (count == 0) {
    //         if (!reset) { // if not continue
    //             break;
    //         }
    //         BUILDIN_LED_SET(BlinkLEDTimer::BlinkType::FLICKER);
    //     }
    //     // set counter to 10 for debouncing
    //     if (reset) {
    //         count = 10;
    //     }
    //     // count down if reset is not pressed
    //     count--;
    //     if (count == 0) { // set LED back to solid, reset button states and start..
    //         KFCFWConfiguration::setWiFiConnectLedMode();
    //         _pinState = PinState();
    //         _autoSleepTimeout = kAutoSleepDisabled;
    //         break;
    //     }
    //     delay(5);
    // }
    // while(true);

    __DBG_printf("feed time=%u state=%u read=%u active_low=%u %s", _pinState._time, _pinState._state, _pinState._read, _pinState.activeLow(), _pinState.toString().c_str());

    // reset state and feed debouncer with initial values and state
    pinMonitor.feed(_pinState._time, _pinState._state, _pinState._read, _pinState.activeLow());
    // pinMonitor._loop();

    WiFiCallbacks::add(WiFiCallbacks::EventType::ANY, wifiCallback);
    LoopFunctions::add(loop);

    _setup();
    _resetAutoSleep();
    // _resolveActionHostnames();

    _updateSystemComboButtonLED();
}

void RemoteControlPlugin::reconfigure(const String &source)
{
    _readConfig();
    _reconfigure();
    _updateButtonConfig();
    publishAutoDiscovery();
}

void RemoteControlPlugin::shutdown()
{
    _shutdown();
    pinMonitor.detach(this);

    LoopFunctions::remove(loop);
    WiFiCallbacks::remove(WiFiCallbacks::EventType::ANY, wifiCallback);
}

void RemoteControlPlugin::prepareDeepSleep(uint32_t sleepTimeMillis)
{
    __LDBG_printf("time=%u", sleepTimeMillis);
    ADCManager::getInstance().terminate(false);
    digitalWrite(IOT_REMOTE_CONTROL_AWAKE_PIN, LOW);
}

void RemoteControlPlugin::getStatus(Print &output)
{
    output.printf_P(PSTR("%u Button Remote Control"), kButtonPins.size());
    if (isAutoSleepEnabled()) {
        output.print(F(", auto sleep enabled"));
    }
    else {
        output.print(F(", auto sleep disabled"));
    }
    output.printf_P(PSTR(", force deep sleep after %u minutes"), _config.max_awake_time);
}

void RemoteControlPlugin::createMenu()
{
    auto config = bootstrapMenu.getMenuItem(navMenu.config);
    auto subMenu = config.addSubMenu(getFriendlyName());
    subMenu.addMenuItem(F("General"), F("remotectrl/general.html"));
    subMenu.addDivider();
    subMenu.addMenuItem(F("Button Events"), F("remotectrl/events.html"));
    subMenu.addMenuItem(F("Button 1"), F("remotectrl/buttons-1.html"));
    subMenu.addMenuItem(F("Button 2"), F("remotectrl/buttons-2.html"));
    subMenu.addMenuItem(F("Button 3"), F("remotectrl/buttons-3.html"));
    subMenu.addMenuItem(F("Button 4"), F("remotectrl/buttons-4.html"));

    auto device = bootstrapMenu.getMenuItem(navMenu.device);
    device.addDivider();
    device.addMenuItem(F("Enable Auto Sleep"), F("#"));
    device.addMenuItem(F("Disable Auto Sleep"), F("#"));
    device.addMenuItem(F("Enable Deep Sleep"), F("#"));
}


#if AT_MODE_SUPPORTED

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(RCDSLP, "RCDSLP", "[<time in seconds>]", "Set or disable auto sleep");

void RemoteControlPlugin::atModeHelpGenerator()
{
    auto name = getName_P();
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND(RCDSLP), name);
}

bool RemoteControlPlugin::atModeHandler(AtModeArgs &args)
{
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(RCDSLP))) {
        int time = args.toInt(0);
        if (time < 1) {
            _disableAutoSleepTimeout();
            _updateMaxAwakeTimeout();
            args.printf_P(PSTR("Auto sleep disabled"));
        }
        else {
            static constexpr uint32_t kMaxAwakeExtratime = 300; // seconds
            _setAutoSleepTimeout(millis() + (time * 1000U), kMaxAwakeExtratime);
            args.printf_P(PSTR("Auto sleep timeout in %u seconds, deep sleep timeout in %.1f minutes"), time, (time + kMaxAwakeExtratime) / 60.0);
        }
        return true;
    }
    return false;
}


#endif

// void RemoteControlPlugin::_onShortPress(Button &button)
// {
//     __LDBG_printf("%u SHORT PRESS", button.getButtonNum());
// }

// void RemoteControlPlugin::_onLongPress(Button &button)
// {
//     __LDBG_printf("%u LONG PRESS", button.getButtonNum());
// }

// void RemoteControlPlugin::_onRepeat(Button &button)
// {
//     __LDBG_printf("%u REPEAT", button.getButtonNum());
// }


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
            BUILDIN_LED_SET(WiFi.isConnected() ? BlinkLEDTimer::BlinkType::SOLID : BlinkLEDTimer::BlinkType::OFF);
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

void RemoteControlPlugin::wifiCallback(WiFiCallbacks::EventType event, void *payload)
{
    plugin._updateSystemComboButtonLED();
    if (event == WiFiCallbacks::EventType::CONNECTED) {
        plugin._resetAutoSleep();
    }
}

void RemoteControlPlugin::_loop()
{
    auto _millis = millis();
    if (_maxAwakeTimeout && _millis >= _maxAwakeTimeout) {
        __LDBG_printf("forcing deep sleep @ max awake timeout=%u", _maxAwakeTimeout);
        _enterDeepSleep();
    }

    if (_millis >= _readUsbPinTimeout && _isUsbPowered()) {
        _readUsbPinTimeout = _millis + 100;
        _resetAutoSleep();
        if (_autoDiscoveryRunOnce && MQTTClient::safeIsConnected() && IS_TIME_VALID(time(nullptr))) {
            __LDBG_printf("usb power detected, publishing auto discovery");
            _autoDiscoveryRunOnce = false;
            publishAutoDiscovery();
        }
    }
    else if (isAutoSleepEnabled()) {
        if (_millis >= __autoSleepTimeout) {
            if (
                _hasEvents() ||
                _isSystemComboActive() ||
#if MQTT_AUTO_DISCOVERY
                MQTTClient::safeIsAutoDiscoveryRunning() ||
#endif
#if HTTP2SERIAL_SUPPORT
                Http2Serial::hasAuthenticatedClients() ||
#endif
                WebUISocket::hasAuthenticatedClients()
            ) {
                // check again in 500ms
                __autoSleepTimeout = _millis + 500;

                // start blinking after the timeout or 10 seconds
                if (!_signalWarning && (_millis > std::max((_config.auto_sleep_time * 1000U), 10000U))) {
                    BUILDIN_LED_SETP(200, BlinkLEDTimer::Bitset(0b000000000000000001010101U, 24U));
                    _signalWarning = true;
                }
            }
            else {
                _enterDeepSleep();
            }
        }
    }

    _updateSystemComboButton();

    delay(_hasEvents() ? 1 : 10);
}

void RemoteControlPlugin::_enterDeepSleep()
{
    _startupTimings.setDeepSleep(millis());
    _startupTimings.log();
    _disableAutoSleepTimeout();

#if SYSLOG_SUPPORT
    SyslogPlugin::waitForQueue(std::max<uint16_t>(500, std::min<uint16_t>(_config.auto_sleep_time / 10, 1500)));
#endif

    __LDBG_printf("entering deep sleep (auto=%d, time=%us)", _config.deep_sleep_time, _config.deep_sleep_time);
    config.enterDeepSleep(KFCFWConfiguration::seconds(_config.deep_sleep_time), WAKE_NO_RFCAL, 1);
}

bool RemoteControlPlugin::_isUsbPowered() const
{
    return IOT_SENSOR_BATTERY_ON_EXTERNAL;
}

void RemoteControlPlugin::_readConfig()
{
    _config = Plugins::RemoteControl::getConfig();
    _updateMaxAwakeTimeout();
}

#if 0
void RemoteControlPlugin::_sendEvents()
{
    __LDBG_printf("wifi=%u events=%u locked=%s", WiFi.isConnected(), _events.size(), String(_buttonsLocked & 0b1111, 2).c_str());

    if (WiFi.isConnected() && _events.size()) {
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
