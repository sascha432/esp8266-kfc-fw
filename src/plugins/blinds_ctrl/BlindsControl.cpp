/**
 * Author: sascha_lammers@gmx.de
 */

#include "WebUISocket.h"
#include "BlindsControl.h"
#include "blinds_plugin.h"
#include "kfc_fw_config.h"
#include "../src/plugins/mqtt/mqtt_client.h"

#if DEBUG_IOT_BLINDS_CTRL
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

extern int8_t operator *(const BlindsControl::ChannelType type);

static constexpr float kAdcIntegralMultiplierDivider = 1000.0 * 10.0;

int8_t operator *(const BlindsControl::ChannelType type) {
    return static_cast<int8_t>(type);
}

BlindsControl::BlindsControl() :
    MQTTComponent(ComponentType::SWITCH), _queue(*this),
    _activeChannel(ChannelType::NONE),
    _adcIntegralMultiplier(2500 / kAdcIntegralMultiplierDivider),
    _adc(ADCManager::getInstance())
{
}

void BlindsControl::_setup()
{
    __LDBG_println();
    _readConfig();
    for(auto pin : _config.pins) {
        digitalWrite(pin, LOW);
        pinMode(pin, OUTPUT);
    }

#if IOT_BLINDS_CTRL_RPM_PIN
    attachScheduledInterrupt(digitalPinToInterrupt(IOT_BLINDS_CTRL_RPM_PIN), BlindsControl::rpmIntCallback, RISING);
    pinMode(IOT_BLINDS_CTRL_RPM_PIN, INPUT);
#endif
}

MQTT::AutoDiscovery::EntityPtr BlindsControl::getAutoDiscovery(FormatType format, uint8_t num)
{
    auto discovery = __LDBG_new(MQTT::AutoDiscovery::Entity);
    if (num < kChannelCount) {
        ChannelType channel = (ChannelType)num;
        if (discovery->create(this, PrintString(FSPGM(channel__u, "channel_%u"), channel), format)) {
            discovery->addStateTopic(_getTopic(channel, TopicType::STATE));
            discovery->addCommandTopic(_getTopic(channel, TopicType::SET));
        }
    }
    else if (num == kChannelCount) {
        if (discovery->create(this, FSPGM(channels), format)) {
            discovery->addStateTopic(_getTopic(ChannelType::ALL, TopicType::STATE));
            discovery->addCommandTopic(_getTopic(ChannelType::ALL, TopicType::SET));
        }
    }
    else if (num == kChannelCount + 1) {
        if (discovery->create(MQTTComponent::ComponentType::SENSOR, FSPGM(binary), format)) {
            discovery->addStateTopic(_getTopic(ChannelType::NONE, TopicType::METRICS));
            discovery->addValueTemplate(FSPGM(binary));
        }
    }
    else if (num == kChannelCount + 2) {
        if (discovery->create(MQTTComponent::ComponentType::SENSOR, FSPGM(busy, "busy"), format)) {
            discovery->addStateTopic(_getTopic(ChannelType::NONE, TopicType::METRICS));
            discovery->addValueTemplate(FSPGM(busy));
        }
    }
    else if (num >= kChannelCount + 3) {
        ChannelType channel = (ChannelType)(num - (kChannelCount + 3));
        if (discovery->create(MQTTComponent::ComponentType::SENSOR, PrintString(FSPGM(channel__u), channel), format)) {
            discovery->addStateTopic(_getTopic(ChannelType::NONE, TopicType::CHANNELS));
            discovery->addValueTemplate(PrintString(FSPGM(channel__u), channel));
        }
    }
    return discovery;
}

uint8_t BlindsControl::getAutoDiscoveryCount() const
{
    return kChannelCount * 2 + 3;
}

void BlindsControl::getValues(JsonArray &array)
{
    JsonUnnamedObject *obj;

    obj = &array.addObject(3);
    obj->add(JJ(id), FSPGM(set_all, "set_all"));
    obj->add(JJ(value), _states[0].isOpen() || _states[1].isOpen() ? 1 : 0);
    obj->add(JJ(state), true);

    for(const auto channel: _states.channels()) {
        String prefix = PrintString(FSPGM(channel__u), channel);

        obj = &array.addObject(3);
        obj->add(JJ(id), prefix + F("_state"));
        obj->add(JJ(value), _getStateStr(channel));
        obj->add(JJ(state), true);

        obj = &array.addObject(3);
        obj->add(JJ(id), prefix + F("_set"));
        obj->add(JJ(value), _states[channel].isOpen() ? 1 : 0);
        obj->add(JJ(state), true);
    }
}

