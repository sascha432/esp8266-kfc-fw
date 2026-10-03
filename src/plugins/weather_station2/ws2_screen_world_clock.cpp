/**
 * Author: sascha_lammers@gmx.de
 */

#include "ws2_screens.h"

#include <kfc_fw_config.h>

// World clock screen: the local clock of the device and one row per enabled clock of the
// weather configuration. Name, POSIX time zone and the 12/24 hour format of a clock come from
// the shared weather configuration, i.e. the "World Clock" form of this plugin writes the same
// values the 1.x plugin uses (WeatherStation::getName/getTZ, additionalClocks[_time_format_24h]
// and show_regular_clock_on_world_clocks).
//
// A clock is rendered by applying its POSIX time zone to the TZ environment of the libc,
// formatting the time and restoring the time zone of the device afterwards. The 1.x plugin does
// the same (WSDraw::_drawWorldClocks), it only adds an interrupt lock around it - not possible
// with the libc time functions on this target.

namespace WeatherStation2 {

using Plugins = KFCConfigurationClasses::PluginsType;

static constexpr uint8_t kMaxClocks = WEATHER_STATION_MAX_CLOCKS;

// gap between two rows
static constexpr lv_coord_t kRowGap = 6;
// a single row never grows above this, fewer clocks are centered in the content area
static constexpr lv_coord_t kRowMaxHeight = 52;
// distance of the name/detail and the time from the edges of a row
static constexpr lv_coord_t kRowPadding = 12;
// left column of a row (name and detail), the time uses the rest
static constexpr lv_coord_t kNameWidth = 180;
// below this height the rows use the smaller fonts (5 rows are 33 px high)
static constexpr lv_coord_t kCompactRowHeight = 40;

// 2020-09-13: anything before that means the clock was not set (NTP) yet, the same threshold
// the data source uses to decide whether the moon can be calculated
static constexpr time_t kMinValidTime = 1600000000;

// Time and date of one clock, written into the buffers of the caller (the screen refreshes every
// second and a String per row is one heap allocation per row and tick). The POSIX time zone is
// applied to the libc and restored afterwards, so the rest of the firmware keeps the time zone of
// the device
static void _formatClock(const String &tz, const char *deviceTz, bool format24h, time_t utc, char *time, size_t timeSize, char *detail, size_t detailSize)
{
    if (tz.length()) {
        safeSetTZ(tz);
    }

    struct tm tm;
    localtime_r(&utc, &tm);

    if (strftime(time, timeSize, format24h ? "%H:%M:%S" : "%I:%M:%S %p", &tm) <= 0) {
        time[0] = 0;
    }
    // the date and the time zone name/time zone abbreviation of the clock
    if (strftime(detail, detailSize, "%a %b %d %Z", &tm) <= 0) {
        detail[0] = 0;
    }

    if (tz.length()) {
        safeSetTZ(deviceTz);
    }
}

// Sets the text of a label the screen refreshes once per second. An unchanged text is left
// alone: lv_label_set_text() only compares the pointer and reallocates the buffer of the label
// for the new string
static void _setValue(lv_obj_t *label, const char *text)
{
    if (!label || strcmp(lv_label_get_text(label), text) == 0) {
        return;
    }
    LVGLUI::fitTextDown(label, text);
}

void WorldClockScreen::create(lv_obj_t *parent)
{
    _page = LVGLUI::createPage(parent, "World Clock");
    // container of the rows, only its children are rebuilt when the configuration changes, the
    // page (top bar, title) around them stays
    _content = LVGLUI::createContainer(parent, 0, 0, kScreenWidth, kScreenHeight);
    // shown instead of the clocks while the clock (NTP) is not set or nothing is configured
    _status = LVGLUI::addLabel(parent, kMargin, static_cast<lv_coord_t>(kContentTop + 60), "", LVGLUI::kFontLarge,
                               LVGLUI::kColorAlert, static_cast<lv_coord_t>(kScreenWidth - 2 * kMargin), LV_TEXT_ALIGN_CENTER);
    lv_obj_add_flag(_status, LV_OBJ_FLAG_HIDDEN);

    // the rows of the last screen time are gone, build them from the current configuration
    _readClocks();
    _buildRows();
}

bool WorldClockScreen::_readClocks()
{
    const auto cfg = Plugins::WeatherStation::getConfig();

    // Which clocks exist and how they are formatted decides whether the rows have to be
    // rebuilt. Everything below the comparison is only done when they change, the name/TZ
    // getters split a stored triple and would allocate on every call
    const bool local = cfg.show_regular_clock_on_world_clocks;
    uint16_t signature = 0;
    uint8_t count = 0;

    if (local) {
        signature |= 1;
        if (_data.isTimeFormat24h()) {
            signature |= static_cast<uint16_t>(1) << 5;
        }
        count++;
    }
    for (uint8_t i = 0; i < kMaxClocks; i++) {
        if (!cfg.additionalClocks[i].isEnabled()) {
            continue;
        }
        signature |= static_cast<uint16_t>(1) << (1 + i);
        if (cfg.additionalClocks[i]._time_format_24h) {
            signature |= static_cast<uint16_t>(1) << (5 + count);
        }
        count++;
    }
    _count = count;
    if (signature == _signature) {
        return false;
    }
    _signature = signature;

    uint8_t row = 0;
    if (local) {
        auto &clock = _clocks[row++];
        clock.name = F("Local");
        // empty: the format helper keeps the time zone of the device
        clock.tz = String();
        clock.format24h = _data.isTimeFormat24h();
    }
    for (uint8_t i = 0; i < kMaxClocks; i++) {
        if (!cfg.additionalClocks[i].isEnabled()) {
            continue;
        }
        auto &clock = _clocks[row++];
        clock.name = Plugins::WeatherStation::getName(i);
        clock.tz = Plugins::WeatherStation::getTZ(i);
        clock.format24h = cfg.additionalClocks[i]._time_format_24h;
    }
    return true;
}

void WorldClockScreen::_buildRows()
{
    if (_content) {
        lv_obj_clean(_content);
    }
    for (uint8_t i = 0; i < kMaxRows; i++) {
        _rows[i] = nullptr;
        _names[i] = nullptr;
        _times[i] = nullptr;
        _details[i] = nullptr;
    }
    if (_count == 0) {
        return;
    }

    const auto width = static_cast<lv_coord_t>(kScreenWidth - 2 * kMargin);
    auto height = static_cast<lv_coord_t>((kContentHeight - kRowGap * (_count - 1)) / _count);
    if (height > kRowMaxHeight) {
        height = kRowMaxHeight;
    }
    const auto total = static_cast<lv_coord_t>(height * _count + kRowGap * (_count - 1));
    auto y = static_cast<lv_coord_t>(kContentTop + (kContentHeight - total) / 2);

    // the time shrinks with the number of rows, the name and the detail are one step smaller
    // (a row of five clocks is only 33 px high)
    const bool compact = (height < kCompactRowHeight);
    const auto nameFont = compact ? LVGLUI::kFontSmall : LVGLUI::kFontMedium;
    const auto timeFont = compact ? LVGLUI::kFontMedium : ((_count <= 2) ? LVGLUI::kFontHuge : LVGLUI::kFontLarge);

    const auto nameHeight = lv_font_get_line_height(nameFont);
    const auto detailHeight = lv_font_get_line_height(LVGLUI::kFontSmall);
    const auto timeHeight = lv_font_get_line_height(timeFont);
    const auto timeX = static_cast<lv_coord_t>(kRowPadding + kNameWidth + 8);
    const auto timeWidth = static_cast<lv_coord_t>(width - timeX - kRowPadding);

    for (uint8_t i = 0; i < _count; i++) {
        const auto &clock = _clocks[i];
        auto row = _rows[i] = LVGLUI::createCard(_content, kMargin, y, width, height);
        // alternate the fill of the rows, like the mockup of the screen does
        if (i & 1) {
            lv_obj_set_style_bg_color(row, lv_color_hex(LVGLUI::kColorCardAlt), LV_PART_MAIN);
        }

        const auto textTop = static_cast<lv_coord_t>((height - (nameHeight + detailHeight)) / 2);
        auto name = _names[i] = LVGLUI::addLabel(row, kRowPadding, textTop, clock.name.c_str(), nameFont, LVGLUI::kColorAccent, kNameWidth);
        LVGLUI::fitTextDown(name, clock.name.c_str(), kNameWidth);
        _details[i] = LVGLUI::addLabel(row, kRowPadding, static_cast<lv_coord_t>(textTop + nameHeight), "", LVGLUI::kFontSmall,
                                       LVGLUI::kColorHighlight, kNameWidth);
        // the time is right aligned in the rest of the row
        _times[i] = LVGLUI::addLabel(row, timeX, static_cast<lv_coord_t>((height - timeHeight) / 2), "", timeFont,
                                     LVGLUI::kColorText, timeWidth, LV_TEXT_ALIGN_RIGHT);

        y += static_cast<lv_coord_t>(height + kRowGap);
    }
}

void WorldClockScreen::_showStatus(const char *text)
{
    LVGLUI::setText(_status, text, LVGLUI::kFontLarge, LVGLUI::kColorAlert);
    lv_obj_clear_flag(_status, LV_OBJ_FLAG_HIDDEN);
    for (uint8_t i = 0; i < _count; i++) {
        lv_obj_add_flag(_rows[i], LV_OBJ_FLAG_HIDDEN);
    }
}

void WorldClockScreen::update()
{
    updateClock();

    // the "World Clock" form of the plugin writes the same configuration, a change only rebuilds
    // the rows (the number of clocks or a time format)
    if (_readClocks()) {
        _buildRows();
    }

    if (_count == 0) {
        _showStatus("No clocks configured");
        return;
    }

    const auto utc = ::time(nullptr);
    if (utc < kMinValidTime) {
        _showStatus("Waiting for the clock (NTP)");
        return;
    }

    lv_obj_add_flag(_status, LV_OBJ_FLAG_HIDDEN);

    // the time zone of the device is restored after every clock, capture it once. The value of the
    // environment is used as it is, no String is built from it
    const char *deviceTz = getenv("TZ");
    if (!deviceTz) {
        deviceTz = "";
    }

    char time[32];
    char detail[40];
    for (uint8_t i = 0; i < _count; i++) {
        lv_obj_clear_flag(_rows[i], LV_OBJ_FLAG_HIDDEN);

        const auto &clock = _clocks[i];
        _formatClock(clock.tz, deviceTz, clock.format24h, utc, time, sizeof(time), detail, sizeof(detail));

        _setValue(_times[i], time[0] ? time : "--:--:--");
        _setValue(_details[i], detail);
    }
}

} // namespace WeatherStation2
