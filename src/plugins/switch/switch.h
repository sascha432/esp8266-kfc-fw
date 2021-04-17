/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#ifndef DEBUG_IOT_SWITCH
#define DEBUG_IOT_SWITCH                            0
#endif

#include <Arduino_compat.h>
#include <EventScheduler.h>
#include <WebUIComponent.h>
#include <BitsToStr.h>
#include "../src/plugins/mqtt/mqtt_json.h"
#include "../src/plugins/plugins.h"
#include "kfc_fw_config.h"

#ifndef IOT_SWITCH_ON_STATE
#define IOT_SWITCH_ON_STATE                         HIGH
#endif

// interval to publish the state of all channels in milliseconds, 0 to disable
#ifndef IOT_SWITCH_PUBLISH_MQTT_INTERVAL
#define IOT_SWITCH_PUBLISH_MQTT_INTERVAL            (120 * 1000)
#endif

// store states on file system
#ifndef IOT_SWITCH_STORE_STATES
#define IOT_SWITCH_STORE_STATES                     1
#endif

// write delay on the file system in milliseconds
#ifndef IOT_SWITCH_STORE_STATES_WRITE_DELAY
#define IOT_SWITCH_STORE_STATES_WRITE_DELAY         (10 * 1000)
#endif

#ifndef IOT_SWITCH_CHANNEL_NUM
#error IOT_SWITCH_CHANNEL_NUM not defined
#endif

#ifndef IOT_SWITCH_CHANNEL_PINS
#error IOT_SWITCH_CHANNEL_PINS not defined
#endif

class SwitchPlugin : public PluginComponent, public MQTTComponent {
public:
    class States {
    public:
        States() : _states(0) {}

        bool operator[](int index) const {
            return _states & _BV(index);
        }

        void setState(int index, bool state) {
            if (state) {
                _states |= _BV(index);
            }
            else {
                _states &= ~_BV(index);
            }
        }

        const String toString() const {
            return BitsToStr<IOT_SWITCH_CHANNEL_NUM, true>(_states).toString();
        }

        bool write(File &file) const {
            return file.write(getData(), length()) == length();
        }

        bool read(File &file) {
            return file.read(getData(), length()) == length();
        }

    private:
        const uint8_t *getData() const {
            return reinterpret_cast<const uint8_t *>(&_states);
        }

        uint8_t *getData() {
            return reinterpret_cast<uint8_t *>(&_states);
        }

        size_t length() const {
            return sizeof(_states);
        }

    private:
        uint32_t _states;
    };

    class ChannelName {
    public:
        ChannelName() {}
        ChannelName(const String &name) : _name(name) {}

        // toString() returns the name or "Channel #" if no name is set
        const String toString(uint8_t channel) const {
            return _name.length() ? _name : PrintString(F("Channel %u"), channel);
        }

        // handling custom names
        ChannelName &operator=(const String &name) {
            _name = name;
            return *this;
        }

        operator const String &() const {
            return _name;
        }

        operator String &() {
            return _name;
        }

        size_t length() const {
            return _name.length();
        }

    private:
        String _name;
    };

public:
    SwitchPlugin();

// PluginComponent
public:
    virtual void setup(SetupModeType mode, const PluginComponents::DependenciesPtr &dependencies) override;
    virtual void reconfigure(const String &source) override;
    virtual void shutdown() override;
    virtual void getStatus(Print &output) override;

    virtual void createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request) override;

// WebUI
public:
    virtual void createWebUI(WebUINS::Root &webUI) override;
    virtual void getValues(WebUINS::Events &array) override;
    virtual void setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState) override;

// MQTTComponent
public:
    virtual AutoDiscovery::EntityPtr getAutoDiscovery(FormatType format, uint8_t num) override;
    virtual uint8_t getAutoDiscoveryCount() const override;
    virtual void onConnect() override;
    virtual void onMessage(const char *topic, const char *payload, size_t len) override;

private:
    void _publishState(int8_t channel = -1);

#if IOT_SWITCH_PUBLISH_MQTT_INTERVAL
    Event::Timer _updateTimer;
#endif
#if IOT_SWITCH_STORE_STATES
    Event::Timer _delayedWrite;
#endif

private:
    void _setChannel(uint8_t channel, bool state);
    bool _getChannel(uint8_t channel) const;
    void _readConfig();
    void _readStates();
    void _writeStatesDelayed();
    void _writeStatesNow();

private:
    using SwitchConfig = KFCConfigurationClasses::Plugins::IOTSwitch::SwitchConfig;
    using SwitchStateEnum = KFCConfigurationClasses::Plugins::IOTSwitch::StateEnum;
    using WebUIEnum = KFCConfigurationClasses::Plugins::IOTSwitch::WebUIEnum;

    std::array<ChannelName, IOT_SWITCH_CHANNEL_NUM> _names;
    std::array<SwitchConfig, IOT_SWITCH_CHANNEL_NUM> _configs;
    States _states;
    const std::array<uint8_t, IOT_SWITCH_CHANNEL_NUM> _pins;
};
