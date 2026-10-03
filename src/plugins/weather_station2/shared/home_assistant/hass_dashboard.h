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
// one slot - the dashboard shows one panel at a time. The lists are the items the panel offers,
// comma separated (the panel splits them) and empty when the entity does not have that attribute.
struct Detail {
    // the values below are valid
    bool valid{false};
    // index of the tile the panel belongs to, kNoTile while no panel is open
    TileIndex tile{kNoTile};
    // increases with every response, the panel rebuilds its lists when it changes
    uint32_t generation{0};
    // climate: hvac mode (the state of the entity)
    char mode[16]{};
    // climate: preset mode ("none" when the entity has none)
    char preset[16]{};
    // climate: fan mode
    char fanMode[16]{};
    // light: current effect, empty when none is running
    char effect[24]{};
    // light: color mode of the entity ("hs", "color_temp", "brightness", ...)
    char colorMode[16]{};
    // light: hue in degrees and saturation in percent (hs_color)
    float hue{0};
    float saturation{0};
    // light: brightness in percent
    uint8_t level{0};
    // light: the colour modes the entity supports ("hs,rgb,color_temp"), empty when unknown
    char colorModes[48]{};
    // light: colour temperature in kelvin, 0 when the entity has none, and the range of the slider
    float colorTemp{0};
    float minColorTemp{0};
    float maxColorTemp{0};
    String modeList;
    String presetList;
    String fanModeList;
    String effectList;
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

    // text of the status line of the screen, empty while everything is fine
    String getScreenStatus() const;
    // lines of the plugin status output
    void getStatus(Print &output) const;

private:
    // (re)reads the configuration and (re)starts the client
    void _reload();
    // tells the client which picture tiles of the visible page need an image
    void _updateImages();
    // maps the response of the request task into the tiles
    void _applyResponse(const char *payload, PageIndex page);
    // maps the detail response of the open panel into _detail
    void _applyDetail(const char *payload);
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
    // attributes of the entity of the open panel (one at a time)
    Detail _detail;
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