void BlindsControl::_publishState()
{
    if (!isConnected()) {
        return;
    }
    __LDBG_printf("state %s/%s", _getStateStr(ChannelType::CHANNEL0), _getStateStr(ChannelType::CHANNEL1));

    if (isConnected()) {
        JsonUnnamedObject metrics(2);
        JsonUnnamedObject channels(kChannelCount);
        String binaryState = String('b');

        publish(_getTopic(ChannelType::ALL, TopicType::STATE), true, MQTT::Client::toBoolOnOff(_states[0].isOpen() || _states[1].isOpen()));

        for(const auto channel: _states.channels()) {
            auto &state = _states[channel];
            auto isOpen = state.getCharState();
            binaryState += isOpen;
            publish(_getTopic(channel, TopicType::STATE), true, MQTT::Client::toBoolOnOff(isOpen));
            channels.add(PrintString(FSPGM(channel__u), channel), _getStateStr(channel));
        }
        metrics.add(FSPGM(binary), binaryState);
        metrics.add(FSPGM(busy), !_queue.empty());

        PrintString buffer;
        buffer.reserve(metrics.length());
        metrics.printTo(buffer);

        publish(_getTopic(ChannelType::NONE, TopicType::METRICS), true, buffer);

        buffer = PrintString();
        buffer.reserve(channels.length());
        channels.printTo(buffer);
        publish(_getTopic(ChannelType::NONE, TopicType::CHANNELS), true, buffer);
    }

    if (WebUISocket::hasAuthenticatedClients()) {
        JsonUnnamedObject webUI(2);
        webUI.add(JJ(type), JJ(update_events));
        getValues(webUI.addArray(JJ(events), kChannelCount * 2));
        WebUISocket::broadcast(WebUISocket::getSender(), webUI);
    }
}

void BlindsControl::onConnect()
{
    __LDBG_printf("client=%p", client);
    _publishState();
    subscribe(_getTopic(ChannelType::ALL, TopicType::SET));
    for(const auto channel: _states.channels()) {
        subscribe(_getTopic(channel, TopicType::SET));
    }
}

void BlindsControl::onMessage(const char *topic, const char *payload, size_t len)
{
    ChannelType channel;
    if (strstr_P(topic, PSTR("/channel_0/"))) {
        channel = ChannelType::CHANNEL0;
    }
    else if (strstr_P(topic, PSTR("/channel_1/"))) {
        channel = ChannelType::CHANNEL1;
    }
    else {
        channel = ChannelType::ALL;
    }
    auto state = atoi(payload); // payload is NUL terminated
    __LDBG_printf("topic=%s state=%u payload=%s", topic, state, payload);
    _executeAction(channel, state);
}


void BlindsControl::setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState)
{
    __LDBG_printf("id=%s hasValue=%u value=%s hasState=%u state=%u", id.c_str(), hasValue, value.c_str(), hasState, state);
    if (hasValue) {
        bool open = value.toInt();
        if (id.endsWith(F("_set"))) {
            size_t channel = atoi(id.c_str() + id.indexOf('_') + 1);
            if (channel < _states.size()) {
                _executeAction(static_cast<ChannelType>(channel), open);
            }
        }
        else if (id.endsWith(FSPGM(set_all))) {
            _executeAction(ChannelType::ALL, open);
        }
    }
}

void BlindsControl::startToneTimer(uint32_t maxLength)
{
    BlindsControlPlugin::getInstance()._startToneTimer(maxLength);
}

void BlindsControl::_playTone(uint8_t pin, uint16_t pwm, uint32_t frequency)
{
    analogWriteFreq(frequency);
    analogWrite(pin, pwm);
}

void BlindsControl::_setDac(uint16_t pwm)
{
    analogWriteRange(PWMRANGE);
    analogWriteFreq(kPwmMaxFrequency);
    analogWrite(_config.pins[5], pwm);
    __LDBG_printf("dac pin=%u pwm=%u frequency=%u", _config.pins[5], pwm, kPwmMaxFrequency);
}

#if HAVE_IMPERIAL_MARCH

void BlindsControl::playImerialMarch(uint16_t speed, int8_t zweiklang, uint8_t repeat)
{
    BlindsControlPlugin::getInstance().playImerialMarch(speed, zweiklang, repeat);
}

