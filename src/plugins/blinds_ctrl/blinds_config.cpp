
/**
 * Author: sascha_lammers@gmx.de
 */

#include <Configuration.h>
#include <kfc_fw_config.h>
#include "blinds_defines.h"

namespace KFCConfigurationClasses {

#ifndef  _MSC_VER
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverflow"
#endif

    Plugins::Blinds::BlindsConfig_t::BlindsConfig_t() :
        channels(), pins{IOT_BLINDS_CTRL_M1_PIN, IOT_BLINDS_CTRL_M2_PIN, IOT_BLINDS_CTRL_M3_PIN, IOT_BLINDS_CTRL_M4_PIN, IOT_BLINDS_CTRL_MULTIPLEXER_PIN, IOT_BLINDS_CTRL_DAC_PIN},
        pwm_frequency(kDefaultValueFor_pwm_frequency),
        adc_recovery_time(kDefaultValueFor_adc_recovery_time),
        adc_read_interval(kDefaultValueFor_adc_read_interval),
        adc_recoveries_per_second(kDefaultValueFor_adc_recoveries_per_second),
        adc_multiplexer(kDefaultValueFor_adc_recovery_time),
        adc_offset(kDefaultValueFor_adc_offset),
        pwm_softstart_time(kDefaultValueFor_pwm_softstart_time),
        play_tone_channel(kDefaultValueFor_play_tone_channel),
        tone_frequency(kDefaultValueFor_tone_frequency),
        tone_pwm_value(kDefaultValueFor_tone_pwm_value)             // warning: large integer implicitly truncated to unsigned type [-Woverflow]
                                                                    // value is 150U, uint32_t: 10, 0U-1023U
    {
    }

#ifndef  _MSC_VER
#pragma GCC diagnostic pop
#endif

    Plugins::Blinds::BlindsConfigChannel_t::BlindsConfigChannel_t() :
        current_limit_mA(kDefaultValueFor_current_limit_mA),
        dac_pwm_value(kDefaultValueFor_dac_pwm_value),
        pwm_value(kDefaultValueFor_pwm_value),
        current_avg_period_us(kDefaultValueFor_current_avg_period_us),
        open_time_ms(kDefaultValueFor_open_time_ms),
        close_time_ms(kDefaultValueFor_close_time_ms)
    {
    }

    void Plugins::Blinds::defaults()
    {
        BlindsConfig_t cfg = {};

        cfg.open[0]._set_enum_action(OperationType::OPEN_CHANNEL0);
        cfg.open[1]._set_enum_action(OperationType::OPEN_CHANNEL0_FOR_CHANNEL1);
        cfg.open[2]._set_enum_action(OperationType::OPEN_CHANNEL1);

        // cfg.close[0]._set_enum_action(OperationType::OPEN_CHANNEL0);
        cfg.close[0]._set_enum_action(OperationType::OPEN_CHANNEL0_FOR_CHANNEL1);

        cfg.close[1]._set_enum_action(OperationType::CLOSE_CHANNEL1_FOR_CHANNEL0);
        cfg.close[1].delay = 30;
        cfg.close[1].relative_delay = 0;
        cfg.close[1]._set_enum_play_tone(PlayToneType::INTERVAL_SPEED_UP);

        cfg.close[2]._set_enum_action(OperationType::CLOSE_CHANNEL1);
        cfg.close[2].delay = 30;
        cfg.close[2].relative_delay = 0;
        cfg.close[2]._set_enum_play_tone(PlayToneType::INTERVAL_SPEED_UP);

        cfg.close[3]._set_enum_action(OperationType::CLOSE_CHANNEL0);

        setConfig(cfg);
        setChannel0Name(FSPGM(Turn));
        setChannel1Name(FSPGM(Move));
    }

}
