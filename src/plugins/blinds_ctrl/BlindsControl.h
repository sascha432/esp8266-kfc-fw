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
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

using Plugins = KFCConfigurationClasses::PluginsType;

namespace WebServer {
    class Plugin;
}

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

    static constexpr uint8_t kInvalidPin = 255;

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
    static_assert(kChannelCount == 2, "only 2 channels are supported");

    static const uint32_t kPwmMaxFrequency = 40000;
    static constexpr float kAdcIntegralMultiplierDivider = 1000.0 * 10.0;

public:

    class ChannelState {
    public:
        ChannelState();

        ChannelState &operator=(StateType state);

        // void setToggleOpenClosed(StateType state);

        StateType getState() const;
        // return binary state as char
        char getCharState() const;
        bool isOpen() const;
        uint8_t isOpenInt() const;
        bool isClosed() const;

        explicit operator unsigned() const;

        NameType _getFPStr() const;
        static NameType __getFPStr(StateType state);

    private:
        StateType _state;
    };

    template<size_t N>
    class ChannelStateArray : public std::array<ChannelState, N> {
    public:
        using Type = std::array<ChannelState, N>;

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

        constexpr size_t getByteSize() const {
            return N * sizeof(ChannelState);
        }

        bool read(File &file) {
            return static_cast<size_t>(file.read(reinterpret_cast<uint8_t *>(Type::data()), getByteSize())) == getByteSize();
        }

        bool write(File &file) const {
            return file.write(reinterpret_cast<const uint8_t *>(Type::data()), getByteSize()) == getByteSize();
        }

        constexpr auto channels() {
            return std::array<ChannelType, N>({ChannelType::CHANNEL0, ChannelType::CHANNEL1});
        }
    };

    using ChannelStateArrayType = ChannelStateArray<kChannelCount>;

protected:

    class ChannelAction {
    public:
        ChannelAction(uint32_t startTime = 0, uint16_t delay = 0, ActionType state = ActionType::NONE, ChannelType channel = ChannelType::NONE, PlayToneType playTone = PlayToneType::NONE, bool relativeDelay = false);

        ChannelType getChannel() const;
        ActionType getAction() const;
        ActionStateType getState() const;
        PlayToneType getPlayTone() const;

        // return delay in milliseconds
        uint32_t getDelay() const;

        // return timeout for millis()
        uint32_t getTimeout() const;

        // returns true if the delay is relative to the start of the action
        bool isRelativeDelay() const;

        bool getOpen() const;

        void monitorDelay();
        void beginDoNothing(ChannelType channel);
        void begin(ChannelType channel, bool open);
        void next();
        void end();

    private:
        uint32_t _startTime;
        uint16_t _delay;
        ActionStateType _state;
        ActionType _action;
        ChannelType _channel;
        PlayToneType _playTone;
        bool _relativeDelay;
        bool _open;
    };

    static constexpr auto kChannelActionSize = sizeof(ChannelAction);