void BlindsControl::_playNote(uint8_t pin, uint16_t pwm, uint8_t note)
{
    uint8_t tmp = note + MPERIAL_MARCH_NOTE_OFFSET;
    if (tmp < NOTE_TO_FREQUENCY_COUNT) {
        _playTone(pin, pwm, NOTE_FP_TO_INT(pgm_read_word(note_to_frequency + tmp)));
    }
}

void BlindsControl::_playImerialMarch(uint16_t speed, int8_t zweiklang, uint8_t repeat)
{
    _stopToneTimer();
    if (_config.play_tone_channel == 0) {
        __LDBG_printf("tone timer disabled");
        return;
    }
    uint8_t channel = (_config.play_tone_channel - 1) % kChannelCount;
    uint8_t channel2 = _config.play_tone_channel % kChannelCount;
    uint8_t index = (channel * kChannelCount) + (_states[channel].isClosed() ? 1 : 0);
    uint8_t index2 = (channel2 * kChannelCount) + (_states[channel2].isClosed() ? 1 : 0);
    struct {
        uint16_t pwmValue;
        uint16_t counter;
        uint16_t nextNote;
        uint16_t speed;
        uint8_t notePosition;
        uint8_t pin;
        uint8_t pin2;
        uint8_t repeat;
        int8_t zweiklang;
        uint8_t offDelayCounter;
    } settings = {
        (uint16_t)_config.tone_pwm_value,
        0,  // counter
        0,  // nextNote
        speed,
        0,  // notePosition
        _config.pins[index], // use channel 1, and use open/close pin that is jammed
        _config.pins[index2], // use channel 1, and use open/close pin that is jammed
        repeat,
        zweiklang,
        0   // offDelayCounter
    };

    _setDac(PWMRANGE);

    __LDBG_printf("imperial march length=%u speed=%u zweiklang=%d repeat=%u pin=%u pin2=%u", sizeof(imperial_march_notes), settings.speed, settings.zweiklang, settings.repeat, settings.pin, settings.pin2);

    _Timer(_toneTimer).add(settings.speed, true, [this, settings](Event::CallbackTimerPtr timer) mutable {

        if (settings.offDelayCounter) {
            if (--settings.offDelayCounter == 0 || settings.pwmValue <= settings.repeat) {
                _stop();
                return;
            }
            uint8_t note = pgm_read_byte(imperial_march_notes + IMPERIAL_MARCH_NOTES_COUNT - 1);
            _playNote(settings.pin, settings.pwmValue, note);
            if (settings.zweiklang) {
                _playNote(settings.pin2, settings.pwmValue, note + settings.zweiklang);
            }
            settings.pwmValue -= settings.repeat;
            return;
        }

        if (settings.notePosition >= IMPERIAL_MARCH_NOTES_COUNT) {
            if (settings.repeat-- == 0) {
                settings.offDelayCounter = (4000 / settings.speed) + 1; // fade last node within 4 seconds
                settings.repeat = (settings.pwmValue / settings.offDelayCounter) + 1; // decrease pwm value steps
                return;
            }
            else {
                switch(settings.zweiklang) {
                    case 12:
                        settings.zweiklang = -12;
                        break;
                    case 7:
                        settings.zweiklang = 12;
                        break;
                    case 5:
                        settings.zweiklang = 7;
                        break;
                    case 0:
                        settings.zweiklang = 5;
                        break;
                    case -5:
                        settings.zweiklang = 0;
                        break;
                    case -7:
                        settings.zweiklang = -5;
                        break;
                    case -12:
                        settings.zweiklang = -7;
                        break;
                    default:
                        settings.zweiklang = 0;
                        break;
                }
                __LDBG_printf("repeat imperial=%u zweiklang=%d", settings.repeat, settings.zweiklang);
                settings.notePosition = 0;
                settings.counter = 0;
                settings.nextNote = 0;
            }
        }

        if (settings.counter++ >= settings.nextNote) {
            uint8_t note = pgm_read_byte(imperial_march_notes + settings.notePosition);
            uint8_t length = pgm_read_byte(imperial_march_lengths + settings.notePosition);

            analogWrite(settings.pin, 0);
            analogWrite(settings.pin2, 0);
            delay(5);

            _playNote(settings.pin, settings.pwmValue, note);
            if (settings.zweiklang) {
                _playNote(settings.pin2, settings.pwmValue, note + settings.zweiklang);
            }

            settings.nextNote = settings.counter + length;
            settings.notePosition++;

        }
    });
}

