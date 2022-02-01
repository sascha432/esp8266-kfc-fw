/**
 * Author: sascha_lammers@gmx.de
 */

#include "WebUISocket.h"
#include "BlindsControl.h"
#include "blinds_plugin.h"
#include "kfc_fw_config.h"
#include <stl_ext/algorithm.h>

#if DEBUG_IOT_BLINDS_CTRL
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

extern "C" std::underlying_type<BlindsControl::ChannelType>::type operator *(const BlindsControl::ChannelType type);

std::underlying_type<BlindsControl::ChannelType>::type operator *(const BlindsControl::ChannelType type) {
    return static_cast<std::underlying_type<BlindsControl::ChannelType>::type>(type);
}

#if HAVE_IMPERIAL_MARCH

void BlindsControl::_playImperialMarch(uint16_t speed, int8_t zweiklang, uint8_t repeat)
{
    _stopToneTimer();
    if ((_config.play_tone_channel & 0x03) == 0) {
        __LDBG_printf("tone timer disabled");
        return;
    }
    uint8_t channel = (_config.play_tone_channel - 1) % kChannelCount;
    uint8_t channel2 = (channel + 1) % kChannelCount;
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
            auto note = pgm_read_byte(imperial_march_notes + IMPERIAL_MARCH_NOTES_COUNT - 1);
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
            auto note = pgm_read_byte(imperial_march_notes + settings.notePosition);
            auto length = pgm_read_byte(imperial_march_lengths + settings.notePosition);

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

void BlindsControl::_startToneTimer(uint32_t timeout)
{
    _stopToneTimer();
    if ((_config.play_tone_channel & 0x03) == 0) {
        __LDBG_printf("tone timer disabled");
        return;
    }

    uint8_t pins[2] = { kInvalidPin, kInvalidPin };
    auto pinPtr = &pins[0];
    if (_config.play_tone_channel & 0x01) {
        *pinPtr++ = _config.pins[0 * kChannelCount];
    }
    if (_config.play_tone_channel & 0x02) {
        *pinPtr = _config.pins[1 * kChannelCount];
    }

    // stored in the callback lambda function
    ToneSettings settings(_config.tone_frequency, _config.tone_pwm_value, pins, timeout);

    __LDBG_printf("start tone timer pin=%u,%u states=%s,%s freq=%u pwm=%u interval=%ums tone=%ums max_duration=%.1fs runtime=%u",
        settings.pin[0],
        settings.pin[1],
        _states[0]._getFPStr(),
        _states[1]._getFPStr(),
        settings.frequency,
        settings.pwmValue,
        settings.interval,
        ToneSettings::kToneDuration,
        ToneSettings::kMaxLength / 1000.0,
        settings.runtime
    );

    _setDac(PWMRANGE);

    _Timer(_toneTimer).add(ToneSettings::kTimerInterval, true, [this, settings](Event::CallbackTimerPtr timer) mutable {
        auto interval = settings.runtime ? std::clamp<uint16_t>(ToneSettings::kToneInterval - (((settings.counter * (ToneSettings::kToneInterval - ToneSettings::kToneMinInterval)) / (settings.runtime / ToneSettings::kTimerInterval)) + ToneSettings::kToneMinInterval), ToneSettings::kToneMinInterval, ToneSettings::kToneInterval) : settings.interval;
        // __LDBG_printf("_toneTimer loop=%u counter=%u/%u interval=%u interval_loops=%u", loop, settings.counter, settings.runtime / ToneSettings::kTimerInterval, interval, interval / ToneSettings::kTimerInterval);
        if (settings.loop == 0) {
            // __LDBG_printf("_toneTimer ON counter=%u interval=%u", settings.counter, interval);
            // start tone
            _playTone(settings.pin.data(), settings.pwmValue, settings.frequency);
        }
        else if (settings.loop == ToneSettings::kToneMaxLoop) {
            // stop tone
            // __LDBG_printf("_toneTimer OFF loop=%u/%u", settings.loop, (interval / ToneSettings::kTimerInterval));
            analogWrite(settings.pin[0], 0);
#if !defined(ESP8266)
            // analogWrite checks the pin
            if (settings.hasPin2())
#endif
            {
                analogWrite(settings.pin[1], 0);
            }
        }
        if (++settings.loop > (interval / ToneSettings::kTimerInterval)) {
            // __LDBG_printf("_toneTimer IVAL counter=%u", settings.counter);
            settings.loop = 0;
        }
        // max. limit
        if (++settings.counter > (ToneSettings::kMaxLength / ToneSettings::kTimerInterval)) {
            __LDBG_printf("_toneTimer max. timeout reached");
            timer->disarm();
            LoopFunctions::callOnce([this]() {
                _stop();
            });
            return;
        }
    });
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
                __LDBG_printf("queue action=%u channel=%u delay=%u relative=%u", action, channel, automation[i].delay, automation[i].relative_delay);
                _queue.push_back(ChannelAction(_queue.getStartTime(), automation[i]._get_delay(), action, channel, automation[i]._get_enum_play_tone(), automation[i].relative_delay));
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

// BlindsControl::ActionToChannel::ActionToChannel(ActionType action, ChannelType channel) : ActionToChannel()
// {
//     switch(action) { // translate action into which channel to check and which to open/close
//         case ActionType::DO_NOTHING:
//             _for = ChannelType::ALL;
//             _open = ChannelType::NONE;
//             _close = ChannelType::NONE;
//             break;
//         case ActionType::DO_NOTHING_CHANNEL0:
//             _for = ChannelType::CHANNEL0;
//             _open = ChannelType::NONE;
//             _close = ChannelType::NONE;
//             break;
//         case ActionType::DO_NOTHING_CHANNEL1:
//             _for = ChannelType::CHANNEL1;
//             _open = ChannelType::NONE;
//             _close = ChannelType::NONE;
//             break;
//         case ActionType::OPEN_CHANNEL0:
//             _for = channel == ChannelType::ALL ? ChannelType::ALL : ChannelType::CHANNEL0;
//             _open = ChannelType::CHANNEL0;
//             break;
//         case ActionType::OPEN_CHANNEL1:
//             _for = channel == ChannelType::ALL ? ChannelType::ALL : ChannelType::CHANNEL1;
//             _open = ChannelType::CHANNEL1;
//             break;
//         case ActionType::OPEN_CHANNEL0_FOR_CHANNEL1:
//             _for = ChannelType::CHANNEL1;
//             _open = ChannelType::CHANNEL0;
//             break;
//         case ActionType::OPEN_CHANNEL1_FOR_CHANNEL0:
//             _for = ChannelType::CHANNEL0;
//             _open = ChannelType::CHANNEL1;
//             break;
//         case ActionType::CLOSE_CHANNEL0:
//             _for = channel == ChannelType::ALL ? ChannelType::ALL : ChannelType::CHANNEL0;
//             _close = ChannelType::CHANNEL0;
//             break;
//         case ActionType::CLOSE_CHANNEL1:
//             _for = channel == ChannelType::ALL ? ChannelType::ALL : ChannelType::CHANNEL1;
//             _close = ChannelType::CHANNEL1;
//             break;
//         case ActionType::CLOSE_CHANNEL0_FOR_CHANNEL1:
//             _for = ChannelType::CHANNEL1;
//             _close = ChannelType::CHANNEL0;
//             break;
//         case ActionType::CLOSE_CHANNEL1_FOR_CHANNEL0:
//             _for = ChannelType::CHANNEL0;
//             _close = ChannelType::CHANNEL1;
//             break;
//         default:
//             break;
//     }
// }

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
            __LDBG_printf("soft start finished time=%.3fms pin=%u pwm=%u updates=%u", diff / 1000.0, _motorPin, _motorPWMValue, _softStartUpdateCount);
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



void BlindsControl::_clearAdc()
{
    auto channel = *_activeChannel;

    stopToneTimer();

    _setDac(_config.channels[channel].dac_pwm_value);

    analogWriteFreq(_config.pwm_frequency);

    uint8_t state = (channel == _config.adc_multiplexer) ? HIGH : LOW;
    digitalWrite(_config.pins[kMultiplexerPin], state);
    __LDBG_printf("multiplexer pin=%u channel=%u multiplexer=%u state=%u delay=%u", _config.pins[kMultiplexerPin], channel, _config.adc_multiplexer, state, IOT_BLINDS_CTRL_SETUP_DELAY);

    delay(IOT_BLINDS_CTRL_SETUP_DELAY);

    _adcLastUpdate = 0;
    _adcIntegral = 0;
    _adcIntegralPeak = 0;
    _currentTimer.start();
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
