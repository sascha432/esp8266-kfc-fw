/**
 * Author: sascha_lammers@gmx.de
 */

#include <Configuration.h>
#include <kfc_fw_config.h>

namespace KFCConfigurationClasses {

    Plugins::ClockConfig::RainbowMultiplier_t::RainbowMultiplier_t() :
        value(1.23),
        min(0.1),
        max(16.0),
        incr(0.001)
    {}

    Plugins::ClockConfig::RainbowColor_t::RainbowColor_t() :
        min(0x000000),
        factor(0xffffff),
        red_incr(0.1),
        green_incr(0.15),
        blue_incr(0.125)
    {}

    Plugins::ClockConfig::FireAnimation_t::FireAnimation_t() :
        cooling(60),
        sparking(95),
        speed(50),
        orientation(cast_int_orientation(Orientation::VERTICAL)),
        invert_direction(false),
        factor(0xffff00)
    {}

    Plugins::ClockConfig::ClockConfig_t::ClockConfig_t() :
        solid_color(0xff00ff),
        animation(cast_int_animation(AnimationType::SOLID)),
#if IOT_LED_MATRIX
        initial_state(cast_int_initial_state(InitialStateType::RESTORE)),
#else
        initial_state(cast_int_initial_state(InitialStateType::ON)),
        time_format_24h(true),
#endif
        dithering(false),
        standby_led(true),
        fading_time(kDefaultValueFor_fading_time),
        power_limit(kDefaultValueFor_power_limit),
        brightness(kDefaultValueFor_brightness),
        auto_brightness(kDefaultValueFor_auto_brightness),
        blink_colon_speed(kDefaultValueFor_blink_colon_speed),
        flashing_speed(kDefaultValueFor_flashing_speed),
        power({static_cast<uint16_t>(16.3 * 5 * kPowerNumLeds), static_cast<uint16_t>(16.4 * 5 * kPowerNumLeds), static_cast<uint16_t>(16.3 * 5 * kPowerNumLeds), static_cast<uint16_t>(0.83 * 5 * kPowerNumLeds)}),
        protection( { { 55, 70 }, 75} ),
        rainbow{ RainbowMultiplier_t(), RainbowColor_t(), 30 },
        alarm{ { 0xaa0000 }, 250 },
        fading{ fading.kDefaultValueFor_speed, fading.kDefaultValueFor_delay, 0xffffff }
#if IOT_LED_MATRIX
        ,
        fire(),
        interleaved({ 2, 0, 60000 })
#endif
    {
    }

    void Plugins::Clock::defaults()
    {
        ClockConfig_t cfg = {};
        setConfig(cfg);
        KFCFS.remove(FSPGM(iot_clock_save_state_file));
    }

}
