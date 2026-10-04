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
//   - HassScreen      Home Assistant dashboard, a grid of tiles from /hass.yaml (only compiled
//                     with IOT_HASS_DASHBOARD, see docs/hass_config.md)
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
#include "shared/home_assistant/hass_dashboard.h"

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

// Home Assistant dashboard: a grid of tiles (one per configured entity) with an arc for the
// dimmer and the climate. The screen only reads from HomeAssistant::Dashboard, the layout comes
// from /hass.yaml. Without the configuration file the screen shows a message instead of the grid
// (or the built-in test configuration, see DEBUG_HASS_TEST_CONFIG)
class HassScreen : public Screen {
public:
    // Sentinels of _findTile(). kBackTile is the back tile that closes an area page, kNoTile means
    // that the object belongs to no tile (no tile can have either index - the model is a vector and
    // never gets close to its size)
    static constexpr HomeAssistant::TileIndex kNoTile = HomeAssistant::kNoTile;
    static constexpr HomeAssistant::TileIndex kBackTile = 0xfffffffe;
    // Value of TileWidgets::arcState while the tile was not drawn with a state yet. It is above
    // every state key (which uses at most 6 bits) and fits the 7 bit field
    static constexpr uint8_t kArcStateNone = 0x7f;
    // size of the per-tile render state of the visible page (boot log / diagnostics)
    static size_t getTileWidgetsSize() {
        return sizeof(TileWidgets);
    }
    static constexpr uint32_t kRefreshInterval = 200;
    // Delay before a change of the quick settings sheet is written to the configuration (NVS).
    // The setters only change the stored parameter in RAM, the write collects a slider drag or
    // several steps of a timeout instead of writing the flash on every change
    static constexpr uint32_t kSettingsWriteDelay = 5000;
    // A tap that a tile consumed is ignored by the manager for this long (the manager runs its
    // single tap action after the double tap window)
    static constexpr uint32_t kTileTapWindow = 800;
    // A tap on a dimmer tile is held back for this long: a second tap inside the window switches
    // the entity, a single tap opens the panel. The window is the one of the screen manager, so a
    // double tap on a tile is not a double tap of the screen either
    static constexpr uint32_t kTileDoubleTapTime = LVGLScreenManager::kDoubleTapTime;

    HassScreen(DataSource &data, HomeAssistant::Dashboard &dashboard) :
        Screen(data),
        _dashboard(dashboard),
        _rotation(0),
        _portrait(false),
        _sensorRotation(-1),
        _orientationPending(false),
        _width(kScreenWidth),
        _height(kScreenHeight),
        _grid(nullptr),
        _status(nullptr),
        _message(nullptr),
        _fullRoot(nullptr),
        _fullImage(nullptr),
        _fullTile(-1),
        _pendingFullscreen(-1),
        _pendingFullscreenClose(false),
        _fullscreenTime(0),
        _panel(Panel::NONE),
        _panelTile(-1),
        _panelView(PanelView::ARC),
        _panelButtons(0),
        _listView(-1),
        _statsHours(HomeAssistant::Dashboard::kStatsDefaultHours),
        _statsGeneration(0xffffffff),
        _pendingStatsRange(-1),
        _panelDetail(0),
        _pendingPanel(-1),
        _pendingPanelClose(false),
        _pendingPanelView(-1),
        _settingsView(SettingsView::MAIN),
        _settingsOpen(false),
        _settingsPending(false),
        _settingsPendingView(-1),
        _settingsAction(SettingsAction::NONE),
        _settingsSignal(0xff),
        _settingsUpdate(0),
        _pendingTileTap(-1),
        _pendingTileTapTime(0),
        _areaPage(0),
        _pendingPage(-1),
        _controlPressed(false),
        _sliderLevel(0),
        _sliderPressY(0),
        _sliderMoved(false),
        _expectedItem{},
        _expectedItemView(0),
        _expectedItemTime(0),
        _configLoaded(false),
        _configGeneration(0),
        _lastTileClick(0)
    {
    }

    // Orientation of the dashboard, the value of WeatherStation::HassRotation (the numbering is the
    // one of the display driver, see WT32_SC01::Rotation). The display is rotated while this
    // screen is shown and switched back to landscape when it is left, the other screens of the
    // plugin keep their landscape layout. Called from setup() and reconfigure(); a change while
    // the dashboard is on screen is applied by update()
    void setOrientation(uint8_t rotation);

    // name of the configured orientation, for the status page of the plugin
    const char *getOrientationName() const;

    // Rotation reported by the motion sensor of the sensor plugin (the MPU-6050), in 90 degree
    // steps (0/90/180/270). The dashboard follows the device while WeatherStation::getHassRotationLock()
    // is false - the sensor is only registered when the env compiles one (IOT_SENSOR_HAVE_MPU6050).
    // The value is not stored: a reboot starts at the configured orientation and the sensor
    // synchronizes it again. The step is mirrored when it is applied, see _applySensorRotation()
    void setSensorRotation(uint16_t rotation);