private:
    static_assert(kChannelCount <= 2, "Only 2 channels supported");

    enum class AutoDiscoveryType : uint8_t {
        CHANNEL_0,
        CHANNEL_1,
        CHANNELS_ALL /* = kChannelCount * 2 */,
        BINARY_STATUS,
        BUSY_STATUS,
        MAX
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
    uint16_t getCurrent() const;

    static void startToneTimer(uint32_t timeout = 0);
    #if HAVE_IMPERIAL_MARCH
        static void playImperialMarch(uint16_t speed, int8_t zweiklang, uint8_t repeat);
    #endif
    static void stopToneTimer(ActionStateType state = ActionStateType::NONE);

protected:
    void _loopMethod();
    void _startToneTimer(uint32_t timeout = 0);
    void _playTone(uint8_t *pins, uint16_t pwm, uint32_t frequency);
    void _setDac(uint16_t pwm);
    #if HAVE_IMPERIAL_MARCH
        void _playImperialMarch(uint16_t speed, int8_t zweiklang, uint8_t repeat);
        void _playNote(uint8_t pin, uint16_t pwm, uint8_t note);
    #endif
    void _stopToneTimer();

protected:
    NameType _getStateStr(ChannelType channel) const;

    void _readConfig();
    void _setup();

    void _publishState();
    void _executeAction(ChannelType channel, bool open);

    void _startTone();

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

public:
    static void _saveState(StateType *channels, uint8_t numChannels);

protected:
    class ChannelQueue : protected std::vector<ChannelAction> {
    public:
        using Type = std::vector<ChannelAction>;
        using Type::size;
        using Type::empty;
        using Type::clear;
        using Type::begin;
        using Type::end;

        ChannelQueue(BlindsControl &control);

        void push_back(ChannelAction &&action);

        ChannelAction &getAction();
        const ChannelAction &getAction() const;
        void removeAction(const ChannelAction &action);

        void resetStartTime(uint32_t startTime = millis());
        uint32_t getStartTime() const;

    private:
        BlindsControl &_control;
        uint32_t _startTime;
    };

    class ActionToChannel {
    public:
        ActionToChannel();
        ActionToChannel(ActionType action, ChannelType channel);

        bool isOpenValid() const;
        bool isCloseValid() const;
        bool isDoNothing() const;

        ChannelType _for;
        ChannelType _open;
        ChannelType _close;
    };

    struct ToneSettings {
        // max. time before the tone gets stopped in milliseconds
        static constexpr uint32_t kMaxLength = 120 * 1000;
        // default interval in milliseconds
        static constexpr uint16_t kToneInterval = 2000;
        // default length of the tone in milliseconds
        static constexpr uint16_t kToneDuration = 150;
        // decrease interval until this value has been reached
        static constexpr uint16_t kToneMinInterval = 100 + kToneDuration;
        // interval of the timer in milliseconds
        static constexpr uint16_t kTimerInterval = 30;

        static_assert((kToneDuration % kTimerInterval) == 0, "kToneDuration must be a multiple of kTimerInterval");
        static_assert(kToneDuration >= kTimerInterval, "kToneDuration must be greater or equal kTimerInterval");
        static constexpr auto kToneMaxLoop = stdex::max<uint8_t>({1, kToneDuration / kTimerInterval});

        uint16_t counter;
        uint16_t loop;
        uint32_t runtime;
        uint16_t frequency;
        uint16_t pwmValue;
        uint16_t interval;
        std::array<uint8_t, 2> pin;

        ToneSettings(uint16_t _frequency, uint16_t _pwmValue, uint8_t _pins[2], uint32_t _timeout = 0);
        bool hasPin(uint8_t num) const;
        bool hasPin1() const;
        bool hasPin2() const;
    };

    static constexpr auto kToneSettingsSize = sizeof(ToneSettings);

    bool _cleanQueue();

    static String _getTopic(ChannelType channel, TopicType topic);

    Plugins::Blinds::ConfigStructType _config;

    ChannelStateArrayType _states;
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

private:
    ADCManager &_adc;
};

inline std::underlying_type<BlindsControl::ChannelType>::type operator *(const BlindsControl::ChannelType type)
{
    return static_cast<std::underlying_type<BlindsControl::ChannelType>::type>(type);
}

inline bool operator==(const BlindsControl::ChannelState &__lhs, const BlindsControl::ChannelState &__rhs)
{
    return __lhs.getState() == __rhs.getState();
}

inline bool operator!=(const BlindsControl::ChannelState &__lhs, const BlindsControl::ChannelState &__rhs)
{
    return __lhs.getState() != __rhs.getState();
}

#include "blinds_control.hpp"
#include "blinds_audio.hpp"
#include "blinds_channelstate.hpp"
#include "blinds_channelaction.hpp"
#include "blinds_channeltoaction.hpp"
#include "blinds_channelstate.hpp"
#include "blinds_channelqueue.hpp"
#include "blinds_motor.hpp"

#if DEBUG_IOT_BLINDS_CTRL
#    include <debug_helper_disable.h>
#endif
