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
        cooling(80),
        sparking(120),
        speed(50),
        orientation(cast_int_orientation(Orientation::VERTICAL)),
        invert_direction(false)
    {}

    Plugins::ClockConfig::ClockConfig_t::ClockConfig_t() :
        solid_color(0x00ff00),
        animation(cast_int_animation(AnimationType::NONE)),
#if IOT_LED_MATRIX
        initial_state(cast_int_initial_state(InitialStateType::OFF)),
#else
        time_format_24h(true),
#endif
        brightness(128),
        auto_brightness(-1),
#if !IOT_LED_MATRIX
        blink_colon_speed(1000),
#endif
        flashing_speed(150),
        protection( { { 55, 85 }, 75} ),
        rainbow{ RainbowMultiplier_t(), RainbowColor_t(), 30 },
        alarm{ { 0xff0000 }, 250 },
        fading{ .5f, 2, 0xffffff }
#if IOT_LED_MATRIX
        ,
        fire(),
        skip_rows({ 2, 0, 60000 })
#endif
    {
    }

    void Plugins::Clock::defaults()
    {
        ClockConfig_t cfg = {};
        setConfig(cfg);
    }

}
