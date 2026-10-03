/**
 * Author: sascha_lammers@gmx.de
 */

#include "ws2_screens.h"

namespace WeatherStation2 {

// Vertical positions inside a day card. The values never move, only the font size changes
// (fitTextDown never increases it, fitText would grow the font with the card width and the rows
// would overlap). A card is kContentHeight = 190 px high, the rows are laid out with the line
// height of the largest font they may use:
//
//     8 .. 35   day
//    42 .. 106  icon
//   112 .. 134  maximum (20 px font)
//   140 .. 158  minimum (16 px font)
//   164 .. 179  rain (12 px font)
static constexpr lv_coord_t kDayTop = 8;
static constexpr lv_coord_t kIconTop = 42;
static constexpr lv_coord_t kIconMaxSize = 64;
static constexpr lv_coord_t kMaxTempTop = 112;
static constexpr lv_coord_t kMinTempTop = 140;
static constexpr lv_coord_t kRainTop = 164;

// The layout of the four parts of the day has four cards instead of up to three, so a card is
// narrower. The rows of a card are (same fonts as above, the name is one step smaller because
// "Afternoon" does not fit a 110 px card in 24 px):
//
//     8 ..  27   part of the day (16 px font)
//    30 ..  44   local time of the hourly entry (12 px font)
//    48 .. 111   icon
//   116 .. 139   temperature (20 px font)
//   142 .. 156   feels like (12 px font)
//   162 .. 176   rain or probability of precipitation (12 px font)
static constexpr lv_coord_t kPartNameTop = 8;
static constexpr lv_coord_t kPartTimeTop = 30;
static constexpr lv_coord_t kPartIconTop = 48;
static constexpr lv_coord_t kPartTempTop = 116;
static constexpr lv_coord_t kPartFeelsTop = 142;
static constexpr lv_coord_t kPartRainTop = 162;

// gap between two cards of a row
static constexpr lv_coord_t kCardGap = 8;

// probability of precipitation of an hourly entry as "62%"
static void _popText(float pop, char *output, size_t size)
{
    snprintf(output, size, "%d%%", static_cast<int>(pop * 100.0f + 0.5f));
}

void ForecastScreen::create(lv_obj_t *parent)
{
    // the page (top bar, title) is the same for both layouts and stays, only the cards in the
    // container are rebuilt when the layout changes
    _page = LVGLUI::createPage(parent, "Weather Forecast");
    _content = LVGLUI::createContainer(parent, 0, 0, kScreenWidth, kScreenHeight);
    _lastSwitch = millis();
    _createCards();
}

void ForecastScreen::_switchLayout()
{
    _layout = (_layout == Layout::DAYS) ? Layout::DAY_PARTS : Layout::DAYS;
    _lastSwitch = millis();
    if (_content) {
        lv_obj_clean(_content);
    }
    _createCards();
}

void ForecastScreen::_createCards()
{
    if (_layout == Layout::DAY_PARTS) {
        _createDayPartCards();
    }
    else {
        _createDayCards();
    }
}

void ForecastScreen::_showStatus()
{
    // no real data: tell the user why instead of showing made up values
    const auto text = _data.getWeatherStatusText();
    LVGLUI::addLabel(_content, kMargin, static_cast<lv_coord_t>(kContentTop + 50), text.c_str(), LVGLUI::kFontLarge,
                     LVGLUI::kColorAlert, static_cast<lv_coord_t>(kScreenWidth - 2 * kMargin), LV_TEXT_ALIGN_CENTER);
}

void ForecastScreen::_createDayCards()
{
    const auto count = _data.getForecastCount();
    if (count == 0) {
        _showStatus();
        return;
    }

    const auto forecast = _data.getForecast();
    const auto width = static_cast<lv_coord_t>((kScreenWidth - 2 * kMargin - (count - 1) * kCardGap) / count);
    const auto iconSize = static_cast<lv_coord_t>((width - 20 < kIconMaxSize) ? width - 20 : kIconMaxSize);
    // the value labels are 4 px away from both edges of the card
    const auto textWidth = static_cast<lv_coord_t>(width - 8);

    for (uint8_t i = 0; i < count; i++) {
        const auto &day = forecast[i];
        const auto x = static_cast<lv_coord_t>(kMargin + i * (width + kCardGap));
        auto card = LVGLUI::createCard(_content, x, kContentTop, width, kContentHeight);

        auto label = LVGLUI::addLabel(card, 4, kDayTop, day.day.c_str(), LVGLUI::kFontTitle,
                                      LVGLUI::kColorAccent, textWidth, LV_TEXT_ALIGN_CENTER);
        LVGLUI::fitTextDown(label, day.day.c_str(), textWidth);

        LVGLUI::createIcon(card, toIconType(day.icon), static_cast<lv_coord_t>((width - iconSize) / 2), kIconTop, iconSize);

        char text[DataSource::kFormatSize];
        _data.formatTemperature(day.maxTemperature, text, sizeof(text));
        label = LVGLUI::addLabel(card, 4, kMaxTempTop, text, LVGLUI::kFontLarge,
                                 LVGLUI::kColorHighlight, textWidth, LV_TEXT_ALIGN_CENTER);
        LVGLUI::fitTextDown(label, text, textWidth);

        _data.formatTemperature(day.minTemperature, text, sizeof(text));
        label = LVGLUI::addLabel(card, 4, kMinTempTop, text, LVGLUI::kFontMedium,
                                 LVGLUI::kColorTextValue, textWidth, LV_TEXT_ALIGN_CENTER);
        LVGLUI::fitTextDown(label, text, textWidth);

        // the rain row only exists for a day with precipitation, the space below the minimum
        // temperature is always free for it
        if (day.rain > 0) {
            _data.formatRain(day.rain, text, sizeof(text));
            label = LVGLUI::addLabel(card, 4, kRainTop, text, LVGLUI::kFontSmall,
                                     LVGLUI::kColorAccent, textWidth, LV_TEXT_ALIGN_CENTER);
            LVGLUI::fitTextDown(label, text, textWidth);
        }
    }
}

void ForecastScreen::_createDayPartCards()
{
    const auto parts = _data.getDayParts();

    // All four cards display the hourly forecast of the response. Without it the layout would be
    // empty, that is the status message
    bool hasValues = false;
    for (uint8_t i = 0; i < kNumDayParts; i++) {
        hasValues |= parts[i].valid;
    }
    if (!hasValues) {
        _showStatus();
        return;
    }

    const auto width = static_cast<lv_coord_t>((kScreenWidth - 2 * kMargin - (kNumDayParts - 1) * kCardGap) / kNumDayParts);
    const auto iconSize = static_cast<lv_coord_t>((width - 20 < kIconMaxSize) ? width - 20 : kIconMaxSize);
    const auto textWidth = static_cast<lv_coord_t>(width - 8);

    for (uint8_t i = 0; i < kNumDayParts; i++) {
        const auto &part = parts[i];
        const auto x = static_cast<lv_coord_t>(kMargin + i * (width + kCardGap));
        auto card = LVGLUI::createCard(_content, x, kContentTop, width, kContentHeight);

        // the part of the day and the local time of the hourly entry the values were taken from
        // (the weekday is added when the entry is not from today)
        String name = getDayPartName(i);
        auto label = LVGLUI::addLabel(card, 4, kPartNameTop, name.c_str(), LVGLUI::kFontMedium,
                                      LVGLUI::kColorAccent, textWidth, LV_TEXT_ALIGN_CENTER);
        LVGLUI::fitTextDown(label, name.c_str(), textWidth);

        label = LVGLUI::addLabel(card, 4, kPartTimeTop, part.time.c_str(), LVGLUI::kFontSmall,
                                 LVGLUI::kColorTextMuted, textWidth, LV_TEXT_ALIGN_CENTER);
        LVGLUI::fitTextDown(label, part.time.c_str(), textWidth);

        LVGLUI::createIcon(card, toIconType(part.icon), static_cast<lv_coord_t>((width - iconSize) / 2), kPartIconTop, iconSize);

        // a part of the day without an hourly entry shows "--" like the indoor metrics of the
        // other screens, the values are never made up
        char text[48];
        if (part.valid) {
            _data.formatTemperature(part.temperature, text, sizeof(text));
        }
        else {
            memcpy(text, "--", 3);
        }
        label = LVGLUI::addLabel(card, 4, kPartTempTop, text, LVGLUI::kFontLarge,
                                 LVGLUI::kColorHighlight, textWidth, LV_TEXT_ALIGN_CENTER);
        LVGLUI::fitTextDown(label, text, textWidth);

        if (!part.valid) {
            continue;
        }

        char feels[DataSource::kFormatSize];
        _data.formatTemperature(part.feelsLike, feels, sizeof(feels));
        snprintf(text, sizeof(text), "feels %s", feels);
        label = LVGLUI::addLabel(card, 4, kPartFeelsTop, text, LVGLUI::kFontSmall,
                                 LVGLUI::kColorTextValue, textWidth, LV_TEXT_ALIGN_CENTER);
        LVGLUI::fitTextDown(label, text, textWidth);

        // the rain of the hour, with the probability of precipitation in front of it when the
        // API expects rain. Without both the row does not exist
        char rain[DataSource::kFormatSize];
        if (part.rain > 0) {
            _data.formatRain(part.rain, rain, sizeof(rain));
            if (part.pop >= 0.05f) {
                char pop[8];
                _popText(part.pop, pop, sizeof(pop));
                snprintf(text, sizeof(text), "%s %s", pop, rain);
            }
            else {
                strncpy(text, rain, sizeof(text) - 1);
                text[sizeof(text) - 1] = 0;
            }
        }
        else if (part.pop >= 0.05f) {
            _popText(part.pop, text, sizeof(text));
        }
        else {
            text[0] = 0;
        }
        if (text[0]) {
            label = LVGLUI::addLabel(card, 4, kPartRainTop, text, LVGLUI::kFontSmall,
                                     LVGLUI::kColorAccent, textWidth, LV_TEXT_ALIGN_CENTER);
            LVGLUI::fitTextDown(label, text, textWidth);
        }
    }
}

bool ForecastScreen::onSwipe(SwipeDirection direction)
{
    // A swipe up or down switches between the two layouts before the timer of update() does.
    // There are only two of them, so the direction does not matter - a vertical swipe always
    // shows the other one. _switchLayout() also restarts the kLayoutTime timer
    if (!isVerticalSwipe(direction)) {
        // the horizontal swipes keep their default action (previous/next screen)
        return false;
    }
    _switchLayout();
    return true;
}

void ForecastScreen::update()
{
    updateClock();

    // The forecast values change once a day, but the two layouts alternate while the screen is
    // visible. getScreenTime() is two times kLayoutTime, so both are shown before the manager
    // rotates to the next screen. The rebuild does not touch the manager (a reload() would restart
    // its rotation timer and the screen would never be left)
    if (static_cast<uint32_t>(millis() - _lastSwitch) >= kLayoutTime) {
        _switchLayout();
    }
}

} // namespace WeatherStation2
