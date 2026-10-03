/**
 * Author: sascha_lammers@gmx.de
 */

#include "ws2_screens.h"

namespace WeatherStation2 {

static constexpr lv_coord_t kValueCardHeight = 42;
static constexpr lv_coord_t kValueCardGap = 6;

// Sets the text only when it changed: the screen refreshes once per second and a new pointer
// reallocates the buffer of the label and restarts the scroll animation of a long value
static void _setValue(lv_obj_t *label, const char *text, uint32_t color)
{
    const auto current = lv_label_get_text(label);
    if (current && !strcmp(current, text)) {
        return;
    }
    LVGLUI::setText(label, text, LVGLUI::kFontLarge, color);
}

void IndoorScreen::create(lv_obj_t *parent)
{
    _page = LVGLUI::createPage(parent, "Indoor Climate");

    // the sources of the indoor metrics are configuration, so the icon card is static
    auto card = LVGLUI::createCard(parent, kMargin, kContentTop, 150, kContentHeight);
    LVGLUI::createHouseIcon(card, 25, 30, 100);
    _sensorLabel = LVGLUI::addLabel(card, 8, kContentHeight - 26, _data.getSystem().sensors.c_str(),
                                    LVGLUI::kFontSmall, LVGLUI::kColorTextMuted, 134, LV_TEXT_ALIGN_CENTER);

    // one card per metric. The source of a metric is only read by the data source before the
    // first screen is built, so "configured" is already valid here
    const auto &indoor = _data.getIndoor();
    const auto x = static_cast<lv_coord_t>(166);
    const auto width = static_cast<lv_coord_t>(kScreenWidth - x - kMargin);
    for (uint8_t i = 0; i < IndoorValues::kNumMetrics; i++) {
        const auto metric = static_cast<IndoorValues::Metric>(i);
        auto row = LVGLUI::createCard(parent, x, static_cast<lv_coord_t>(kContentTop + i * (kValueCardHeight + kValueCardGap)), width, kValueCardHeight);
        String title = getMetricTitle(metric);
        LVGLUI::addLabel(row, 12, 8, title.c_str(), LVGLUI::kFontNormal, LVGLUI::kColorTextLabel, width / 2);
        _values[i] = LVGLUI::addLabel(row, static_cast<lv_coord_t>(width / 2), 4, "", LVGLUI::kFontLarge,
                                      LVGLUI::kColorText, static_cast<lv_coord_t>(width / 2 - 12), LV_TEXT_ALIGN_RIGHT);
        _rows[i] = row;

        // eCO2 is optional, the row only exists with a source (the other three are always shown,
        // a missing source is visible there as "--")
        if (metric == IndoorValues::Metric::ECO2 && !indoor.get(metric).configured) {
            lv_obj_add_flag(row, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void IndoorScreen::update()
{
    const auto &indoor = _data.getIndoor();
    updateClock();

    for (uint8_t i = 0; i < IndoorValues::kNumMetrics; i++) {
        const auto metric = static_cast<IndoorValues::Metric>(i);
        const auto &value = indoor.get(metric);

        // the eCO2 row follows the configuration (a source added/removed by the form)
        if (metric == IndoorValues::Metric::ECO2) {
            if (value.configured) {
                lv_obj_clear_flag(_rows[i], LV_OBJ_FLAG_HIDDEN);
            }
            else {
                lv_obj_add_flag(_rows[i], LV_OBJ_FLAG_HIDDEN);
            }
        }

        char text[DataSource::kFormatSize];
        _data.formatIndoorValue(metric, value, text, sizeof(text));
        _setValue(_values[i], text, toIndoorColor(value.getState()));
    }

    if (_sensorLabel) {
        LVGLUI::fitTextDown(_sensorLabel, _data.getSystem().sensors.c_str(), lv_obj_get_width(_sensorLabel));
    }
}

} // namespace WeatherStation2
