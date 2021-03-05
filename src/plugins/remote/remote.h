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
        MQTTComponent(ComponentType::DEVICE_AUTOMATION)
    {
    }

    virtual AutoDiscovery::EntityPtr getAutoDiscovery(FormatType format, uint8_t num) override;
    virtual uint8_t getAutoDiscoveryCount() const override;

    virtual void onConnect() {
        if (_autoDiscoveryPending) {
            publishAutoDiscovery();
        }
    }

    void publishAutoDiscovery() {
        if (isConnected()) {
            __LDBG_printf("auto discovery running=%u registered=%u", client().isAutoDiscoveryRunning(), MQTTClient::isComponentRegistered(this));
            if (client().publishAutoDiscovery()) {
                _autoDiscoveryPending = false;
            }
        }
        else {
            _autoDiscoveryPending = true;
        }
    }

    void _setup() {
        _updateAutoDiscoveryCount();
        MQTTClient::registerComponent(this);
    }

    void _reconfigure() {
        _updateAutoDiscoveryCount();
    }

    void _shutdown() {
        MQTTClient::unregisterComponent(this);
    }

    static String getMQTTTopic() {
        return MQTTClient::formatTopic(F("/triggers"));
    }

private:
    void _updateAutoDiscoveryCount();

private:
    bool _autoDiscoveryPending;
    uint8_t _autoDiscoveryCount;
};

#else

class MqttRemote {
public:
    void _setup() {}
    void _reconfigure() {}
    void _shutdown() {}
    void publishAutoDiscovery() {}
};

#endif

class RemoteControlPlugin;

struct __attribute__((packed)) PinState {
public:
    static constexpr uint32_t _time = 15000;     // micros() is not available when called inside preinit
    uint32_t _state;
    uint32_t _read: 31;
    uint32_t _valid: 1;

    PinState(bool valid = false) : _state(0), _read(0), _valid(valid) {
        if (_valid) {
            _readStates();
        }
    }

    inline __attribute__((__always_inline__))
    bool isValid() const {
        return _valid;
    }

    static constexpr bool activeLow() {
        return PIN_MONITOR_ACTIVE_STATE == PinMonitor::ActiveStateType::PRESSED_WHEN_LOW;
    }

    static constexpr bool activeHigh() {
        return PIN_MONITOR_ACTIVE_STATE == PinMonitor::ActiveStateType::PRESSED_WHEN_HIGH;
    }

    inline __attribute__((__always_inline__))
    bool anyPressed() const {
        return isValid() && ((activeLow() ? _state ^ _read : _state) & _read);
    }

    inline __attribute__((__always_inline__))
    bool getPin(uint8_t pin) const {
        return _state & _BV(pin);
    }

    inline __attribute__((__always_inline__))
    void _readStates() {
        for(const auto pin: kButtonPins) {
            setPin(_BV(pin), digitalRead(pin));
        }
    }

private:
    friend RemoteControlPlugin;

    inline __attribute__((__always_inline__))
    void setPin(uint32_t mask, bool value) {
        _read |= mask;
        _state = value ? (_state | mask) : (_state & ~mask);
    }

    String toString();
};

class RemoteControlPlugin : public PluginComponent, public Base, public MqttRemote {
public:
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

    static RemoteControlPlugin &getInstance();

private:
    // virtual void _onShortPress(Button &button);
    // virtual void _onLongPress(Button &button);
    // virtual void _onRepeat(Button &button);

    void _updateButtonConfig();
    // reset buttons states to remove all events. if buttons are pressed the debouncer will filter the events when released
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

public:
    void _enterDeepSleep();

private:
    using ActionPtr = std::unique_ptr<Action>;
    using ActionVector = std::vector<ActionPtr>;

    ActionVector _actions;
    uint32_t _readUsbPinTimeout;
    // Event::Timer _loopTimer;
};

inline bool RemoteControlPlugin::_hasEvents() const
{
    return !_queue.empty();
}

inline void RemoteControlPlugin::disableAutoSleep()
{
    __LDBG_printf("disabled");
    getInstance()._disableAutoSleepTimeout();
}

extern "C" PinState _pinState;

void remotectrl_preinit_function();

#include <debug_helper_disable.h>