    // rotation of the motion sensor, -1 while none reported one (status page and debug)
    int16_t getSensorRotation() const {
        return _sensorRotation;
    }

    // Applies the rotation the motion sensor reported again - the device may have been turned
    // while the rotation was locked. Does nothing while no rotation was reported or the lock is
    // still set
    void refreshSensorRotation() {
        _applySensorRotation();
    }

    virtual const char *getName() const override {
        return "HASS";
    }
    virtual const char *getTitle() const override {
        return "Home Assistant";
    }
    // the page that is shown (0 = the main page of the document), for the status page and the
    // debug tool - the screen can only be navigated by tapping an area tile
    HomeAssistant::PageIndex getPage() const {
        return _areaPage;
    }
    virtual LVGLUI::IconType getIcon() const override {
        return LVGLUI::IconType::HOUSE;
    }
    virtual uint32_t getRefreshInterval() const override {
        // update() applies a pending orientation change. The value arrives from the web task while
        // the display can only be switched by the main loop, so do not wait for the regular
        // interval - the saved setting is on screen right away
        return _orientationPending ? 20 : kRefreshInterval;
    }
    // the dashboard stays on screen until another screen is selected
    virtual uint32_t getScreenTime() const override {
        return LVGLScreenManager::kScreenTimeNoRotation;
    }
    virtual void create(lv_obj_t *parent) override;
    virtual void update() override;
    virtual void release() override;
    // this screen has no top bar, the tiles use the whole display
    virtual bool hasTopBar() const override {
        return false;
    }
    // a tap on a tile must not open the screen overview
    virtual bool onTap() override;
    virtual bool onDoubleTap() override;
    // A panel is a full screen of controls: a drag on one of them (the color wheel, the arc, a
    // slider) is not a gesture of the screen manager. A centered swipe down from the top opens
    // quick settings; the start point follows the current display orientation.
    virtual bool onSwipe(SwipeDirection direction, const lv_point_t &startPoint) override;

private:
    // panel that is drawn instead of the grid. A tap on a light/dimmer tile opens the dimmer
    // panel, a climate tile opens the climate panel and a sensor tile the sensor panel (its live
    // value and the history graph of its long term statistics)
    enum class Panel : uint8_t {
        NONE,
        CLIMATE,
        DIMMER,
        SENSOR,
    };

    // range buttons of the sensor panel, left to right. 1 h is not offered: the buckets are 5
    // minute aggregates, so a shorter range would be a dozen points
    enum class StatsRange : uint8_t {
        HOURS_48,
        HOURS_24,
        HOURS_12,
        COUNT,
    };
    static constexpr uint8_t kStatsRangeCount = static_cast<uint8_t>(StatsRange::COUNT);
    // hours of a range button
    static uint8_t statsRangeHours(StatsRange range);
    // range a number of hours belongs to (the one the panel was opened with)
    static StatsRange statsRangeOf(uint8_t hours);
    // number of 5 minute buckets of a range: the window plus the bucket that is running at the
    // moment (the graph has one column per bucket)
    static uint16_t statsRangeBuckets(uint8_t hours) {
        return static_cast<uint16_t>(hours) * 12 + 1;
    }
    // Ticks of the X axis of the history graph: one grid line every 4 hours at whole local hours
    // (see kSensorGridHours). The 48 hour range has 12 intervals and can have a line at both ends of
    // the window, 13 covers every range (a 24 hour window has 6 or 7)
    static constexpr uint8_t kSensorTimeTicks = 13;

    // sub view of an open panel. Climate: the arc with the steppers, or the list of the mode,
    // the preset and the fan mode. Light/dimmer: the level slider, the color wheel, the color
    // temperature slider or the list of the effects
    enum class PanelView : uint8_t {
        ARC,       // climate: arc + steppers
        OPTION_1,  // climate: hvac modes
        OPTION_2,  // climate: preset modes
        OPTION_3,  // climate: fan modes
        LEVEL,     // light: level slider + steppers
        COLOR,     // light: color wheel
        COLOR_TEMP, // light: color temperature slider
        EFFECTS,   // light: list of the effects
    };

    // buttons of the light panel, stacked in the left column of the panel in this order. Power and
    // brightness are always there, the other three appear when the entity reports the attribute
    // (supported_color_modes / effect_list)
    enum class LightButton : uint8_t {
        POWER,
        BRIGHTNESS,
        COLOR,
        COLOR_TEMP,
        EFFECTS,
        COUNT,
    };

