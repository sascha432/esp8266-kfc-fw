/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

//
// Model of the Home Assistant screen: owns the configuration (/hass.yaml), the HTTP client and
// the values of the tiles. update() has to be called from the main loop (request task response,
// new version of the configuration file), the LVGL screen only reads the values.
//
// The state and the reported value of a tile are part of the tile model (TileState/TileValue in
// hass_config.h), the small per-tile flags the main loop keeps for an action that is on its way
// are in the DRAM here (TileFlags).
//

#include <Arduino_compat.h>

#include "hass_config.h"
#include "hass_client.h"

namespace WeatherStation2 {
namespace HomeAssistant {

// Attributes of the entity a panel shows. They are only polled while a panel is open and stored in
// one slot - the dashboard shows one panel at a time. Every string points into the single buffer of
// the slot (allocated once per response, see reserve()/shrink()); the lists are the items the panel
// offers, comma separated, and empty when the entity does not have that attribute. An attribute
// that was not part of the response points at the empty string instead of nullptr, so neither the
// parser nor the screen has to check it.
struct Detail {
    // The struct is never copied: the pointers belong to the buffer and a copy would duplicate
    // both. The dashboard keeps two slots and swaps them (see Dashboard::_detailBefore)
    Detail(const Detail &) = delete;
    Detail &operator=(const Detail &) = delete;

    // longest buffer a response is parsed into. The payload of a page is a few kilobytes but the
    // panel only reads the values of its own entity - a handful of names and lists. The largest
    // value is the effect list of a light (the LED matrix has 169 characters)
    static constexpr size_t kMaxBuffer = 4096;

    // value of an attribute that was not set
    static constexpr const char *kUnset = "";

    Detail() = default;
    ~Detail();

    // drops the values and releases the buffer
    void reset();
    // drops the values, the buffer is kept for the next response
    void clear();
    // exchanges two slots: the strings and the buffer, nothing is allocated or copied
    void swap(Detail &other);
    // makes room for a payload of `length` bytes (never more than kMaxBuffer). The buffer of the
    // previous response is reused, it is only replaced when it is too small
    bool reserve(size_t length);
    // gives the unused tail of the buffer back
    void shrink();

    // the values below are valid
    bool valid{false};
    // index of the tile the panel belongs to, kNoTile while no panel is open
    TileIndex tile{kNoTile};
    // increases with every response, the panel rebuilds its lists when it changes
    uint32_t generation{0};
    // climate: hvac mode (the state of the entity)
    const char *mode{kUnset};
    // climate: preset mode ("none" when the entity has none)
    const char *preset{kUnset};
    // climate: fan mode
    const char *fanMode{kUnset};
    // light: current effect, empty when none is running (an effect name is longer than a mode, the
    // longest of the LED matrix is "Spectrum Single Color Bars")
    const char *effect{kUnset};
    // light: color mode of the entity ("hs", "color_temp", "brightness", ...)
    const char *colorMode{kUnset};
    // light: hue in degrees and saturation in percent (hs_color)
    float hue{0};
    float saturation{0};
    // light: brightness in percent
    uint8_t level{0};
    // light: the color modes the entity supports ("hs,rgb,color_temp"), empty when unknown
    const char *colorModes{kUnset};
    // light: color temperature in kelvin, 0 when the entity has none, and the range of the slider
    float colorTemp{0};
    float minColorTemp{0};
    float maxColorTemp{0};
    // the items the panel offers, comma separated, empty when the entity does not have that
    // attribute
    const char *modeList{kUnset};
    const char *presetList{kUnset};
    const char *fanModeList{kUnset};
    const char *effectList{kUnset};

    // the buffer every string above points into (PSRAM, allocated on demand and reused)
    char *buffer{nullptr};
    // bytes of `buffer` the response used (the write cursor of the parser)
    size_t used{0};
    // size of the allocation
    size_t capacity{0};
};

// Capabilities of the panel of a tile, one bit per control. The screen maps them to the buttons of
// its light panel (the order matches: power, level, color, color temperature, effects). They are
// part of the state response, so the panel of a tile is complete the moment it is opened - the
// detail request of an open panel arrives about half a second later.
static constexpr uint8_t kCapPower = 1 << 0;
static constexpr uint8_t kCapLevel = 1 << 1;
static constexpr uint8_t kCapColor = 1 << 2;
static constexpr uint8_t kCapColorTemp = 1 << 3;
static constexpr uint8_t kCapEffects = 1 << 4;

class Dashboard {
public:
    // the configuration file is checked for a new version this often
    static constexpr uint32_t kConfigCheckInterval = 3000;
    // update() is a no-op within this time (it is called by the plugin loop and the screen)
    static constexpr uint32_t kMinUpdateInterval = 200;
    // an action is at most this long pending: an entity that never reports the change (or a command
    // that was ignored) stops the fast polls of the client after it
    static constexpr uint32_t kPendingTimeout = 10000;

