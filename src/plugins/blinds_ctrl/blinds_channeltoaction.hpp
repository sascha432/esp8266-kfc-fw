/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "BlindsControl.h"

inline BlindsControl::ActionToChannel::ActionToChannel() :
    _for(ChannelType::NONE),
    _open(ChannelType::NONE),
    _close(ChannelType::NONE)
{
}

inline BlindsControl::ActionToChannel::ActionToChannel(ActionType action, ChannelType channel) :
    ActionToChannel()
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

inline bool BlindsControl::ActionToChannel::isOpenValid() const
{
    return _open != ChannelType::NONE;
}

inline bool BlindsControl::ActionToChannel::isCloseValid() const
{
    return _close != ChannelType::NONE;
}

inline bool BlindsControl::ActionToChannel::isDoNothing() const
{
    return _close == ChannelType::NONE && _open == ChannelType::NONE;
}

