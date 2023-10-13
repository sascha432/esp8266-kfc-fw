/**
 * Author: sascha_lammers@gmx.de
 */

#include <Configuration.hpp>
#include <kfc_fw_config.h>
#include <PinMonitor.h>

namespace KFCConfigurationClasses {

    namespace Plugins {

        namespace DimmerConfigNS {

            DimmerConfig::DimmerConfig_t::DimmerConfig_t() :
                    #if IOT_ATOMIC_SUN_V2
                        #ifdef IOT_ATOMIC_SUN_CHANNEL_WW1
                            channel_mapping{ IOT_ATOMIC_SUN_CHANNEL_WW1, IOT_ATOMIC_SUN_CHANNEL_WW2, IOT_ATOMIC_SUN_CHANNEL_CW1, IOT_ATOMIC_SUN_CHANNEL_CW2 },
                        #else
                            channel_mapping{ 0, 1, 2, 3 },
                        #endif
                    #endif
                    #if IOT_DIMMER_MODULE_CHANNELS
                        level{},
                    #endif
                    #if IOT_DIMMER_MODULE_HAS_BUTTONS
                        pin_ch0_down(kDefaultValueFor_pin_ch0_down),
                        pin_ch0_down_inverted(kDefaultValueFor_pin_ch0_down_inverted),
                        pin_ch0_up(kDefaultValueFor_pin_ch0_up),
                        pin_ch0_up_inverted(kDefaultValueFor_pin_ch0_up_inverted),
                        single_click_time(kDefaultValueFor_single_click_time),
                        longpress_time(kDefaultValueFor_longpress_time),
                        min_brightness(kDefaultValueFor_min_brightness),
                        shortpress_steps(kDefaultValueFor_shortpress_steps),
                        longpress_max_brightness(kDefaultValueFor_longpress_max_brightness),
                        longpress_min_brightness(kDefaultValueFor_longpress_min_brightness),
                        max_brightness(kDefaultValueFor_max_brightness),
                        shortpress_time(kDefaultValueFor_shortpress_time),
                        lp_fadetime(kDefaultValueFor_lp_fadetime),
                    #else
                        min_brightness(kDefaultValueFor_min_brightness),
                        max_brightness(kDefaultValueFor_max_brightness),
                    #endif
                    on_fadetime(kDefaultValueFor_on_fadetime),
                    off_fadetime(kDefaultValueFor_off_fadetime)
            {
                #if IOT_DIMMER_MODULE_CHANNELS
                    for(uint8_t i = 0; i < IOT_DIMMER_MODULE_CHANNELS; i++) {
                        level.to[i] = IOT_DIMMER_MODULE_MAX_BRIGHTNESS;
                    }
                #endif
            }

            void Dimmer::defaults()
            {
                setConfig(DimmerConfig_t());
            }

        }

    }

}
