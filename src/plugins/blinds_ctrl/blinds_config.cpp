
/**
 * Author: sascha_lammers@gmx.de
 */

#include <Configuration.h>
#include <kfc_fw_config.h>
#include "blinds_defines.h"

namespace KFCConfigurationClasses {

    Plugins::Blinds::BlindsConfig_t::BlindsConfig_t() :
        channels(), pins{IOT_BLINDS_CTRL_M1_PIN, IOT_BLINDS_CTRL_M2_PIN, IOT_BLINDS_CTRL_M3_PIN, IOT_BLINDS_CTRL_M4_PIN}
    {
    }

    void Plugins::Blinds::defaults()
    {
        BlindsConfig_t cfg = {};
        cfg.channels[0].operation.close_delay = 60; // wait 60 seconds after channel 1 closed before closing channel 0
        BlindsConfigOperation_t::set_enum_close_state(cfg.channels[1].operation, BlindsStateType::OPEN); // require channel 0 to be open for opening and closing channel 1
        BlindsConfigOperation_t::set_enum_open_state(cfg.channels[1].operation, BlindsStateType::OPEN);
        setConfig(cfg);
        setChannel0Name(FSPGM(Turn));
        setChannel1Name(FSPGM(Move));
    }

}
