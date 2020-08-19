/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <OSTimer.h>

class FadeTimer : public OSTimer
{
public:
    FadeTimer() : _timerPtr(nullptr) {
        __DBG_printf("timer_arg=%p", _etsTimer.timer_arg);
    }

    FadeTimer(FadeTimer **_timerPtr, uint16_t fromLevel, uint16_t toLevel, int8_t step) :
        _fromLevel(fromLevel),
        _toLevel(toLevel),
        _step(step),
        _direction(fromLevel > toLevel ? -step : step)
    {
        __DBG_printf("timer_arg=%p", _etsTimer.timer_arg);
        analogWrite(TFT_PIN_LED, fromLevel);
        startTimer(10, true);
    }

    virtual void ICACHE_RAM_ATTR detach() override;
    virtual void ICACHE_RAM_ATTR run() override;

private:
    FadeTimer **_timerPtr;
    uint16_t _fromLevel;
    uint16_t _toLevel;
    int8_t _step;
    int8_t _direction;
};
