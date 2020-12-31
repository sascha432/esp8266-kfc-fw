/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <Scheduler.h>
#include "../src/plugins/mqtt/mqtt_client.h"
#include "remote_def.h"
#include "remote_base.h"
#include "remote_form.h"
#include "remote_action.h"
#include "remote_event_queue.h"
#include <plugins.h>

using namespace RemoteControl;

class RemoteControlPlugin : public PluginComponent, public Base {
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
    uint8_t detectKeyPress();
    uint8_t getKeysPressed() const;
    uint32_t getKeysPressedTime() const;

    static RemoteControlPlugin &getInstance();

private:
    virtual void _onShortPress(Button &button);
    virtual void _onLongPress(Button &button);
    virtual void _onRepeat(Button &button);

    void _updateButtonConfig();
    void _loop();
    bool _isUsbPowered() const;
    void _readConfig();
    void _installWebhooks();
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
    uint8_t _pressedKeys;
    uint32_t _pressedKeysTime;
};

inline bool RemoteControlPlugin::_hasEvents() const
{
    return _queue.size() != 0;
}

inline void RemoteControlPlugin::disableAutoSleep()
{
    __LDBG_printf("disabled");
    getInstance()._autoSleepTimeout = kAutoSleepDisabled;
}

inline uint8_t RemoteControlPlugin::getKeysPressed() const
{
    return _pressedKeys;
}

inline uint32_t RemoteControlPlugin::getKeysPressedTime() const
{
    return _pressedKeysTime;
}

#include <debug_helper_disable.h>
