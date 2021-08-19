/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <stl_ext/memory>
#include <EventScheduler.h>
#include <WebUIComponent.h>
#include <BitsToStr.h>
#include "switch_def.h"
#include "../src/plugins/mqtt/mqtt_json.h"
#include "../src/plugins/plugins.h"
#include "kfc_fw_config.h"

class SwitchPlugin : public PluginComponent, public MQTTComponent {
public:
    using Plugins = KFCConfigurationClasses::PluginsType;
    using SwitchConfig = KFCConfigurationClasses::Plugins::SwitchConfigNS::SwitchConfigType;
    using SwitchStateEnum = KFCConfigurationClasses::Plugins::SwitchConfigNS::StateEnum;
    using SwitchWebUIEnum = KFCConfigurationClasses::Plugins::SwitchConfigNS::WebUIEnum;
    using PinArrayType = std::array<uint8_t, IOT_SWITCH_CHANNEL_NUM>;

public:
    class States {
    public:
        States() : _states(0), _config(0) {}

        bool operator[](int index) const {
            return _states & _stateMask(index);
        }

        void setState(int index, bool state) {
            if (state) {
                _states |= _stateMask(index);
            }
            else {
                _states &= ~_stateMask(index);
            }
        }

        SwitchStateEnum getConfig(int index) const {
            return static_cast<SwitchStateEnum>((_config & _configMask(index)) >> _configShift(index));
        }

        void setConfig(int index, SwitchStateEnum config) {
            _config = (_config & ~_configMask(index)) | (static_cast<uint8_t>(config) << _configShift(index));
        }

        const String toString() const {
            auto str = BitsToStr<IOT_SWITCH_CHANNEL_NUM, true>(_states).toString();
            str += ' ';
            for(uint8_t i = 0; i < IOT_SWITCH_CHANNEL_NUM; i++) {
                switch(getConfig(i)) {
                    case SwitchStateEnum::ON:
                        str += '+';
                        break;
                    case SwitchStateEnum::RESTORE:
                        str += '*';
                        break;
                    case SwitchStateEnum::OFF:
                        str += '-';
                        break;
                    default:
                        str += '?';
                        break;
                }
            }
            return str;
        }

        bool write(File &file) const {
            return file.write(*this, size()) == size();
        }

        bool read(File &file) {
            return static_cast<size_t>(file.read(*this, size())) == size();
        }

        bool compare(File &file) {
            States state;
            if (static_cast<size_t>(file.read(state, state.size())) != state.size()) {
                return false;
            }
            uint32_t crc;
            if (static_cast<size_t>(file.read(reinterpret_cast<uint8_t *>(&crc), sizeof(crc))) != sizeof(crc)) {
                return false;
            }
            return (state._states == _states) && (state._config == _config) && (crc == crc32(state, sizeof(state)));
        }

        operator const uint8_t *() const {
            return reinterpret_cast<const uint8_t *>(this);
        }

        operator uint8_t *() {
            return reinterpret_cast<uint8_t *>(this);
        }

        constexpr size_t size() const {
            return sizeof(*this);
        }

    private:
        inline uint32_t _stateMask(int index) const {
            return 1 << index;
        }

        inline uint32_t _configMask(int index) const {
            return 3 << (1 << index);
        }

        inline uint32_t _configShift(int index) const {
            return index << 1;

        }

    private:
        uint32_t _states: 12;   // 1 bit per channel
        uint32_t _config: 24;   // 2 bits per channel
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

    String _formatTopic(uint8_t channel, bool state) const;

#if IOT_SWITCH_STORE_STATES_RTC_MEM
public:
    static void _rtcMemStoreState();
    static void _rtcMemLoadState();
#endif

private:
    void _publishState(int8_t channel = -1);

    #if IOT_SWITCH_PUBLISH_MQTT_INTERVAL
        Event::Timer _updateTimer;
    #endif
    #if IOT_SWITCH_STORE_STATES_FS
        Event::Timer _delayedWrite;
    #endif

private:
    void _setChannel(uint8_t channel, bool state);
    bool _getChannel(uint8_t channel) const;
    void _readConfig();
    void _readStates();
    void _writeStatesDelayed();
    void _writeStatesNow();

    static constexpr uint8_t _channelPinValue(bool state) {
        return state ? IOT_SWITCH_ON_STATE : !IOT_SWITCH_ON_STATE;
    }

private:
    std::array<ChannelName, IOT_SWITCH_CHANNEL_NUM> _names;
    std::array<SwitchConfig, IOT_SWITCH_CHANNEL_NUM> _configs;
#if IOT_SWITCH_STORE_STATES_RTC_MEM
    static PinArrayType &_pins;
    static States &_states;
#else
    static PinArrayType _pins;
    static States _states;
#endif
    static bool _statesInitialized;
};
