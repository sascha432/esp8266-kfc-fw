/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "BlindsControl.h"

inline BlindsControl::ChannelState::ChannelState() :
    _state(StateType::UNKNOWN)
{
}

inline BlindsControl::ChannelState &BlindsControl::ChannelState::operator=(StateType state)
{
    _state = state;
    return *this;
}

// inline void BlindsControl::ChannelState::setToggleOpenClosed(StateType state)
// {
//     _state = (state == StateType::OPEN ? StateType::CLOSED : StateType::OPEN);
// }

inline BlindsControl::StateType BlindsControl::ChannelState::getState() const
{
    return _state;
}
// return binary state as char
inline char BlindsControl::ChannelState::getCharState() const
{
    return isOpen() ? '1' : '0';
}

inline bool BlindsControl::ChannelState::isOpen() const
{
    return _state == StateType::OPEN;
}

inline uint8_t BlindsControl::ChannelState::isOpenInt() const
{
    return _state == StateType::OPEN;
}

inline bool BlindsControl::ChannelState::isClosed() const
{
    return _state == StateType::CLOSED;
}

inline BlindsControl::ChannelState::operator unsigned() const
{
    return static_cast<unsigned>(_state);
}

inline BlindsControl::NameType BlindsControl::ChannelState::_getFPStr() const
{
    return __getFPStr(_state);
}

inline BlindsControl::NameType BlindsControl::ChannelState::__getFPStr(StateType state)
{
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
