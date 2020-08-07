
/**
 * Author: sascha_lammers@gmx.de
 */

#include <Configuration.h>
#include <kfc_fw_config.h>
#include "blinds_defines.h"

namespace KFCConfigurationClasses {

    Plugins::Blinds::BlindsConfig_t::BlindsConfig_t() :
        channels(), pins{IOT_BLINDS_CTRL_M1_PIN, IOT_BLINDS_CTRL_M2_PIN, IOT_BLINDS_CTRL_M3_PIN, IOT_BLINDS_CTRL_M4_PIN, IOT_BLINDS_CTRL_RSSEL_PIN}, multiplexer(1)
    {
    }

    void Plugins::Blinds::defaults()
    {
        BlindsConfig_t cfg = {};
        cfg.open[0].type = BlindsConfigOperation_t::cast_int_type(OperationType::OPEN_CHANNEL0);
        cfg.open[1].type = BlindsConfigOperation_t::cast_int_type(OperationType::OPEN_CHANNEL0_FOR_CHANNEL1);
        cfg.open[2].type = BlindsConfigOperation_t::cast_int_type(OperationType::OPEN_CHANNEL1);

        cfg.close[0].type = BlindsConfigOperation_t::cast_int_type(OperationType::OPEN_CHANNEL0);
        cfg.close[1].type = BlindsConfigOperation_t::cast_int_type(OperationType::OPEN_CHANNEL0_FOR_CHANNEL1);
        cfg.close[2].type = BlindsConfigOperation_t::cast_int_type(OperationType::CLOSE_CHANNEL1_FOR_CHANNEL0);
        cfg.close[2].delay = 60;
        cfg.close[3].type = BlindsConfigOperation_t::cast_int_type(OperationType::CLOSE_CHANNEL1);
        cfg.close[3].delay = 60;
        cfg.close[4].type = BlindsConfigOperation_t::cast_int_type(OperationType::CLOSE_CHANNEL0);

        setConfig(cfg);
        setChannel0Name(FSPGM(Turn));
        setChannel1Name(FSPGM(Move));
    }

}
