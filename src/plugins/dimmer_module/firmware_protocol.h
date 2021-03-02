/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include  <Arduino_compat.h>
#include "dimmer_def.h"
#include "dimmer_twowire.h"

#ifndef DIMMER_FIRMWARE_VERSION
#define DIMMER_FIRMWARE_VERSION                     0x020200
#endif

#define DIMMER_CURRENT_LEVEL                        -1

#if DIMMER_FIRMWARE_VERSION < 0x020200

#if !defined(I2C_OVER_UART_SLAVE_RESPONSE_MASTER_TRANSMIT) || !I2C_OVER_UART_SLAVE_RESPONSE_MASTER_TRANSMIT
#error requires I2C_OVER_UART_SLAVE_RESPONSE_MASTER_TRANSMIT=1
#endif

#define DIMMER_I2C_SLAVE                            1
#define DIMMER_TEMP_OFFSET_DIVIDER                  4.0
#define DIMMER_FIRMWARE_VERSION_MAJOR                2
#define DIMMER_FIRMWARE_VERSION_MINOR                1

#include "./firmware/2.1.x/dimmer_commands.h"

#elif DIMMER_FIRMWARE_VERSION >= 0x020200

#if defined(I2C_OVER_UART_SLAVE_RESPONSE_MASTER_TRANSMIT) && I2C_OVER_UART_SLAVE_RESPONSE_MASTER_TRANSMIT
#error requires I2C_OVER_UART_SLAVE_RESPONSE_MASTER_TRANSMIT=0
#endif

#define DIMMER_I2C_SLAVE                            1
#define DIMMER_FIRMWARE_VERSION_MAJOR                2
#define DIMMER_FIRMWARE_VERSION_MINOR                2

#include "./firmware/2.2.x/dimmer_commands.h"

#elif DIMMER_FIRMWARE_VERSION >= 0x030000

#if defined(I2C_OVER_UART_SLAVE_RESPONSE_MASTER_TRANSMIT) && I2C_OVER_UART_SLAVE_RESPONSE_MASTER_TRANSMIT
#error requires I2C_OVER_UART_SLAVE_RESPONSE_MASTER_TRANSMIT=0
#endif

#define DIMMER_FIRMWARE_VERSION_MAJOR                3
#define DIMMER_FIRMWARE_VERSION_MINOR                0

#include "./firmware/3.0.x/dimmer_commands.h"

#endif

#include "./firmware/dimmer_commands.h"
