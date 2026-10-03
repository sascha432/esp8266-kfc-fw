/**
 * Author: sascha_lammers@gmx.de
 */

#include "ws2_screens.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <StrView.h>

#include "lvgl_plugin.h"

#ifndef DEBUG_WEATHER_STATION2
#    define DEBUG_WEATHER_STATION2 0
#endif

#if DEBUG_WEATHER_STATION2
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

namespace WeatherStation2 {

// ------------------------------------------------------------------------------------------
// layout of the RD6006 style page (480x320, see the mockup
// docs/screens/lvgl_10_power_monitor_480x320.svg). The row of the three readouts uses the area
// below the title, the energy card and the chart share the row below it
// ------------------------------------------------------------------------------------------
static constexpr lv_coord_t kReadoutY = static_cast<lv_coord_t>(kContentTop + kStatusHeight);
static constexpr lv_coord_t kReadoutHeight = 96;
static constexpr lv_coord_t kReadoutX[PowerScreen::kNumGraphSources] = {8, 166, 324};
static constexpr lv_coord_t kReadoutWidth[PowerScreen::kNumGraphSources] = {150, 150, 148};

// the screen has no footer, the energy card and the chart use the rest of the height below the
// readouts (8 px gap above, kMargin below)
static constexpr lv_coord_t kBottomY = static_cast<lv_coord_t>(kReadoutY + kReadoutHeight + 8);
static constexpr lv_coord_t kBottomHeight = static_cast<lv_coord_t>(kScreenHeight - kMargin - kBottomY);

static constexpr lv_coord_t kEnergyX = 8;
static constexpr lv_coord_t kEnergyWidth = 150;
static constexpr lv_coord_t kChartX = 166;
static constexpr lv_coord_t kChartWidth = 306;

// compact state line (channel, source, state) and the channel chips in the band above the
// readouts, the chips are right aligned next to the status line
static constexpr lv_coord_t kStatusY = kContentTop;

// chip of the channel selector
static constexpr lv_coord_t kChipHeight = 22;
static constexpr lv_coord_t kChipWidth = 36;
static constexpr lv_coord_t kChipGap = 6;
static constexpr lv_coord_t kTitleChipY = kContentTop;

// The chart fills its card completely (the card has no title and no chips). The minimum and the
// maximum are overlaid on the left edge
static constexpr lv_coord_t kChartInset = 2;

// the chips are drawn by hand, the colors are the ones of the mockup
static constexpr uint32_t kColorChipActive = 0x2a6ea8;
static constexpr uint32_t kColorChipIdle = 0x1d2731;

// line color of the chart per graph source - it makes the selected source obvious at a glance
static constexpr uint32_t kGraphColor[PowerScreen::kNumGraphSources] = {0x00e0ff, 0xffd042, 0x8ce99a};

// lv_coord_t is 16 bit, LV_CHART_POINT_NONE is INT16_MAX: keep the scaled samples below it
static constexpr float kMaxChartValue = 32000.0f;

static const char *const kReadoutTitles[PowerScreen::kNumGraphSources] = {"Voltage", "Current", "Power"};
static const char *const kReadoutUnits[PowerScreen::kNumGraphSources] = {"V", "A", "W"};

// the chart values are scaled to integers (the chart of LVGL 8.4 only takes lv_coord_t), the
// factor is per quantity so the resolution of the sensor is kept
static float _graphScaleFactor(PowerScreen::GraphSource source)
{
    switch (source) {
    case PowerScreen::GraphSource::VOLTAGE:
        return 100.0f;
    case PowerScreen::GraphSource::CURRENT:
        return 1000.0f;
    default:
        return 10.0f;
    }
}

static lv_coord_t _clampChartValue(float value)
{
    if (value > kMaxChartValue) {
        return static_cast<lv_coord_t>(kMaxChartValue);
    }
    if (value < -kMaxChartValue) {
        return static_cast<lv_coord_t>(-kMaxChartValue);
    }
    return static_cast<lv_coord_t>(lroundf(value));
}

lv_coord_t PowerScreen::_scaleGraphValue(GraphSource source, float value)
{
    return _clampChartValue(value * _graphScaleFactor(source));
}

// ------------------------------------------------------------------------------------------
// chips
// ------------------------------------------------------------------------------------------

void PowerScreen::_createChip(lv_obj_t *parent, Chip &chip, const char *text, lv_coord_t x, lv_coord_t y,
                              lv_event_cb_t callback, uint8_t index)
{
    auto obj = lv_obj_create(parent);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, kChipWidth, kChipHeight);
    lv_obj_set_style_radius(obj, static_cast<lv_coord_t>(kChipHeight / 2), LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN);
    lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);

    chip.obj = obj;
    chip.index = index;
    chip.screen = this;
    // the label is not clickable, LVGL routes the click to the chip below it
    const auto labelHeight = lv_font_get_line_height(LVGLUI::kFontSmall);
    chip.label = LVGLUI::addLabel(obj, 0, static_cast<lv_coord_t>((kChipHeight - labelHeight) / 2), text,
                                  LVGLUI::kFontSmall, LVGLUI::kColorTextMuted, kChipWidth, LV_TEXT_ALIGN_CENTER);
    // PRESSED instead of CLICKED: the chips are small, and CLICKED is only sent when the finger did
    // not move (a scroll gesture cancels it) - the selection is a toggle, so acting on the press is
    // both reliable and more responsive
    lv_obj_add_event_cb(obj, callback, LV_EVENT_PRESSED, &chip);
}

