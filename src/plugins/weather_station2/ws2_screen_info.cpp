/**
 * Author: sascha_lammers@gmx.de
 */

#include "ws2_screens.h"

namespace WeatherStation2 {

static constexpr lv_coord_t kRowTop = 34;
static constexpr lv_coord_t kRowStep = 25;
static constexpr lv_coord_t kLabelX = 8;
// the card is kCardWidth wide, the row uses the full width minus kLabelX on both sides
static constexpr lv_coord_t kRowWidth = 212;
static constexpr lv_coord_t kCardWidth = 228;
static constexpr lv_coord_t kKeyValueGap = 8;

// heap and PSRAM as kB/MB
static void _formatBytes(uint32_t value, char *output, size_t size)
{
    if (value >= 1024 * 1024) {
        snprintf(output, size, "%.1f MB", value / (1024.0f * 1024.0f));
    }
    else {
        snprintf(output, size, "%.0f kB", value / 1024.0f);
    }
}

static void _formatRssi(int16_t value, char *output, size_t size)
{
    snprintf(output, size, "%d dBm", static_cast<int>(value));
}

// width of the key column of a card: as wide as the longest key so that every value starts at
// the same offset and the column does not waste any of the row (the value gets the rest)
static lv_coord_t _keyColumnWidth(const char *const *keys, uint8_t count)
{
    lv_coord_t width = 0;
    for (uint8_t i = 0; i < count; i++) {
        const auto textWidth = LVGLUI::getTextWidth(keys[i], LVGLUI::kFontNormal);
        if (textWidth > width) {
            width = textWidth;
        }
    }
    return width;
}

// Sets one value of a row, an unchanged value is left alone. The screen refreshes once per
// second and setting a label again reallocates its buffer and restarts the scroll animation of
// a long value, which would make it jump back to the beginning all the time
static void _setValue(lv_obj_t *label, const char *text, bool allowScroll)
{
    if (!label || strcmp(lv_label_get_text(label), text) == 0) {
        return;
    }
    LVGLUI::fitTextDown(label, text, lv_obj_get_width(label), allowScroll);
}

void InfoScreen::create(lv_obj_t *parent)
{
    _page = LVGLUI::createPage(parent, "System Info");

    // both cards are filled by update(), the values change while the screen is shown (WiFi
    // state, uptime, heap), only the keys are fixed
    auto network = LVGLUI::createCard(parent, kMargin, kContentTop, kCardWidth, kContentHeight, "Network");
    static const char *networkKeys[kNumNetworkValues] = { "Hostname", "SSID", "IP", "Gateway", "DNS 1", "DNS 2" };
    const auto networkKeyWidth = _keyColumnWidth(networkKeys, kNumNetworkValues);
    for (uint8_t i = 0; i < kNumNetworkValues; i++) {
        auto y = static_cast<lv_coord_t>(kRowTop + i * kRowStep);
        _networkValues[i] = LVGLUI::addKeyValue(network, kLabelX, y, kRowWidth, kKeyValueGap, networkKeys[i], "", networkKeyWidth);
        LVGLUI::addSeparator(network, kLabelX, static_cast<lv_coord_t>(y + 17), kRowWidth);
    }

    auto systemCard = LVGLUI::createCard(parent, static_cast<lv_coord_t>(kMargin + kCardWidth + 8), kContentTop,
                                         kCardWidth, kContentHeight, "System");
    static const char *systemKeys[kNumSystemValues] = { "Source", "Firmware", "Uptime", "Free heap", "Free PSRAM", "WiFi RSSI" };
    const auto systemKeyWidth = _keyColumnWidth(systemKeys, kNumSystemValues);
    for (uint8_t i = 0; i < kNumSystemValues; i++) {
        auto y = static_cast<lv_coord_t>(kRowTop + i * kRowStep);
        _systemValues[i] = LVGLUI::addKeyValue(systemCard, kLabelX, y, kRowWidth, kKeyValueGap, systemKeys[i], "", systemKeyWidth);
        LVGLUI::addSeparator(systemCard, kLabelX, static_cast<lv_coord_t>(y + 17), kRowWidth);
    }
}

void InfoScreen::update()
{
    const auto &system = _data.getSystem();
    updateClock();

    // The network and system values are read as they are: copying them into arrays of Strings was
    // a dozen heap allocations per second (the screen refreshes at 1 Hz). The formatted values are
    // written into stack buffers and the members of the model are read through their c_str()
    const char *const network[kNumNetworkValues] = {
        system.hostname.c_str(), system.ssid.c_str(), system.ip.c_str(),
        system.gateway.c_str(), system.dns1.c_str(), system.dns2.c_str(),
    };
    char uptime[DataSource::kFormatSize];
    char freeHeap[24];
    char freePsram[24];
    char rssi[24];
    DataSource::formatUptime(system.uptime, uptime, sizeof(uptime));
    _formatBytes(system.freeHeap, freeHeap, sizeof(freeHeap));
    _formatBytes(system.freePsram, freePsram, sizeof(freePsram));
    if (system.rssi) {
        _formatRssi(system.rssi, rssi, sizeof(rssi));
    }
    else {
        memcpy(rssi, "--", 3);
    }
    const char *const values[kNumSystemValues] = {
        flashStringToCStr(_data.getName()),
        system.firmware.c_str(),
        uptime,
        freeHeap,
        freePsram,
        rssi,
    };
    for (uint8_t i = 0; i < kNumNetworkValues; i++) {
        // a host name and an SSID can be up to 32 characters long, those two scroll instead of
        // being cut off, the other values are short enough for the row
        _setValue(_networkValues[i].value, network[i], i <= 1);
    }
    for (uint8_t i = 0; i < kNumSystemValues; i++) {
        _setValue(_systemValues[i].value, values[i], false);
    }
}

} // namespace WeatherStation2