    // widgets of one tile
    struct TileRefs {
        TileRefs() :
            tile(nullptr),
            icon(nullptr),
            name(nullptr),
            value(nullptr),
            state(nullptr),
            action(nullptr),
            fill(nullptr),
            drag(nullptr),
            stepDown(nullptr),
            stepUp(nullptr),
            arc(nullptr)
        {
        }
        lv_obj_t *tile;
        lv_obj_t *icon;
        lv_obj_t *name;
        // value of the tile (sensor reading, level in percent, target temperature)
        lv_obj_t *value;
        // second line: the state of a switch/light or the current temperature of a climate
        lv_obj_t *state;
        // climate: the action of the entity ("Heating")
        lv_obj_t *action;
        // dimmer: the level fill and the object that receives the vertical drag
        lv_obj_t *fill;
        lv_obj_t *drag;
        // climate: the + and - bars
        lv_obj_t *stepDown;
        lv_obj_t *stepUp;
        // climate panel: the arc of the setpoint
        lv_obj_t *arc;
    };

    // The per-tile state of the VISIBLE page: the widget pointers (TileRefs) and the state of the
    // arc/drag of a dimmer or a climate tile. It is one entry per tile of the page that is built,
    // owned as a DRAM vector (see _tiles) - the widget pointers and the drag state are touched on
    // every tap and every update and the DRAM use does not grow with the number of pages (a page
    // has no more tiles than its grid has cells).
    //
    // The members are ordered by size and the small fields share two packed bytes: the entry is 60
    // bytes (a padded layout with the uint8_t members between the int16_t ones was 68)
    struct TileWidgets {
        TileWidgets() :
            globalTile(kNoTile),
            arcValue(0),
            arcMin(0),
            arcMax(0),
            arcPressValue(0),
            dragStartY(0),
            arcState(kArcStateNone),
            arcPressed(0),
            dragLevel(0),
            dragging(0)
        {
        }

        // tile of the configuration this entry belongs to
        HomeAssistant::TileIndex globalTile;
        TileRefs refs;
        // last value drawn by the arc (0.1 degree steps) and the range of the climate arc (0 = not
        // set yet), the value the arc had when it was pressed (a tap without a drag toggles it)
        int16_t arcValue;
        int16_t arcMin;
        int16_t arcMax;
        int16_t arcPressValue;
        // dimmer tile: position the drag started with (a drag without a movement is a tap)
        lv_coord_t dragStartY;
        // state the tile was drawn with (kArcStateNone while it was not drawn yet)
        uint8_t arcState : 7;
        // the arc is being dragged, the polled value must not overwrite it
        uint8_t arcPressed : 1;
        // dimmer tile: level the drag started with (a drag without a movement is a tap)
        uint8_t dragLevel : 7;
        uint8_t dragging : 1;
    };
    // the entry is hand-packed to 60 bytes, the boot log prints it (getTileWidgetsSize())
    static_assert(sizeof(TileWidgets) == 60, "TileWidgets must stay 60 bytes");

    // A camera preview of a picture tile. The state is kept outside TileRefs (a table with the
    // buffer and the descriptor of an image would cost kilobytes of RAM for a handful of previews)
    // and limited to the number of images the client fetches
    static constexpr uint8_t kMaxPictures = HomeAssistant::Client::kMaxImageTiles;
    struct Picture {
        Picture() :
            tile(kNoTile),
            image(nullptr),
            width(0),
            height(0),
            buffer(nullptr),
            dsc{}
        {
        }
        // tile of the configuration this preview belongs to, kNoTile while the slot is free
        HomeAssistant::TileIndex tile;
        lv_obj_t *image;
        // pixel box of the tile the buffer was fetched for. A frame of another box (the box
        // changed while the request was on its way) is dropped instead of drawn
        uint16_t width;
        uint16_t height;
        // pixels in PSRAM, owned by the screen (free() releases them)
        uint16_t *buffer;
        // descriptor the lv_img points at
        lv_img_dsc_t dsc;
    };

    // widgets of the open panel. The controls that belong to the tile (arc of the climate, the
    // steppers) are in _tiles[_panelTile], this holds the frame around them
    struct PanelRefs {
        PanelRefs() :
            back(nullptr),
            title(nullptr),
            label(nullptr),
            headerValue(nullptr),
            pills{nullptr, nullptr, nullptr},
            pillIcons{nullptr, nullptr, nullptr},
            pillValue{nullptr, nullptr, nullptr},
            list(nullptr),
            buttons{},
            slider(nullptr),
            wheel(nullptr),
            tempSlider(nullptr)
        {
        }
        lv_obj_t *back;
        lv_obj_t *title;
        // climate header: "Current temperature" and its value
        lv_obj_t *label;
        lv_obj_t *headerValue;
        // climate: the three option pills (mode, preset, fan mode), the glyph of the value that is
        // set (see climateOptionIcon()) and the value itself
        lv_obj_t *pills[3];
        lv_obj_t *pillIcons[3];
        lv_obj_t *pillValue[3];
        // the list that replaces the arc/slider while an option is open
        lv_obj_t *list;
        // light: the buttons of the control bar (LightButton), the unsupported ones are hidden
        lv_obj_t *buttons[static_cast<uint8_t>(LightButton::COUNT)];
        // light: level slider, color wheel and color temperature slider (one of them is visible)
        lv_obj_t *slider;
        lv_obj_t *wheel;
        lv_obj_t *tempSlider;
    };

