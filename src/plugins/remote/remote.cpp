/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
// #include <WiFiCallbacks.h>
#include "WebUISocket.h"
#include "blink_led_timer.h"
#include "deep_sleep.h"
#include "kfc_fw_config.h"
#include "plugins_menu.h"
#include "push_button.h"
#include "remote.h"
#include "remote_button.h"
#include "remote_def.h"
#include "web_server.h"
#include "web_server_action.h"
#include <BitsToStr.h>
#include <LoopFunctions.h>
#include <ReadADC.h>
#include "../src/plugins/sensor/sensor.h"
#if HTTP2SERIAL_SUPPORT
#    include "../src/plugins/http2serial/http2serial.h"
#    define IF_HTTP2SERIAL_SUPPORT(...) __VA_ARGS__
#else
#    define IF_HTTP2SERIAL_SUPPORT(...)
#endif
#if SYSLOG_SUPPORT
#    include "../src/plugins/syslog/syslog_plugin.h"
#endif

#if DEBUG_IOT_REMOTE_CONTROL
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

#if ESP8266
#    define SHORT_POINTER_FMT  "%05x"
#    define SHORT_POINTER(ptr) ((uint32_t)ptr & 0xfffff)
#else
#    define SHORT_POINTER_FMT  "%08x"
#    define SHORT_POINTER(ptr) ptr
#endif

using Plugins = KFCConfigurationClasses::PluginsType;
using namespace RemoteControl;

static RemoteControlPlugin plugin;

PROGMEM_DEFINE_PLUGIN_OPTIONS(
    RemoteControlPlugin,
    "remotectrl", // name
    "Remote Control", // friendly name
    "", // web_templates
    // config_forms
    "general,events,combos,actions,buttons-1,buttons-2,buttons-3,buttons-4",
    "", // reconfigure_dependencies
    PluginComponent::PriorityType::REMOTE,
    PluginComponent::RTCMemoryId::DEEP_SLEEP,
    static_cast<uint8_t>(PluginComponent::MenuType::CUSTOM),
    false, // allow_safe_mode
    true, // setup_after_deep_sleep
    true, // has_get_status
    true, // has_config_forms
    false, // has_web_ui
    false, // has_web_templates
    true, // has_at_mode
    0 // __reserved
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
    for (const auto &buttonPtr : pinMonitor.getHandlers()) {
        if (std::find(kButtonPins.begin(), kButtonPins.end(), buttonPtr->getPin()) != kButtonPins.end()) {
            auto &button = static_cast<Button &>(*buttonPtr);
            // __LDBG_printf("arg=%p btn=%p pin=%u digital_read=%u inverted=%u owner=%u", button.getArg(), &button, button.getPin(), digitalRead(button.getPin()), button.isInverted(), button.getBase() == this);
            if (button.getBase() == this) { // auto downcast of this
                #if DEBUG_IOT_REMOTE_CONTROL && 0
                    auto &cfg = _config.actions[button.getButtonNum()];
                    __LDBG_printf("button#=%u on_action.id=%u,%u,%u,%u,%u,%u,%u,%u udp.enabled=%s mqtt.enabled=%s",
                        button.getButtonNum(),
                        cfg.down, cfg.up, cfg.press, cfg.single_click, cfg.double_click, cfg.long_press, cfg.hold, cfg.hold_released,
                        BitsToStr<9, true>(cfg.udp.event_bits).c_str(), BitsToStr<9, true>(cfg.udp.event_bits).c_str());
                #endif
                button.updateConfig();
            }
        }
    }
}