#endif

void BlindsControl::_startToneTimer(uint32_t maxLength)
{
    _stopToneTimer();
    if (_config.play_tone_channel == 0) {
        __LDBG_printf("tone timer disabled");
        return;
    }
    static constexpr uint16_t tone_interval = 2000;
    static constexpr uint16_t tone_duration = 150;
    static constexpr uint32_t max_length = 120 * 10000; // stop after 120 seconds

    uint8_t channel = (_config.play_tone_channel - 1) % kChannelCount;
    uint8_t index = (channel * kChannelCount) + (_states[channel].isClosed() ? 1 : 0);
    struct {
        uint32_t counter;
        uint32_t maxLength;
        uint16_t frequency;
        uint16_t pwmValue;
        uint8_t pin;
    } settings = {
        0,
        maxLength == 0 ? max_length : maxLength,
        (uint16_t)_config.tone_frequency,
        (uint16_t)_config.tone_pwm_value,
        _config.pins[index], // use channel 1, and use open/close pin that is jammed
    };

    __LDBG_printf("start tone timer pin=%u(#%u) channel=%u state=%s frequency=%u pwm=%u interval=%u duration=%u max_duration=%.1fs",
        settings.pin,
        index,
        channel,
        _states[channel]._getFPStr(),
        settings.frequency,
        settings.pwmValue,
        tone_interval,
        tone_duration,
        ((settings.maxLength / tone_duration) * tone_duration) / 1000.0
    );

    _setDac(PWMRANGE);

    _Timer(_toneTimer).add(tone_duration, true, [this, settings](Event::CallbackTimerPtr timer) mutable {
        uint16_t loop = settings.counter % (tone_interval / tone_duration);
        if (loop == 0) {
            _playTone(settings.pin, settings.pwmValue, settings.frequency);
        }
        else if (loop == 1) {
            analogWrite(settings.pin, 0);
        }
        if (++settings.counter > (settings.maxLength / tone_duration)) {
            _stop();
            return;
        }
    });
}

void BlindsControl::stopToneTimer()
{
    BlindsControlPlugin::getInstance()._stopToneTimer();
}

void BlindsControl::_stopToneTimer()
{
    __LDBG_printf("stopping tone timer and disabling motors");
    _toneTimer.remove();
    _disableMotors();
}

void BlindsControl::_executeAction(ChannelType channel, bool open)
{
    __LDBG_printf("channel=%u open=%u queue=%u", channel, open, _queue.size());
    if (_queue.empty()) {
        // create queue from open or close automation
        auto &automation = open ? _config.open : _config.close;
        for(size_t i = 0; i < sizeof(automation) / sizeof(automation[0]); i++) {
            auto action = automation[i]._get_enum_action();
            if (action != ActionType::NONE) {
                uint16_t delay = automation[i].delay;
                __LDBG_printf("queue action=%u channel=%u delay=%u", action, channel, delay);
                auto playTone = (PlayToneType)automation[i].play_tone;
                bool relativeDelay = automation[i].relative_delay;
                _queue.emplace_back(action, channel, delay, relativeDelay, playTone);
            }
        }
    }
    else {
        // abort current operation
        auto activeChannel = _activeChannel;
        _stop();
        _queue.clear();
        for(const auto channel: _states.channels()) {
            if (channel == activeChannel) {
                _states[channel] = StateType::STOPPED; // mark as stopped if it was running
            }
        }
        LoopFunctions::callOnce([this]() {
            _publishState();
            _saveState();
        });
    }
}

