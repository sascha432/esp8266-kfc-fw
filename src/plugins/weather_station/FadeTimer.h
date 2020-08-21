/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <EventScheduler.h>

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
        _direction(fromLevel > toLevel ? -step : step),
        _analogWrite({fromLevel, false, false})
    {
        __DBG_printf("timer_arg=%p", _etsTimer.timer_arg);
        analogWrite(TFT_PIN_LED, fromLevel);
        LoopFunctions::add([this]() {
            this->loop();
        }, this);
        startTimer(10, true);
    }

    virtual void detach() override;
    virtual void run() override;

    void loop() {
        noInterrupts();
        analogWrite_t tmp = { _analogWrite.level, _analogWrite.update, _analogWrite.done };
        _analogWrite.update = 0;
        interrupts();

        if (tmp.done) {
            LoopFunctions::remove(this);
            auto ptr = *_timerPtr;
            *_timerPtr = nullptr;
            delete ptr;
        }
        else if (tmp.update) {
            analogWrite(TFT_PIN_LED, tmp.level);
        }
    }

private:
    FadeTimer **_timerPtr;
    uint16_t _fromLevel;
    uint16_t _toLevel;
    int8_t _step;
    int8_t _direction;
    typedef struct {
        uint16_t level: 14;
        uint16_t update: 1;
        uint16_t done: 1;
    } analogWrite_t;
    volatile analogWrite_t _analogWrite;
};