    void begin();
    void stop();
    // main loop: applies the response of the request task and notices a new configuration
    void update();
    // the configuration of the plugin was saved
    void reconfigure();

    // the screen is visible, the client polls only while it is
    void setActive(bool active);
    bool isActive() const {
        return _active;
    }

    // Page the screen shows (0 = the main page). Only the tiles and the picture images of that
    // page are requested - the values of the other pages are polled when they are shown and
    // their camera images would be a constant download. The page that is opened is requested
    // right away
    void setVisiblePage(PageIndex page);
    void setPictureBox(TileIndex index, uint16_t width, uint16_t height);
    // hands a decoded camera image over to the screen, which owns the PSRAM buffer and has to
    // release it with free() (see Client::takeImage())
    bool takeImage(TileIndex &tile, uint16_t *&data, uint16_t &width, uint16_t &height, uint32_t &stamp);

    bool isLoaded() const {
        return _config.isLoaded();
    }
    // the file is missing (a parse error is in getConfigError())
    bool isFileMissing() const {
        return _fileMissing;
    }
    const String &getConfigError() const {
        return _configError;
    }
    const Config &getConfig() const {
        return _config;
    }
    TileIndex getTileCount() const {
        return _config.isLoaded() ? _config.getTileCount() : 0;
    }
    // the state Home Assistant reported for a tile
    const TileValue &getValue(TileIndex index) const {
        return _config.getTile(index).value;
    }
    // capabilities of the panel of a tile (kCapXxx), 0 while the entity was not polled yet
    uint8_t getCapabilities(TileIndex index) const {
        return (index < _flags.size()) ? _flags[index].capabilities : 0;
    }
    // an action of the tile is on its way to the entity (the tile is drawn as pending)
    bool isPending(TileIndex index) const {
        return (index < _flags.size()) ? _flags[index].pending : false;
    }
    // size of the per-tile flags (boot log / diagnostics)
    static size_t getTileFlagsSize() {
        return sizeof(TileFlags);
    }
    // increases with every loaded version of the configuration file. The screen rebuilds its
    // widget tree when it changes (the tiles of the old version do not match the new one)
    uint32_t getConfigGeneration() const {
        return _configGeneration;
    }

    // actions of the tiles
    void toggle(TileIndex index);
    void setLevel(TileIndex index, uint8_t percent);
    void setTemperature(TileIndex index, float temperature);
    void setMode(TileIndex index, const char *mode);
    // actions of the panels
    void setPreset(TileIndex index, const char *preset);
    void setFanMode(TileIndex index, const char *fanMode);
    void setEffect(TileIndex index, const char *effect);
    void setColor(TileIndex index, float hue, float saturation);
    void setColorTemp(TileIndex index, float kelvin);

    // the panel of a tile was opened, its detail attributes are polled until closeDetail()
    void requestDetail(TileIndex index);
    void closeDetail();
    const Detail &getDetail() const {
        return _detail;
    }
    // changes with every applied detail response, the panel rebuilds when it changes
    uint32_t getDetailGeneration() const {
        return _detail.generation;
    }

    // ------------------------------------------------------------------------------------------
    // history graph of the sensor panel
    // ------------------------------------------------------------------------------------------
    // value of an index that does not point to a statistic request
    static constexpr TileIndex kStatsNone = HomeAssistant::Client::kNoStatsTile;
    // the graph of an open sensor panel is refreshed this often. The buckets of the recorder are
    // 5 minute aggregates, so a new one appears every 5 minutes - the minute keeps the window on
    // the clock and puts the newest bucket on screen within a minute of it being written
    static constexpr uint32_t kStatsRefreshInterval = 60000;
    static constexpr uint8_t kStatsDefaultHours = HomeAssistant::Client::kDefaultStatsHours;

    // The sensor panel is open: the statistics of the entity of the tile are requested (right away
    // and then every kStatsRefreshInterval) until closeStats() is called. A new range drops the
    // buckets of the previous one
    void requestStats(TileIndex index, uint8_t hours);
    void closeStats();
    // statistics the graph is drawn from: the tile they belong to (kStatsNone while the panel is
    // closed), the range in hours, the window (epoch seconds) and the buckets
    TileIndex getStatsTile() const {
        return _statsTile;
    }
    uint8_t getStatsHours() const {
        return _statsHours;
    }
    uint32_t getStatsStart() const {
        return _statsStart;
    }
    uint32_t getStatsEnd() const {
        return _statsEnd;
    }
    uint16_t getStatsCount() const {
        return _statsCount;
    }
    const Socket::Point *getStatsPoints() const {
        return _statsPoints;
    }
    // error of the last request, empty while it succeeded. No buckets and no error means that the
    // entity has no long term statistics
    const String &getStatsError() const {
        return _statsError;
    }
    // changes with every applied statistics response, the graph is redrawn when it changes
    uint32_t getStatsGeneration() const {
        return _statsGeneration;
    }
    // the request task is fetching the statistics
    bool isStatsPending() const {
        return _statsPending;
    }

