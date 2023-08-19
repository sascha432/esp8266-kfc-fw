/**
 * Author: sascha_lammers@gmx.de
 */

#include "WebUISocket.h"
#include "BlindsControl.h"
#include "blinds_plugin.h"
#include "kfc_fw_config.h"
#include <stl_ext/algorithm.h>

#if DEBUG_IOT_BLINDS_CTRL
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

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
        uint32_t diff = get_time_since(_motorStartTime, micros());
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

