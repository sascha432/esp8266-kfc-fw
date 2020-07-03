/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#ifndef DIMMER_FIRMWARE_VERSION
#define DIMMER_FIRMWARE_VERSION                     0x020104
#endif

#if DIMMER_FIRMWARE_VERSION==0x020104

#define DIMMER_I2C_SLAVE 1

#include "./fw2.1.4/dimmer_protocol.h"
#include "./fw2.1.4/dimmer_reg_mem.h"

#define DIMMER_REGISTER_CONFIG_OFS                  (DIMMER_REGISTER_START_ADDR + offsetof(register_mem_t, cfg))
#define DIMMER_REGISTER_CONFIG_SZ                   sizeof(register_mem_t)

#endif
