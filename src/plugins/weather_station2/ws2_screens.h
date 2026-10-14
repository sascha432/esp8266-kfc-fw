/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

// Screens of the weather station plugin 2.x. Each screen is a LVGLScreen that is registered
// with the screen manager of the lvgl plugin, the common widgets come from LVGLUI:
//
//   - MainScreen      local weather, indoor strip in the footer
//   - IndoorScreen    indoor climate (temperature, humidity, pressure, gas)
//   - ForecastScreen  one card per forecast day (up to 5) and the four parts of the day
//                     (morning, noon, afternoon, night), they alternate every kLayoutTime and a
//                     swipe up/down switches immediately
//   - WorldClockScreen the local clock and one row per enabled clock of the weather
//                     configuration (name, POSIX time zone and time format per clock), the same
//                     configuration the "World Clock" form of the 1.x plugin writes
//   - MoonPhaseScreen moon disc, phase and the four next phases
//   - PowerScreen     the power monitor channels (RD6006 style: voltage/current/power readouts,
//                     total energy and a 5 minute chart with a V/A/W selector)
//   - InfoScreen      network and system values
//
// The screens only read from WeatherStation2::DataSource, they never touch a sensor or the
// network themselves.

#include <Arduino_compat.h>

#include "global.h"

#include <lvgl.h>
#include "lvgl_screen.h"
#include "lvgl_ui.h"
#include <EventScheduler.h>
#include "ws2_data.h"

namespace WeatherStation2 {

// ------------------------------------------------------------------------------------------
// layout (WT32-SC01, 480x320): the top bar and the title use the first 78 px, the footer
// strip the last 46 px, everything between is the content area of the screens
// ------------------------------------------------------------------------------------------
static constexpr lv_coord_t kScreenWidth = IOT_WT32_SC01_TFT_WIDTH;
static constexpr lv_coord_t kScreenHeight = IOT_WT32_SC01_TFT_HEIGHT;
// the top bar holds the title and the clock, the content starts right below it
static constexpr lv_coord_t kContentTop = 44;
// screens that draw a status line get a band of this height above their content
static constexpr lv_coord_t kStatusHeight = 16;
static constexpr lv_coord_t kContentBottom = kScreenHeight - LVGLUI::kFooterHeight - 6;
static constexpr lv_coord_t kContentHeight = kContentBottom - kContentTop;
static constexpr lv_coord_t kMargin = 8;

// maps the weather condition to the icon the UI draws
static inline LVGLUI::IconType toIconType(WeatherIcon icon)
{
    switch (icon) {
        case WeatherIcon::SUN:
            return LVGLUI::IconType::SUN;
        case WeatherIcon::PARTLY_CLOUDY:
            return LVGLUI::IconType::PARTLY_CLOUDY;
        case WeatherIcon::CLOUDY:
            return LVGLUI::IconType::CLOUDY;
        case WeatherIcon::RAIN:
            return LVGLUI::IconType::RAIN;
        case WeatherIcon::SNOW:
            return LVGLUI::IconType::SNOW;
        case WeatherIcon::STORM:
            return LVGLUI::IconType::STORM;
        case WeatherIcon::FOG:
            return LVGLUI::IconType::FOG;
        default:
            return LVGLUI::IconType::UNKNOWN;
    }
}

// maps the state of an indoor metric to the color of its value, "offline" is an error and a
// missing value is muted
static inline uint32_t toIndoorColor(MetricState state, uint32_t normalColor = LVGLUI::kColorText)
{
    switch (state) {
        case MetricState::HAS_VALUE:
            return normalColor;
        case MetricState::SOURCE_OFFLINE:
            return LVGLUI::kColorError;
        default:
            return LVGLUI::kColorTextMuted;
    }
}

// common part of the screens: the data source and the page handle of the top bar
class Screen : public LVGLScreen {
public:
    explicit Screen(DataSource &data) : _data(data) {}

protected:
    // refreshes the date/time/timezone of the top bar
    void updateClock() {
        LVGLUI::setClock(_page, _data.isTimeFormat24h());
    }

protected:
    DataSource &_data;
    LVGLUI::PageRefs _page;
};

class MainScreen : public Screen {
public:
    explicit MainScreen(DataSource &data) :
        Screen(data),
        _card(nullptr),
        _icon(nullptr),
        _iconType(LVGLUI::IconType::UNKNOWN),
        _location(nullptr),
        _temperature(nullptr),
        _description(nullptr),
        _details(nullptr),
        _extra(nullptr),
        _status(nullptr),
        _indoorTemperature(nullptr),
        _indoorHumidity(nullptr),
        _indoorPressure(nullptr)
    {
    }

