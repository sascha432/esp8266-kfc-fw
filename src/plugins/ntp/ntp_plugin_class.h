/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <EventScheduler.h>
#include <kfc_fw_config.h>
#include "ntp_plugin.h"
#include "plugins.h"

class AtModeArgs;
class AsyncWebServerRequest;

using Plugins = KFCConfigurationClasses::PluginsType;

class NTPPlugin : public PluginComponent {
public:
    NTPPlugin();

    virtual void setup(SetupModeType mode, const PluginComponents::DependenciesPtr &dependencies) override;
    virtual void reconfigure(const String &source) override;
    virtual void shutdown() override;
    virtual void getStatus(Print &output) override;
    virtual void createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request) override;

#if AT_MODE_SUPPORTED
    virtual ATModeCommandHelpArrayPtr atModeCommandHelp(size_t &size) const override;
    virtual bool atModeHandler(AtModeArgs &args) override;
#endif

public:
    static void updateNtpCallback(settimeofday_cb_args_t);
    static void checkTimerCallback(Event::CallbackTimerPtr timer);
    // static void wifiCallback(WiFiCallbacks::EventType type, void *);

private:
    void _updateNtpCallback();
    // void _wifiCallback(WiFiCallbacks::EventType type, void *);
    void _checkTimerCallback(Event::CallbackTimerPtr timer);
    void _execConfigTime();

// private:
public:
    static constexpr uint32_t kCheckInterval = 60000;

    enum class CallbackState : uint8_t {
        NONE = 0,
        STARTUP,
        SHUTDOWN,
        // WIFI_LOST,
        // WAITING_FOR_1ST_CALLBACK,
        WAITING_FOR_CALLBACK,
        WAITING_FOR_REFRESH,
    };

    const __FlashStringHelper *getCallbackState() {
        switch(_callbackState) {
        case CallbackState::NONE:
            return F("NONE");
        case CallbackState::STARTUP:
            return F("Waiting for first response");
        case CallbackState::SHUTDOWN:
            return F("SNTP client not running");
        // case CallbackState::WIFI_LOST:
        //     return F("WiFi connection lost");
        // case CallbackState::WAITING_FOR_1ST_CALLBACK:
        case CallbackState::WAITING_FOR_CALLBACK:
            return F("Waiting for response");
        case CallbackState::WAITING_FOR_REFRESH:
            return F("Waiting for periodic refresh");
        default:
            break;
        }
        return F("<invalid value>");
    }

    Event::Timer _checkTimer;
    CallbackState _callbackState;
    std::array<char *, Plugins::NTPClient::kServersMax> _servers;
    uint32_t _lastUpdateSeconds;
};

