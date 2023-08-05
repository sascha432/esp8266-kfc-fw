/**
 * Author: sascha_lammers@gmx.de
 */

//
// includes the firmware headers from https://github.com/sascha432/trailing_edge_dimmer.git
//

#pragma once

#define DIMMER_HEADERS_ONLY 1

#include "dimmer_def.h"
#include "dimmer_twowire.h"
#include <Arduino_compat.h>

#ifndef DIMMER_FIRMWARE_VERSION
#    define DIMMER_FIRMWARE_VERSION 0x020203
#endif

#if DIMMER_FIRMWARE_VERSION < 0x020203
#    error not support anymore
#endif

#define DIMMER_CURRENT_LEVEL -1

// required major and minor version
#define DIMMER_FIRMWARE_VERSION_MAJOR 2
#define DIMMER_FIRMWARE_VERSION_MINOR 2

#if defined(I2C_OVER_UART_SLAVE_RESPONSE_MASTER_TRANSMIT) && I2C_OVER_UART_SLAVE_RESPONSE_MASTER_TRANSMIT
#    error requires I2C_OVER_UART_SLAVE_RESPONSE_MASTER_TRANSMIT=0
#endif

#include "dimmer_protocol.h"
#include "dimmer_reg_mem.h"
#include "firmware/dimmer_commands.h"
