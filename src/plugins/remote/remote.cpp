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
#include "deep_sleep.h"
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

RemoteControlPlugin::RemoteControlPlugin() :
    PluginComponent(PROGMEM_GET_PLUGIN_OPTIONS(RemoteControlPlugin)),
    Base(),
    _readUsbPinTimeout(0)
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
        // __LDBG_printf("arg=%p btn=%p pin=%u digital_read=%u inverted=%u owner=%u", button.getArg(), &button, button.getPin(), digitalRead(button.getPin()), button.isInverted(), button.getBase() == this);
        if (button.getBase() == this) { // auto downcast of this
#if DEBUG_IOT_REMOTE_CONTROL
            auto &cfg = _getConfig().actions[button.getButtonNum()];
            __LDBG_printf("button#=%u on_action.id=%u,%u,%u,%u,%u,%u,%u,%u udp.enabled=%s mqtt.enabled=%s",
                button.getButtonNum(),
                cfg.down, cfg.up, cfg.press, cfg.single_click, cfg.double_click, cfg.long_press, cfg.hold, cfg.hold_released,
                BitsToStr<9, true>(cfg.udp.event_bits).c_str(), BitsToStr<9, true>(cfg.udp.event_bits).c_str()
            );
#endif
            button.updateConfig();
        }
    }
}

void RemoteControlPlugin::setup(SetupModeType mode, const PluginComponents::DependenciesPtr &dependencies)
{
    deepSleepPinState.merge();
    pinMonitor.begin();

#if PIN_MONITOR_BUTTON_GROUPS
    SingleClickGroupPtr group1;
    group1.reset(_config.click_time);
    SingleClickGroupPtr group2;
    group2.reset(_config.click_time);
#endif

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
        // for(const auto &pinPtr: pinMonitor.getPins()) {
        //     if (pinPtr->getPin() == pin) { {
        //         lookupTable.set(pin, Interrupt::LookupType::HARDWARE_PIN, pinPtr.get());
        //     }
        // }
        pinMode(pin, INPUT);
    }

#if PIN_MONITOR_USE_GPIO_INTERRUPT
    // for some reason, pin 12 gets a on change interrupt
    if (GPI & kButtonPinsMask) {
        __LDBG_printf("buttons down gpi=%s mask=%s", BitsToStr<16, true>(GPI).c_str(), BitsToStr<16, true>(kButtonPinsMask).c_str());
    }
    PinMonitor::GPIOInterruptsEnable();
#endif

    // _buttonsLocked = 0;
    _readConfig();
    _updateButtonConfig();

   // reset state and add buttons to the queue that were pressed when the device rebooted
    __LDBG_printf("disabling GPIO interrupts");
    ETS_GPIO_INTR_DISABLE();

   __LDBG_printf("reset button state");
    PinMonitor::eventBuffer.clear();
    auto states = deepSleepPinState.getStates();
    interrupt_levels = deepSleepPinState.getValues();

    for(auto &pinPtr: pinMonitor.getPins()) {
        uint8_t pin = pinPtr->getPin();
        auto debounce = pinPtr->getDebounce();
        if (debounce) {
            // debounce
            debounce->setState(deepSleepPinState.activeHigh() == false);
        }
        if (states & _BV(pin)) {
            PinMonitor::eventBuffer.emplace_back(2000, pin, interrupt_levels); // place them at 2ms
        }
        __LDBG_printf("pin=%02u init debouncer=%d queued=%u states=%s interrupt_levels=%s mask=%s", debounce ? (deepSleepPinState.activeHigh()) == false : -1, pin, (states & _BV(pin)) != 0, BitsToStr<16, true>(states).c_str(), BitsToStr<16, true>(interrupt_levels).c_str(), BitsToStr<16, true>(_BV(pin)).c_str());

    }

    __LDBG_printf("feeding queue size=%u", PinMonitor::eventBuffer.size());
    pinMonitor._loop();


    __LDBG_printf("enabling GPIO interrupts");

    // read pins again
    auto currentStates = deepSleepPinState._readStates();
    // update interrupt levels
    interrupt_levels = deepSleepPinState.getValues(currentStates);

    __DBG_printf("final states %s %s ", BitsToStr<16, true>(states&kButtonPinsMask).c_str(), BitsToStr<16, true>(currentStates&kButtonPinsMask).c_str());

    // and compare if another state must be pushed
    for(auto pin: kButtonPins) {
        if ((states & _BV(pin)) ^ (currentStates & _BV(pin))) {
            PinMonitor::eventBuffer.emplace_back(micros(), pin, interrupt_levels); // add final state
            __LDBG_printf("pin=%02u final queued=%u states=%s interrupt_levels=%s mask=%s", pin, (currentStates & _BV(pin)) != 0, BitsToStr<16, true>(currentStates).c_str(), BitsToStr<16, true>(interrupt_levels).c_str(), BitsToStr<16, true>(_BV(pin)).c_str());
        }
    }

    // clear all interrupts now
    GPIEC = GPIE;
    ETS_GPIO_INTR_ENABLE();

    __DBG_printf("---");
    for(auto &event: PinMonitor::eventBuffer) {
        __DBG_printf("pin=%u value=%u time=%u", event.pin(), event.value(), event.getTime());
    }

