/**
 * Author: sascha_lammers@gmx.de
 */

#include <Configuration.h>
#include <kfc_fw_config.h>
#include <PinMonitor.h>

namespace KFCConfigurationClasses {

    Plugins::DimmerConfig::DimmerConfig_t::DimmerConfig_t() : fw(), on_off_fade_time(12.5), fade_time(4.5), config_valid(false)
        #if IOT_ATOMIC_SUN_V2
        ,
        #ifdef IOT_ATOMIC_SUN_CHANNEL_WW1
            channel_mapping{ IOT_ATOMIC_SUN_CHANNEL_WW1, IOT_ATOMIC_SUN_CHANNEL_WW2, IOT_ATOMIC_SUN_CHANNEL_CW1, IOT_ATOMIC_SUN_CHANNEL_CW2 }
        #else
            channel_mapping{ 0, 1, 2, 3 }
        #endif
        #endif
        #if IOT_DIMMER_MODULE_HAS_BUTTONS
        ,
            shortpress_time(275),
            longpress_time(750),
            single_click_time(1000),
            min_brightness(20),
            shortpress_steps(15),
            longpress_max_brightness(85),
            longpress_min_brightness(33),
            longpress_fadetime(3.5),

            pins_inverted(IOT_SWITCH_PRESSED_STATE == PinMonitor::ActiveStateType::INVERTED ? 0xff : 0x00),
        #if IOT_SENSOR_HAVE_HLW8012
            pins{ D2, D7 }
        #else
            pins{ D6, D7 }
        #endif
        #endif
    {
    }

    void Plugins::Dimmer::defaults()
    {
        DimmerConfig_t cfg = {};
        setConfig(cfg);
    }

}
