/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <WebUIComponent.h>
#include <MicrosTimer.h>
#include <MillisTimer.h>
#include <ReadADC.h>
#include <Buffer.h>
#include <FunctionalInterrupt.h>
#include "../src/plugins/mqtt/mqtt_component.h"
#include <kfc_fw_config.h>
#include "blinds_defines.h"

#if DEBUG_IOT_BLINDS_CTRL
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using KFCConfigurationClasses::Plugins;

class BlindsControl : public MQTTComponent {
public:
    using NameType = const __FlashStringHelper *;
    using ActionType = Plugins::Blinds::OperationType;
    using Actions = Plugins::Blinds::BlindsConfigOperation_t;

    enum class TopicType : uint8_t {
        SET,
        STATE,
        METRICS,
        CHANNELS,
    };
    enum class ChannelType : int8_t {
        NONE = -1,
        CHANNEL0,
        CHANNEL1,
        ALL,
        MAX,
    };
    enum class StateType : uint8_t {
        UNKNOWN,
        OPEN,
        CLOSED,
        STOPPED,
        DELAY,
        MAX
    };

    enum class ActionStateType : uint8_t {
        NONE = 0,
        DELAY,
        START_MOTOR,
        WAIT_FOR_MOTOR,
        REMOVE,
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

        // void setToggleOpenClosed(StateType state) {
        //     _state = (state == StateType::OPEN ? StateType::CLOSED : StateType::OPEN);
        // }

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
            return __getFPStr(_state);
        }

        static NameType __getFPStr(StateType state) {
            switch(state) {
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
        ChannelAction() : _state(ActionStateType::NONE), _action(ActionType::NONE), _channel(ChannelType::NONE), _delay(0), _open(false) {
        }
        ChannelAction(ActionType state, ChannelType channel, uint16_t delay) : _state(ActionStateType::NONE), _action(state), _channel(channel), _delay(delay * 1000U), _open(false) {
        }

        ChannelType getChannel() const {
            return _channel;
        }

        ActionType getAction() const {
            return _action;
        }

        ActionStateType getState() const {
            return _state;
        }

        bool getOpen() const {
            return _open;
        }

        void monitorDelay() {
            if (_state == ActionStateType::DELAY && millis() >= _delay) {
                __LDBG_printf("delay=%u finished", _delay);
                next();
            }
        }

        void begin(ChannelType channel, bool open) {
            __LDBG_printf("begin=%u channel=%u open=%u", _state, channel, open);
            // update channel to change and the state
            _open = open;
            _channel = channel;
            next();
        }

        void next() {
            __LDBG_printf("next=%u", _state);
            switch (_state) {
                case ActionStateType::NONE:
                    _state = ActionStateType::START_MOTOR;
                    break;
                case ActionStateType::START_MOTOR:
                    _state = ActionStateType::WAIT_FOR_MOTOR;
                    break;
                case ActionStateType::WAIT_FOR_MOTOR:
                    if (_delay) {
                        _state = ActionStateType::DELAY;
                        __LDBG_printf("delay=%u start=%lu", _delay, millis());
                        _delay += millis();
                        break;
                    }
                    // fallthrough
                default:
                    end();
                    break;
            }
        }

        void end() {
            __LDBG_printf("end=%u", _state);
            _delay = 0;
            _state = ActionStateType::REMOVE;
        }

    private:
        ActionStateType _state;
        ActionType _action;
        ChannelType _channel;
        uint32_t _delay;
        bool _open;
    };

public:
    BlindsControl();

    virtual MQTTAutoDiscoveryPtr nextAutoDiscovery(MQTTAutoDiscovery::FormatType format, uint8_t num) override;
    virtual uint8_t getAutoDiscoveryCount() const override;
    virtual void onConnect(MQTTClient *client) override;
    virtual void onMessage(MQTTClient *client, char *topic, char *payload, size_t len) override;

    void getValues(JsonArray &array);
    void setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState);

    // void setChannel(ChannelType channel, StateType state);

protected:
    void _loopMethod();

protected:
    NameType _getStateStr(ChannelType channel) const;

    void _readConfig();
    void _setup();

    void _publishState(MQTTClient *client = nullptr);
    void _executeAction(ChannelType channel, bool open);
    void _startMotor(ChannelType channel, bool open);
    void _monitorMotor(ChannelAction &action);

    void _clearAdc();
    void _updateAdc();
    void _setMotorSpeed(ChannelType channel, uint16_t speed, bool open);
    void _setMotorBrake(ChannelType channel);
    void _stop();

    void _loadState();
    void _saveState();

protected:
    class ChannelQueue : public std::vector<ChannelAction>
    {
    public:
        using Type = std::vector<ChannelAction>;

        ChannelQueue(BlindsControl &control) : Type(), _control(control) {}

        ChannelAction &getAction() {
            return front();
        }

        const ChannelAction &getAction() const {
            return front();
        }

        void removeAction(const ChannelAction &action) {
            __LDBG_printf("remove action=%p", &action);
            erase(std::remove_if(begin(), end(), [&action](const ChannelAction &_action) {
                return &action == &_action;
            }), end());
        }

    private:
        BlindsControl &_control;
    };

    class ActionToChannel {
    public:
        ActionToChannel() : _for(ChannelType::NONE), _open(ChannelType::NONE), _close(ChannelType::NONE) {}
        ActionToChannel(ActionType action, ChannelType channel);

        bool isOpenValid() const {
            return _open != ChannelType::NONE;
        }
        bool isCloseValid() const {
            return _close != ChannelType::NONE;
        }

        ChannelType _for;
        ChannelType _open;
        ChannelType _close;
    };

    bool _cleanQueue();

    static String _getTopic(ChannelType channel, TopicType topic);

    Plugins::Blinds::ConfigStructType _config;

    ChannelStateArray<kChannelCount> _states;
    ChannelQueue _queue;
    ChannelType _activeChannel;

    MillisTimer _motorTimeout;
    MillisTimer _currentLimitTimer;
    uint16_t _currentLimit;

    uint32_t _adcLastUpdate;
    float _adcIntegral;
    float _adcIntegralMultiplier;
    MicrosTimer _currentTimer;

#if IOT_BLINDS_CTRL_TESTMODE
    class CurrentValueType {
    public:
        CurrentValueType(uint16_t time, uint16_t value, int32_t timer) : _time(time), _value(value), _timer(timer) {
        }
        uint32_t getTime() const {
            return _time;
        }
        uint32_t getCurrent() const {
            return _value * BlindsControllerConversion::kConvertADCValueToCurrentMulitplier;
        }
        uint32_t getValue() const {
            return _value;
        }
        int32_t getTimer() const {
            return _timer;
        }
        private:
        friend BlindsControl;
        struct __attribute__packed__ {
            uint16_t _time;
            uint16_t _value;
            int8_t _timer;
        };
    };
    using CurrentValueVector = std::vector<CurrentValueType>;
    static constexpr size_t kCurrentValueTypeSize = sizeof(CurrentValueType);

    uint32_t _startCurrentValues;
    CurrentValueVector _currentValues;
    bool _storeValues;
#endif

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

private:
    ADCManager &_adc;
};

#include <debug_helper_disable.h>
