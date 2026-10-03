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
        char text[DataSource::kFormatSize];
        _data.formatTemperature(current.temperature, text, sizeof(text));
        LVGLUI::fitTextDown(_temperature, text, lv_obj_get_width(_temperature));
        LVGLUI::fitTextDown(_location, current.location.c_str(), lv_obj_get_width(_location));
        LVGLUI::fitTextDown(_description, current.description.c_str(), lv_obj_get_width(_description));

        // The two lines below concatenate several values. They are composed in stack buffers
        // instead of a String that every append can reallocate (the screen refreshes at 1 Hz).
        // The cursor is the length that was really written (strlen of the terminated buffer), so
        // the appends stay inside the buffer even when a value is truncated
        char line[160];
        char value[DataSource::kFormatSize];
        char second[DataSource::kFormatSize];
        _data.formatTemperature(current.feelsLike, value, sizeof(value));
        _data.formatTemperature(current.minTemperature, second, sizeof(second));
        snprintf(line, sizeof(line), "feels like %s   min %s   max ", value, second);
        auto used = strlen(line);
        _data.formatTemperature(current.maxTemperature, value, sizeof(value));
        snprintf(line + used, sizeof(line) - used, "%s", value);
        LVGLUI::fitTextDown(_details, line, lv_obj_get_width(_details));

        _data.formatTimeOfDay(current.sunRise, value, sizeof(value));
        _data.formatTimeOfDay(current.sunSet, second, sizeof(second));
        snprintf(line, sizeof(line), "sun %s / %s     UV ", value, second);
        used = strlen(line);
        _data.formatUvIndex(current.uvIndex, value, sizeof(value));
        snprintf(line + used, sizeof(line) - used, "%s     wind ", value);
        used = strlen(line);
        _data.formatWind(current.windSpeed, value, sizeof(value));
        snprintf(line + used, sizeof(line) - used, "%s", value);
        LVGLUI::fitTextDown(_extra, line, lv_obj_get_width(_extra));
    }
    else {
        // hide the weather card and the value labels, the message uses the whole content area
        lv_obj_add_flag(_card, LV_OBJ_FLAG_HIDDEN);
        for (auto label : valueLabels) {
            lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
        }
        lv_obj_clear_flag(_status, LV_OBJ_FLAG_HIDDEN);
        LVGLUI::setText(_extra, "", LVGLUI::kFontSmall, LVGLUI::kColorTextValue);
        auto &status = _statusText;
        status.clear();
        _data.getWeatherStatusText(status);
        LVGLUI::setText(_status, status.c_str(), LVGLUI::kFontLarge, LVGLUI::kColorAlert);
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
        char text[DataSource::kFormatSize];
        _data.formatIndoorValue(footerMetrics[i], value, text, sizeof(text));
        LVGLUI::setText(footerLabels[i], text, LVGLUI::kFontLarge, toIndoorColor(value.getState(), footerColors[i]));
    }
}

} // namespace WeatherStation2