    virtual const char *getName() const override {
        return "MAIN";
    }
    virtual const char *getTitle() const override {
        return "Weather";
    }
    virtual LVGLUI::IconType getIcon() const override {
        return LVGLUI::IconType::PARTLY_CLOUDY;
    }
    virtual uint32_t getScreenTime() const override {
        return 10;
    }
    virtual void create(lv_obj_t *parent) override;
    virtual void update() override;

private:
    lv_obj_t *_card;
    lv_obj_t *_icon;
    LVGLUI::IconType _iconType;
    lv_obj_t *_location;
    lv_obj_t *_temperature;
    lv_obj_t *_description;
    lv_obj_t *_details;
    lv_obj_t *_extra;
    // state message shown instead of the values while there is no real weather data
    lv_obj_t *_status;
    lv_obj_t *_indoorTemperature;
    lv_obj_t *_indoorHumidity;
    lv_obj_t *_indoorPressure;
    // buffer of the state message (getWeatherStatusText()): it is read once per second and the
    // String keeps its capacity, so nothing is allocated per tick
    String _statusText;
};

class IndoorScreen : public Screen {
public:
    explicit IndoorScreen(DataSource &data) :
        Screen(data),
        _rows{},
        _values{},
        _sensorLabel(nullptr)
    {
    }

    virtual const char *getName() const override {
        return "INDOOR";
    }
    virtual const char *getTitle() const override {
        return "Indoor";
    }
    virtual LVGLUI::IconType getIcon() const override {
        return LVGLUI::IconType::HOUSE;
    }
    virtual uint32_t getScreenTime() const override {
        return 10;
    }
    virtual void create(lv_obj_t *parent) override;
    virtual void update() override;

private:
    // one card per indoor metric, see IndoorValues
    lv_obj_t *_rows[IndoorValues::kNumMetrics];
    lv_obj_t *_values[IndoorValues::kNumMetrics];
    lv_obj_t *_sensorLabel;
};

class ForecastScreen : public Screen {
public:
    // One layout is displayed for this long, then the screen switches to the other one, so the
    // forecast days and the parts of the day alternate while the screen is visible
    static constexpr uint32_t kLayoutTime = 10 * 1000;

    explicit ForecastScreen(DataSource &data) :
        Screen(data),
        _layout(Layout::DAYS),
        _content(nullptr),
        _lastSwitch(0)
    {
    }

    virtual const char *getName() const override {
        return "FORECAST";
    }
    virtual const char *getTitle() const override {
        return "Forecast";
    }
    virtual LVGLUI::IconType getIcon() const override {
        return LVGLUI::IconType::CALENDAR;
    }
    virtual uint32_t getScreenTime() const override {
        // both layouts, one kLayoutTime each
        return 2 * (kLayoutTime / 1000);
    }
    virtual void create(lv_obj_t *parent) override;
    virtual void update() override;
    // a swipe up/down switches between the forecast days and the four parts of the day before the
    // timer does, the two layouts are the same page with a different content
    virtual bool onSwipe(SwipeDirection direction) override;

private:
    // content of the page, switched with a swipe up/down and by kLayoutTime
    enum class Layout : uint8_t {
        DAYS = 0,
        DAY_PARTS,
    };

    // builds the cards of the current layout below _content
    void _createCards();
    // clears the content and builds the other layout, used by update() and a swipe
    void _switchLayout();
    // one card per forecast day (up to 5)
    void _createDayCards();
    // one card per part of the day (morning, noon, afternoon, night), the values and the icon are
    // the hourly forecast of the hour of the part of the day
    void _createDayPartCards();
    // message of the data source instead of values while the weather data is missing
    void _showStatus();

