/**
 * Author: sascha_lammers@gmx.de
 */

// https://raw.githubusercontent.com/sascha432/trailing_edge_dimmer/276f72a01a5fae1249d6984745e8a4176309a28d/src/dimmer_protocol.h

#pragma once

#ifndef DIMMER_I2C_ADDRESS
#define DIMMER_I2C_ADDRESS                  0x17
#endif

#if DIMMER_I2C_SLAVE

#define DIMMER_REGISTER_START_ADDR          0x80
#define DIMMER_REGISTER_FROM_LEVEL          (DIMMER_REGISTER_START_ADDR + offsetof(register_mem_t, from_level))
#define DIMMER_REGISTER_CHANNEL             (DIMMER_REGISTER_START_ADDR + offsetof(register_mem_t, channel))
#define DIMMER_REGISTER_TO_LEVEL            (DIMMER_REGISTER_START_ADDR + offsetof(register_mem_t, to_level))
#define DIMMER_REGISTER_TIME                (DIMMER_REGISTER_START_ADDR + offsetof(register_mem_t, time))
#define DIMMER_REGISTER_COMMAND             (DIMMER_REGISTER_START_ADDR + offsetof(register_mem_t, cmd) + offsetof(register_mem_command_t, command))
#define DIMMER_REGISTER_READ_LENGTH         (DIMMER_REGISTER_START_ADDR + offsetof(register_mem_t, cmd) + offsetof(register_mem_command_t, read_length))
#define DIMMER_REGISTER_COMMAND_STATUS      (DIMMER_REGISTER_START_ADDR + offsetof(register_mem_t, cmd) + offsetof(register_mem_command_t, status))
#define DIMMER_REGISTER_CHANNELS_START      (DIMMER_REGISTER_START_ADDR + offsetof(register_mem_t, level))
#define DIMMER_REGISTER_CH0_LEVEL           (DIMMER_REGISTER_CHANNELS_START)
#define DIMMER_REGISTER_CH1_LEVEL           (DIMMER_REGISTER_CHANNELS_START + sizeof(register_mem_t().level[0]) * 1)
#define DIMMER_REGISTER_CH2_LEVEL           (DIMMER_REGISTER_CHANNELS_START + sizeof(register_mem_t().level[0]) * 2)
#define DIMMER_REGISTER_CH3_LEVEL           (DIMMER_REGISTER_CHANNELS_START + sizeof(register_mem_t().level[0]) * 3)
#define DIMMER_REGISTER_CH4_LEVEL           (DIMMER_REGISTER_CHANNELS_START + sizeof(register_mem_t().level[0]) * 4)
#define DIMMER_REGISTER_CH5_LEVEL           (DIMMER_REGISTER_CHANNELS_START + sizeof(register_mem_t().level[0]) * 5)
#define DIMMER_REGISTER_CH6_LEVEL           (DIMMER_REGISTER_CHANNELS_START + sizeof(register_mem_t().level[0]) * 6)
#define DIMMER_REGISTER_CH7_LEVEL           (DIMMER_REGISTER_CHANNELS_START + sizeof(register_mem_t().level[0]) * 7)
#define DIMMER_REGISTER_CHANNELS_END        (DIMMER_REGISTER_CHANNELS_START + sizeof(register_mem_t().level))
#define DIMMER_REGISTER_TEMP                (DIMMER_REGISTER_START_ADDR + offsetof(register_mem_t, temp))
#define DIMMER_REGISTER_VCC                 (DIMMER_REGISTER_START_ADDR + offsetof(register_mem_t, vcc))
#define DIMMER_REGISTER_OPTIONS             (DIMMER_REGISTER_START_ADDR + offsetof(register_mem_t, cfg) + offsetof(register_mem_cfg_t, options))
#define DIMMER_REGISTER_MAX_TEMP            (DIMMER_REGISTER_START_ADDR + offsetof(register_mem_t, cfg) + offsetof(register_mem_cfg_t, max_temp))
#define DIMMER_REGISTER_FADE_IN_TIME        (DIMMER_REGISTER_START_ADDR + offsetof(register_mem_t, cfg) + offsetof(register_mem_cfg_t, fade_in_time))
#define DIMMER_REGISTER_TEMP_CHECK_INT      (DIMMER_REGISTER_START_ADDR + offsetof(register_mem_t, cfg) + offsetof(register_mem_cfg_t, temp_check_interval))
#define DIMMER_REGISTER_LC_FACTOR           (DIMMER_REGISTER_START_ADDR + offsetof(register_mem_t, cfg) + offsetof(register_mem_cfg_t, linear_correction_factor))
#define DIMMER_REGISTER_ZC_DELAY_TICKS      (DIMMER_REGISTER_START_ADDR + offsetof(register_mem_t, cfg) + offsetof(register_mem_cfg_t, zero_crossing_delay_ticks))
#define DIMMER_REGISTER_MIN_ON_TIME_TICKS   (DIMMER_REGISTER_START_ADDR + offsetof(register_mem_t, cfg) + offsetof(register_mem_cfg_t, minimum_on_time_ticks))
#define DIMMER_REGISTER_ADJ_HALFWAVE_TICKS  (DIMMER_REGISTER_START_ADDR + offsetof(register_mem_t, cfg) + offsetof(register_mem_cfg_t, adjust_halfwave_time_ticks))
#define DIMMER_REGISTER_INT_1_1V_REF        (DIMMER_REGISTER_START_ADDR + offsetof(register_mem_t, cfg) + offsetof(register_mem_cfg_t, internal_1_1v_ref))
#define DIMMER_REGISTER_INT_TEMP_OFS        (DIMMER_REGISTER_START_ADDR + offsetof(register_mem_t, cfg) + offsetof(register_mem_cfg_t, int_temp_offset))
#define DIMMER_REGISTER_METRICS_INT         (DIMMER_REGISTER_START_ADDR + offsetof(register_mem_t, cfg) + offsetof(register_mem_cfg_t, report_metrics_max_interval))
#define DIMMER_REGISTER_VERSION             (DIMMER_REGISTER_START_ADDR + offsetof(register_mem_t, version))
#define DIMMER_REGISTER_ADDRESS             (DIMMER_REGISTER_START_ADDR + offsetof(register_mem_t, address))
#define DIMMER_REGISTER_END_ADDR            (DIMMER_REGISTER_START_ADDR + sizeof(register_mem_t))

