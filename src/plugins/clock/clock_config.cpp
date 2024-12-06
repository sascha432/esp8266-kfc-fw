/**
 * Author: sascha_lammers@gmx.de
 */

#include <Configuration.hpp>
#include <kfc_fw_config.h>

#include "clock_def.h"

#ifndef IOT_CLOCK_ALARM_COLOR
#define IOT_CLOCK_ALARM_COLOR 0x880000
#endif

#ifndef IOT_CLOCK_ALARM_FLASHING_SPEED
#define IOT_CLOCK_ALARM_FLASHING_SPEED 250
#endif

namespace KFCConfigurationClasses {

// #if IOT_LED_MATRIX
//     Plugins::ClockConfig::RainbowMultiplier_t::RainbowMultiplier_t() :
//         value(1.23),
//         min(2.5),
//         max(11.0),
//         incr(0.00326)
//     {
//     }
// #else
//     Plugins::ClockConfig::RainbowMultiplier_t::RainbowMultiplier_t() :
//         value(5.23),
//         min(0),
//         max(0),
//         incr(0)
//     {
//     }
// #endif

    // Plugins::ClockConfig::RainbowMultiplier_t::RainbowMultiplier_t(float a, float b, float c, float d) :
    //     value(a),
    //     min(b),
    //     max(c),
    //     incr(d)
    // {
    // }

// #if IOT_LED_MATRIX
//     Plugins::ClockConfig::RainbowColor_t::RainbowColor_t() :
//         min(0x000000),
//         factor(0xffffff),
//         red_incr(0.1),
//         green_incr(0.25),
//         blue_incr(0.125)
//     {
//     }
// #else
//     Plugins::ClockConfig::RainbowColor_t::RainbowColor_t() :
//         min(0x000000),
//         factor(0xffffff),
//         red_incr(0),
//         green_incr(0),
//         blue_incr(0)
//     {
//     }
// #endif

    // Plugins::ClockConfig::RainbowColor_t::RainbowColor_t(uint32_t _min, uint32_t _max, float r, float g, float b) :
    //     min(_min),
    //     factor(_max),
    //     red_incr(r),
    //     green_incr(g),
    //     blue_incr(b)
    // {
    // }

    namespace Plugins {

        namespace ClockConfigNS {

            AlarmType::AlarmType() : color(IOT_CLOCK_ALARM_COLOR), speed(IOT_CLOCK_ALARM_FLASHING_SPEED)
            {
            }

            ClockConfigType::ClockConfigType() :
                #if IOT_LED_MATRIX
                    animation(cast(AnimationType::SOLID)),
                    initial_state(cast(InitialStateType::RESTORE)),
                #else
                    animation(cast(AnimationType::RAINBOW)),
                    initial_state(cast(InitialStateType::ON)),
                    time_format_24h(kDefaultValueFor_time_format_24h),
                #endif
                dithering(kDefaultValueFor_dithering),
                standby_led(kDefaultValueFor_standby_led),
                #if IOT_LED_MATRIX
                    enabled(false),
                #else
                    enabled(true),
                #endif
                method(kDefaultValueFor_method),
                fading_time(kDefaultValueFor_fading_time),
                #if IOT_CLOCK_HAVE_POWER_LIMIT
                    power_limit(kDefaultValueFor_power_limit),
                #endif
                    brightness(kDefaultValueFor_brightness),
                    group_pixels(kDefaultValueFor_group_pixels),
                #if !IOT_LED_MATRIX
                    blink_colon_speed(kDefaultValueFor_blink_colon_speed),
                #endif
                    flashing_speed(kDefaultValueFor_flashing_speed),
                #if IOT_CLOCK_HAVE_MOTION_SENSOR
                    motion_auto_off(kDefaultValueFor_motion_auto_off),
                    motion_trigger_timeout(kDefaultValueFor_motion_trigger_timeout),
                #endif
                #if IOT_LED_MATRIX_FAN_CONTROL
                    fan_speed(kDefaultValueFor_fan_speed),
                    min_fan_speed(kDefaultValueFor_min_fan_speed),
                    max_fan_speed(kDefaultValueFor_max_fan_speed),
                #endif
                power()
            {
            }