    // The four areas a light/dimmer or climate panel is built from, in pixels (see _panelLayout()):
    // the tile that closes the panel, the options of the entity (the buttons of a light and the
    // pills of a climate), the name of the entity and the control of the option that is selected.
    // `value*` is the row of the big readout above the control of a light and `steps*` the row of
    // the steppers of a climate; both are portrait only (in landscape the readout is written on the
    // track of the slider and the steppers live in the control area beside the arc/slider). In
    // portrait `control*` is the band the arc of a climate (centered on its ring) or the slider of
    // a light (which fills it) uses
    struct PanelLayout {
        PanelLayout() :
            backX(0),
            backY(0),
            backW(0),
            backH(0),
            optionsX(0),
            optionsY(0),
            optionsW(0),
            optionsH(0),
            headerX(0),
            headerY(0),
            headerW(0),
            controlX(0),
            controlY(0),
            controlW(0),
            controlH(0),
            listX(0),
            listY(0),
            listW(0),
            listH(0),
            valueY(0),
            valueH(0),
            stepsX(0),
            stepsY(0),
            stepsH(0)
        {
        }
        lv_coord_t backX;
        lv_coord_t backY;
        lv_coord_t backW;
        lv_coord_t backH;
        lv_coord_t optionsX;
        lv_coord_t optionsY;
        lv_coord_t optionsW;
        lv_coord_t optionsH;
        lv_coord_t headerX;
        lv_coord_t headerY;
        lv_coord_t headerW;
        lv_coord_t controlX;
        lv_coord_t controlY;
        lv_coord_t controlW;
        lv_coord_t controlH;
        // the list of the options (climate) and of the effects (light panel) replaces the control.
        // In landscape it is the control area, in portrait it reaches down to the buttons: a list
        // does not need the row the steppers of a climate sit in
        lv_coord_t listX;
        lv_coord_t listY;
        lv_coord_t listW;
        lv_coord_t listH;
        lv_coord_t valueY;
        lv_coord_t valueH;
        lv_coord_t stepsX;
        lv_coord_t stepsY;
        lv_coord_t stepsH;
    };

    // --------------------------------------------------------------------------------------
    // quick settings of the dashboard
    //
    // A swipe left or right opens this sheet instead of leaving the screen (see onSwipe()). It is
    // an overlay of this screen - a child of the object the grid lives in, like the fullscreen
    // image of a picture tile - so a rebuild of the grid does not remove it and the tiles behind it
    // keep their values. The reference layout is docs/hass_layout/quick_settings_preview.html
    // --------------------------------------------------------------------------------------
    // one tile of the sheet. Every setting is one tile, a tap opens its editor (except the two
    // that act on the tap: the rotation cycles the orientations, the lock toggles)
    enum class SettingsTile : uint8_t {
        IDLE_BRIGHTNESS,   // power_saving_level
        IDLE_TIMEOUT,      // power_saving_timeout
        STANDBY_TIMEOUT,   // standby_timeout
        ROTATE,            // cycles the four orientations of the dashboard
        ROTATION_LOCK,     // blocks the rotation
        SLEEP,             // turns the display off (the standby state)
        COUNT,
    };
    static constexpr uint8_t kSettingsTiles = static_cast<uint8_t>(SettingsTile::COUNT);

    // View of the sheet that is built: the tiles, or the editor of one setting. The editor replaces
    // the brightness row, the tiles and the leave button, the header stays
    enum class SettingsView : uint8_t {
        MAIN,
        IDLE_BRIGHTNESS,
        IDLE_TIMEOUT,
        STANDBY_TIMEOUT,
    };

    // Action of the sheet that is applied by update(): a tap must not switch the screen, rotate the
    // display or rebuild the widget tree from inside the LVGL event callback
    enum class SettingsAction : uint8_t {
        NONE,
        CLOSE,
        LEAVE,
        SLEEP,
        ROTATE,
    };

    // widgets of the sheet. The tree is built for one view at a time (a view change deletes it),
    // so only the widgets of the current view are set. The panel has three rows: the header, the
    // content and the row at the bottom (see docs/hass_layout/quick_settings2.html)
    struct SettingsRefs {
        SettingsRefs() :
            root(nullptr),
            panel(nullptr),
            top(nullptr),
            center(nullptr),
            action(nullptr),
            actionLabel(nullptr),
            time(nullptr),
            date(nullptr),
            signal(nullptr),
            close(nullptr),
            brightnessSlider(nullptr),
            brightnessValue(nullptr),
            tiles{},
            tileIcons{},
            tileValues{},
            tileLabels{},
            editTitle(nullptr),
            editValue(nullptr),
            editSlider(nullptr),
            editDown(nullptr),
            editUp(nullptr)
        {
        }
        lv_obj_t *root;
        lv_obj_t *panel;
        // the three rows. The content row has a darker fill than the two rows around it
        lv_obj_t *top;
        lv_obj_t *center;
        lv_obj_t *action;
        lv_obj_t *actionLabel;
        // header: the clock, the date, the WiFi signal and the cell with the button that closes it
        lv_obj_t *time;
        lv_obj_t *date;
        lv_obj_t *signal;
        lv_obj_t *close;
        // main view: the screen brightness and the tiles
        lv_obj_t *brightnessSlider;
        lv_obj_t *brightnessValue;
        lv_obj_t *tiles[kSettingsTiles];
        lv_obj_t *tileIcons[kSettingsTiles];
        lv_obj_t *tileValues[kSettingsTiles];
        lv_obj_t *tileLabels[kSettingsTiles];
        // editor view: the title, the big value and the control
        lv_obj_t *editTitle;
        lv_obj_t *editValue;
        lv_obj_t *editSlider;
        lv_obj_t *editDown;
        lv_obj_t *editUp;
    };

