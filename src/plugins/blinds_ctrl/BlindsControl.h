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
#include "../src/plugins/mqtt/component.h"
#include "../src/plugins/mqtt/mqtt_json.h"
#include "PluginComponent.h"
#include <kfc_fw_config.h>
#include "blinds_defines.h"
#include <stl_ext/algorithm.h>

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
    using PlayToneType = Plugins::Blinds::PlayToneType;
    using Actions = Plugins::Blinds::BlindsConfigOperation_t;

    static constexpr size_t kChannel0Open = Plugins::Blinds::kChannel0_OpenPinArrayIndex;
    static constexpr size_t kChannel0Close = Plugins::Blinds::kChannel0_ClosePinArrayIndex;
    static constexpr size_t kChannel1Open = Plugins::Blinds::kChannel1_OpenPinArrayIndex;
    static constexpr size_t kChannel1Close = Plugins::Blinds::kChannel1_ClosePinArrayIndex;
    static constexpr size_t kMultiplexerPin = Plugins::Blinds::kMultiplexerPinArrayIndex;
    static constexpr size_t kDACPin = Plugins::Blinds::kDACPinArrayIndex;

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
        START_DELAY,
        REMOVE,
        MAX
    };

    static constexpr size_t kChannelCount = IOT_BLINDS_CTRL_CHANNEL_COUNT;
    static const uint32_t kPwmMaxFrequency = 40000;

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
        ChannelAction() :
            _delay(0),
            _state(ActionStateType::NONE),
            _action(ActionType::NONE),
            _channel(ChannelType::NONE),
            _relativeDelay(false),
            _open(false)
        {
        }
        ChannelAction(ActionType state, ChannelType channel, uint16_t delay, bool relativeDelay, PlayToneType playTone) :
            _delay(delay ? ((relativeDelay ? 0 : millis()) + (delay * 1000U)) : 0),
            _state(ActionStateType::NONE),
            _action(state),
            _channel(channel),
            _relativeDelay(relativeDelay),
            _open(false),
            _playTone(playTone)
        {
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

        void beginDoNothing(ChannelType channel) {
            __LDBG_printf("begin=%u channel=%u do_nothing", _state, channel);
            _state = ActionStateType::START_DELAY;
        }

        void begin(ChannelType channel, bool open) {
            __LDBG_printf("begin=%u channel=%u open=%u", _state, channel, open);
            // update channel to change and the state
            _open = open;
            _channel = channel;
            next();
        }

        void next() {
            BlindsControl::stopToneTimer();
            __LDBG_printf("next=%u", _state);
            switch (_state) {
                case ActionStateType::NONE:
                    _state = ActionStateType::START_MOTOR;
                    break;
                case ActionStateType::START_MOTOR:
                    _state = ActionStateType::WAIT_FOR_MOTOR;
                    break;
                case ActionStateType::START_DELAY:
                case ActionStateType::WAIT_FOR_MOTOR:
                    if (_delay) {
                        _state = ActionStateType::DELAY;
                        if (_relativeDelay) {
                            _delay += millis();
                        }
                        __LDBG_printf("delay=%u (in %ums) relative=%u play_tone=%u", _delay, (uint32_t)(millis() - _delay), _relativeDelay, _playTone);
                        switch(_playTone) {
                            case PlayToneType::INTERVAL:
                                BlindsControl::startToneTimer();
                                break;
#if HAVE_IMPERIAL_MARCH
                            case PlayToneType::IMPERIAL_MARCH:
                                BlindsControl::playImerialMarch(80, 0, 1);
                                break;
#endif
                            default:
                                break;

                        }

                        break;
                    }
                    // fallthrough
                default:
                    end();
                    break;
            }
        }

        void end() {
            BlindsControl::stopToneTimer();
            __LDBG_printf("end=%u", _state);
            _delay = 0;
            _state = ActionStateType::REMOVE;
        }

    private:
        uint32_t _delay;
        ActionStateType _state;
        ActionType _action;
        ChannelType _channel;
        bool _relativeDelay: 1;
        bool _open: 1;
        PlayToneType _playTone;
    };

public:
    BlindsControl();

    virtual AutoDiscovery::EntityPtr getAutoDiscovery(FormatType format, uint8_t num) override;
    virtual uint8_t getAutoDiscoveryCount() const override;
    virtual void onConnect() override;
    virtual void onMessage(const char *topic, const char *payload, size_t len) override;

    void getValues(WebUINS::Events &array);
    void setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState);

    // void setChannel(ChannelType channel, StateType state);

    // returns ADC value * 64.0 of the current measurement
    uint16_t getCurrent() {
        return std::clamp<uint32_t>(_adcIntegral * 64.0, 0, 0xffff);
    }

    static void startToneTimer(uint32_t maxLength = 0);
#if HAVE_IMPERIAL_MARCH
    static void playImerialMarch(uint16_t speed, int8_t zweiklang, uint8_t repeat);
#endif
    static void stopToneTimer();

protected:
    void _loopMethod();
    void _startToneTimer(uint32_t maxLength = 0);
    void _playTone(uint8_t pin, uint16_t pwm, uint32_t frequency);
    void _setDac(uint16_t pwm);
#if HAVE_IMPERIAL_MARCH
    void _playImerialMarch(uint16_t speed, int8_t zweiklang, uint8_t repeat);
    void _playNote(uint8_t pin, uint16_t pwm, uint8_t note);
#endif
    void _stopToneTimer();

protected:
    NameType _getStateStr(ChannelType channel) const;

    void _readConfig();
    void _setup();

    void _publishState();
    void _executeAction(ChannelType channel, bool open);
    void _startMotor(ChannelType channel, bool open);
    void _monitorMotor(ChannelAction &action);
    bool _checkMotor();

    void _clearAdc();
    void _updateAdc();
    void _setMotorSpeed(ChannelType channel, uint16_t speed, bool open);
    // void _setMotorSpeedUpdate(ChannelType channel, uint16_t speed, bool open);
    void _setMotorBrake(ChannelType channel);
    void _stop();
    void _disableMotors();

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
        bool isDoNothing() const {
            return _close == ChannelType::NONE && _open == ChannelType::NONE;
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

    uint32_t _motorStartTime;
    uint8_t _motorPin;
    uint16_t _motorPWMValue;
#if DEBUG_IOT_BLINDS_CTRL
    uint32_t _softStartUpdateCount;
#endif
    MillisTimer _motorTimeout;
    float _currentLimit;

    uint32_t _adcLastUpdate;
    float _adcIntegral;
    float _adcIntegralMultiplier;
    float _adcIntegralPeak;
    MicrosTimer _currentTimer;
    Event::Timer _toneTimer;

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