void RemoteControlPlugin::setup(SetupModeType mode, const PluginComponents::DependenciesPtr &dependencies)
{
    if (mode == SetupModeType::DELAYED_AUTO_WAKE_UP) {
        __LDBG_printf("delayed mode");
        return;
    }
    DeepSleep::deepSleepPinState.merge();
     _readConfig();

    #if PIN_MONITOR_BUTTON_GROUPS
        SingleClickGroupPtr group1;
        group1.reset(_config.click_time);
        SingleClickGroupPtr group2;
        group2.reset(_config.click_time);
    #endif

    for (uint8_t buttonNum = 0; buttonNum < kButtonPins.size(); buttonNum++) {
        auto pin = kButtonPins[buttonNum];
        #if PIN_MONITOR_BUTTON_GROUPS
            auto &button = pinMonitor.attach<Button>(pin, buttonNum, this);
            using EventNameType = Plugins::RemoteControl::EventNameType;

            if (!_config.enabled[EventNameType::BUTTON_PRESS]) {
                if (buttonNum == 0 || buttonNum == 3) {
                    button.setSingleClickGroup(group1);
                }
                else {
                    button.setSingleClickGroup(group2);
                }
            } else {
                button.setSingleClickGroup(group1);
                group1.reset(_config.click_time);
            }
        #else
            pinMonitor.attach<Button>(pin, n, this);
        #endif
    }

    #if IOT_REMOTE_CONTROL_CHARGING_PIN != -1
        pinMonitor.attachPinType<ChargingDetection>(HardwarePinType::SIMPLE, IOT_REMOTE_CONTROL_CHARGING_PIN, this);
        pinMode(IOT_SENSOR_BATTERY_CHARGING_COMPLETE_PIN, INPUT);
    #endif

    // freeze interrupt states
    ETS_GPIO_INTR_DISABLE();
    // clear interrupts for all pins
    GPIEC = kButtonPinsMask;

    // read all pins to setup buttons
    DeepSleep::deepSleepPinState.merge();
    _updateButtonConfig();

    // start pin monitor after adding the pins
    pinMonitor.begin();

    // apply pressed states to pin monitor
    for(auto &pin: pinMonitor.getPins()) {
        if (pin->getHardwarePinType() == HardwarePinType::DEBOUNCE) {
            auto time = DeepSleep::deepSleepPinState.getFirstPressed(pin->getPin());
            if (time) {
                __LDBG_printf("pin=%u pressed=%u", pin->getPin(), time);
                pin->getDebounce()->setPressed(1);
            }
        }
    }

    // check if the system menu was activated
    auto states = DeepSleep::deepSleepPinState.getStates();
    __LDBG_printf("states=%04x mask=%04x combo=%u", states, kGPIOSystemComboBitMask, (states & kGPIOSystemComboBitMask) == kGPIOSystemComboBitMask);
    if ((states & kGPIOSystemComboBitMask) == kGPIOSystemComboBitMask) {
        systemButtonComboEvent(true); // enable combo
    }

    // store current pin levels
    interrupt_levels = DeepSleep::deepSleepPinState.getGPIOStates();
    // enable GPIO interrupts
    ETS_GPIO_INTR_ENABLE();

    #if 1
    // TODO if a button is released between 60 and 85ms, the interrupt is not triggered
    // DeepSleep::deepSleepPinState.getGPIOStates() and GPIO::read() show the difference
    // find and manually add them to the queue and clear PinMonitor
    for (uint8_t buttonNum = 0; buttonNum < kButtonPins.size(); buttonNum++) {
        auto pin = kButtonPins[buttonNum];
        auto time = DeepSleep::deepSleepPinState.getFirstPressed(pin);
        if (time) {
            if ((GPIO::read() & GPIO_PIN_TO_MASK(pin)) == DeepSleep::PinState::activeLow()) {
                __LDBG_printf("queueEvent pin=%u button=%u", pin, buttonNum);
                queueEvent(EventType::DOWN, buttonNum, 0, time / 1000U + 1, 0);
                auto now = millis();
                queueEvent(EventType::PRESSED, buttonNum, 0, now, 0);
                queueEvent(EventType::SINGLE_CLICK, buttonNum, 1, now, 0);
                queueEvent(EventType::UP, buttonNum, 0, now, 0);
                // clear events
                for(auto &pinPtr: pinMonitor.getPins()) {
                    if (pinPtr->getPin() == pin) {
                        pinPtr->clear();
                        break;
                    }
                }
            }
        }
    }
    #endif

    #if DEBUG_PIN_STATE && 0
        LoopFunctions::callOnce([]() {
            // display button states @boot and @now
            DeepSleep::deepSleepPinState.merge();
            __LDBG_printf("pin states init time=%uus: ", DeepSleep::deepSleepPinState._time);
            for (uint8_t i = 0; i < DeepSleep::deepSleepPinState._count; i++) {
                __LDBG_printf("state %u: %s", i, DeepSleep::deepSleepPinState.toString(DeepSleep::deepSleepPinState._states[i], DeepSleep::deepSleepPinState._times[i]).c_str());
            }
            __LDBG_printf("sequence: ");
            for (uint8_t i = 0; i < 16; i++) {
                __LDBG_printf("down %u @ %uus", i, DeepSleep::deepSleepPinState.getFirstPressed(i));
            }
        });
    #endif

    WiFiCallbacks::add(WiFiCallbacks::EventType::ANY, wifiCallback);
    LOOP_FUNCTION_ADD(loop);

    _setup();
    _resetAutoSleep();
    // _resolveActionHostnames();

    // check if the device is charging
    if ((_isCharging = digitalRead(IOT_REMOTE_CONTROL_CHARGING_PIN))) {
        // fire event
        _chargingStateChanged(true);
    }
    else if (DeepSleep::deepSleepParams.getWakeupMode() == DeepSleep::WakeupMode::AUTO) {
        __LDBG_print("auto wakeup, sending battery state");
        // allow up to 30 seconds to connect to WiFi and send MQTT status
        _setAutoSleepTimeout(millis() + 30000, ~0U);
        _sendBatteryStateAndGotoSleep = true;
    }

    _updateSystemComboButtonLED();

    __LDBG_printf("setup done");

    dependencies->dependsOn(
        F("http"), [](const PluginComponent *plugin, DependencyResponseType response) {
            if (response != DependencyResponseType::SUCCESS) {
                return;
            }
            auto server = WebServer::Plugin::getWebServerObject();
            if (!server) {
                return;
            }
            WebServer::Plugin::addHandler(F(REMOTE_CONTROL_WEB_HANDLER_PREFIX "*"), webHandler);
        },
        this
    );
}