    // text of the status line of the screen, empty while everything is fine. The screen reads it
    // on every tick (up to twice, 5 Hz), so the text is composed into the member and only rebuilt
    // when it changed - a String built per call would be one heap allocation per tick
    const String &getScreenStatus() const;
    // lines of the plugin status output
    void getStatus(Print &output) const;

private:
    // (re)reads the configuration and (re)starts the client
    void _reload();
    // tells the client which picture tiles of the visible page need an image
    void _updateImages();
    // maps the response of the request task into the tiles
    void _applyResponse(const char *payload, PageIndex page);
    // maps the detail response of the open panel into _detail (`length` is the length of the
    // payload, the buffer is sized from it)
    void _applyDetail(const char *payload, size_t length);
    // reads the capabilities of the panel of a tile (kCapXxx) from a state response
    void _readCapabilities(const char *payload, const Tile &tile, TileIndex index);
    TileValue &_value(TileIndex index) {
        return _config.getTileMutable(index).value;
    }
    bool _queue(Client::Action::Type type, TileIndex index, float value = 0, const char *text = nullptr, float value2 = 0);
    // marks a tile as waiting for the confirmation of a queued action
    void _markPending(TileIndex index);
    // compares what the entity reports with the action that is on its way (see expectedState)
    void _reconcilePending(TileIndex index, TileValue &value, const char *state);
    // gives up on an action (it failed or the entity never confirmed it): the tile shows the state
    // of the entity again
    void _revertPending(TileIndex index, const char *reason);

private:
    // pixel box of a picture tile on the panel, 0 while the screen did not build the tile
    struct PictureBox {
        uint16_t width{0};
        uint16_t height{0};
    };

    // The flags of one tile the main loop has to keep: the capabilities of its panel and the state
    // of an action that is on its way to the entity (the optimistic update). They are small and
    // read on every response and every tap, so they live in the internal DRAM - the reported values
    // are part of the tile model in the PSRAM (see Config::_tiles)
    struct TileFlags {
        // panel capabilities (kCapXxx)
        uint8_t capabilities{0};
        // an action was queued and the entity has not reported the change yet
        bool pending{false};
        // The state the user tapped and the entity is expected to report (UNKNOWN while no action
        // is on its way). The action is applied to the tile right away (optimistic): the screen
        // shows the result of the tap while the request is on its way and a response that still
        // reports the state of pendingState - the request was in flight while the action was sent -
        // does not take it back (see _reconcilePending()/_revertPending())
        TileState expectedState{TileState::UNKNOWN};
        // the state and the value the entity had when the action was queued (a response that still
        // reports them did not confirm the action, the fast polls have to continue)
        uint8_t pendingState{0};
        float pendingValue{0};
        uint32_t pendingSince{0};
        // last state a switch/light tile was drawn with (the poll repeats the same state and a
        // trace per tile and poll buries everything else), 0xff while it was not drawn yet
        uint8_t lastState{0xff};
    };

    Config _config;
    Client _client;
    // per-tile flags (capabilities, pending action), in the DRAM (see TileFlags)
    std::vector<TileFlags> _flags;
    // boxes of the picture tiles, registered by the screen
    PsramVector<PictureBox> _pictureBox;
    // page of the screen that is visible
    PageIndex _visiblePage{0};
    // attributes of the entity of the open panel (one at a time) and the response of the poll
    // before it. The two slots only exchange pointers (see Detail::swap), the values an entity
    // stopped reporting are carried over from the older one (see _applyDetail())
    Detail _detail;
    Detail _detailBefore;
    TileIndex _detailTile{kNoTile};
    // statistics of the open sensor panel: the tile and the range the graph was requested for, the
    // buckets of the last response (a copy of the ones of the client, the screen reads them from
    // the main loop) and the window they cover
    TileIndex _statsTile{kStatsNone};
    uint8_t _statsHours{kStatsDefaultHours};
    Socket::Point _statsPoints[Socket::kMaxPoints];
    uint16_t _statsCount{0};
    uint32_t _statsStart{0};
    uint32_t _statsEnd{0};
    String _statsError;
    uint32_t _statsGeneration{0};
    // when the next refresh of the graph is due
    uint32_t _statsNextFetch{0};
    bool _statsPending{false};
    String _configError;
    String _response;
    String _requestError;
    // buffer of getScreenStatus(), keeps its capacity between the ticks of the screen
    mutable String _screenStatus;
    int16_t _statusCode{0};
    uint32_t _duration{0};
    uint32_t _responseTime{0};
    uint32_t _requestCount{0};
    uint32_t _lastUpdate{0};
    // millis() of the last queued action, part of the trace (how long a state change needed)
    uint32_t _lastAction{0};
    uint32_t _lastConfigCheck{0};
    // incremented by _reload() for every version of the file that was loaded
    uint32_t _configGeneration{0};
    bool _fileMissing{false};
    bool _active{false};
};

} // namespace HomeAssistant
} // namespace WeatherStation2