#if DEBUG_PIN_STATE
    LoopFunctions::callOnce([]() {
        // display button states @boot and @now
        deepSleepPinState.merge();
        __LDBG_printf("boot pin states: ");
        for(uint8_t i = 0; i < deepSleepPinState._count; i++) {
            __LDBG_printf("state %u: %s", i, deepSleepPinState.toString(deepSleepPinState._states[i], deepSleepPinState._times[i]).c_str());
        }
    });
#endif

    WiFiCallbacks::add(WiFiCallbacks::EventType::ANY, wifiCallback);
    LoopFunctions::add(loop);

    _setup();
    _resetAutoSleep();
    // _resolveActionHostnames();

    _updateSystemComboButtonLED();
    __LDBG_printf("setup() done");

    dependencies->dependsOn(F("http"), [](const PluginComponent *plugin) {
        auto server = WebServer::Plugin::getWebServerObject();
        if (server) {
            WebServer::Plugin::addHandler(F("/rc/*"), [](AsyncWebServerRequest *request) {
                String message;
                __DBG_printf("remote control web server handler url=%s", request->url().c_str());
                if (request->url().endsWith(F("/deep_sleep.html"))) {
                    message = F("Device entering deep sleep...");
                    request->onDisconnect([]() {
                        // cannot be called from ISRs
                        LoopFunctions::callOnce([]() {
                            RemoteControlPlugin::enterDeepSleep();
                        });
                    });
                }
                else if (request->url().endsWith(F("/disable_auto_sleep.html"))) {
                    message = F("Auto sleep has been disabled");
                    RemoteControlPlugin::disableAutoSleep();
                }
                else if (request->url().endsWith(F("/enable_auto_sleep.html"))) {
                    message = F("Auto sleep has been enabled");
                    RemoteControlPlugin::enableAutoSleep();
                }
                else {
                    WebServer::Plugin::send(404, request);
                    return;
                }
                HttpHeaders headers;
                if (!WebServer::Plugin::sendFileResponse(200, F("/.message.html"), request, headers, new MessageTemplate(message))) {
                    __DBG_printf("failed to send /.message.html");
                }
            });
        }
    }, this);
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
    pinMode(IOT_REMOTE_CONTROL_AWAKE_PIN, INPUT);
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
    device.addMenuItem(F("Enable Auto Sleep"), F("rc/enable_auto_sleep.html"));
    device.addMenuItem(F("Disable Auto Sleep"), F("rc/disable_auto_sleep.html"));
    device.addMenuItem(F("Enable Deep Sleep"), F("rc/deep_sleep.html"));
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
        // start delayed plugins
        PluginComponents::Register::setDelayedStartupTime(0);
        if (_autoDiscoveryRunOnce && MQTTClient::safeIsConnected() && IS_TIME_VALID(time(nullptr))) {
            __LDBG_printf("usb power detected, publishing auto discovery");
            _autoDiscoveryRunOnce = false;
            publishAutoDiscovery();
        }
    }
    else if (isAutoSleepEnabled()) {
        if (_millis >= __autoSleepTimeout) {
            if (
                (_minAwakeTimeout && (millis() <_minAwakeTimeout)) ||
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
                    // BUILDIN_LED_SETP(200, BlinkLEDTimer::Bitset(0b000000000000000001010101U, 24U));
                    BlinkLEDTimer::setPattern(__LED_BUILTIN, 100, BlinkLEDTimer::Bitset(0b000000000000000001010101U, 24U));
                    _signalWarning = true;
                    __DBG_printf("signal warning");
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
    config.enterDeepSleep(std::chrono::duration_cast<KFCFWConfiguration::milliseconds>(KFCFWConfiguration::minutes(_config.deep_sleep_time)), WAKE_NO_RFCAL, 1);
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
{xxxxxx_LDBG_printf("wifi=%u events=%u locked=%s", WiFi.isConnected(), _events.size(), String(_buttonsLocked & 0b1111, 2).c_str());

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