void BlindsControl::_startMotor(ChannelType channel, bool open)
{
    // __LDBG_printf("channel=%u open=%u", channel, open);

    // disables all motor pins
    _stop();
    // set active channel
    _activeChannel = channel;

    auto &cfg = _config.channels[*channel];
    _currentLimit = cfg.current_limit_mA * BlindsControllerConversion::kConvertCurrentToADCValueMulitplier;

    _states[channel] = open ? StateType::OPEN : StateType::CLOSED;

    _publishState();

    _motorTimeout.set(open ? cfg.open_time_ms : cfg.close_time_ms);
    _adcIntegralMultiplier = cfg.current_avg_period_us / kAdcIntegralMultiplierDivider;
    // clear ADC last
    _clearAdc();

    if (_config.pwm_softstart_time > 0) {

        // soft start enabled
        _setMotorSpeed(channel, 0, open);

        // set timer, pwm and pin
        // the pwm value is updated in the loop function
        _motorStartTime = micros();
        if (_motorStartTime == 0) {
            _motorStartTime++;
        }
    }
    else {
        _setMotorSpeed(channel, cfg.pwm_value, open);
    }
    _motorPWMValue = cfg.pwm_value;
#if DEBUG_IOT_BLINDS_CTRL
    _softStartUpdateCount = 0;
#endif

    __LDBG_printf("pin=%u pwm=%u soft-start=%ums I-limit=%umA (%.2f) dir=%s adc-mul=%.6f timeout=%ums",
        _motorPin, _motorPWMValue, _config.pwm_softstart_time,
        cfg.current_limit_mA, _currentLimit,
        open ? PSTR("open") : PSTR("close"),
        _adcIntegralMultiplier,
        _motorTimeout.getTimeLeft()
    );
}

bool BlindsControl::_checkMotor()
{
    _updateAdc();
    if (_adcIntegral > _currentLimit) {
        __LDBG_printf("current limit time=%.2f @ %u ms", _adcIntegral, _motorTimeout.getDelay() - _motorTimeout.getTimeLeft());
        Logger_notice("Channel %u, current limit %umA (%.2f/%.2f) triggered after %ums (max. %ums)", _activeChannel, (uint32_t)(_adcIntegral * BlindsControllerConversion::kConvertADCValueToCurrentMulitplier), _adcIntegral, _currentLimit, _motorTimeout.getDelay() - _motorTimeout.getTimeLeft(), _motorTimeout.getDelay());
        return true;
    }
    if (_motorTimeout.reached()) {
        __LDBG_printf("timeout");
        Logger_notice("Channel %u, motor stopped after %ums timeout, peak current %umA (%.2f/%.2f)", _activeChannel, _motorTimeout.getDelay(), (uint32_t)(_adcIntegralPeak * BlindsControllerConversion::kConvertADCValueToCurrentMulitplier), _adcIntegralPeak, _currentLimit);
        return true;
    }
#if IOT_BLINDS_CTRL_RPM_PIN
    if (_hasStalled()) {
        __LDBG_printf("stalled");
        return false;
    }
#endif
    return false;
}

void BlindsControl::_monitorMotor(ChannelAction &action)
{
    if (!_motorTimeout.isActive()) {
        __DBG_panic("motor timeout inactive");
    }
    if (_checkMotor()) {
        action.next();
        _stop();
        if (_cleanQueue()) {
            _saveState();
        }
        _publishState();
    }
}

BlindsControl::ActionToChannel::ActionToChannel(ActionType action, ChannelType channel) : ActionToChannel()
{
    switch(action) { // translate action into which channel to check and which to open/close
        case ActionType::DO_NOTHING:
            _for = ChannelType::ALL;
            _open = ChannelType::NONE;
            _close = ChannelType::NONE;
            break;
        case ActionType::DO_NOTHING_CHANNEL0:
            _for = ChannelType::CHANNEL0;
            _open = ChannelType::NONE;
            _close = ChannelType::NONE;
            break;
        case ActionType::DO_NOTHING_CHANNEL1:
            _for = ChannelType::CHANNEL1;
            _open = ChannelType::NONE;
            _close = ChannelType::NONE;
            break;
        case ActionType::OPEN_CHANNEL0:
            _for = channel == ChannelType::ALL ? ChannelType::ALL : ChannelType::CHANNEL0;
            _open = ChannelType::CHANNEL0;
            break;
        case ActionType::OPEN_CHANNEL1:
            _for = channel == ChannelType::ALL ? ChannelType::ALL : ChannelType::CHANNEL1;
            _open = ChannelType::CHANNEL1;
            break;
        case ActionType::OPEN_CHANNEL0_FOR_CHANNEL1:
            _for = ChannelType::CHANNEL1;
            _open = ChannelType::CHANNEL0;
            break;
        case ActionType::OPEN_CHANNEL1_FOR_CHANNEL0:
            _for = ChannelType::CHANNEL0;
            _open = ChannelType::CHANNEL1;
            break;
        case ActionType::CLOSE_CHANNEL0:
            _for = channel == ChannelType::ALL ? ChannelType::ALL : ChannelType::CHANNEL0;
            _close = ChannelType::CHANNEL0;
            break;
        case ActionType::CLOSE_CHANNEL1:
            _for = channel == ChannelType::ALL ? ChannelType::ALL : ChannelType::CHANNEL1;
            _close = ChannelType::CHANNEL1;
            break;
        case ActionType::CLOSE_CHANNEL0_FOR_CHANNEL1:
            _for = ChannelType::CHANNEL1;
            _close = ChannelType::CHANNEL0;
            break;
        case ActionType::CLOSE_CHANNEL1_FOR_CHANNEL0:
            _for = ChannelType::CHANNEL0;
            _close = ChannelType::CHANNEL1;
            break;
        default:
            break;
    }
}

