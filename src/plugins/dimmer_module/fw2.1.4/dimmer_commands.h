/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

class DimmerFadeCommand {
public:
    typedef struct __attribute__packed__ {
        uint8_t address;
        int16_t from_level;
        int8_t channel;
        int16_t to_level;
        float time;
        uint8_t command;
    } dimmer_fade_t;

    DimmerFadeCommand(int8_t channel, int16_t fromLevel, int16_t toLevel, float time) : _fade({DIMMER_REGISTER_FROM_LEVEL, fromLevel, channel, toLevel, time, DIMMER_COMMAND_FADE}) {}

    dimmer_fade_t &data() {
        return _fade;
    }

private:
    dimmer_fade_t _fade;
};
