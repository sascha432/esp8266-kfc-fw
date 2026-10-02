/**
 * Author: sascha_lammers@gmx.de
 */

#include "ws2_screens.h"

namespace WeatherStation2 {

void MainScreen::create(lv_obj_t *parent)
{
    _page = LVGLUI::createPage(parent, "Local Weather");
    const auto &current = _data.getCurrent();

    // weather card with the current condition icon, the strip below it carries the values that
    // do not fit into the right column (sun rise/set, UV, wind)
    static constexpr lv_coord_t kStripHeight = 28;
    static constexpr lv_coord_t kCardHeight = kContentHeight - kStripHeight - 6;
    _card = LVGLUI::createCard(parent, kMargin, kContentTop, 200, kCardHeight);
    _iconType = toIconType(current.icon);
    _icon = LVGLUI::createIcon(_card, _iconType, 34, 12, 132);

    // city, temperature, description and the details below
    const auto x = static_cast<lv_coord_t>(218);
    const auto width = static_cast<lv_coord_t>(kScreenWidth - x - kMargin);
    _location = LVGLUI::addLabel(parent, x, 84, current.location.c_str(), LVGLUI::kFontLarge, LVGLUI::kColorAccent, width);
    _temperature = LVGLUI::addLabel(parent, x, 110, "", LVGLUI::kFontHuge, LVGLUI::kColorText, width);
    _description = LVGLUI::addLabel(parent, x, 152, "", LVGLUI::kFontLarge, LVGLUI::kColorHighlight, width);
    _details = LVGLUI::addLabel(parent, x, 180, "", LVGLUI::kFontSmall, LVGLUI::kColorTextValue, width);
    // shown instead of the values while the data source has no real weather (not configured, no
    // response yet or the last request failed)
    _status = LVGLUI::addLabel(parent, kMargin, static_cast<lv_coord_t>(kContentTop + 46), "", LVGLUI::kFontLarge,
                              LVGLUI::kColorAlert, static_cast<lv_coord_t>(kScreenWidth - 2 * kMargin), LV_TEXT_ALIGN_CENTER);
    lv_obj_add_flag(_status, LV_OBJ_FLAG_HIDDEN);

    auto strip = LVGLUI::createCard(parent, kMargin, static_cast<lv_coord_t>(kContentTop + kContentHeight - kStripHeight),
                                    static_cast<lv_coord_t>(kScreenWidth - 2 * kMargin), kStripHeight);
    _extra = LVGLUI::addLabel(strip, 10, 6, "", LVGLUI::kFontSmall, LVGLUI::kColorTextValue,
                              static_cast<lv_coord_t>(kScreenWidth - 2 * kMargin - 20));

    // indoor climate in the footer
    auto footer = LVGLUI::createFooter(parent);
    _indoorTemperature = LVGLUI::addLabel(footer, 10, 12, "", LVGLUI::kFontLarge, LVGLUI::kColorText, 150);
    _indoorHumidity = LVGLUI::addLabel(footer, 170, 12, "", LVGLUI::kFontLarge, LVGLUI::kColorAccent, 150);
    _indoorPressure = LVGLUI::addLabel(footer, 330, 12, "", LVGLUI::kFontLarge, LVGLUI::kColorText, 140);
}

void MainScreen::update()
{
    const auto &current = _data.getCurrent();
    const auto &indoor = _data.getIndoor();
    updateClock();

    // values are only displayed for READY, everything else shows the message of the data source
    // instead of made up values
    const auto state = _data.getWeatherState();
    const bool hasWeather = (state == WeatherState::READY);

    // the icon is only redrawn when the condition changed, without data it is the "unknown" icon
    auto iconType = hasWeather ? toIconType(current.icon) : LVGLUI::IconType::UNKNOWN;
    if (iconType != _iconType) {
        _iconType = iconType;
        LVGLUI::setIcon(_icon, iconType, 132);
    }

    lv_obj_t *const valueLabels[4] = { _location, _temperature, _description, _details };
    if (hasWeather) {
        lv_obj_clear_flag(_card, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(_status, LV_OBJ_FLAG_HIDDEN);
        for (auto label : valueLabels) {
            lv_obj_clear_flag(label, LV_OBJ_FLAG_HIDDEN);
        }

        // fixed layout: the labels keep the font size they were created with and only shrink
        auto text = _data.formatTemperature(current.temperature);
        LVGLUI::fitTextDown(_temperature, text.c_str(), lv_obj_get_width(_temperature));
        LVGLUI::fitTextDown(_location, current.location.c_str(), lv_obj_get_width(_location));
        LVGLUI::fitTextDown(_description, current.description.c_str(), lv_obj_get_width(_description));

        text = F("feels like ");
        text += _data.formatTemperature(current.feelsLike);
        text += F("   min ");
        text += _data.formatTemperature(current.minTemperature);
        text += F("   max ");
        text += _data.formatTemperature(current.maxTemperature);
        LVGLUI::fitTextDown(_details, text.c_str(), lv_obj_get_width(_details));

        text = F("sun ");
        text += _data.formatTimeOfDay(current.sunRise);
        text += F(" / ");
        text += _data.formatTimeOfDay(current.sunSet);
        text += F("     UV ");
        text += _data.formatUvIndex(current.uvIndex);
        text += F("     wind ");
        text += _data.formatWind(current.windSpeed);
        LVGLUI::fitTextDown(_extra, text.c_str(), lv_obj_get_width(_extra));
    }
    else {
        // hide the weather card and the value labels, the message uses the whole content area
        lv_obj_add_flag(_card, LV_OBJ_FLAG_HIDDEN);
        for (auto label : valueLabels) {
            lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
        }
        lv_obj_clear_flag(_status, LV_OBJ_FLAG_HIDDEN);
        LVGLUI::setText(_extra, "", LVGLUI::kFontSmall, LVGLUI::kColorTextValue);
        const auto text = _data.getWeatherStatusText();
        LVGLUI::setText(_status, text.c_str(), LVGLUI::kFontLarge, LVGLUI::kColorAlert);
    }

    // indoor climate in the footer, "offline" or "--" is shown while a source does not deliver
    static const IndoorValues::Metric footerMetrics[3] = {
        IndoorValues::Metric::TEMPERATURE, IndoorValues::Metric::HUMIDITY, IndoorValues::Metric::PRESSURE,
    };
    lv_obj_t *const footerLabels[3] = { _indoorTemperature, _indoorHumidity, _indoorPressure };
    // the footer keeps its colors while the values are valid
    static const uint32_t footerColors[3] = { LVGLUI::kColorText, LVGLUI::kColorAccent, LVGLUI::kColorText };
    for (uint8_t i = 0; i < 3; i++) {
        const auto &value = indoor.get(footerMetrics[i]);
        const auto footerText = _data.formatIndoorValue(footerMetrics[i], value);
        LVGLUI::setText(footerLabels[i], footerText.c_str(), LVGLUI::kFontLarge, toIndoorColor(value.getState(), footerColors[i]));
    }
}

} // namespace WeatherStation2