bool BlindsControl::_cleanQueue()
{
    // check queue for any actions left otherwise clear it
    for(auto &action: _queue) {
        switch(action.getAction()) {
            // ignore do nothing actions
            case ActionType::DO_NOTHING:
            case ActionType::DO_NOTHING_CHANNEL0:
            case ActionType::DO_NOTHING_CHANNEL1:
                break;
            default:
                switch(action.getState()) {
                    case ActionStateType::NONE: {
                        auto channel = action.getChannel();
                        ActionToChannel channels(action.getAction(), channel);
                        __LDBG_printf("action=%d channel=%u for=%d open=%d close=%d channel0=%s channel1=%s", action.getAction(), channel, channels._for, channels._open, channels._close, _states[0]._getFPStr(), _states[1]._getFPStr());
                        if (channel == channels._for) {
                            if (channels.isOpenValid() && !_states[channels._open].isOpen()) {
                                return false;
                            }
                            else if (channels.isCloseValid() && !_states[channels._close].isClosed()) {
                                return false;
                            }
                        }
                    } break;
                    case ActionStateType::START_MOTOR:
                    case ActionStateType::WAIT_FOR_MOTOR:
                        return false;
                    case ActionStateType::DELAY:
                    case ActionStateType::REMOVE:
                        // remove finished actions and delays
                        break;
                    default:
                        __LDBG_panic("state=%u", action.getState());
                        break;
                }
                break;
        }
    }
    __DBG_printf("clear queue size=%u", _queue.size());
    _queue.clear();
    return true;
}

void BlindsControl::_loopMethod()
{
    // soft start is running, update PWM
    if (_motorStartTime) {
        uint32_t diff = get_time_diff(_motorStartTime, micros());
        uint32_t stime = _config.pwm_softstart_time * 1000U;
        if (diff >= stime) { // finished, set final pwm value
            analogWrite(_motorPin, _motorPWMValue);
            __LDBG_printf("soft start finished time=%u pin=%u pwm=%u updates=%u", diff, _motorPin, _motorPWMValue, _softStartUpdateCount);
            _motorStartTime = 0;
        }
        else {
            analogWrite(_motorPin, _motorPWMValue * diff / stime);
#if DEBUG_IOT_BLINDS_CTRL
            _softStartUpdateCount++;
#endif
        }
    }
    if (_queue.empty()) {
        // if the queue is empty check motor timeout only
        // this also updates the ADC readings and checks the current limit
        if (_motorTimeout.isActive()) {
            if (_checkMotor()) {
                _stop();
            }
        }
    }
    else {
        auto &action = _queue.getAction();
        auto channel = action.getChannel();
        switch(action.getState()) {
            case ActionStateType::START_DELAY: {
                ActionToChannel channels(action.getAction(), channel);
                __LDBG_printf("action=%d channel=%u for=%d do nothing", action.getAction(), channel, channels._for);
                if (channel == channels._for || channels._for != ChannelType::ALL) {
                    action.beginDoNothing(channel);
                }
                else {
                    action.end();
                }
            } break;
            case ActionStateType::NONE: {
                ActionToChannel channels(action.getAction(), channel);
                __LDBG_printf("action=%d channel=%u for=%d open=%d close=%d channel0=%s channel1=%s", action.getAction(), channel, channels._for, channels._open, channels._close, _states[0]._getFPStr(), _states[1]._getFPStr());
                if (channel == channels._for) {
                    if (channels.isOpenValid() && !_states[channels._open].isOpen()) {
                        action.begin(channels._open, true);
                    }
                    else if (channels.isCloseValid() && !_states[channels._close].isClosed()) {
                        action.begin(channels._close, false);
                    }
                    else {
                        action.end();
                    }
                }
                else {
                    action.end();
                }
            } break;
            case ActionStateType::START_MOTOR:
                _startMotor(channel, action.getOpen());
                action.next();
                break;
            case ActionStateType::WAIT_FOR_MOTOR:
                _monitorMotor(action);
                break;
            case ActionStateType::DELAY:
                action.monitorDelay();
                break;
            case ActionStateType::REMOVE:
                _queue.removeAction(action);
                if (_cleanQueue()) {
                    _publishState();
                    _saveState();
                }
                break;
            default:
                __LDBG_panic("state=%u", action.getState());
                break;
        }
    }
}