static String _getJsonBatteryStatus(const Sensor_Battery::Status &status)
{
    using KFCConfigurationClasses::System;
    PrintString json(F("{\"device\":\""));
    json.print(System::Device::getName());
    json.print(F("\",\"event\":\"battery\""));
    json.printf_P(PSTR(",\"voltage\":%.2f"), status.getVoltage());
    json.printf_P(PSTR(",\"level\":%u"), status.getLevel());
    json.printf_P(PSTR(",\"status\":\"%s\""), status.getChargingStatus().c_str());
    json.printf_P(PSTR(",\"ts\":%u}"), millis());
    return json;
}

bool RemoteControlPlugin::_sendBatteryStateAndSleep()
{
    // send message to MQTT and UDP
    for(const auto &sensor: SensorPlugin::getSensors()) {
        if (sensor->getType() == SensorPlugin::SensorType::BATTERY) {
            sensor->setNextMqttUpdate(sensor->DEFAULT_MQTT_UPDATE_RATE);
            auto batterySensor = reinterpret_cast<Sensor_Battery *>(sensor);
            batterySensor->readADC(batterySensor->kADCNumReads * 4);
            auto status = batterySensor->readSensor();
            __DBG_printf("battery state voltage=%.3f level=%u", status.getVoltage(), status.getLevel());
            if (status.getVoltage() == 0) {
                return false;
            }
            if (_config.udp_enable) {
                auto payload = _getJsonBatteryStatus(status);
                WiFiUDP udp;
                udp.beginPacket(Plugins::RemoteControl::getUdpHost(), _config.udp_port) && (udp.write(payload.c_str(), payload.length()) == payload.length()) && udp.endPacket();
            }
            sensor->publishState();
            break;
        }
    }
    // reset to default auto sleep
    _resetAutoSleep();
    return true;
}