            // WebUI
            const __FlashStringHelper *ClockConfigType::getAnimationNames() {
                return F(
                    "Solid,"
                    "Gradient,"
                    "Rainbow,"
                    "Rainbow FastLED,"
                    "Flash,"
                    "Color Fade,"
                    "Fire,"
                    "Plasma,"
                    #if IOT_LED_MATRIX_ENABLE_VISUALIZER
                        "Visualizer,"
                    #endif
                    "Interleaved,"
                    "Xmas"
                );
            }

            // MQTT
            const __FlashStringHelper *ClockConfigType::getAnimationNamesJsonArray() {
                return F("["
                    "\042Solid\042,"
                    "\042Gradient\042,"
                    "\042Rainbow\042,"
                    "\042Rainbow FastLED\042,"
                    "\042Flash\042,"
                    "\042Color Fade\042,"
                    "\042Fire\042,"
                    "\042Plasma\042,"
                    #if IOT_LED_MATRIX_ENABLE_VISUALIZER
                        "\042Visualizer\042,"
                    #endif
                    "\042Interleaved\042,"
                    "\042Xmas\042,"
                    #if !IOT_LED_MATRIX
                        ",\042Colon: Solid\042,"
                        "\042Colon: Blink Slowly\042,"
                        "\042Colon: Blink Fast\042"
                    #endif
                    "]");
            }


            AnimationType ClockConfigType::getAnimationType(const __FlashStringHelper *_name)
            {
                String name = _name;
                char *endPtr = nullptr;
                // test if the name is the animation type as integer
                auto typeNum = strtoul(name.c_str(), &endPtr, 10);
                // __LDBG_printf("name=%s type=%u name=%p endPtr=%p *endPtr=%d valid=%u", _name, typeNum, name.c_str(), endPtr, endPtr ? *endPtr : -1, (typeNum < static_cast<decltype(typeNum)>(AnimationType::MAX)));
                // check that endPtr points to the end of the string and that it contains a valid integer
                if (endPtr && (name.c_str() != endPtr) && !*endPtr && (typeNum < static_cast<decltype(typeNum)>(AnimationType::LAST))) {
                    return static_cast<AnimationType>(typeNum);
                }
                name.replace(' ', '-');
                name.replace('_', '-');
                // search slugs first since it just is a stricmp
                for(int i = 0; i < static_cast<int>(AnimationType::LAST); i++) {
                    // __LDBG_printf("cmp %s==%s", name.c_str(), getAnimationNameSlug(static_cast<AnimationType>(i)));
                    if (name.equalsIgnoreCase(getAnimationNameSlug(static_cast<AnimationType>(i)))) {
                        return static_cast<AnimationType>(i);
                    }
                }
                for(int i = 0; i <static_cast<int>(AnimationType::LAST); i++) {
                    String str = getAnimationName(static_cast<AnimationType>(i));
                    str.replace(' ', '-');
                    str.replace('_', '-');
                    // __LDBG_printf("cmp %s==%s", name.c_str(), str.c_str());
                    if (name.equalsIgnoreCase(str)) {
                        return static_cast<AnimationType>(i);
                    }
                }
                __DBG_printf("invalid name=%s", _name);
                return AnimationType::MAX;
            }

