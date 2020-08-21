/**
  Author: sascha_lammers@gmx.de
*/

#include "FadeTimer.h"

#ifndef TFT_PIN_LED
#include "WSDraw.h"
#endif

void FadeTimer::run()
{
    if (abs(_toLevel - _fromLevel) > _step) {
        _fromLevel += _direction;
    } else {
        _fromLevel = _toLevel;
        detach();
    }
    _analogWrite.update = true;
}

void FadeTimer::detach()
{
    if (_timerPtr && _analogWrite.done == false) {
        // detach timer
        // OSTimer::detach();
        _ostimer_detach(&_etsTimer, this);
        // mark as done
        _analogWrite.done = true;
    }
}
