/**
 * Author: sascha_lammers@gmx.de
 */

#include <Configuration.h>
#include <kfc_fw_config.h>
#include <PinMonitor.h>

namespace KFCConfigurationClasses {

    Plugins::DimmerConfig::DimmerConfig_t::DimmerConfig_t() :
            version(),
            // info({}),
            cfg({}),
        #if IOT_ATOMIC_SUN_V2
        #ifdef IOT_ATOMIC_SUN_CHANNEL_WW1
            channel_mapping{ IOT_ATOMIC_SUN_CHANNEL_WW1, IOT_ATOMIC_SUN_CHANNEL_WW2, IOT_ATOMIC_SUN_CHANNEL_CW1, IOT_ATOMIC_SUN_CHANNEL_CW2 },
        #else
            channel_mapping{ 0, 1, 2, 3 },
        #endif
            level{},
            on_fadetime(12.5),
            off_fadetime(12.5),
        #else
            level{},
            on_fadetime(4.5),
            off_fadetime(4.5),
        #endif
            lp_fadetime(3.5)
        #if IOT_DIMMER_MODULE_HAS_BUTTONS
            ,off_delay(kDefaultValueFor_off_delay),
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
        #endif
    {
    }

    void Plugins::Dimmer::defaults()
    {
        DimmerConfig_t cfg = {};
        // std::fill(std::begin(cfg.level.to), std::end(cfg.level.to), IOT_DIMMER_MODULE_MAX_BRIGHTNESS);
        // for(auto &value: cfg.level.to) {
        //     value = IOT_DIMMER_MODULE_MAX_BRIGHTNESS;
        // }
        for(uint8_t i = 0; i < IOT_DIMMER_MODULE_CHANNELS; i++) {
            cfg.level.to[i] = IOT_DIMMER_MODULE_MAX_BRIGHTNESS;
        }
        setConfig(cfg);
    }

}
