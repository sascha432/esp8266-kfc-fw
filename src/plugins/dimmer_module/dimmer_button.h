/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if IOT_DIMMER_MODULE_HAS_BUTTONS

#include "dimmer_buttons.h"
#include <EnumHelper.h>
#include <PinMonitor.h>

namespace Dimmer {

    using PushButtonConfig = PinMonitor::PushButtonConfig;
    using SingleClickGroupPtr = PinMonitor::SingleClickGroupPtr;
    using PushButton = PinMonitor::PushButton;

    class ButtonConfig : public PushButtonConfig
    {
    public:
        ButtonConfig(ConfigType &config) :
            PushButtonConfig(
                EnumHelper::Bitset::all(EventType::UP, EventType::DOWN, EventType::LONG_PRESSED, EventType::HOLD/*, EventType::REPEATED_CLICK*/),
                config._base.shortpress_time,
                config._base.longpress_time,
                IOT_DIMMER_MODULE_HOLD_REPEAT_TIME,
                config._base.shortpress_steps,
                config._base.single_click_time
            )
        {
        }
    };

    class Button : public PushButton {
    public:
    public:
        Button(uint8_t pin, uint8_t channel, uint8_t button, Buttons &dimmer, SingleClickGroupPtr singleClickGroup);

        virtual void event(EventType state, uint32_t now) override;

    private:
        void _setLevel(int32_t newLevel, int16_t curLevel, float fadeTime);
        void _setLevel(int32_t newLevel, float fadeTime);
        void _changeLevel(int32_t changeLevel, float fadeTime);
        void _changeLevelSingle(uint16_t steps, bool invert, float time);
        void _changeLevelRepeat(uint16_t repeatTime, bool invert);
        void _freezeLevel();

    private:
        Buttons &_dimmer;
        // stores current level for switching on/off
        int16_t _level;
        // channel number
        uint8_t _channel;
        // button number
        uint8_t _button;
        // bitset for buttons sending HOLD_REPEAT events
        uint8_t _repeat;
    };

 }

#endif