    // value a control of a panel was set to. It is kept until the entity reports it (or the
    // deadline passes): a response that was already in flight reports the old value and would
    // yank the control back, which looks like the control did not work
    struct ExpectedValue {
        ExpectedValue() :
            tile(HomeAssistant::kNoTile),
            active(false),
            value(0),
            value2(0),
            deadline(0)
        {
        }
        HomeAssistant::TileIndex tile;
        bool active;
        float value;
        float value2;
        uint32_t deadline;
    };

    // Widgets of the sensor panel: the live value of the entity, the range buttons and the history
    // graph of its long term statistics. The name is _panelRefs.title and the value _panelRefs.headerValue
    // (the header of the panel), the rest belongs to this panel only
    struct SensorRefs {
        SensorRefs() :
            icon(nullptr),
            title(nullptr),
            unit(nullptr),
            chips{},
            chipLabels{},
            levels{},
            grid{},
            times{},
            chart(nullptr),
            series(nullptr),
            buckets(0),
            drawnHours(0),
            drawnGeneration(0),
            info(nullptr)
        {
        }
        // icon of the entity, the same glyph the tile draws
        lv_obj_t *icon;
        // "History" and the unit of the values (above the level labels)
        lv_obj_t *title;
        lv_obj_t *unit;
        // range buttons (StatsRange) and the labels inside them
        lv_obj_t *chips[kStatsRangeCount];
        lv_obj_t *chipLabels[kStatsRangeCount];
        // maximum, middle and minimum of the window at the left of the graph
        lv_obj_t *levels[3];
        // One grid line and one label per tick of the X axis. The lines are drawn by the screen
        // (lv_chart cannot place its dividers at a time), the labels sit under their line - see
        // _drawSensorChart()
        lv_obj_t *grid[kSensorTimeTicks];
        lv_obj_t *times[kSensorTimeTicks];
        // the graph and its series (see _buildSensorPanel())
        lv_obj_t *chart;
        lv_chart_series_t *series;
        // columns of the graph (the buckets of the range it was built for), the range it was drawn
        // with and the version of the statistics it shows (the graph is only filled again when one
        // of them changes)
        uint16_t buckets;
        uint8_t drawnHours;
        uint32_t drawnGeneration;
        // message over the graph ("loading", "no history", the error of a failed request)
        lv_obj_t *info;
    };
    // how long a control keeps its value when the entity never reports it
    static constexpr uint32_t kExpectedTimeout = 10000;
    // true while the control keeps the value of the user (what = name of the control, only used by
    // the trace of the serial log)
    bool _expects(ExpectedValue &expected, HomeAssistant::TileIndex tile, const char *what, float value, float value2, float tolerance);
    // remembers the value a control was set to
    static void _expect(ExpectedValue &expected, HomeAssistant::TileIndex tile, const char *what, float value, float value2);