BlindsControl::NameType BlindsControl::_getStateStr(ChannelType channel) const
{
    if (_queue.empty()) {
        return _states[channel]._getFPStr();
    }
    else if (_activeChannel == channel) {
        return FSPGM(Running, "Running");
    }
    else if (!_queue.empty() && _queue.getAction().getState() == ActionStateType::DELAY) {
        return F("Waiting");
    }
    return FSPGM(Busy);
}

void BlindsControl::_clearAdc()
{
    auto channel = *_activeChannel;

    stopToneTimer();

    _setDac(_config.channels[channel].dac_pwm_value);

    analogWriteFreq(_config.pwm_frequency);

    uint8_t state = (channel == _config.adc_multiplexer) ? HIGH : LOW;
    digitalWrite(_config.pins[4], state);
    __LDBG_printf("multiplexer pin=%u channel=%u multiplexer=%u state=%u delay=%u", _config.pins[4], channel, _config.adc_multiplexer, state, IOT_BLINDS_CTRL_SETUP_DELAY);

    delay(IOT_BLINDS_CTRL_SETUP_DELAY);

    _adcLastUpdate = 0;
    _adcIntegral = 0;
    _adcIntegralPeak = 0;
    _currentTimer.start();

#if IOT_BLINDS_CTRL_RPM_PIN
    _rpmReset();
#endif
}

void BlindsControl::_updateAdc()
{
    auto micros = _currentTimer.getTime();
    if (micros < ADCManager::kMinDelayMicros) {
        return;
    }
    uint32_t tmp;
    auto reading = _adc.readValue(tmp);
    if (tmp == _adcLastUpdate) {
        return;
    }
    _adcLastUpdate = tmp;

    _currentTimer.start();
    float count = micros * _adcIntegralMultiplier;
    _adcIntegral = ((count * _adcIntegral) + reading) / (count + 1.0f);
    if (_adcIntegral > _adcIntegralPeak) {
        _adcIntegralPeak = _adcIntegral;
    }
}

void BlindsControl::_setMotorBrake(ChannelType channel)
{
    // shorts motor wires and brakes in slow decay mode
    // both pins low leaves motor wires floating -> _setMotorSpeed(channel, 0, ...)

    __LDBG_printf("channel=%u braking pins=%u,%u", channel, *(&_config.pins[(*channel * kChannelCount)]), *(&_config.pins[(*channel * kChannelCount)] + 1));

    _motorStartTime = 0;
    _motorPWMValue = 0;
    // make sure both pins are set to high at exactly the same time
    noInterrupts();
    auto ptr = &_config.pins[(*channel * kChannelCount)];
    digitalWrite(*ptr++, HIGH);
    digitalWrite(*ptr, HIGH);
    interrupts();
}

void BlindsControl::_setMotorSpeed(ChannelType channelType, uint16_t speed, bool open)
{
    auto channel = *channelType;
    _motorPin = _config.pins[(channel * kChannelCount) + !open]; // first pin is open pin
    uint8_t pin2 = _config.pins[(channel * kChannelCount) + open]; // second is close pin
#if DEBUG_IOT_BLINDS_CTRL
    __LDBG_printf("channel=%u speed=%u open=%u pins=%u,%u frequency=%u", channel, speed, open, _motorPin, pin2, _config.pwm_frequency);
#endif

    _motorStartTime = 0;
    analogWriteRange(PWMRANGE);
    analogWriteFreq(_config.pwm_frequency);

    noInterrupts();
    if (speed == 0) {
        digitalWrite(_motorPin, LOW);
        _motorPWMValue = 0;
    }
    else {
        analogWrite(_motorPin, speed);
    }
    digitalWrite(pin2, LOW);
    interrupts();
}