    Layout _layout;
    // container of the cards. Only its children are rebuilt when the layout changes, the page
    // (top bar, title) around them stays
    lv_obj_t *_content;
    // when the current layout was built, used by update()
    uint32_t _lastSwitch;
};

class WorldClockScreen : public Screen {
public:
    // the local clock row plus one row per configured clock
    static constexpr uint8_t kMaxRows = WEATHER_STATION_MAX_CLOCKS + 1;

    explicit WorldClockScreen(DataSource &data) :
        Screen(data),
        _count(0),
        _signature(0),
        _content(nullptr),
        _rows{},
        _names{},
        _times{},
        _details{},
        _status(nullptr)
    {
    }

    virtual const char *getName() const override {
        return "WORLD_CLOCK";
    }
    virtual const char *getTitle() const override {
        return "World Clock";
    }
    virtual LVGLUI::IconType getIcon() const override {
        return LVGLUI::IconType::GLOBE;
    }
    virtual uint32_t getScreenTime() const override {
        return 10;
    }
    virtual void create(lv_obj_t *parent) override;
    virtual void update() override;

private:
    // one clock of the screen. The local clock uses the time zone of the device, the additional
    // clocks carry the POSIX time zone of the configuration
    struct Clock {
        Clock() :
            format24h(true)
        {
        }
        String name;
        String tz;
        bool format24h;
    };

    // reads the clocks of the weather configuration into _clocks/_count and returns true when
    // the rows have to be rebuilt (the number of rows or their time format changed)
    bool _readClocks();
    // (re)builds the rows below _content for the clocks read by _readClocks()
    void _buildRows();
    // hides the rows and shows a message instead (no clock configured or the clock is not set)
    void _showStatus(const char *text);

    Clock _clocks[kMaxRows];
    uint8_t _count;
    // signature of the rows _buildRows() created (the local clock, the enabled clocks and the
    // 24h/12h format per row), used to rebuild them only when the configuration changed
    uint16_t _signature;
    // container of the rows, only its children are rebuilt
    lv_obj_t *_content;
    lv_obj_t *_rows[kMaxRows];
    lv_obj_t *_names[kMaxRows];
    lv_obj_t *_times[kMaxRows];
    lv_obj_t *_details[kMaxRows];
    // shown instead of the clocks while the clock (NTP) is not set or nothing is configured
    lv_obj_t *_status;
};

class MoonPhaseScreen : public Screen {
public:
    explicit MoonPhaseScreen(DataSource &data) :
        Screen(data),
        _card(nullptr),
        _moon(nullptr),
        _phase(nullptr),
        _details(nullptr),
        _rows{},
        _phaseNames{},
        _phaseDates{},
        _status(nullptr)
    {
    }

    virtual const char *getName() const override {
        return "MOON_PHASE";
    }
    virtual const char *getTitle() const override {
        return "Moon";
    }
    virtual LVGLUI::IconType getIcon() const override {
        return LVGLUI::IconType::MOON;
    }
    virtual uint32_t getScreenTime() const override {
        return 10;
    }
    virtual void create(lv_obj_t *parent) override;
    virtual void update() override;

private:
    // moon card with the disc and the values, hidden while the clock is not set
    lv_obj_t *_card;
    lv_obj_t *_moon;
    lv_obj_t *_phase;
    lv_obj_t *_details;
    // one card per phase, hidden while the clock is not set
    lv_obj_t *_rows[MoonInfo::kNumPhases];
    lv_obj_t *_phaseNames[MoonInfo::kNumPhases];
    lv_obj_t *_phaseDates[MoonInfo::kNumPhases];
    // shown instead of the moon while the clock (NTP) is not set
    lv_obj_t *_status;
};

// Power / energy monitor (RD6006 style). One configured channel is displayed at a time - the
// channel selector in the title row switches between them (the local INA219 or a channel of the
// remote rpi-power-monitor server, see the "Power Monitor" group of the weather2 form). Tapping one
// of the three readouts selects which quantity the chart shows and highlights the readout. The
// energy card shows the total counter of the source only, the local INA219 has none.
class PowerScreen : public Screen {
public:
    // quantity the chart shows, the active readout is highlighted
    enum class GraphSource : uint8_t {
        VOLTAGE = 0,
        CURRENT,
        POWER,
    };
    static constexpr uint8_t kNumGraphSources = 3;
    // One bucket per column of the chart, holding the average of all samples of its time slice. There
    // are exactly kChartPoints slices in the window, the window is configurable (1..60 minutes) and a
    // slice is `minutes * kRefreshInterval` long (1 min = 200 ms, 60 min = 12 s)
    static constexpr uint16_t kChartPoints = 300;
    // The readouts follow the samples of the sources at 5 fps (the data source copies them at the
    // same rate, the local INA219 samples every 68 ms and the remote server every ~133 ms). The
    // clock only changes once per second. The same 200 ms tick is the sample interval of the chart:
    // one bucket holds `minutes` samples
    static constexpr uint32_t kRefreshInterval = 200;
    static constexpr uint32_t kClockInterval = 1000;

