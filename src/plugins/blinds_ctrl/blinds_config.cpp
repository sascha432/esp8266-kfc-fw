
/**
 * Author: sascha_lammers@gmx.de
 */

#include <Configuration.h>
#include <kfc_fw_config.h>
#include "blinds_defines.h"

namespace KFCConfigurationClasses {

    Plugins::Blinds::BlindsConfig_t::BlindsConfig_t() :
        channels(), pins{IOT_BLINDS_CTRL_M1_PIN, IOT_BLINDS_CTRL_M2_PIN, IOT_BLINDS_CTRL_M3_PIN, IOT_BLINDS_CTRL_M4_PIN, IOT_BLINDS_CTRL_MULTIPLEXER_PIN, IOT_BLINDS_CTRL_DAC_PIN},
        pwm_frequency(kDefaultValueFor_pwm_frequency),
        adc_recovery_time(kDefaultValueFor_adc_recovery_time),
        adc_read_interval(kDefaultValueFor_adc_read_interval),
        adc_recoveries_per_second(kDefaultValueFor_adc_recoveries_per_second),
        adc_multiplexer(kDefaultValueFor_adc_recovery_time),
        adc_offset(kDefaultValueFor_adc_offset),
        pwm_softstart_time(kDefaultValueFor_pwm_softstart_time)
    {
    }


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
        cfg.open[0].type = OperationType::OPEN_CHANNEL0;
        cfg.open[1].type = OperationType::OPEN_CHANNEL0_FOR_CHANNEL1;
        cfg.open[2].type = OperationType::OPEN_CHANNEL1;

        cfg.close[0].type = OperationType::OPEN_CHANNEL0;
        cfg.close[1].type = OperationType::OPEN_CHANNEL0_FOR_CHANNEL1;
        cfg.close[2].type = OperationType::CLOSE_CHANNEL1_FOR_CHANNEL0;
        cfg.close[2].delay = 20;
        cfg.close[3].type = OperationType::CLOSE_CHANNEL1;
        cfg.close[3].delay = 20;
        cfg.close[4].type = OperationType::CLOSE_CHANNEL0;

        setConfig(cfg);
        setChannel0Name(FSPGM(Turn));
        setChannel1Name(FSPGM(Move));
    }

}
