/**
 * Author: sascha_lammers@gmx.de
 */

#include <Configuration.h>
#include <kfc_fw_config.h>

namespace KFCConfigurationClasses {

    Plugins::ClockConfig::ClockConfig_t::ClockConfig_t() :
        solid_color{ 0x00ff00 },
        animation(0),
        time_format_24h(true),
        brightness(128),
        auto_brightness(-1),
        blink_colon_speed(1000),
        flashing_speed(150),
        protection{ 55, 65, 75 },
        rainbow{ 5.23f, 30, { 0xffffff }, { 0x000000 }},
        alarm{{ 0xff0000 }, 250},
        fading{ 7.5f, 2, { 0xffffff }}
    {
    }

    void Plugins::Clock::defaults()
    {
        ClockConfig_t cfg = {};
        setConfig(cfg);
    }

}
