/**
 * Author: sascha_lammers@gmx.de
 */

#include "moon_phase_model.h"

namespace WeatherStation2 {

namespace MoonPhase {

void applyTo(MoonInfo &info, time_t utc, int32_t timezoneOffset, const __FlashStringHelper *dateFormat)
{
    static_assert(MoonInfo::kNumPhases == 4, "nextPhases() returns exactly the four quarterly phases");

    const auto state = calculate(utc);
    info.valid = true;
    info.illumination = static_cast<float>(state.illumination);
    info.waxing = state.waxing;
    info.age = static_cast<float>(state.age);
    info.phase = name(state.phase);

    PhaseTime phases[MoonInfo::kNumPhases];
    nextPhases(utc, phases);
    const auto offset = static_cast<time_t>(timezoneOffset);
    for (uint8_t i = 0; i < MoonInfo::kNumPhases; i++) {
        auto value = phases[i].utc + offset;
        struct tm tm;
        gmtime_r(&value, &tm);
        // the date of the phase, written into a stack buffer (the model only fills the phases when
        // the clock changed, but no String has to be built for it)
        char dateTime[32];
        const auto format = flashStringToCStr(dateFormat);
        if (!format || !*format || (::strftime(dateTime, sizeof(dateTime), format, &tm) <= 0)) {
            dateTime[0] = 0;
        }
        info.phases[i].name = quarterName(phases[i].phase);
        info.phases[i].dateTime = dateTime;
    }
}

} // namespace MoonPhase

} // namespace WeatherStation2