    explicit PowerScreen(DataSource &data) :
        Screen(data),
        _channel(0),
        _graphSource(GraphSource::POWER),
        _pendingChannel(-1),
        _pendingGraphSource(-1),
        _lastChannelCount(0xff),
        _lastClockUpdate(0),
        _content(nullptr),
        _channelChipCount(0),
        _energyCard(nullptr),
        _energyValue(nullptr),
        _energyUnit(nullptr),
        _energyState(nullptr),
        _chartCard(nullptr),
        _chart(nullptr),
        _chartSeries(nullptr),
        _chartMax(nullptr),
        _chartMin(nullptr),
        _status(nullptr),
        _history(nullptr),
        _historyCount{},
        _bucketSamples{},
        _samplesPerBucket(1),
        _historyMinutes(0)
    {
    }
    virtual ~PowerScreen();

    virtual const char *getName() const override {
        return "POWER";
    }
    virtual const char *getTitle() const override {
        return "Power";
    }
    virtual LVGLUI::IconType getIcon() const override {
        return LVGLUI::IconType::POWER;
    }
    virtual uint32_t getScreenTime() const override {
        return 10;
    }
    // the readouts update at 5 fps, the manager default is 1 Hz
    virtual uint32_t getRefreshInterval() const override {
        return kRefreshInterval;
    }
    virtual void create(lv_obj_t *parent) override;
    virtual void update() override;
    // The taps of this screen belong to its own widgets (the channel and the V/A/W selectors), so
    // they are consumed and never open the screen overview. The mockup asks for the same
    // ("taps do not leave the screen"); a swipe still changes the screen
    virtual bool onTap() override {
        return true;
    }
    virtual bool onDoubleTap() override {
        return true;
    }

private:
    // one clickable selector chip. The index is the value the click sets (a channel index or a
    // GraphSource), screen is the instance the callback forwards the selection to
    struct Chip {
        Chip() :
            obj(nullptr),
            label(nullptr),
            screen(nullptr),
            index(0)
        {
        }
        lv_obj_t *obj;
        lv_obj_t *label;
        PowerScreen *screen;
        uint8_t index;
    };

    // LVGL event callbacks, they only record the selection - update() (called from the main loop
    // tick) applies it. An LVGL callback must never rebuild the tree it is dispatching from
    static void _channelChipCallback(lv_event_t *event);
    static void _readoutCardCallback(lv_event_t *event);

    // builds the channel selector in the title row (hidden for a single channel)
    void _createChannelChips(lv_obj_t *parent);
    // creates one selector chip
    void _createChip(lv_obj_t *parent, Chip &chip, const char *text, lv_coord_t x, lv_coord_t y,
                     lv_event_cb_t callback, uint8_t index);
    // colors the chips of the current selection
    void _updateChips();
    // builds the cards (readouts, energy, chart) below the title row
    void _buildCards(lv_obj_t *parent);
    // chart range, buckets and labels of the selected graph source
    void _updateChart();
    // readout values and the energy card of the selected channel
    void _updateValues(const PowerValues &value);
    // merges the sample into the slice of the channel that is currently filled
    void _pushSample(uint8_t channel, const PowerValues &value);
    // allocates the PSRAM block of all channels once. Its size does not depend on the configured
    // window (a bucket holds the average of a slice, not every sample)
    void _allocateHistory();
    // drops the content of every channel (used when the window or the channel configuration changed)
    void _clearHistory();
    // sets the length of a slice from the configured window and drops the data if it changed
    void _setGraphMinutes(uint8_t minutes);
    // first bucket of one series of one channel inside the PSRAM block (kChartPoints floats)
    float *_historySeries(uint8_t channel, uint8_t source) const;
    // value of the graph source of one sample, formatted for the chart labels into the buffer of
    // the caller (the labels are rewritten on every tick of the chart)
    void _formatGraphValue(float value, char *output, size_t size) const;
    // chart value of one sample, scaled to the resolution of the graph source
    static lv_coord_t _scaleGraphValue(GraphSource source, float value);
    // position of the selected channel among the configured ones ("Channel 2/3")
    uint8_t _channelPosition() const;