void PowerScreen::_channelChipCallback(lv_event_t *event)
{
    auto chip = static_cast<Chip *>(lv_event_get_user_data(event));
    if (chip && chip->screen) {
        // only the selection is recorded, update() (main loop) rebuilds/applies it. An LVGL
        // event callback must not modify the tree it is dispatching from
        chip->screen->_pendingChannel = static_cast<int8_t>(chip->index);
        __LDBG_printf("channel chip %u pressed", static_cast<unsigned>(chip->index));
    }
}

void PowerScreen::_readoutCardCallback(lv_event_t *event)
{
    // Tapping one of the three readouts selects the graph source - they are the only selector now,
    // the chart card is a plain chart without a title and without chips
    auto data = static_cast<ReadoutData *>(lv_event_get_user_data(event));
    if (data && data->screen) {
        data->screen->_pendingGraphSource = static_cast<int8_t>(data->index);
        __LDBG_printf("readout %u tapped", static_cast<unsigned>(data->index));
    }
}

void PowerScreen::_createChannelChips(lv_obj_t *parent)
{
    _channelChipCount = 0;
    const auto &power = _data.getPower();
    const auto count = power.getCount();
    if (count < 2) {
        // a single channel needs no selector
        return;
    }
    auto x = static_cast<lv_coord_t>(kScreenWidth - kMargin - (count * kChipWidth + (count - 1) * kChipGap));
    uint8_t number = 0;
    for (uint8_t i = 0; i < PowerChannels::kNumChannels; i++) {
        if (!power.values[i].configured) {
            continue;
        }
        String text(static_cast<unsigned>(number + 1));
        _createChip(parent, _channelChips[_channelChipCount], text.c_str(), x, kTitleChipY, _channelChipCallback, i);
        _channelChipCount++;
        x = static_cast<lv_coord_t>(x + kChipWidth + kChipGap);
        number++;
    }
}

void PowerScreen::_updateChips()
{
    // the channel chips only exist for more than one configured channel, the objects are NULL
    // otherwise and the LVGL style setters do not accept a NULL object
    for (uint8_t i = 0; i < _channelChipCount; i++) {
        auto &chip = _channelChips[i];
        if (!chip.obj) {
            continue;
        }
        const bool active = (chip.index == _channel);
        lv_obj_set_style_bg_color(chip.obj, lv_color_hex(active ? kColorChipActive : kColorChipIdle), LV_PART_MAIN);
        lv_obj_set_style_border_color(chip.obj, lv_color_hex(active ? LVGLUI::kColorAccent : LVGLUI::kColorBorder), LV_PART_MAIN);
        lv_obj_set_style_text_color(chip.label, lv_color_hex(active ? LVGLUI::kColorText : LVGLUI::kColorTextMuted), LV_PART_MAIN);
    }
}

