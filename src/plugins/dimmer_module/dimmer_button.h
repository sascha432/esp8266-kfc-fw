/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "dimmer_base.h"

class DimmerButton : public PinMonitor::Pin {
public:
    using ConfigType = Dimmer_Base::ConfigType;

public:
    DimmerButton(uint8_t pin, uint8_t channel, uint8_t button, DimmerButtons &dimmer);

    virtual void event(StateType state, TimeType now) override;
    virtual void loop() override;

private:
    void _buttonReleased();

    void _setLevel(int32_t newLevel, int32_t curLevel, float fadeTime);
    void _setLevel(int32_t newLevel, float fadeTime);
    void _changeLevel(int32_t change, float fadeTime);

private:
    DimmerButtons &_dimmer;
    uint32_t _startTimer;
    uint32_t _noRepeatTimer;
    uint32_t _duration;
    uint16_t _repeatCount;
    int16_t _level;
    uint8_t _channel: 3;
    uint8_t _button: 1;
    uint8_t _startTimerRunning: 1;
    uint8_t _noRepeatTimerRunning: 1;
    uint8_t _noRepeatCount: 2;
};
