/**
 * Author: sascha_lammers@gmx.de
 */

// https://raw.githubusercontent.com/sascha432/trailing_edge_dimmer/276f72a01a5fae1249d6984745e8a4176309a28d/src/dimmer_reg_mem.h

#pragma once

#ifndef __attribute__packed__
#define __attribute__packed__           __attribute__((packed))
#endif

typedef struct __attribute__packed__ {
    uint8_t command;
    int8_t read_length;
    int8_t status;
} register_mem_command_t;

typedef struct {
    uint8_t restore_level: 1;
    uint8_t report_metrics: 1;
    uint8_t temperature_alert: 1;
    uint8_t frequency_low: 1;
    uint8_t frequency_high: 1;
    uint8_t ___reserved: 3;
} config_options_t;

typedef struct __attribute__packed__ {
    union {
        config_options_t bits;
        uint8_t options;
    };
    uint8_t max_temp;
    float fade_in_time;
    uint8_t temp_check_interval;
    float linear_correction_factor;
    uint8_t zero_crossing_delay_ticks;
    uint16_t minimum_on_time_ticks;
    uint16_t minimum_off_time_ticks;
    float internal_1_1v_ref;
    int8_t int_temp_offset;
    int8_t ntc_temp_offset;
    uint8_t report_metrics_interval;
} register_mem_cfg_t;

typedef struct __attribute__packed__ {
    uint8_t frequency_low;
    uint8_t frequency_high;
    uint8_t zc_misfire;
} register_mem_errors_t;

typedef struct __attribute__packed__ {
    int16_t from_level;
    int8_t channel;
    int16_t to_level;
    float time;
    register_mem_command_t cmd;
    uint16_t level[8];
    float temp;
    uint16_t vcc;
    register_mem_cfg_t cfg;
    uint16_t version;
    register_mem_errors_t errors;
    uint8_t address;
} register_mem_t;

typedef union __attribute__packed__ {
    register_mem_t data;
    uint8_t raw[sizeof(register_mem_t)];
} register_mem_union_t;

typedef struct __attribute__packed__ {
    uint16_t vcc;
    float frequency;
    float ntc_temp;
    float int_temp;
    bool has_vcc() const;
    bool has_int_temp() const;
    bool has_ntc_temp() const;
    bool has_frequency() const;
    float get_vcc() const;
    float get_int_temp() const;
    float get_ntc_temp() const;
    float get_freqency() const;
} register_mem_metrics_t;

struct __attribute_packed__ dimmer_metrics_t
{
    uint8_t temp_check_value;
    register_mem_metrics_t metrics;
};

typedef struct __attribute__packed__ {
    uint32_t write_cycle;
    uint16_t write_position;
    uint8_t bytes_written;      // might be 0 if the data has not been changed
} dimmer_eeprom_written_t;

typedef struct __attribute__packed__ {
    uint8_t channel;
    uint16_t level;
} dimmer_fading_complete_event_t;

struct __attribute_packed__ dimmer_version_t {
    union __attribute_packed__ {
        uint16_t _word;
        struct __attribute_packed__ {
            uint16_t revision: 5;
            uint16_t minor: 5;
            uint16_t major: 6;
        };
    };
    operator uint16_t() const {
        return _word;
    }
    operator bool() const {
        return _word != 0;
    }

    dimmer_version_t() = default;
    dimmer_version_t(uint16_t version) : _word(version) {}
    dimmer_version_t(const uint8_t _major, const uint8_t _minor, const uint8_t _revision) : revision(_revision), minor(_minor), major(_major) {}
};

struct __attribute_packed__ dimmer_config_info_t
{
    uint16_t max_levels;
    uint8_t channel_count;
    uint8_t cfg_start_address;
    uint8_t length;
};

struct __attribute_packed__ dimmer_command_fade_t {
    uint8_t register_address;
    int16_t from_level;
    int8_t channel;
    int16_t to_level;
    float time;
    uint8_t command;

    dimmer_command_fade_t() = default;

    dimmer_command_fade_t(uint8_t p_channel, int16_t p_from_level, int16_t p_to_level, float time) :
        register_address(DIMMER_REGISTER_FROM_LEVEL),
        from_level(p_from_level),
        channel(p_channel),
        to_level(p_to_level),
        time(time),
        command(DIMMER_COMMAND_FADE)
    {}
};

struct __attribute_packed__ dimmer_over_temperature_event_t
{
    uint8_t current_temp;
    uint8_t max_temp;
};

namespace Dimmer {

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

    inline static bool isValidFrequency(float frequency) {
        return !isnan(frequency) && frequency != 0;
    }

};


inline bool register_mem_metrics_t::has_vcc() const {
    return Dimmer::isValidVoltage(vcc);
}

inline bool register_mem_metrics_t::has_int_temp() const {
    return Dimmer::isValidTemperature(int_temp);
}

inline bool register_mem_metrics_t::has_ntc_temp() const {
    return Dimmer::isValidTemperature(ntc_temp);
}

inline bool register_mem_metrics_t::has_frequency() const {
    return Dimmer::isValidFrequency(frequency);
}

inline float register_mem_metrics_t::get_vcc() const {
    if (has_vcc()) {
        return vcc / 1000.0;
    }
    return NAN;
}

inline float register_mem_metrics_t::get_int_temp() const {
    if (has_int_temp()) {
        return int_temp;
    }
    return NAN;
}

inline float register_mem_metrics_t::get_ntc_temp() const {
    if (has_ntc_temp()) {
        return ntc_temp;
    }
    return NAN;
}

inline float register_mem_metrics_t::get_freqency() const {
    if (has_frequency()) {
        return frequency;
    }
    return NAN;

}
