/**
 * Author: sascha_lammers@gmx.de
 */

#include <Configuration.h>
#include <kfc_fw_config.h>
#include <PinMonitor.h>

namespace KFCConfigurationClasses {

    Plugins::DimmerConfig::DimmerConfig_t::DimmerConfig_t() :
            fw({}),
        #if IOT_ATOMIC_SUN_V2
        #ifdef IOT_ATOMIC_SUN_CHANNEL_WW1
            channel_mapping{ IOT_ATOMIC_SUN_CHANNEL_WW1, IOT_ATOMIC_SUN_CHANNEL_WW2, IOT_ATOMIC_SUN_CHANNEL_CW1, IOT_ATOMIC_SUN_CHANNEL_CW2 },
        #else
            channel_mapping{ 0, 1, 2, 3 },
        #endif
            on_fadetime(12.5),
            off_fadetime(12.5),
        #else
            on_fadetime(4.5),
            off_fadetime(4.5),
        #endif
            lp_fadetime(3.5),
        #if IOT_DIMMER_MODULE_HAS_BUTTONS
            config_valid(kDefaultValueFor_config_valid),
            off_delay(kDefaultValueFor_off_delay),
            off_delay_signal(kDefaultValueFor_off_delay_signal),
            pin_ch0_down_inverted(kDefaultValueFor_pin_ch0_down_inverted),
            pin_ch0_up(kDefaultValueFor_pin_ch0_up),
            pin_ch0_up_inverted(kDefaultValueFor_pin_ch0_up_inverted),
            single_click_time(kDefaultValueFor_single_click_time),
            longpress_time(kDefaultValueFor_longpress_time),
            min_brightness(kDefaultValueFor_min_brightness),
            pin_ch0_down(kDefaultValueFor_pin_ch0_down),
            shortpress_steps(kDefaultValueFor_shortpress_steps),
            longpress_max_brightness(kDefaultValueFor_longpress_max_brightness),
            longpress_min_brightness(kDefaultValueFor_longpress_min_brightness),
            max_brightness(kDefaultValueFor_max_brightness),
            shortpress_time(kDefaultValueFor_shortpress_time)
        #else
            config_valid(kDefaultValueFor_config_valid)
        #endif
    {
    }

    void Plugins::Dimmer::defaults()
    {
        DimmerConfig_t cfg = {};
        setConfig(cfg);
    }

}
