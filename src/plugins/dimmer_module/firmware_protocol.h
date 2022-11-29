/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

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

#if defined(I2C_OVER_UART_SLAVE_RESPONSE_MASTER_TRANSMIT) && I2C_OVER_UART_SLAVE_RESPONSE_MASTER_TRANSMIT
#    error requires I2C_OVER_UART_SLAVE_RESPONSE_MASTER_TRANSMIT=0
#endif

#define DIMMER_I2C_SLAVE              1
#define DIMMER_FIRMWARE_VERSION_MAJOR 2
#define DIMMER_FIRMWARE_VERSION_MINOR 2

#include "dimmer_protocol.h"
#include "dimmer_reg_mem.h"
#include "firmware/dimmer_commands.h"
