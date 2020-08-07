/**
 * Author: sascha_lammers@gmx.de
 */

#include <Configuration.h>
#include <kfc_fw_config.h>

namespace KFCConfigurationClasses {

    Plugins::ClockConfig::ClockConfig_t::ClockConfig_t() :
        animation(0),
        time_format_24h(true),
        solid_color{ 0x00ff00 },
        auto_brightness(-1),
        blink_colon_speed(1000),
        flashing_speed(150),
        brightness(128),
        protection_temperature_75(55),
        protection_temperature_50(65),
        protection_max_temperature(75),
        rainbow_multiplier(5.23f),
        rainbow_speed(30),
        rainbow_factor({ 0xffffff }),
        rainbow_minimum({ 0x000000 }),
        alarm_color{ 0xff0000 },
        alarm_speed(250),
        fading_speed(7.5f),
        fading_delay(2),
        fading_factor{ 0xffffff }
    {
    }

    void Plugins::Clock::defaults()
    {
        ClockConfig_t cfg = {};
        setConfig(cfg);
    }

}