    // selected channel (index into PowerChannels) and graph source. A chip tap only sets the
    // pending value, update() applies it
    uint8_t _channel;
    GraphSource _graphSource;
    int8_t _pendingChannel;
    int8_t _pendingGraphSource;
    // number of configured channels create() was built with, a change reloads the screen
    uint8_t _lastChannelCount;
    // millis() of the last clock update
    uint32_t _lastClockUpdate;

    lv_obj_t *_content;
    Chip _channelChips[PowerChannels::kNumChannels];
    uint8_t _channelChipCount;
    // event data of the readout cards: tapping a card selects the graph source (the only selector,
    // the chart itself has no title and no chips - it fills its card completely)
    struct ReadoutData {
        ReadoutData() :
            screen(nullptr),
            index(0)
        {
        }
        PowerScreen *screen;
        uint8_t index;
    };
    ReadoutData _readoutData[kNumGraphSources];
    // 0 = voltage, 1 = current, 2 = power
    lv_obj_t *_readoutCards[kNumGraphSources];
    lv_obj_t *_readoutValues[kNumGraphSources];
    lv_obj_t *_readoutUnits[kNumGraphSources];
    lv_obj_t *_energyCard;
    lv_obj_t *_energyValue;
    lv_obj_t *_energyUnit;
    lv_obj_t *_energyState;
    lv_obj_t *_chartCard;
    lv_obj_t *_chart;
    // the curve (the average of every slice) lives as long as the chart (see the LVGL landmine in
    // _buildCards)
    lv_chart_series_t *_chartSeries;
    lv_obj_t *_chartMax;
    lv_obj_t *_chartMin;
    // compact state line in the title row (channel, source, state). The screen has no footer, the
    // cards use the full height
    lv_obj_t *_status;
    // buffer of the state line: it is rebuilt on every tick (5 fps) and keeps its capacity, so the
    // text is only written to the label when it really changed
    String _statusText;
    // Per channel history in PSRAM, layout [channel][series][bucket] = the average of the samples of
    // that slice. 4 channels x 3 series x 300 buckets x 4 bytes = 14,400 bytes, independent of the
    // configured window - the window only changes the length of a slice and with it the number of
    // samples that are averaged into a bucket
    float *_history;
    // number of buckets that hold data (1..kChartPoints), the oldest one is the first
    uint16_t _historyCount[PowerChannels::kNumChannels];
    // samples that went into the bucket that is filled at the moment
    uint8_t _bucketSamples[PowerChannels::kNumChannels];
    // samples one bucket holds (= the window in minutes, the sample tick is kRefreshInterval long)
    uint16_t _samplesPerBucket;
    // window the buckets were filled with, a change drops the data
    uint8_t _historyMinutes;
};

// network and system values, both cards are refreshed by update()
class InfoScreen : public Screen {
public:
    static constexpr uint8_t kNumNetworkValues = 6;
    static constexpr uint8_t kNumSystemValues = 6;

    explicit InfoScreen(DataSource &data) : Screen(data) {}

    virtual const char *getName() const override {
        return "INFO";
    }
    virtual const char *getTitle() const override {
        return "Info";
    }
    virtual LVGLUI::IconType getIcon() const override {
        return LVGLUI::IconType::INFO;
    }
    virtual uint32_t getScreenTime() const override {
        return 20;
    }
    virtual void create(lv_obj_t *parent) override;
    virtual void update() override;

private:
    LVGLUI::KeyValueRefs _networkValues[kNumNetworkValues];
    LVGLUI::KeyValueRefs _systemValues[kNumSystemValues];
};

} // namespace WeatherStation2
