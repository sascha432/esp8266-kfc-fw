/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "BlindsControl.h"

inline BlindsControl::ChannelQueue::ChannelQueue(BlindsControl &control) :
    Type(),
    _control(control),
    _startTime(millis())
{
}

inline void BlindsControl::ChannelQueue::push_back(ChannelAction &&action)
{
    if (empty()) {
        resetStartTime();
    }
    emplace_back(std::move(action));
}

inline BlindsControl::ChannelAction &BlindsControl::ChannelQueue::getAction()
{
    return front();
}

inline const BlindsControl::ChannelAction &BlindsControl::ChannelQueue::getAction() const
{
    return front();
}

inline void BlindsControl::ChannelQueue::removeAction(const ChannelAction &action)
{
    __LDBG_printf("remove action=%p", &action);
    erase(std::remove_if(begin(), end(), [&action](const ChannelAction &_action) {
        return &action == &_action;
    }), end());
}

inline void BlindsControl::ChannelQueue::resetStartTime(uint32_t startTime)
{
    _startTime = startTime;
}

inline uint32_t BlindsControl::ChannelQueue::getStartTime() const
{
    return _startTime;
}