void RemoteControlPlugin::webHandler(AsyncWebServerRequest *request)
{
    using AuthType = WebServer::Action::AuthType;
    using StateType = WebServer::Action::StateType;
    using Handler = WebServer::Action::Handler;
    using Plugin = WebServer::Plugin;
    using MessageType = WebServer::MessageType;

    if (!Plugin::isAuthenticated(request)) {
        Plugin::send(403, request);
        return;
    }

    __LDBG_printf("remote control web server handler url=%s", request->url().c_str());

    if (request->url() == F(REMOTE_CONTROL_WEB_HANDLER_PREFIX "deep_sleep.html")) {
        auto &session = Handler::getInstance().initSession(request, F(REMOTE_CONTROL_WEB_HANDLER_PREFIX "deep_sleep.html"), F("Remote Control"), AuthType::AUTH);
        if (session.isNew()) {
            session.setStatus(F("Device is entering deep sleep... Press any button to wake it up"), MessageType::WARNING);
            session = StateType::EXECUTING;
            // give it 15 seconds to reload the page
            request->onDisconnect([]() {
                _Scheduler.add(Event::seconds(15), false, [](Event::CallbackTimerPtr) {
                    RemoteControlPlugin::enterDeepSleep();
                });
            });
        }
        else if (session.isExecuting()) {
            session = StateType::FINISHED;
            // after displaying the status, goto deep sleep
            request->onDisconnect([]() {
                // cannot be called from ISRs
                LoopFunctions::callOnce([]() {
                    RemoteControlPlugin::enterDeepSleep();
                });
            });
        }
    }
    else if (request->url() == F(REMOTE_CONTROL_WEB_HANDLER_PREFIX "disable_auto_sleep.html")) {
        RemoteControlPlugin::disableAutoSleep();
        Plugin::message(request, WebServer::MessageType::INFO, F("Auto sleep has been disabled"), F("Remote Control"));
    }
    else if (request->url() == F(REMOTE_CONTROL_WEB_HANDLER_PREFIX "enable_auto_sleep.html")) {
        RemoteControlPlugin::enableAutoSleep();
        Plugin::message(request, MessageType::SUCCESS, F("Auto sleep has been enabled"), F("Remote Control"));
    }
    else {
        Plugin::send(404, request);
    }
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
    #if PIN_MONITOR_USE_GPIO_INTERRUPT
        // disable GPIO interrupts before detaching the Pin Monitor
        PinMonitor::GPIOInterruptsDisable();
    #endif

    _shutdown();
    pinMonitor.detach(this);

    LoopFunctions::remove(loop);
    WiFiCallbacks::remove(WiFiCallbacks::EventType::ANY, wifiCallback);
}

void RemoteControlPlugin::prepareDeepSleep(uint32_t sleepTimeMillis)
{
    __LDBG_printf("time=%u", sleepTimeMillis);

    #if PIN_MONITOR_USE_GPIO_INTERRUPT
        PinMonitor::GPIOInterruptsDisable();
    #endif

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
    output.printf_P(PSTR(HTML_S(br) "force deep sleep after %u minutes"), _config.max_awake_time);
    if (_config.udp_enable) {
        output.printf_P(PSTR(", UDP %s:%u enabled"), Plugins::RemoteControl::getUdpHost(), _config._get_udp_port());
    }
    auto deepSleepTime = _config.deep_sleep_time;
    if (deepSleepTime) {
        output.printf_P(PSTR(", Report battery state every %s"), formatTimeShort(F(", "), F(" and "), false, 0, (deepSleepTime % 60), (deepSleepTime / 60) % 24, (deepSleepTime / 1440)).c_str());
    }
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
    device.addMenuItem(F("Enable Auto Sleep"), FPSTR(PSTR(REMOTE_CONTROL_WEB_HANDLER_PREFIX "enable_auto_sleep.html") + 1));
    device.addMenuItem(F("Disable Auto Sleep"), FPSTR(PSTR(REMOTE_CONTROL_WEB_HANDLER_PREFIX "disable_auto_sleep.html") + 1));
    device.addMenuItem(F("Enter Deep Sleep..."), FPSTR(PSTR(REMOTE_CONTROL_WEB_HANDLER_PREFIX "deep_sleep.html") + 1));
}

#if AT_MODE_SUPPORTED

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(RCDSLP, "RCDSLP", "[<time in seconds|0>]", "Set or disable auto sleep");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(RCBAT, "RCBAT", "[<interval in seconds|0>]", "Publish battery status");

#if AT_MODE_HELP_SUPPORTED

ATModeCommandHelpArrayPtr RemoteControlPlugin::atModeCommandHelp(size_t &size) const
{
    static ATModeCommandHelpArray tmp PROGMEM = {
        PROGMEM_AT_MODE_HELP_COMMAND(RCDSLP),
        PROGMEM_AT_MODE_HELP_COMMAND(RCBAT)
    };
    size = sizeof(tmp) / sizeof(tmp[0]);
    return tmp;
}

#endif

