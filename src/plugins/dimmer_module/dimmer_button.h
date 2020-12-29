/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if IOT_DIMMER_MODULE_HAS_BUTTONS

#include "dimmer_buttons.h"
#include <EnumHelper.h>
#include <PinMonitor.h>

using namespace PinMonitor;

class DimmerButtonConfig : public PushButtonConfig
{
public:
    using ConfigType = Plugins::DimmerConfig::DimmerConfig_t;

    DimmerButtonConfig(ConfigType &config) :
        PushButtonConfig(
            EnumHelper::Bitset::all(EventType::REPEATED_CLICK, EventType::CLICK, EventType::LONG_CLICK),
            config.shortpress_time,
            config.longpress_time,
            IOT_DIMMER_MODULE_HOLD_REPEAT_TIME,
            config.shortpress_steps
        )
    {
    }
};

class DimmerButton : public PushButton {
public:
    using ConfigType = DimmerButtonConfig::ConfigType;

public:
    DimmerButton(uint8_t pin, uint8_t channel, uint8_t button, DimmerButtons &dimmer, SingleClickGroupPtr singleClickGroup);

    virtual void event(EventType state, uint32_t now) override;

private:
    void _setLevel(int32_t newLevel, int16_t curLevel, float fadeTime);
    void _setLevel(int32_t newLevel, float fadeTime);
    void _changeLevel(int32_t changeLevel, float fadeTime);
    void _changeLevelSingle(uint16_t steps, bool invert);
    void _changeLevelRepeat(uint16_t repeatTime, bool invert);

private:
    DimmerButtons &_dimmer;
    int16_t _level;
    uint8_t _channel: 3;
    uint8_t _button: 1;
};

#endif