// ------------------------------------------------------------------------------------------
// cards
// ------------------------------------------------------------------------------------------

void PowerScreen::_buildCards(lv_obj_t *parent)
{
    const auto &power = _data.getPower();
    _lastChannelCount = power.getCount();

    if (_lastChannelCount == 0) {
        // no channel configured: a clear message instead of zeros, the same pattern the curated
        // art screen uses for an empty directory
        auto card = LVGLUI::createCard(parent, kMargin, kReadoutY, kScreenWidth - 2 * kMargin, kContentHeight, "Power Monitor");
        auto label = LVGLUI::addLabel(card, 20, 60, "No power channel configured.\nAdd a channel in the \"Power Monitor\" group of the weather2 form.",
                                      LVGLUI::kFontMedium, LVGLUI::kColorTextMuted,
                                      static_cast<lv_coord_t>(kScreenWidth - 2 * kMargin - 40), LV_TEXT_ALIGN_CENTER);
        lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
        return;
    }

    // the three large readouts. The one matching the graph source is outlined, tapping a readout
    // selects it as well (the large target, the V/A/W chips are the compact one)
    for (uint8_t i = 0; i < kNumGraphSources; i++) {
        auto card = LVGLUI::createCard(parent, kReadoutX[i], kReadoutY, kReadoutWidth[i], kReadoutHeight);
        _readoutCards[i] = card;
        LVGLUI::addLabel(card, 12, 12, kReadoutTitles[i], LVGLUI::kFontNormal, LVGLUI::kColorTextLabel, static_cast<lv_coord_t>(kReadoutWidth[i] / 2));
        _readoutUnits[i] = LVGLUI::addLabel(card, 12, 10, kReadoutUnits[i], LVGLUI::kFontMedium, LVGLUI::kColorTextMuted,
                                            static_cast<lv_coord_t>(kReadoutWidth[i] - 24), LV_TEXT_ALIGN_RIGHT);
        _readoutValues[i] = LVGLUI::addLabel(card, 12, 44, "--", LVGLUI::kFontHuge, LVGLUI::kColorText, static_cast<lv_coord_t>(kReadoutWidth[i] - 24));
        _readoutData[i].screen = this;
        _readoutData[i].index = i;
        lv_obj_add_event_cb(card, _readoutCardCallback, LV_EVENT_CLICKED, &_readoutData[i]);
    }

    // the energy card: only the total counter of the source, the local INA219 has none
    _energyCard = LVGLUI::createCard(parent, kEnergyX, kBottomY, kEnergyWidth, kBottomHeight, "Energy");
    _energyUnit = LVGLUI::addLabel(_energyCard, 12, 8, "kWh", LVGLUI::kFontNormal, LVGLUI::kColorTextLabel,
                                   static_cast<lv_coord_t>(kEnergyWidth - 24), LV_TEXT_ALIGN_RIGHT);
    _energyValue = LVGLUI::addLabel(_energyCard, 12, 58, "--", LVGLUI::kFontValue, LVGLUI::kColorText, static_cast<lv_coord_t>(kEnergyWidth - 24));
    _energyState = LVGLUI::addLabel(_energyCard, 12, 104, "", LVGLUI::kFontSmall, LVGLUI::kColorTextMuted, static_cast<lv_coord_t>(kEnergyWidth - 24));

    // the chart card is a plain chart that fills the whole card - no title and no chips, the
    // readouts above select the source that is displayed
    _chartCard = LVGLUI::createCard(parent, kChartX, kBottomY, kChartWidth, kBottomHeight);

    _chart = lv_chart_create(_chartCard);
    lv_obj_set_pos(_chart, kChartInset, kChartInset);
    lv_obj_set_size(_chart, static_cast<lv_coord_t>(kChartWidth - 2 * kChartInset), static_cast<lv_coord_t>(kBottomHeight - 2 * kChartInset));
    lv_obj_set_style_bg_opa(_chart, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(_chart, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_chart, 0, LV_PART_MAIN);
    lv_obj_set_style_line_color(_chart, lv_color_hex(LVGLUI::kColorBorder), LV_PART_MAIN);
    lv_obj_set_style_width(_chart, 0, LV_PART_INDICATOR);
    lv_obj_set_style_height(_chart, 0, LV_PART_INDICATOR);
    lv_obj_set_style_line_width(_chart, 2, LV_PART_ITEMS);
    lv_obj_set_style_line_color(_chart, lv_color_hex(LVGLUI::kColorAccent), LV_PART_ITEMS);
    lv_chart_set_type(_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_div_line_count(_chart, 4, 5);
    // the point count has to be set before the series is added, the series allocates its array
    // with the current count
    lv_chart_set_point_count(_chart, kChartPoints);
    // ONE series for the whole lifetime of the chart, the graph source only changes the data (the
    // color is the same for all three). lv_chart_remove_series() must never be used:
    // lv_chart_add_series() does not initialize x_points/x_ext_buf_assigned of a non scatter
    // series (LVGL 8.4, lv_chart.c:355-370), so the pointer is heap garbage and removing the series
    // frees it - StoreProhibited in lv_tlsf_free/insert_free_block (lv_chart.c:391)
    _chartSeries = lv_chart_add_series(_chart, lv_color_hex(LVGLUI::kColorAccent), LV_CHART_AXIS_PRIMARY_Y);
    if (_chartSeries) {
        _chartSeries->x_points = nullptr;
        _chartSeries->x_ext_buf_assigned = 0;
    }
    // overlaid on the chart (created after it, so they are drawn on top), small and muted
    _chartMax = LVGLUI::addLabel(_chartCard, 6, 4, "", LVGLUI::kFontSmall, LVGLUI::kColorTextMuted, 30);
    _chartMin = LVGLUI::addLabel(_chartCard, 6, static_cast<lv_coord_t>(kBottomHeight - 19), "", LVGLUI::kFontSmall, LVGLUI::kColorTextMuted, 30);
    _updateChart();
}

// ------------------------------------------------------------------------------------------
// history (PSRAM)
// ------------------------------------------------------------------------------------------

PowerScreen::~PowerScreen()
{
    if (_history) {
        // the block was allocated with ps_malloc() (or malloc() when there is no PSRAM), free()
        // routes it back to the right heap
        free(_history);
        _history = nullptr;
    }
}

float *PowerScreen::_historySeries(uint8_t channel, uint8_t source) const
{
    // [channel][series][bucket], the block of one series is kChartPoints floats long
    return _history + (static_cast<size_t>(channel) * kNumGraphSources + source) * kChartPoints;
}

void PowerScreen::_clearHistory()
{
    memset(_historyCount, 0, sizeof(_historyCount));
    memset(_bucketSamples, 0, sizeof(_bucketSamples));
}

void PowerScreen::_setGraphMinutes(uint8_t minutes)
{
    if (minutes < kMinPowerGraphMinutes) {
        minutes = kMinPowerGraphMinutes;
    }
    else if (minutes > kMaxPowerGraphMinutes) {
        minutes = kMaxPowerGraphMinutes;
    }
    if (minutes == _historyMinutes) {
        return;
    }
    // The samples of the previous window were merged into longer or shorter slices, the data cannot
    // be reused. One bucket holds `minutes` samples (`minutes` x 200 ms = window / kChartPoints)
    _historyMinutes = minutes;
    _samplesPerBucket = minutes;
    _clearHistory();
}

void PowerScreen::_allocateHistory()
{
    if (_history) {
        return; // the block is fixed, only the length of a slice changes with the window
    }
    // 4 channels x 3 series x 300 buckets x 4 bytes = 14,400 bytes
    const auto size = static_cast<size_t>(PowerChannels::kNumChannels) * kNumGraphSources * kChartPoints * sizeof(float);
    if (psramFound()) {
        _history = static_cast<float *>(ps_malloc(size));
    }
    if (!_history) {
        // without PSRAM it has to fit into the internal RAM
        _history = static_cast<float *>(malloc(size));
        if (!_history) {
            __LDBG_printf_E("cannot allocate %u bytes for the power history", static_cast<unsigned>(size));
            return;
        }
    }
    // a channel that was not filled yet must not show garbage
    memset(_history, 0, size);
    __LDBG_printf("power history: %u bucket(s) per channel, %u bytes", static_cast<unsigned>(kChartPoints), static_cast<unsigned>(size));
}

void PowerScreen::_pushSample(uint8_t channel, const PowerValues &value)
{
    // while the source is offline the last sample stays visible (as the muted/error colored
    // readout), but it is not added to the history again - the chart would show a flat line that
    // was never measured
    if (!_history || !value.available || !value.online) {
        return;
    }
    auto &count = _historyCount[channel];
    auto &samples = _bucketSamples[channel];

    if (count == 0) {
        // the first sample of this channel opens the first bucket
        count = 1;
    }
    else if (samples >= _samplesPerBucket) {
        // the bucket that was filled is complete
        if (count < kChartPoints) {
            count++;
        }
        else {
            // the ring is full, the oldest bucket is dropped (the newest is always the last one)
            for (uint8_t i = 0; i < kNumGraphSources; i++) {
                auto *series = _historySeries(channel, i);
                memmove(series, series + 1, static_cast<size_t>(kChartPoints - 1) * sizeof(float));
            }
        }
        samples = 0;
    }

    // The bucket that is filled at the moment is the newest one and holds the average of its
    // samples. The running mean keeps the value usable while the slice is still open (it only moves
    // towards the average of the slice instead of jumping like a peak would)
    const auto bucket = static_cast<uint16_t>(count - 1);
    const auto sampleCount = ++samples;
    // the order matches GraphSource (VOLTAGE, CURRENT, POWER)
    const float sample[kNumGraphSources] = { value.voltage, value.current, value.power };
    for (uint8_t i = 0; i < kNumGraphSources; i++) {
        auto *average = _historySeries(channel, i) + bucket;
        if (sampleCount == 1) {
            *average = sample[i];
        }
        else {
            *average += (sample[i] - *average) / sampleCount;
        }
    }
}

// ------------------------------------------------------------------------------------------
// chart
// ------------------------------------------------------------------------------------------

void PowerScreen::_updateChart()
{
    if (!_chart || !_chartSeries) {
        return;
    }
    const auto source = static_cast<uint8_t>(_graphSource);
    // the color follows the source, it is set with the safe setter (the series is never removed,
    // see _buildCards)
    lv_chart_set_series_color(_chart, _chartSeries, lv_color_hex(kGraphColor[source]));

    const auto count = _history ? _historyCount[_channel] : 0;
    // The slice that is filled at the moment is not drawn - its average still changes until the
    // slice is complete. The newest column of the chart is always a complete slice, so the curve
    // does not wobble at the right edge
    auto drawn = count;
    if (drawn && _bucketSamples[_channel] < _samplesPerBucket) {
        drawn--;
    }
    if (drawn == 0) {
        // no complete slice of the selected channel yet
        lv_chart_set_all_value(_chart, _chartSeries, LV_CHART_POINT_NONE);
        lv_chart_set_range(_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 1);
        lv_chart_refresh(_chart);
        LVGLUI::setText(_chartMax, "", LVGLUI::kFontSmall, LVGLUI::kColorTextMuted);
        LVGLUI::setText(_chartMin, "", LVGLUI::kFontSmall, LVGLUI::kColorTextMuted);
        return;
    }

    // Auto scale from the averages of the window plus ~10% margin, the labels show the minimum and
    // the maximum of the window
    const auto *series = _historySeries(_channel, source);
    auto min = series[0];
    auto max = min;
    for (uint16_t i = 1; i < drawn; i++) {
        const auto value = series[i];
        if (value < min) {
            min = value;
        }
        if (value > max) {
            max = value;
        }
    }
    const auto margin = (max - min) * 0.1f;
    auto low = _scaleGraphValue(_graphSource, min - margin);
    auto high = _scaleGraphValue(_graphSource, max + margin);
    if (static_cast<int32_t>(high) - static_cast<int32_t>(low) < 4) {
        // a flat line would fill the whole chart, keep a minimum span
        low = static_cast<lv_coord_t>(low - 2);
        high = static_cast<lv_coord_t>(high + 2);
    }
    lv_chart_set_range(_chart, LV_CHART_AXIS_PRIMARY_Y, low, high);

    // one average per column, the newest complete slice is at the right edge. A partly filled
    // history starts at the right edge as well (the chart fills up over the window time)
    const auto first = static_cast<uint16_t>(kChartPoints - drawn);
    for (uint16_t p = 0; p < kChartPoints; p++) {
        auto value = static_cast<lv_coord_t>(LV_CHART_POINT_NONE);
        if (p >= first) {
            value = _scaleGraphValue(_graphSource, series[p - first]);
        }
        lv_chart_set_value_by_id(_chart, _chartSeries, p, value);
    }
    lv_chart_refresh(_chart);

    char valueText[24];
    _formatGraphValue(max, valueText, sizeof(valueText));
    LVGLUI::setText(_chartMax, valueText, LVGLUI::kFontSmall, LVGLUI::kColorTextMuted);
    _formatGraphValue(min, valueText, sizeof(valueText));
    LVGLUI::setText(_chartMin, valueText, LVGLUI::kFontSmall, LVGLUI::kColorTextMuted);
}

void PowerScreen::_formatGraphValue(float value, char *output, size_t size) const
{
    switch (_graphSource) {
    case GraphSource::VOLTAGE:
        _data.formatVoltage(value, output, size);
        break;
    case GraphSource::CURRENT:
        _data.formatCurrent(value, output, size);
        break;
    default:
        _data.formatPower(value, output, size);
        break;
    }
}

// ------------------------------------------------------------------------------------------
// values
// ------------------------------------------------------------------------------------------

// lv_label_set_text() shortcuts on the pointer identity only, so the c_str() of a freshly built
// String reallocates the label buffer and restarts a scroll animation although the text is the same
// - the screens refresh once per second. This applies the font and the color always and the text
// only when it differs (same pattern as the value helper of the info screen). Returns true when the
// text changed, i.e. when the caller has to fit the font again (fitTextDown also sets the text)
static bool _setTextIfChanged(lv_obj_t *label, const char *text, const lv_font_t *font, uint32_t color)
{
    if (!label) {
        return false;
    }
    text = text ? text : "";
    if (strcmp(lv_label_get_text(label), text) == 0) {
        lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
        lv_obj_set_style_text_color(label, lv_color_hex(color), LV_PART_MAIN);
        return false;
    }
    LVGLUI::setText(label, text, font, color);
    return true;
}

void PowerScreen::_updateValues(const PowerValues &value)
{
    const auto state = value.getState();
    const auto color = toIndoorColor(state);

    // the readouts are formatted into stack buffers: the screen refreshes at 5 fps and a String
    // per readout would be one heap allocation per card and tick
    char texts[kNumGraphSources][24];
    if (value.available) {
        _data.formatVoltage(value.voltage, texts[static_cast<uint8_t>(GraphSource::VOLTAGE)], sizeof(texts[0]));
        _data.formatCurrent(value.current, texts[static_cast<uint8_t>(GraphSource::CURRENT)], sizeof(texts[0]));
        _data.formatPower(value.power, texts[static_cast<uint8_t>(GraphSource::POWER)], sizeof(texts[0]));
    }
    else {
        for (auto &text : texts) {
            memcpy(text, "--", 3);
        }
    }

    for (uint8_t i = 0; i < kNumGraphSources; i++) {
        const bool active = (static_cast<GraphSource>(i) == _graphSource);
        // the font is reset only when the text changed - fitTextDown only shrinks it (a value can
        // become longer while the screen is visible, e.g. after switching the channel)
        if (_setTextIfChanged(_readoutValues[i], texts[i], LVGLUI::kFontHuge, color)) {
            LVGLUI::fitTextDown(_readoutValues[i], texts[i], 0);
        }
        lv_obj_set_style_border_color(_readoutCards[i], lv_color_hex(active ? LVGLUI::kColorAccent : LVGLUI::kColorBorder), LV_PART_MAIN);
        lv_obj_set_style_border_width(_readoutCards[i], active ? 2 : 1, LV_PART_MAIN);
        _setTextIfChanged(_readoutUnits[i], kReadoutUnits[i], LVGLUI::kFontMedium, active ? LVGLUI::kColorAccent : LVGLUI::kColorTextMuted);
    }

    // the energy counter of the source (total only). The local INA219 has none and the firmware
    // never accumulates one, so the card says so instead of showing a made up number
    if (value.hasEnergy) {
        char text[40];
        _data.formatEnergy(value.energy, text, sizeof(text));
        if (_setTextIfChanged(_energyValue, text, LVGLUI::kFontValue, LVGLUI::kColorText)) {
            LVGLUI::fitTextDown(_energyValue, text, 0);
        }
        _setTextIfChanged(_energyState, "total counter", LVGLUI::kFontSmall, LVGLUI::kColorTextMuted);
    }
    else {
        _setTextIfChanged(_energyValue, "--", LVGLUI::kFontValue, LVGLUI::kColorTextMuted);
        _setTextIfChanged(_energyState,
                          (value.source == PowerSourceType::LOCAL) ? "not available (INA219)" : "no counter",
                          LVGLUI::kFontSmall, LVGLUI::kColorTextMuted);
    }
}

// ------------------------------------------------------------------------------------------
// screen
// ------------------------------------------------------------------------------------------

void PowerScreen::create(lv_obj_t *parent)
{
    _page = LVGLUI::createPage(parent, "Power / Energy Monitor");

    // create() runs again when the manager reloads the screen. The manager cleaned the screen
    // before, so every handle of the previous tree is dangling - drop them all (a chip that does
    // not exist any more must not be touched by _updateChips())
    _content = nullptr;
    _chart = nullptr;
    _chartSeries = nullptr;
    _chartCard = nullptr;
    _chartMax = nullptr;
    _chartMin = nullptr;
    _energyCard = nullptr;
    _energyValue = nullptr;
    _energyUnit = nullptr;
    _energyState = nullptr;
    _status = nullptr;
    for (uint8_t i = 0; i < kNumGraphSources; i++) {
        _readoutCards[i] = nullptr;
        _readoutValues[i] = nullptr;
        _readoutUnits[i] = nullptr;
    }
    for (uint8_t i = 0; i < PowerChannels::kNumChannels; i++) {
        _channelChips[i].obj = nullptr;
        _channelChips[i].label = nullptr;
    }
    _channelChipCount = 0;

    _content = LVGLUI::createContainer(parent, 0, 0, kScreenWidth, kScreenHeight);

    // keep the selected channel of the previous tree, fall back to the first configured one
    const auto &power = _data.getPower();
    if (!power.get(_channel).configured) {
        const auto index = power.getConfiguredIndex(0);
        _channel = (index >= 0) ? static_cast<uint8_t>(index) : 0;
    }
    _pendingChannel = -1;

    // the per channel history lives in PSRAM, its size does not depend on the window
    if (power.getCount()) {
        _setGraphMinutes(_data.getPowerGraphMinutes());
        _allocateHistory();
    }

    _createChannelChips(parent);
    _buildCards(_content);
    _updateChips();

    // Compact state line (channel, source, state) in the band below the top bar instead of a
    // footer bar - the cards use the rest of the screen. The chips of the channel selector sit
    // next to it, the text stops before them
    _status = LVGLUI::addLabel(parent, 10, kStatusY, "", LVGLUI::kFontSmall, LVGLUI::kColorTextMuted,
                               static_cast<lv_coord_t>(kScreenWidth - 20 - (WEATHER_STATION2_NUM_POWER_CHANNELS * (kChipWidth + kChipGap))));
}

void PowerScreen::update()
{
    const auto now = millis();

    // the clock of the top bar only changes once per second
    if (static_cast<uint32_t>(now - _lastClockUpdate) >= kClockInterval) {
        _lastClockUpdate = now;
        updateClock();
    }

    const auto &power = _data.getPower();
    // the configuration can change while the screen is visible (form save -> reconfigure), the
    // number of channels decides whether a selector exists
    if (power.getCount() != _lastChannelCount) {
        // the channels may be different now, the history of the previous ones is meaningless
        _clearHistory();
        LVGLPlugin::screens().reload();
        return;
    }

    // the window is configuration as well - a change only drops the data, the storage is a fixed
    // ring of one bucket per column of the chart
    if (_data.getPowerGraphMinutes() != _historyMinutes) {
        _setGraphMinutes(_data.getPowerGraphMinutes());
    }
    if (power.getCount() && !_history) {
        _allocateHistory();
    }

    // apply the selection of a chip (update() runs from the main loop tick, an LVGL event
    // callback only recorded it)
    if (_pendingChannel >= 0) {
        const auto channel = static_cast<uint8_t>(_pendingChannel);
        _pendingChannel = -1;
        if (channel != _channel) {
            _channel = channel;
            __LDBG_printf("power channel=%u", static_cast<unsigned>(_channel));
        }
    }
    if (_pendingGraphSource >= 0) {
        // the series are not recreated, _updateChart() (called at the end of the tick) re-fills them
        // from the stored history of the new source
        _graphSource = static_cast<GraphSource>(_pendingGraphSource);
        _pendingGraphSource = -1;
        __LDBG_printf("graph source=%u", static_cast<unsigned>(_graphSource));
    }
    _updateChips();

    if (_lastChannelCount == 0) {
        // create() shows the message, only the clock is refreshed
        return;
    }

    const auto &value = power.get(_channel);

    // Every configured channel is sampled on every tick (getRefreshInterval() = 5 fps): the samples
    // of 200 ms are merged into the bucket of the selected window, so a 1 minute window (=300
    // samples of 200 ms) uses the full width of the chart. A channel that is not displayed keeps
    // its history and shows a complete graph when it is selected
    for (uint8_t i = 0; i < PowerChannels::kNumChannels; i++) {
        if (power.values[i].configured) {
            _pushSample(i, power.values[i]);
        }
    }
    _updateChart();
    _updateValues(value);

    // footer: the selected channel, its source and the state. The text is kept stable (no sample
    // counter or age), so the labels are only reallocated when something really changed. It is
    // composed into the member of the screen - the String keeps its buffer and the 5 fps tick does
    // not allocate
    char channelName[32];
    getPowerChannelPart(_channel, 1, channelName, sizeof(channelName));
    auto &statusText = _statusText;
    statusText.clear();
    StrWrapper(statusText).printf("Channel %u/%u  %s", static_cast<unsigned>(_channelPosition() + 1),
                                  static_cast<unsigned>(_lastChannelCount), channelName);
    statusText += "  -  ";
    statusText += getPowerSourceTypeName(value.source);
    if (value.source == PowerSourceType::REMOTE) {
        StrWrapper(statusText).printf(" #%u", static_cast<unsigned>(value.remoteChannelId));
    }
    const auto *stateText = _data.getPowerStateText(value);
    if (stateText[0]) {
        statusText += "  -  ";
        statusText += stateText;
    }
    // the connection state and the error of the last attempt only while the source is not
    // delivering - see appendPowerRemoteStatus()
    if (value.source == PowerSourceType::REMOTE && !value.online) {
        const auto length = statusText.length();
        statusText += "  -  ";
        _data.appendPowerRemoteStatus(statusText);
        if (statusText.length() == (length + 5)) {
            // nothing was appended: drop the separator again
            statusText.remove(length);
        }
    }
    if (_setTextIfChanged(_status, statusText.c_str(), LVGLUI::kFontSmall, stateText[0] ? LVGLUI::kColorError : LVGLUI::kColorTextMuted)) {
        LVGLUI::fitTextDown(_status, statusText.c_str(), 0, true);
    }
}

uint8_t PowerScreen::_channelPosition() const
{
    // position of the selected channel among the configured ones (for "Channel 2/3")
    const auto &power = _data.getPower();
    uint8_t position = 0;
    for (uint8_t i = 0; i < PowerChannels::kNumChannels; i++) {
        if (!power.values[i].configured) {
            continue;
        }
        if (i == _channel) {
            return position;
        }
        position++;
    }
    return position;
}

} // namespace WeatherStation2
