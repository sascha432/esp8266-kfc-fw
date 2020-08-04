/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <WebUIComponent.h>
#include <MicrosTimer.h>
#include <FunctionalInterrupt.h>
#include "../src/plugins/mqtt/mqtt_component.h"
#include <kfc_fw_config.h>
#include "blinds_defines.h"

using KFCConfigurationClasses::Plugins;

class BlindsControl : public MQTTComponent {
public:
    using NameType = const __FlashStringHelper *;

    enum class TopicType : uint8_t {
        SET,
        STATE,
        METRICS
    };
    enum class ChannelType : int8_t {
        NONE = -1,
        CHANNEL0,
        CHANNEL1,
        MAX,
    };
    enum class StateType : uint8_t {
        UNKNOWN,
        OPEN,
        CLOSED,
        STOPPED,
        MAX
    };

    static constexpr size_t kChannelCount = IOT_BLINDS_CTRL_CHANNEL_COUNT;

protected:
    class ChannelState {
    public:
        ChannelState() : _state(StateType::UNKNOWN) {
        }

        ChannelState &operator=(StateType state) {
            _state = state;
            return *this;
        }

        void setToggleOpenClosed(StateType state) {
            _state = (state == StateType::OPEN ? StateType::CLOSED : StateType::OPEN);
        }

        StateType getState() const {
            return _state;
        }
        // return binary state as char
        char getCharState() const {
            return isOpen() ? '1' : '0';
        }

        bool isOpen() const {
            return _state == StateType::OPEN;
        }

        bool isClosed() const {
            return _state == StateType::CLOSED;
        }

        NameType _getFPStr() const {
            switch(_state) {
                case StateType::OPEN:
                    return FSPGM(Open);
                case StateType::CLOSED:
                    return FSPGM(Closed);
                case StateType::STOPPED:
                    return FSPGM(Stopped);
                default:
                    break;
            }
            return FSPGM(n_a);
        }

    private:
        StateType _state;
    };

    template<size_t N>
    class ChannelStateArray : public std::array<ChannelState, N> {
    public:
        using Type = std::array<ChannelState, N>;
        using Type::Type;

#if DEBUG
        ChannelState &at(size_t channel) {
            if (channel < Type::size()) {
                return Type::at(channel);
            }
            __DBG_panic("invalid channel %d", channel);
            return Type::at(0);
        }
        ChannelState at(size_t channel) const {
            if (channel < Type::size()) {
                return Type::at(channel);
            }
            __DBG_panic("invalid channel %d", channel);
            return Type::at(0);
        }
#endif

        ChannelState &operator[](size_t channel) {
            return at(channel);
        }
        ChannelState operator[](size_t channel) const {
            return at(channel);
        }
        ChannelState &operator[](ChannelType channel) {
            return at(static_cast<size_t>(channel));
        }
        ChannelState operator[](ChannelType channel) const {
            return Type::at(static_cast<size_t>(channel));
        }

        constexpr std::array<ChannelType, N> channels() {
            return std::array<ChannelType, N>({ChannelType::CHANNEL0, ChannelType::CHANNEL1});
        }
    };

    class ChannelAction {
    public:
        ChannelAction() : _state(StateType::UNKNOWN), _channel(ChannelType::NONE) {
        }

        void set(StateType state, ChannelType channel) {
            _state = state;
            _channel = channel;
        }

        bool isSet() const {
            return _channel != ChannelType::NONE;
        }

        ChannelType getChannel() const {
            return _channel;
        }

        StateType getState() const {
            return _state;
        }

        void clear() {
            _channel = ChannelType::NONE;
        }

    private:
        StateType _state;
        ChannelType _channel;
    };

public:
    BlindsControl();

    virtual MQTTAutoDiscoveryPtr nextAutoDiscovery(MQTTAutoDiscovery::FormatType format, uint8_t num) override;
    virtual uint8_t getAutoDiscoveryCount() const override {
        return kChannelCount + 1;
    }
    virtual void onConnect(MQTTClient *client) override;

    void getValues(JsonArray &array);
    void setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState);

    void setChannel(ChannelType channel, StateType state);

protected:
    void _loopMethod();

protected:
    bool isBusy(ChannelType channel) const;
    bool isBusyOrError(ChannelType channel) const;
    NameType _getStateStr(ChannelType channel) const;

    void _setup();

    void _clearAdc();
    void _updateAdc();

    void _setMotorSpeed(ChannelType channel, uint16_t speed, bool direction);
    void _stop();

    void _publishState(MQTTClient *client = nullptr);
    void _loadState();
    void _saveState();
    void _readConfig();

protected:
    static String _getTopic(ChannelType channel, TopicType topic);

    Plugins::Blinds::ConfigStructType _config;

    ChannelStateArray<kChannelCount> _states;
    ChannelType _activeChannel;
    ChannelAction _action;

    MillisTimer _motorTimeout;

    uint32_t _adcIntegral;
    uint16_t _currentLimitCounter;
    MicrosTimer _currentTimer;

#if IOT_BLINDS_CTRL_RPM_PIN
protected:
    void _rpmReset();
    void _rpmIntCallback(const InterruptInfo &info);
    uint16_t _getRpm();
    bool _hasStalled() const;

protected:
    MicrosTimer _rpmTimer;
    MicrosTimer _rpmLastInterrupt;
    uint32_t _rpmTimeIntegral;
    uint32_t _rpmCounter;
#endif
};

