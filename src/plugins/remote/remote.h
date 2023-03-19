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
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

using namespace RemoteControl;

#if MQTT_SUPPORT && MQTT_AUTO_DISCOVERY

#   include "remote_mqtt.h"

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


class RemoteControlPlugin : public PluginComponent, public Base, public MqttRemote {
public:
    using Base::_setAutoSleepTimeout;

    RemoteControlPlugin();

    virtual void setup(SetupModeType mode, const PluginComponents::DependenciesPtr &dependencies) override;
    virtual void reconfigure(const String &source) override;
    virtual void shutdown() override;
    virtual void prepareDeepSleep(uint32_t sleepTimeMillis);
    virtual void getStatus(Print &output) override;
    virtual void createMenu() override;

    virtual void createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request) override;

    #if AT_MODE_SUPPORTED
        #if AT_MODE_HELP_SUPPORTED
            virtual ATModeCommandHelpArrayPtr atModeCommandHelp(size_t &size) const;
        #endif
        virtual bool atModeHandler(AtModeArgs &args) override;
    #endif

public:
    static void loop();
    static void wifiCallback(WiFiCallbacks::EventType event, void *payload);

    static void enterDeepSleep();
    static void disableAutoSleep();
    static void enableAutoSleep();
    static void prepareForUpdate();
    static void disableAutoSleepHandler(AsyncWebServerRequest *request);
    static void deepSleepHandler(AsyncWebServerRequest *request);
    static void webHandler(AsyncWebServerRequest *request);

    static RemoteControlPlugin &getInstance();

    inline static bool isCharging() {
        return getInstance()._isCharging;
    }
    void _chargingStateChanged(bool active);

protected:
    // returns true if status has been sent, false if sensor is not ready
    virtual bool _sendBatteryStateAndSleep();

private:
    // virtual void _onShortPress(Button &button);
    // virtual void _onLongPress(Button &button);
    // virtual void _onRepeat(Button &button);

    // update button config, set init to true for first call after boot
    void _updateButtonConfig();
    // reset buttons states to remove all events. if buttons are pressed the debouncer will filter the events when released
    void _loop();
    void _readConfig();
    // void _installWebhooks();
    // void _addButtonEvent(ButtonEvent &&event);
    void _sendEvents();
    bool _hasEvents() const;

    Action *_getAction(ActionIdType id) const;
    // resolve all hostnames that are marked as resolve once and store them as IP
    void _resolveActionHostnames();

    // bool _isButtonLocked(uint8_t button) const {
    //     return _buttonsLocked & (1 << button);
    // }
    // void _lockButton(uint8_t button) {
    //     __LDBG_printf("btn %u locked", button);
    //     _buttonsLocked |= (1 << button);
    // }
    // void _unlockButton(uint8_t button) {
    //     __LDBG_printf("btn %u unlocked", button);
    //     _buttonsLocked &= ~(1 << button);
    // }

    void _prepareForUpdate();
    void _enterDeepSleep();

private:
    using ActionPtr = std::unique_ptr<Action>;
    using ActionVector = std::vector<ActionPtr>;

    ActionVector _actions;
    uint32_t _readUsbPinTimeout;
    // Event::Timer _loopTimer;
};

inline void RemoteControlPlugin::_prepareForUpdate()
{
    _setAutoSleepTimeout(millis() + (300U * 1000U), _config.max_awake_time * 60U);
}

inline bool RemoteControlPlugin::_hasEvents() const
{
    return !_queue.empty();
}

inline void RemoteControlPlugin::disableAutoSleep()
{
    __LDBG_printf("auto sleep disabled");
    getInstance()._disableAutoSleepTimeout();
}

inline void RemoteControlPlugin::enableAutoSleep()
{
    __LDBG_printf("auto sleep enabled");
    getInstance()._setAutoSleepTimeout(1, ~0U);
}

inline void RemoteControlPlugin::prepareForUpdate()
{
    __LDBG_printf("prepare for update");
    getInstance()._prepareForUpdate();
}

inline void RemoteControlPlugin::enterDeepSleep()
{
    __LDBG_printf("enter deep sleep");
    getInstance()._enterDeepSleep();
}

extern bool RemoteControlPluginIsCharging();

#include <debug_helper_disable.h>
