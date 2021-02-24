/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <Scheduler.h>
#include "remote_def.h"
#include "remote_base.h"
#include "remote_form.h"
#include "remote_action.h"
#include "remote_event_queue.h"
#include <plugins.h>
#if MQTT_SUPPORT
#include "../src/plugins/mqtt/mqtt_client.h"
#endif

#if DEBUG_IOT_REMOTE_CONTROL
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using namespace RemoteControl;

#if MQTT_SUPPORT && MQTT_AUTO_DISCOVERY

class MqttRemote : public MQTTComponent {
public:
    MqttRemote() :
        MQTTComponent(ComponentType::DEVICE_AUTOMATION),
        _discoveryPending(false)
    {
    }

    virtual AutoDiscoveryPtr nextAutoDiscovery(FormatType format, uint8_t num) override;
    virtual uint8_t getAutoDiscoveryCount() const override;

    static String getMQTTTopic();

    void publishAutoDiscovery() {
        auto client = MQTTClient::getClient();
        if (!_discoveryPending && client && !client->isAutoDiscoveryRunning()) {
            _discoveryPending = true;
            client->publishAutoDiscovery(true);

        }
    }

    void _setup() {
        MQTTClient::safeRegisterComponent(this);
    }

    void _reconfigure() {
        MQTTClient::safeReRegisterComponent(this);
        _discoveryPending = false;
    }
    void _shutdown() {
        MQTTClient::safeUnregisterComponent(this);
    }

private:
    bool _discoveryPending;
};

#else

class MqttRemote {
public:
    void _setup() {}
    void _reconfigure() {}
    void _shutdown() {}
};

#endif

class RemoteControlPlugin : public PluginComponent, public Base, public MqttRemote {
public:
    struct PinState {
        uint32_t _time;
        uint32_t _state: 17;
        uint32_t _read: 17;
        PinState(uint32_t time = ~0U) : _time(time), _state(0), _read(0) {}
        bool isValid() const {
            return _time != ~0U;
        }
        static constexpr bool activeLow() {
            return PIN_MONITOR_ACTIVE_STATE == PinMonitor::ActiveStateType::PRESSED_WHEN_LOW;
        }
        static constexpr bool activeHigh() {
            return PIN_MONITOR_ACTIVE_STATE == PinMonitor::ActiveStateType::PRESSED_WHEN_HIGH;
        }
        bool anyPressed() const {
            return isValid() && ((activeLow() ? _state ^ _read : _state) & _read);
        }
        bool getPin(uint8_t pin) const {
            return _state & _BV(pin);
        }
        void setPin(uint8_t pin, bool value) {
            _read |= _BV(pin);
            if (value) {
                _state |= _BV(pin);
            }
            else {
                _state &= ~_BV(pin);
            }
        }
        String toString();
    };

public:
    RemoteControlPlugin();

    virtual void setup(SetupModeType mode) override;
    virtual void reconfigure(const String &source) override;
    virtual void shutdown() override;
    virtual void prepareDeepSleep(uint32_t sleepTimeMillis);
    virtual void getStatus(Print &output) override;
    virtual void createMenu() override;

    virtual void createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request) override;

#if AT_MODE_SUPPORTED
    virtual void atModeHelpGenerator() override;
    virtual bool atModeHandler(AtModeArgs &args) override;
#endif

public:
    static void loop();
    static void wifiCallback(WiFiCallbacks::EventType event, void *payload);

    static void disableAutoSleep();
    static void disableAutoSleepHandler(AsyncWebServerRequest *request);
    static void deepSleepHandler(AsyncWebServerRequest *request);

    // store and return keys that are pressed
    // this is for reading the initial state directly after booting and feed it
    // into the pinMonitor
    PinState readPinState();
    PinState getPinState() const {
        return _pinState;
    }

    static RemoteControlPlugin &getInstance();

private:
    // virtual void _onShortPress(Button &button);
    // virtual void _onLongPress(Button &button);
    // virtual void _onRepeat(Button &button);

    void _updateButtonConfig();
    void _loop();
    bool _isUsbPowered() const;
    void _readConfig();
    // void _installWebhooks();
    // void _addButtonEvent(ButtonEvent &&event);
    void _sendEvents();
    bool _hasEvents() const;

    Action *_getAction(ActionIdType id) const;
    // resolve all hostnames that are marked as resolve once and store them as IP
    void _resolveActionHostnames();

    bool _isButtonLocked(uint8_t button) const {
        return _buttonsLocked & (1 << button);
    }
    void _lockButton(uint8_t button) {
        __LDBG_printf("btn %u locked", button);
        _buttonsLocked |= (1 << button);
    }
    void _unlockButton(uint8_t button) {
        __LDBG_printf("btn %u unlocked", button);
        _buttonsLocked &= ~(1 << button);
    }

private:
    using ActionPtr = std::unique_ptr<Action>;
    using ActionVector = std::vector<ActionPtr>;

    ActionVector _actions;
    // Event::Timer _loopTimer;
    PinState _pinState;
    bool _signalWarning;
};

inline bool RemoteControlPlugin::_hasEvents() const
{
    return !_queue.empty();
}

inline void RemoteControlPlugin::disableAutoSleep()
{
    __LDBG_printf("disabled");
    getInstance()._autoSleepTimeout = kAutoSleepDisabled;
}

#include <debug_helper_disable.h>