void BlindsControl::_disableMotors()
{
    // do not allow interrupts when changing a pin pair from high/high to low/low and vice versa
    noInterrupts();
    for(uint8_t i = 0; i < 2 * kChannelCount; i++) {
        digitalWrite(_config.pins[i], LOW);
    }
    interrupts();
}

void BlindsControl::_stop()
{
    _stopToneTimer();

    __LDBG_printf("pwm=%u pin=%u adc=%.2f peak=%.2f stopping motor", _motorPWMValue, _motorPin, _adcIntegral, _adcIntegralPeak);

    _motorStartTime = 0;
    _motorPWMValue = 0;
    _motorTimeout.disable();

    _activeChannel = ChannelType::NONE;
    _disableMotors();
    digitalWrite(_config.pins[4], LOW);
    digitalWrite(_config.pins[5], LOW);

    _adcIntegral = 0;
}

String BlindsControl::_getTopic(ChannelType channel, TopicType type)
{
    if (type == TopicType::METRICS) {
        return MQTTClient::formatTopic(FSPGM(metrics));
    }
    else if (type == TopicType::CHANNELS) {
        return MQTTClient::formatTopic(FSPGM(channels));
    }
    const __FlashStringHelper *str;
    switch(type) {
        case TopicType::SET:
            str = FSPGM(_set);
            break;
        case TopicType::STATE:
        default:
            str = FSPGM(_state);
            break;
    }
    if (channel == ChannelType::ALL) {
        return MQTTClient::formatTopic(str);
    }
    return MQTTClient::formatTopic(PrintString(FSPGM(channel__u), channel), str);
}

void BlindsControl::_loadState()
{
#if IOT_BLINDS_CTRL_SAVE_STATE
    auto file = KFCFS.open(FSPGM(iot_blinds_control_state_file), fs::FileOpenMode::read);
    if (file) {
        file.read(reinterpret_cast<uint8_t *>(_states.data()), _states.size() * sizeof(*_states.data()));
    }
    __LDBG_printf("file=%u state=%u,%u", (bool)file, _states[0], _states[1]);
#endif
}

void BlindsControl::_saveState()
{
#if IOT_BLINDS_CTRL_SAVE_STATE
    auto file = KFCFS.open(FSPGM(iot_blinds_control_state_file), fs::FileOpenMode::write);
    if (file) {
        file.write(reinterpret_cast<const uint8_t *>(_states.data()), _states.size() * sizeof(*_states.data()));
    }
    __LDBG_printf("file=%u state=%u,%u", (bool)file, _states[0], _states[1]);
#endif
}

void BlindsControl::_readConfig()
{
    _config = Plugins::Blinds::getConfig();
    _adcIntegralMultiplier = _config.channels[0].current_avg_period_us / kAdcIntegralMultiplierDivider;

    _adc.setOffset(_config.adc_offset);
    _adc.setMinDelayMicros(_config.adc_read_interval);
    _adc.setMaxDelayMicros(_config.adc_recovery_time);
    _adc.setRepeatMaxDelayPerSecond(_config.adc_recoveries_per_second);
    _adc.setMaxDelayYieldTimeMicros(std::min<uint16_t>(1000, _config.adc_recovery_time / 2));

    _loadState();
}

#if IOT_BLINDS_CTRL_RPM_PIN

void BlindsControl::_rpmReset()
{
    _rpmTimer.start();
    _rpmLastInterrupt.start();
    _rpmCounter = 0;
}

void BlindsControl::_rpmIntCallback(const InterruptInfo &info)
{
    const uint8_t multiplier = IOT_BLINDS_CTRL_RPM_PULSES * 10;
    auto time = _rpmTimer.getTime(info.micro);
    _rpmLastInterrupt.start();
    _rpmTimer.start(info.micro);
    _rpmTimeIntegral = ((_rpmTimeIntegral * multiplier) + time) / (multiplier + 1);
    _rpmCounter++;
}

uint16_t BlindsControl::_getRpm()
{
    return (uint16_t)((1000000UL * 60UL / IOT_BLINDS_CTRL_RPM_PULSES) / _rpmTimeIntegral);
}

bool BlindsControl::_hasStalled()
{
    // interval for 500 rpm = ~40ms with 3 pulses
    return (_rpmLastInterrupt.getTime() > (1000000UL / 500UL/*rpm*/ * 60UL / IOT_BLINDS_CTRL_RPM_PULSES));
}

#endif
