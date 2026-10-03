/**
 * Author: sascha_lammers@gmx.de
 */

#include "ws2_screens.h"

namespace WeatherStation2 {

static constexpr lv_coord_t kMoonSize = 124;
static constexpr lv_coord_t kPhaseRowHeight = 40;
static constexpr lv_coord_t kPhaseRowGap = 6;
// one row per phase, left of it the card with the moon disc
static constexpr lv_coord_t kRowX = 196;
static constexpr lv_coord_t kRowWidth = kScreenWidth - kRowX - kMargin;
// the name and the date of a phase are one group that is centered in the row
static constexpr lv_coord_t kPairMargin = 10;
static constexpr lv_coord_t kPairWidth = kRowWidth - 2 * kPairMargin;
static constexpr lv_coord_t kPairGap = 12;
static constexpr lv_coord_t kPairTop = 11;

void MoonPhaseScreen::create(lv_obj_t *parent)
{
    _page = LVGLUI::createPage(parent, "Moon Phase");

    _card = LVGLUI::createCard(parent, kMargin, kContentTop, 180, kContentHeight);
    _moon = LVGLUI::createMoon(_card, 28, 14, kMoonSize);
    _phase = LVGLUI::addLabel(_card, 8, 148, "", LVGLUI::kFontLarge, LVGLUI::kColorAccent, 164, LV_TEXT_ALIGN_CENTER);
    _details = LVGLUI::addLabel(_card, 8, 172, "", LVGLUI::kFontSmall, LVGLUI::kColorHighlight, 164, LV_TEXT_ALIGN_CENTER);
    // shown instead of the values while the clock (NTP) is not set, the calculation needs it
    _status = LVGLUI::addLabel(parent, kMargin, static_cast<lv_coord_t>(kContentTop + 46), "Waiting for the clock (NTP)",
                               LVGLUI::kFontLarge, LVGLUI::kColorAlert, static_cast<lv_coord_t>(kScreenWidth - 2 * kMargin), LV_TEXT_ALIGN_CENTER);
    lv_obj_add_flag(_status, LV_OBJ_FLAG_HIDDEN);

    const auto x = kRowX;
    const auto width = kRowWidth;
    for (uint8_t i = 0; i < MoonInfo::kNumPhases; i++) {
        auto row = _rows[i] = LVGLUI::createCard(parent, x, static_cast<lv_coord_t>(kContentTop + i * (kPhaseRowHeight + kPhaseRowGap)), width, kPhaseRowHeight);
        // the pair is created at the left edge of the row and moved to the center by
        // layoutCenteredPair(), the font is the maximum and only shrinks
        _phaseNames[i] = LVGLUI::addLabel(row, kPairMargin, kPairTop, "", LVGLUI::kFontMedium, LVGLUI::kColorText);
        _phaseDates[i] = LVGLUI::addLabel(row, kPairMargin, kPairTop, "", LVGLUI::kFontNormal, LVGLUI::kColorHighlight);
    }
}

void MoonPhaseScreen::update()
{
    const auto &moon = _data.getMoon();
    updateClock();

    if (!moon.valid) {
        // The clock (NTP) was not set yet and without it nothing is calculated. A disc with 0 %
        // illumination would look like a new moon, so the reason is shown instead of the values
        lv_obj_add_flag(_card, LV_OBJ_FLAG_HIDDEN);
        for (auto row : _rows) {
            lv_obj_add_flag(row, LV_OBJ_FLAG_HIDDEN);
        }
        lv_obj_clear_flag(_status, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_clear_flag(_card, LV_OBJ_FLAG_HIDDEN);
    for (auto row : _rows) {
        lv_obj_clear_flag(row, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_add_flag(_status, LV_OBJ_FLAG_HIDDEN);

    LVGLUI::setMoonPhase(_moon, moon.illumination, moon.waxing);
    LVGLUI::fitTextDown(_phase, moon.phase.c_str(), lv_obj_get_width(_phase));

    // the illumination and the age are formatted into stack buffers, the two of them are one line
    char illumination[DataSource::kFormatSize];
    char age[DataSource::kFormatSize];
    _data.formatIllumination(moon.illumination, illumination, sizeof(illumination));
    _data.formatAge(moon.age, age, sizeof(age));
    char details[2 * DataSource::kFormatSize + 4];
    snprintf(details, sizeof(details), "%s - %s", illumination, age);
    LVGLUI::fitTextDown(_details, details, lv_obj_get_width(_details));

    for (uint8_t i = 0; i < MoonInfo::kNumPhases; i++) {
        // the name lives in flash. On the ESP32 (the only target of this plugin) that is a plain
        // string pointer, so no String has to be built for it
        LVGLUI::layoutCenteredPair(_phaseNames[i], _phaseDates[i], flashStringToCStr(moon.phases[i].name),
                                   moon.phases[i].dateTime.c_str(), kPairWidth, kPairGap);
    }
}

} // namespace WeatherStation2