#else

#define DIMMER_REGISTER_FROM_LEVEL          0x80
#define DIMMER_REGISTER_CHANNEL             0x82
#define DIMMER_REGISTER_TO_LEVEL            0x83
#define DIMMER_REGISTER_TIME                0x85
#define DIMMER_REGISTER_COMMAND             0x89
#define DIMMER_REGISTER_READ_LENGTH         0x8A
#define DIMMER_REGISTER_COMMAND_STATUS      0x8B
#define DIMMER_REGISTER_CH0_LEVEL           0x8C
#define DIMMER_REGISTER_CH1_LEVEL           0x8E
#define DIMMER_REGISTER_CH2_LEVEL           0x90
#define DIMMER_REGISTER_CH3_LEVEL           0x92
#define DIMMER_REGISTER_CH4_LEVEL           0x94
#define DIMMER_REGISTER_CH5_LEVEL           0x96
#define DIMMER_REGISTER_CH6_LEVEL           0x98
#define DIMMER_REGISTER_CH7_LEVEL           0x9A
#define DIMMER_REGISTER_TEMP                0x9C
#define DIMMER_REGISTER_VCC                 0xA0
#define DIMMER_REGISTER_OPTIONS             0xA2
#define DIMMER_REGISTER_MAX_TEMP            0xA3
#define DIMMER_REGISTER_FADE_IN_TIME        0xA4
#define DIMMER_REGISTER_TEMP_CHECK_INT      0xA8
#define DIMMER_REGISTER_LC_FACTOR           0xA9
#define DIMMER_REGISTER_ZC_DELAY_TICKS      0xAD
#define DIMMER_REGISTER_MIN_ON_TIME_TICKS   0xAE
#define DIMMER_REGISTER_ADJ_HALFWAVE_TICKS  0xB0
#define DIMMER_REGISTER_INT_1_1V_REF        0xB2
#define DIMMER_REGISTER_INT_TEMP_OFS        0xB6
#define DIMMER_REGISTER_NTC_TEMP_OFS        0xB7
#define DIMMER_REGISTER_METRICS_INT         0xB8
#define DIMMER_REGISTER_VERSION             0xB9
#define DIMMER_REGISTER_ERR_FREQ_LOW        0xBB
#define DIMMER_REGISTER_ERR_FREQ_HIGH       0xBC
#define DIMMER_REGISTER_ERR_ZC_MISFIRE      0xBD
#define DIMMER_REGISTER_ADDRESS             0xBE
#define DIMMER_REGISTER_END_ADDR            0xBF

#endif

// first byte in master mode indicates the data that was sent
#define DIMMER_METRICS_REPORT               0xf0
#define DIMMER_TEMPERATURE_ALERT            0xf1
#define DIMMER_FADING_COMPLETE              0xf2
#define DIMMER_EEPROM_WRITTEN               0xf3
#define DIMMER_FREQUENCY_WARNING            0xf4

// DIMMER_REGISTER_COMMAND
#define DIMMER_COMMAND_SET_LEVEL            0x10
#define DIMMER_COMMAND_FADE                 0x11
#define DIMMER_COMMAND_READ_CHANNELS        0x12
#define DIMMER_COMMAND_READ_NTC             0x20
#define DIMMER_COMMAND_READ_INT_TEMP        0x21
#define DIMMER_COMMAND_READ_VCC             0x22
#define DIMMER_COMMAND_READ_AC_FREQUENCY    0x23
#define DIMMER_COMMAND_WRITE_EEPROM         0x50
#define DIMMER_COMMAND_RESTORE_FS           0x51
#define DIMMER_COMMAND_READ_TIMINGS         0x52
#define DIMMER_COMMAND_PRINT_INFO           0x53
#define DIMMER_COMMAND_FORCE_TEMP_CHECK     0x54
#define DIMMER_COMMAND_PRINT_METRICS        0x55
#define DIMMER_COMMAND_ZC_TIMINGS_OUTPUT    0x60
#if DEBUG
#define DIMMER_COMMAND_DUMP_MEM             0xee
#define DIMMER_COMMAND_DUMP_MACROS          0xef
#endif

// DIMMER_REGISTER_COMMAND_STATUS
#define DIMMER_COMMAND_STATUS_OK            0
#define DIMMER_COMMAND_STATUS_ERROR         -1

// DIMMER_REGISTER_OPTIONS
#define DIMMER_OPTIONS_RESTORE_LEVEL        0x01
#define DIMMER_OPTIONS_REPORT_TEMP          0x02
#define DIMMER_OPTIONS_TEMP_ALERT_TRIGGERED 0x04

// DIMMER_COMMAND_READ_TIMINGS
// All timings are float and read from DIMMER_REGISTER_TEMP
#define DIMMER_TIMINGS_TMR1_TICKS_PER_US    0x01
#define DIMMER_TIMINGS_TMR2_TICKS_PER_US    0x02
#define DIMMER_TIMINGS_ZC_DELAY_IN_US       0x03
#define DIMMER_TIMINGS_MIN_ON_TIME_IN_US    0x04
#define DIMMER_TIMINGS_ADJ_HW_TIME_IN_US    0x05
