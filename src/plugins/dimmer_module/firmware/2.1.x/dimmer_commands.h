/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "dimmer_protocol.h"
#include "dimmer_reg_mem.h"
#include "../dimmer_commands.h"


// #define DIMMER_REGISTER_CONFIG_OFS                  (DIMMER_REGISTER_START_ADDR + offsetof(register_mem_t, cfg))
// #define DIMMER_REGISTER_CONFIG_SZ                   sizeof(register_mem_cfg_t)
// #define DIMMER_VERSION_SPLIT(version)               version.getMajor(), version.getMinor(), version.getRevision()


// class DimmerFadeCommand {
// public:
//     typedef struct __attribute__packed__ {
//         uint8_t reg_mem_address;
//         int16_t from_level;
//         int8_t channel;
//         int16_t to_level;
//         float time;
//         uint8_t command;
//     } DimmerFadeCommand_t;

//     DimmerFadeCommand(int8_t channel, int16_t fromLevel, int16_t toLevel, float time) : _fade({DIMMER_REGISTER_FROM_LEVEL, fromLevel, channel, toLevel, time, DIMMER_COMMAND_FADE}) {}

//     DimmerFadeCommand_t &data() {
//         return _fade;
//     }

// private:
//     DimmerFadeCommand_t _fade;
// };