    // creates the widget tree of the grid below _grid
    void _buildGrid();
    // creates one tile (the children that show the value are added by _updateTile())
    void _buildTile(HomeAssistant::TileIndex index);
    // creates the widget tree of the open panel
    void _buildPanel();
    // creates the widget tree of the sensor panel (live value, range buttons and history graph)
    void _buildSensorPanel(HomeAssistant::TileIndex index);
    // refreshes the header of the sensor panel and draws the graph
    void _updateSensorPanel();
    // draws the buckets of the statistics into the graph of the sensor panel
    void _drawSensorChart(const HomeAssistant::Tile &tile);
    // text of the level (maximum/middle/minimum) and of the time labels of the graph. The number of
    // decimals of a level comes from the step of the axis, so all three labels are formatted alike.
    // The text goes into the buffer of the caller: the labels are rebuilt while the graph is drawn
    // and a returned String would be one heap allocation per label
    void _formatStatsValue(float value, uint8_t decimals, char *output, size_t size) const;
    void _formatStatsTime(uint32_t time, char *output, size_t size) const;
    // stacks the buttons of the light panel in the left column and gives the rest of the column to
    // the back tile (an entity without color/effects has fewer buttons)
    void _layoutPanelButtons();
    // items of the option list the open panel shows (the mode, the preset and the fan mode of a
    // climate, the effects of a light), nullptr while the view has no list. The list is comma
    // separated and points into the buffer of the detail slot of the dashboard. A tapped item is
    // looked up in it by its ordinal (see _panelCallback())
    const char *_panelListItems() const;
    // glyph of the value of an option of a climate panel (the mode, the preset and the fan mode of
    // an entity): the mode has a glyph per value, a fan mode uses the fan glyphs and every preset
    // is the same small disc (see the implementation)
    static LVGLUI::IconType _climateOptionIcon(PanelView view, const char *value);
    // fills the list of the panel with the items of the current view
    void _buildPanelList();
    // refreshes the header, the pills, the slider and the list of the open panel
    void _updatePanel();
    // removes the grid and builds it again (the configuration appeared or disappeared)
    void _rebuild();
    // releases the camera images and the preview slots (they are rebuilt with the widget tree,
    // the page change releases the buffers of the tiles that are not visible any more)
    void _releaseImages();
    // preview of a tile, nullptr while it has none (or the tile is not built)
    Picture *_picture(HomeAssistant::TileIndex index);
    // preview of a tile, a free slot is used when it has none (nullptr while all are in use)
    Picture *_pictureFor(HomeAssistant::TileIndex index);
    // Shows the image of a picture tile over the whole display. The widgets are created with the
    // first image that is opened and only shown and hidden afterwards. The image is fetched in
    // the size of the display while it is open (a tile sized frame is centered until it arrives)
    void _openFullscreen(HomeAssistant::TileIndex index);
    // closes the fullscreen image, the tile asks for its own size again
    void _closeFullscreen();
#if DEBUG_HASS_ACTION_TEST
    // self test of the action path, see the comment in ws2_screen_hass.cpp
    void _actionTest();
#endif
    // shows the main page (0) or the page of an area. The widget tree is only rebuilt here,
    // never while an LVGL event is being dispatched
    void _showPage(HomeAssistant::PageIndex page);
    // refreshes the values of one tile
    void _updateTile(HomeAssistant::TileIndex index);
    // Places the icon and the value of a one cell tile: icon, value and name are one centered
    // column (see tileBlockGap()). Returns false while the icon does not fit the cell
    bool _layoutCenteredColumn(TileRefs &refs, lv_coord_t tileHeight, const char *text);
    // Position and size of a block of cells in pixels. `page` selects the grid (a page uses the
    // grid of the document or the grid of the area that owns it, the panels are laid out in the
    // grid of the document)
    void _cellGeometry(HomeAssistant::PageIndex page, uint8_t col, uint8_t row, uint8_t width, uint8_t height, lv_coord_t &x, lv_coord_t &y, lv_coord_t &w, lv_coord_t &h) const;
    // font of the big readout of a portrait panel (the level of a light, its colour temperature):
    // the portrait panel gives the value a row of its own, so it is one step of the ladder larger
    // than the value of the landscape panel, which shares the space with the slider
    const lv_font_t *_panelValueFont() const;
    // position and size of the areas of a light/dimmer or climate panel (the panels are laid out
    // in the grid of the document, landscape and portrait differ, see the implementation)
    PanelLayout _panelLayout() const;