            const __FlashStringHelper *ClockConfigType::getAnimationName(AnimationType type) {
                switch(type) {
                    case AnimationType::GRADIENT:
                        return F("Gradient");
                    case AnimationType::RAINBOW:
                        return F("Rainbow");
                    case AnimationType::RAINBOW_FASTLED:
                        return F("Rainbow FastLED");
                    case AnimationType::FLASHING:
                        return F("Flash");
                    case AnimationType::FADING:
                        return F("Color Fade");
                    case AnimationType::FIRE:
                        return F("Fire");
                    case AnimationType::PLASMA:
                        return F("Plasma");
                    case AnimationType::INTERLEAVED:
                        return F("Interleaved");
                    case AnimationType::XMAS:
                        return F("Xmas");
                    case AnimationType::SOLID:
                        return F("Solid");
                    #if IOT_LED_MATRIX_ENABLE_VISUALIZER
                        case AnimationType::VISUALIZER:
                            return F("Visualizer");
                    #endif
                    #if !IOT_LED_MATRIX
                        case AnimationType::COLON_SOLID:
                            return F("Colon: Solid");
                        case AnimationType::COLON_BLINK_SLOWLY:
                            return F("Colon: Blink Slowly");
                        case AnimationType::COLON_BLINK_FAST:
                            return F("Colon: Blink Fast");
                    #endif
                    case AnimationType::MAX:
                        break;
                }
                __DBG_printf("invalid type=%u", type);
                return nullptr;
            }

            String &ClockConfigType::normalizeSlug(String &slug)
            {
                slug.replace(' ', '_');
                slug.replace('-', '_');
                return slug.toLowerCase();
            }

            const __FlashStringHelper *ClockConfigType::getAnimationNameSlug(AnimationType type)
            {
                switch(type) {
                    case AnimationType::GRADIENT:
                        return F("gradient");
                    case AnimationType::RAINBOW:
                        return F("rainbow");
                    case AnimationType::RAINBOW_FASTLED:
                        return F("rainbow-fastled");
                    case AnimationType::FLASHING:
                        return F("flash");
                    case AnimationType::FADING:
                        return F("color-fade");
                    case AnimationType::FIRE:
                        return F("fire");
                    case AnimationType::PLASMA:
                        return F("plasma");
                    case AnimationType::INTERLEAVED:
                        return F("interleaved");
                    case AnimationType::SOLID:
                        return F("solid");
                    case AnimationType::XMAS:
                        return F("Xmas");
                    #if IOT_LED_MATRIX_ENABLE_VISUALIZER
                        case AnimationType::VISUALIZER:
                            return F("visualizer");
                    #endif
                    #if !IOT_LED_MATRIX
                        case AnimationType::COLON_SOLID:
                        case AnimationType::COLON_BLINK_SLOWLY:
                        case AnimationType::COLON_BLINK_FAST:
                            break;
                    #endif
                    case AnimationType::MAX:
                        break;

                }
                __DBG_printf("invalid type=%u", type);
                return nullptr;
            }

            const __FlashStringHelper *ClockConfigType::getAnimationTitle(AnimationType type)
            {
                switch(type) {
                    case AnimationType::GRADIENT:
                        return F("Gradient Animation");
                    case AnimationType::RAINBOW:
                        return F("Rainbow Animation");
                    case AnimationType::RAINBOW_FASTLED:
                        return F("Rainbow Animation (FastLED)");
                    case AnimationType::FLASHING:
                        return F("Flashing Animation");
                    case AnimationType::FADING:
                        return F("Color Fading");
                    case AnimationType::FIRE:
                        return F("Fire Animation");
                    case AnimationType::PLASMA:
                        return F("Plasma Animation");
                    case AnimationType::INTERLEAVED:
                        return F("Interleaved Animation");
                    case AnimationType::XMAS:
                        return F("Xmas Animation");
                    case AnimationType::SOLID:
                        return F("Solid Color");
                    #if IOT_LED_MATRIX_ENABLE_VISUALIZER
                        case AnimationType::VISUALIZER:
                            return F("Music Visualizer");
                    #endif
                    #if !IOT_LED_MATRIX
                        case AnimationType::COLON_SOLID:
                        case AnimationType::COLON_BLINK_SLOWLY:
                        case AnimationType::COLON_BLINK_FAST:
                            break;
                    #endif
                    case AnimationType::MAX:
                        break;
                }
                __DBG_printf("invalid type=%u", type);
                return nullptr;
            }

            void Clock::defaults()
            {
                setConfig(ClockConfigType());
            }

        }
    }
}
