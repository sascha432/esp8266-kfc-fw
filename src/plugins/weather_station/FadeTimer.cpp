/**
  Author: sascha_lammers@gmx.de
*/

#include "FadeTimer.h"

#ifndef TFT_PIN_LED
#include "WSDraw.h"
#endif

void ICACHE_RAM_ATTR FadeTimer::run()
{
    if (abs(_toLevel - _fromLevel) > _step) {
        _fromLevel += _direction;
    } else {
        _fromLevel = _toLevel;
        detach();
    }
    analogWrite(TFT_PIN_LED, _fromLevel);
}

void ICACHE_RAM_ATTR FadeTimer::detach()
{
    if (_timerPtr) {
        // detach timer
        _ostimer_detach(&_etsTimer, this);
        // get pointer to object, after deleting the member variable will become invalid
        volatile auto tmp = _timerPtr;
        // remove pointer that the destructor won't run this again
        _timerPtr = nullptr;
        // delete itself
        delete *tmp;
        // set pointer to timer to null
        *tmp = nullptr;
    }
}