bool RemoteControlPlugin::atModeHandler(AtModeArgs &args)
{
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(RCDSLP))) {
        int time = args.toInt(0);
        if (time < 1) {
            _disableAutoSleepTimeout();
            _updateMaxAwakeTimeout();
            args.printf_P(PSTR("Auto sleep disabled"));
        } else {
            static constexpr uint32_t kMaxAwakeExtraTime = 300; // seconds
            _setAutoSleepTimeout(millis() + (time * 1000U), kMaxAwakeExtraTime);
            args.printf_P(PSTR("Auto sleep timeout in %u seconds, deep sleep timeout in %.1f minutes"), time, (time + kMaxAwakeExtraTime) / 60.0);
        }
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(RCBAT))) {
        int time = args.toInt(0);
        if (time < 1) {
            _sendBatteryStateAndSleep();
        }
        else {
            _Scheduler.add(Event::seconds(time), 10, [this](Event::CallbackTimerPtr) {
                _sendBatteryStateAndSleep();
            });
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
    if (!isCharging()) {
        auto _millis = millis();

        if (_maxAwakeTimeout && _millis >= _maxAwakeTimeout) {
            __LDBG_printf("forcing deep sleep @ max awake timeout=%u", _maxAwakeTimeout);
            _enterDeepSleep();
        }

        if (isAutoSleepEnabled()) {
            if (_millis >= __autoSleepTimeout) {
                if (
                    (_minAwakeTimeout && (millis() < _minAwakeTimeout)) || _hasEvents() || _isSystemComboActive() ||
                    #if MQTT_AUTO_DISCOVERY
                        MQTT::Client::safeIsAutoDiscoveryRunning() ||
                    #endif
                    #if HTTP2SERIAL_SUPPORT
                        Http2Serial::hasAuthenticatedClients() ||
                    #endif
                    WebUISocket::hasAuthenticatedClients()) {
                    // check again in 500ms
                    __autoSleepTimeout = _millis + 500;

                    // start blinking after the timeout or 10 seconds
                    if (!_signalWarning && (_millis > std::max((_config.auto_sleep_time * 1000U), 10000U))) {
                        // BUILTIN_LED_SETP(200, BlinkLEDTimer::Bitset(0b000000000000000001010101U, 24U));
                        BlinkLEDTimer::setPattern(__LED_BUILTIN, 100, BlinkLEDTimer::Bitset(0b000000000000000001010101U, 24U));
                        _signalWarning = true;
                        __LDBG_printf("signal warning");
                    }
                }
                else {
                    _enterDeepSleep();
                }
            }
        }
    }

    _updateSystemComboButton();

    delay(_hasEvents() ? 1 : 10);
}

void RemoteControlPlugin::_enterDeepSleep()
{
    #if PIN_MONITOR_USE_GPIO_INTERRUPT
        PinMonitor::GPIOInterruptsDisable();
    #endif

    _maxAwakeTimeout = 0; // avoid calling it repeatedly
    _disableAutoSleepTimeout();

    #if SYSLOG_SUPPORT
        SyslogPlugin::waitForQueue(std::clamp<uint16_t>(_config.auto_sleep_time / 10, 500, 1500));
    #endif

    __LDBG_printf("entering deep sleep (auto=%d, time=%us)", _config.deep_sleep_time, _config.deep_sleep_time);
    config.enterDeepSleep(std::chrono::duration_cast<KFCFWConfiguration::milliseconds>(KFCFWConfiguration::minutes(_config.deep_sleep_time)), WAKE_NO_RFCAL, 1);
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
#    if DEBUG_IOT_REMOTE_CONTROL
        uint32_t start = micros();
#    endif
        noInterrupts();
        for(auto &&item: _events) {
            if (item.getType() != EventType::NONE) {
                tmp.emplace_back(std::move(item));
            }
        }
        _events = std::move(tmp);
        interrupts();

#    if DEBUG_IOT_REMOTE_CONTROL
        auto dur = micros() - start;
        __LDBG_printf("cleaning events time=%uus (_events=%u bytes)", dur, sizeof(_events));
#    endif

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
    for (auto &action : _actions) {
        action->resolve();
    }
}

void RemoteControlPlugin::_chargingStateChanged(bool active)
{
    __LDBG_printf("event charging state=%u changed", active);
    if (active) {
        _resetAutoSleep();
        // start delayed plugins
        PluginComponents::Register::setDelayedStartupTime(0);
        if (_autoDiscoveryRunOnce) {
            _autoDiscoveryRunOnce = false;

            _Scheduler.add(Event::seconds(30), false, [this](Event::CallbackTimerPtr) {
                __LDBG_printf("charging state detected, publishing auto discovery");
                publishAutoDiscovery();
            });
        }
    }
}

// external function for battery sensor
bool RemoteControlPluginIsCharging()
{
    return RemoteControlPlugin::isCharging();
}
