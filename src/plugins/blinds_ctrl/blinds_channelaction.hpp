/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "BlindsControl.h"

inline BlindsControl::ChannelAction::ChannelAction(uint32_t startTime, uint16_t delay, ActionType state, ChannelType channel, PlayToneType playTone, bool relativeDelay) :
    _startTime(startTime),
    _delay(delay),
    _state(ActionStateType::NONE),
    _action(state),
    _channel(channel),
    _playTone(playTone),
    _relativeDelay(relativeDelay),
    _open(false)
{
}

inline BlindsControl::ChannelType BlindsControl::ChannelAction::getChannel() const
{
    return _channel;
}

inline BlindsControl::ActionType BlindsControl::ChannelAction::getAction() const
{
    return _action;
}

inline BlindsControl::ActionStateType BlindsControl::ChannelAction::getState() const
{
    return _state;
}

inline BlindsControl::PlayToneType BlindsControl::ChannelAction::getPlayTone() const
{
    return _playTone;
}

inline uint32_t BlindsControl::ChannelAction::getDelay() const
{
    return _delay * 1000U;
}

inline uint32_t BlindsControl::ChannelAction::getTimeout() const
{
    if (_state != ActionStateType::DELAY) {
        return 0;
    }
    return _startTime + getDelay();
}

inline bool BlindsControl::ChannelAction::isRelativeDelay() const
{
    return _relativeDelay;
}

inline bool BlindsControl::ChannelAction::getOpen() const
{
    return _open;
}

inline void BlindsControl::ChannelAction::monitorDelay()
{
    if (_state == ActionStateType::DELAY && get_time_diff(_startTime, millis()) >= getDelay()) {
        __LDBG_printf("start=%u end=%u delay=%u diff=%u relative=%u finished", _startTime, getTimeout(), getDelay(), get_time_diff(_startTime, millis()), isRelativeDelay());
        next();
    }
}

inline void BlindsControl::ChannelAction::beginDoNothing(ChannelType channel)
{
    __LDBG_printf("begin=%u channel=%u do_nothing", _state, channel);
    _state = ActionStateType::START_DELAY;
}

inline void BlindsControl::ChannelAction::begin(ChannelType channel, bool open)
{
    __LDBG_printf("begin=%u channel=%u open=%u", _state, channel, open);
    // update channel to change and the state
    _open = open;
    _channel = channel;
    next();
}

inline void BlindsControl::ChannelAction::next()
{
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
                if (isRelativeDelay()) {
                    _startTime = millis();
                }
                __LDBG_printf("start=%u end=%u delay=%u relative=%u play_tone=%u", _startTime, getTimeout(), _delay, _relativeDelay, _playTone);
                break;
            }
            // fallthrough
        default:
            end();
            break;
    }
}

inline void BlindsControl::ChannelAction::end()
{
    BlindsControl::stopToneTimer(_state);
    __LDBG_printf("end=%u", _state);
    _delay = 0;
    _state = ActionStateType::REMOVE;
}
