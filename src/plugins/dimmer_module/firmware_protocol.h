/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#ifndef DIMMER_FIRMWARE_VERSION
#define DIMMER_FIRMWARE_VERSION                     0x020104
#endif

#define DIMMER_TEMP_OFFSET_DIVIDER                  4.0
#define DIMMER_CURRENT_LEVEL                        -1

#if DIMMER_FIRMWARE_VERSION==0x020104

#define DIMMER_I2C_SLAVE 1

#include "./fw2.1.4/dimmer_protocol.h"
#include "./fw2.1.4/dimmer_reg_mem.h"
#include "./fw2.1.4/dimmer_commands.h"

#endif

#define DIMMER_REGISTER_CONFIG_OFS                  (DIMMER_REGISTER_START_ADDR + offsetof(register_mem_t, cfg))
#define DIMMER_REGISTER_CONFIG_SZ                   sizeof(register_mem_cfg_t)
#define DIMMER_VERSION_SPLIT(version)               (version >> 10), ((version >> 5) & 0x1f), (version & 0x1f)
