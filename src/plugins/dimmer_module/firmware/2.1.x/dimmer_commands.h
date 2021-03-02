/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "dimmer_protocol.h"
#include "dimmer_reg_mem.h"

#ifndef DIMMER_I2C_MASTER_ADDRESS
#define DIMMER_I2C_MASTER_ADDRESS           (DIMMER_I2C_ADDRESS + 1)
#endif

#define DIMMER_EVENT_METRICS_REPORT         DIMMER_METRICS_REPORT
#define DIMMER_EVENT_TEMPERATURE_ALERT      DIMMER_TEMPERATURE_ALERT
#define DIMMER_EVENT_FADING_COMPLETE        DIMMER_FADING_COMPLETE
#define DIMMER_EVENT_EEPROM_WRITTEN         DIMMER_EEPROM_WRITTEN
#define DIMMER_EVENT_FREQUENCY_WARNING      DIMMER_FREQUENCY_WARNING

namespace Dimmer {

    using VersionType = dimmer_version_t;

    struct MetricsType : dimmer_metrics_t {
        MetricsType() : dimmer_metrics_t({}) {
            invalidate();
        }
        operator bool() const {
            return temp_check_value == 0;
        }
        void invalidate() {
            temp_check_value = 0xff;
        }
        void validate() {
            temp_check_value = 0;
        }
    };

    struct __attribute_packed__ Config
    {
        dimmer_version_t version;
        dimmer_config_info_t info;
        register_mem_cfg_t config;
        static constexpr size_t kVersionSize = sizeof(dimmer_version_t);

        Config() : version(), info({DIMMER_MAX_LEVEL, IOT_DIMMER_MODULE_CHANNELS, DIMMER_REGISTER_OPTIONS, sizeof(register_mem_cfg_t)}), config() {}
    };

    static constexpr const uint8_t kRequestVersion[] = { DIMMER_REGISTER_READ_LENGTH, 0x02, DIMMER_REGISTER_VERSION };

    static constexpr int16_t kInvalidTemperature = INT16_MIN;

    inline static bool isValidVoltage(uint16_t value) {
        return value != 0;
    }

    inline static bool isValidTemperature(float value) {
        return !isnan(value);
    }

    inline static bool isValidTemperature(int16_t value) {
        return value != kInvalidTemperature;
    }

    inline static bool isValidTemperature(uint8_t value) {
        return value != UINT8_MAX;
    }

};
