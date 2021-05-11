/**
 * Author: sascha_lammers@gmx.de
 */

#include <Configuration.h>
#include <kfc_fw_config.h>

#ifndef IOT_CLOCK_ALARM_COLOR
#define IOT_CLOCK_ALARM_COLOR 0x880000
#endif

#ifndef IOT_CLOCK_ALARM_FLASHING_SPEED
#define IOT_CLOCK_ALARM_FLASHING_SPEED 250
#endif

namespace KFCConfigurationClasses {

    Plugins::ClockConfig::RainbowMultiplier_t::RainbowMultiplier_t() :
        value(1.23),
        min(2.5),
        max(11.0),
        incr(0.00326)
    {}

    Plugins::ClockConfig::RainbowMultiplier_t::RainbowMultiplier_t(float a, float b, float c, float d) :
        value(a),
        min(b),
        max(c),
        incr(d)
    {}

    Plugins::ClockConfig::RainbowColor_t::RainbowColor_t() :
        min(0x000000),
        factor(0xffffff),
        red_incr(0.1),
        green_incr(0.25),
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
        enabled(false),
        fading_time(kDefaultValueFor_fading_time),
#if IOT_CLOCK_HAVE_POWER_LIMIT
        power_limit(kDefaultValueFor_power_limit),
#endif
        brightness(kDefaultValueFor_brightness),
        auto_brightness(kDefaultValueFor_auto_brightness),
#if !IOT_LED_MATRIX
        blink_colon_speed(kDefaultValueFor_blink_colon_speed),
#endif
        flashing_speed(kDefaultValueFor_flashing_speed),
        motion_auto_off(kDefaultValueFor_motion_auto_off),
        power({static_cast<uint16_t>(79.7617 * kPowerNumLeds), static_cast<uint16_t>(79.9648 * kPowerNumLeds), static_cast<uint16_t>(79.6055 * kPowerNumLeds), static_cast<uint16_t>(4.0586 * kPowerNumLeds)}),
        protection( { { 45, 60 }, 70, 25 } ),
        rainbow{ RainbowMultiplier_t(), RainbowColor_t(), 30 },
        alarm{ { IOT_CLOCK_ALARM_COLOR }, IOT_CLOCK_ALARM_FLASHING_SPEED },
        fading{ fading.kDefaultValueFor_speed, fading.kDefaultValueFor_delay, 0xffffff },
        fire(),
        interleaved({ 2, 0, 60000 })
    {
    }

    void Plugins::Clock::defaults()
    {
        ClockConfig_t cfg = {};
        setConfig(cfg);
        KFCFS.remove(FSPGM(iot_clock_save_state_file));
    }

    // WebUI
    const __FlashStringHelper *Plugins::ClockConfig::ClockConfig_t::getAnimationNames() {
        return F( \
            "Solid," \
            "Rainbow," \
            "Flash," \
            "Color Fade,"
            "Fire," \
            IF_IOT_LED_MATRIX_VIS(
                "Visualizer," \
            ) \
            "Interleaved"
        );
    }

    // MQTT
    const __FlashStringHelper *Plugins::ClockConfig::ClockConfig_t::getAnimationNamesJsonArray() {
        return F("[" \
            "\042Solid\042," \
            "\042Rainbow\042," \
            "\042Flash\042," \
            "\042Color Fade\042," \
            IF_IOT_LED_MATRIX_VIS(
                "\042Visualizer\042," \
            ) \
            "\042Fire\042," \
            "\042Interleaved\042" \
            IF_IOT_CLOCK(
                ",\042Colon: Solid\042," \
                "\042Colon: Blink Slowly\042," \
                "\042Colon: Blink Fast\042") \
            "]");
    }

    const __FlashStringHelper *Plugins::ClockConfig::ClockConfig_t::getAnimationName(AnimationType type) {
        switch(type) {
            case AnimationType::RAINBOW:
                return F("Rainbow");
            case AnimationType::FLASHING:
                return F("Flash");
            case AnimationType::FADING:
                return F("Color Fade");
            case AnimationType::FIRE:
                return F("Fire");
        IF_IOT_LED_MATRIX_VIS(
            case AnimationType::VISUALIZER:
                return F("Visualizer");
        )
            case AnimationType::INTERLEAVED:
                return F("Interleaved");
#if !IOT_LED_MATRIX
            case AnimationType::COLON_SOLID:
                return F("Colon: Solid");
            case AnimationType::COLON_BLINK_SLOWLY:
                return F("Colon: Blink Slowly");
            case AnimationType::COLON_BLINK_FAST:
                return F("Colon: Blink Fast");
#endif
            default:
            case AnimationType::SOLID:
                break;
        }
        return F("Solid");
    }


}