    // opens the quick settings sheet (a swipe left/right of the dashboard)
    void _openSettings();
    // removes the sheet and its tree
    void _closeSettings();
    // creates the widget tree of the sheet for _settingsView
    void _buildSettings();
    // refreshes the clock, the signal strength and the values of the sheet (once per second)
    void _updateSettings();
    // selects the editor of one setting, or the tiles again (applied by update(), the tree is not
    // rebuilt from inside an LVGL event callback)
    void _showSettingsView(SettingsView view);
    // steps the timeout of the editor that is open through its presets (up = the next longer one)
    void _stepSettingsTimeout(bool up);
    // applies the action of the sheet that update() has to run outside an LVGL event callback
    void _applySettingsAction();
    // stores the value of the editor that is open
    void _applySettingsValue();
    // writes the configuration to NVS after a delay (a change of the sheet only marks the stored
    // parameter as changed, without config.write() it would be gone after a reboot)
    void _storeSettings();
    // index of the tile of the sheet an object belongs to, kSettingsTiles when none
    uint8_t _settingsTileAt(const lv_obj_t *object) const;
    // the tile of the setting the open editor belongs to
    SettingsTile _settingsTileOfView() const;
    // the value of a setting as it is shown on its tile and in its editor, written into the buffer
    // of the caller
    static void _settingsValue(SettingsTile tile, char *output, size_t size);
    // label of a setting and the icon of its tile
    static const char *_settingsTitle(SettingsTile tile);
    static LVGLUI::IconType _settingsIcon(SettingsTile tile);
    // LVGL callbacks of the sheet (the tiles, the sliders, the close/done/leave buttons)
    static void _settingsCallback(lv_event_t *event);
    // position and size of a tile in pixels
    void _tileGeometry(const HomeAssistant::Tile &tile, lv_coord_t &x, lv_coord_t &y, lv_coord_t &w, lv_coord_t &h) const;
    // Switches the display to the configured orientation and updates the layout metrics below.
    // Called before the widget tree is built, the display is only touched from the main loop
    void _applyOrientation();
    // Applies the rotation the motion sensor reported (maps the 90 degree steps to the four
    // orientations). Ignored while the rotation is locked; called by setSensorRotation() and when
    // the lock is cleared in the quick settings
    void _applySensorRotation();
    // Layout metrics of the orientation that is active. The grid of the configuration is written
    // for landscape and transposed for a portrait display (see _cellGeometry()), so a cell keeps
    // its shape. kScreenWidth/kScreenHeight of the panel are the landscape values
    lv_coord_t _gridBottom() const;
    lv_coord_t _sensorCardWidth() const;
    lv_coord_t _sensorCardHeight() const;
    lv_coord_t _sensorGraphWidth() const;
    lv_coord_t _sensorGraphHeight() const;
    // sets the text only when it changed (a new pointer restarts the scroll animation). The text is
    // a plain C string: the callers format into a stack buffer, a String for every label of every
    // tick was one heap allocation per label (the screen refreshes five times per second)
    static void _setTextIfChanged(lv_obj_t *label, const char *text, const lv_font_t *font, uint32_t color);
    // index of the tile that owns an object, kNoTile when it does not belong to one
    HomeAssistant::TileIndex _findTile(const lv_obj_t *object) const;
    // widgets of a tile of the page that is built, by its index in the configuration. A tile that
    // is not on the page (or before the tree is built) gets a scratch entry that is not connected
    // to any widget
    TileWidgets &_widgets(HomeAssistant::TileIndex index) {
        for (auto &entry : _tiles) {
            if (entry.globalTile == index) {
                return entry;
            }
        }
        return _noWidgets;
    }
    // A tap on a control tile that can be switched as well (a dimmer): the first tap waits for a
    // second one, a double tap switches the entity (on/off) and only a single tap opens the panel
    void _tapTile(HomeAssistant::TileIndex index);
    static void _tileCallback(lv_event_t *event);
    // touch on the fullscreen image: any tap or swipe closes it
    static void _fullCallback(lv_event_t *event);
    // vertical drag of a dimmer tile (up/down changes the level), a tap opens the panel
    static void _dragCallback(lv_event_t *event);
    // climate: the + and - bars of a tile and of the panel
    static void _stepCallback(lv_event_t *event);
    // climate panel: the arc of the setpoint
    static void _arcCallback(lv_event_t *event);
    // panel: option pills, list items and the mode/effect buttons
    static void _panelCallback(lv_event_t *event);
    // dimmer panel: the level slider
    static void _sliderCallback(lv_event_t *event);
    // dimmer panel: the color temperature slider
    static void _tempSliderCallback(lv_event_t *event);
    // dimmer panel: the color wheel
    static void _wheelCallback(lv_event_t *event);

private:
    HomeAssistant::Dashboard &_dashboard;
    // Orientation of the dashboard (WeatherStation::HassRotation, the numbering of the display
    // driver) and the layout metrics of it. release() switches the display back to landscape, the
    // widget tree is created by create(), which is also when the rotation is applied
    uint8_t _rotation;
    bool _portrait;
    // Rotation of the motion sensor in the same 90 degree steps as _rotation, -1 while no sensor
    // reported one. The dashboard follows it while the rotation is not locked, see
    // _applySensorRotation()
    int16_t _sensorRotation;
    // the orientation that is on screen is not the configured one any more (applied by update())
    bool _orientationPending;
    lv_coord_t _width;
    lv_coord_t _height;
    // container of the grid, rebuilt when the configuration changes
    lv_obj_t *_grid;
    // status line below the top bar (request errors, missing file with the test configuration)
    lv_obj_t *_status;
    // message shown instead of the grid while there is no configuration
    lv_obj_t *_message;
    // The widgets and the arc/drag state of the tiles of the page that is built, one entry per
    // tile. It is owned as a DRAM vector and only holds the page that is shown (see TileWidgets),
    // _noWidgets absorbs a write to a tile that is not built
    std::vector<TileWidgets> _tiles;
    TileWidgets _noWidgets;
    // camera previews of the picture tiles (at most kMaxPictures at a time)
    Picture _pictures[kMaxPictures];
    // Fullscreen image of a picture tile: the container and the lv_img of it (a child of the
    // screen, created with the first image that is opened, so a rebuild of the grid does not
    // remove it) and the tile it shows (kNoTile while no image is open). A tap sets
    // _pendingFullscreen (the widget tree is not touched inside an LVGL callback), a tap or a
    // swipe on the image itself sets _pendingFullscreenClose
    lv_obj_t *_fullRoot;
    lv_obj_t *_fullImage;
    int32_t _fullTile;
    int32_t _pendingFullscreen;
    bool _pendingFullscreenClose;
    // time the fullscreen image was opened or closed: the screen manager runs the gesture of the
    // touch that closed it right after, it must not change to another screen
    uint32_t _fullscreenTime;
    // tile that closes an area page (the first cell of the page)
    TileRefs _back;
    // widgets of the open panel, valid while _panel != Panel::NONE
    PanelRefs _panelRefs;
    // panel that is drawn, and the tile it belongs to
    Panel _panel;
    int32_t _panelTile;
    // sub view of the panel
    PanelView _panelView;
    // bitmask of the buttons of the light panel that are visible (LightButton)
    uint8_t _panelButtons;
    // view the list was filled with (an empty list needs to be built too)
    int8_t _listView;
    // sensor panel: widgets of this panel only, the graph that is drawn and the response it was
    // drawn with (the chart is only filled again when these change)
    SensorRefs _sensor;
    uint8_t _statsHours;
    // sentinel: the graph is filled with the first update after the panel was built
    uint32_t _statsGeneration;
    // range button that was pressed, applied by update() (the tree is not rebuilt from inside an
    // LVGL event callback)
    int8_t _pendingStatsRange;
    // Content the list was built from (the items and the marked one, kept as two members so that no
    // String has to be composed to compare them): the list is only built again when it changes,
    // rebuilding it on every response makes it flicker
    String _listContent;
    String _listCurrent;
    // generation of the detail data the panel was drawn with
    uint32_t _panelDetail;
    // a panel was opened or closed by a tile, applied by update() (the tree must not be rebuilt
    // from inside an LVGL event callback)
    int32_t _pendingPanel;
    bool _pendingPanelClose;
    // control the open panel has to show (0 = the level slider of a light, the arc of a climate,
    // 1..3 = the controls behind it), set by the debug key "hassview" and applied by update()
    int8_t _pendingPanelView;
    // Quick settings sheet: the widgets of the view that is built, the view, whether the sheet is
    // open, the swipe that asked for it, the view that has to be built again (the orientation
    // changed under it) and the action that is applied by update()
    SettingsRefs _settingsRefs;
    SettingsView _settingsView;
    bool _settingsOpen;
    bool _settingsPending;
    int8_t _settingsPendingView;
    SettingsAction _settingsAction;
    // WiFi glyph that is drawn in the header (0xff = none drawn yet, see _updateSettings())
    uint8_t _settingsSignal;
    // millis() of the last refresh of the sheet (the clock and the signal strength are refreshed
    // once per second, the screen runs at 5 fps)
    uint32_t _settingsUpdate;
    // delayed write of the configuration after a change of the sheet (see _storeSettings())
    Event::Timer _settingsWrite;
    // tile that was tapped and waits for a second tap, and when the first tap happened
    int32_t _pendingTileTap;
    uint32_t _pendingTileTapTime;
    // page that is drawn: 0 = main, 1..n = the page of an area tile
    HomeAssistant::PageIndex _areaPage;
    // page a tile asked for, applied by update() (an area tile must not rebuild the tree from
    // inside its own event callback)
    int32_t _pendingPage;
    // dimmer panel: the level slider or the color wheel is being dragged
    bool _controlPressed;
    // Level slider of the panel: the value it showed before the track was pressed and the position
    // the press started at. LVGL writes the value of the pressed position into the slider while the
    // finger is on it, so a tap (a movement of less than kDimmerDragTolerance) switches the entity
    // and only a drag changes the level (see HassScreen::_sliderCallback())
    int32_t _sliderLevel;
    lv_coord_t _sliderPressY;
    bool _sliderMoved;    // The item of an option list (mode, preset, fan mode, effect) the user tapped: the pill and the
    // list mark it until the detail response reports it (the entity confirms the action) or
    // kExpectedTimeout reverts it (see _expectedItemOf()). A name of the model fits, an effect can
    // be longer than a mode
    char _expectedItem[HomeAssistant::kNameLength];
    uint8_t _expectedItemView;
    uint32_t _expectedItemTime;
    // sets the item an option list has to mark until the detail response reports it
    void _expectItem(PanelView view, const char *item);
    // the item a list marks: the one the user tapped (until the detail response reports it or
    // kExpectedTimeout reverts it), else the item the entity reports
    const char * _expectedItemOf(PanelView view, const char *reported);
    // the values the controls keep until the entity reports them: the level of a dimmer tile and of
    // the slider of its panel, the color and the color temperature of the panel, and the setpoint of
    // a climate tile (the arc of its panel and the +/- bars)
    ExpectedValue _expectedLevel;
    ExpectedValue _expectedColor;
    ExpectedValue _expectedTemp;
    ExpectedValue _expectedSetpoint;
    // the configuration was loaded when the grid was built
    bool _configLoaded;
    // version of the configuration the widget tree was built for
    uint32_t _configGeneration;
    // millis() of the last tile touch
    uint32_t _lastTileClick;
};

} // namespace WeatherStation2
