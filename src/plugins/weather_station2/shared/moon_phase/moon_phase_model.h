/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "moon_phase.h"

#include "global.h"

//
// Maps the moon phase calculation into the data model of the screens.
//
// The phases are calculated in UTC and formatted with the offset of the *location* (the same offset
// the OpenWeatherMap response reports), so the times do not depend on the clock or the timezone of
// the device.
//

#include <Arduino_compat.h>
#include "../../ws2_data.h"

// date/time format of the four phases, change here if the screen should show a different format
#ifndef WEATHER_STATION2_MOON_DATE_FORMAT
#    define WEATHER_STATION2_MOON_DATE_FORMAT "%Y-%m-%d %H:%M"
#endif

namespace WeatherStation2 {

namespace MoonPhase {

// fills the moon values of the model: illumination, waxing, age, the phase name and the next four
// phases with their date and time. The format is a PROGMEM string, the times are formatted in UTC +
// offset
void applyTo(MoonInfo &info, time_t utc, int32_t timezoneOffset, const __FlashStringHelper *dateFormat = F(WEATHER_STATION2_MOON_DATE_FORMAT));

} // namespace MoonPhase

} // namespace WeatherStation2
