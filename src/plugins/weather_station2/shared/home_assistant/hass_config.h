/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

//
// Configuration of the Home Assistant screen: one hand written YAML file in the file system
// (/hass.yaml). The parser is line based and supports only the small subset documented in
// src/plugins/weather_station2/docs/hass_config.md: nested maps, block sequences of maps,
// comments, quoted and plain scalars. There is no YAML library in this firmware and the
// document is small enough to keep the parser readable.
//
// The module has no LVGL dependency, the screen maps the parsed model to widgets.
//

#include <Arduino_compat.h>
#include <vector>

#include "psram_allocator.h"

namespace WeatherStation2 {
namespace HomeAssistant {

// Tile and page indices. They are handed around and stored in the model, so the type has to cover
// every tile a file may define. uint32_t is the native word of the 32-bit MCU (no narrowing, no
// masking), and there is no hard tile limit any more - a configuration may hold as many tiles as
// fit into the PSRAM (a single page is bounded by its grid)
using TileIndex = uint32_t;
using PageIndex = uint32_t;

// path of the configuration file in the file system
static constexpr const char *kConfigFile = "/hass.yaml";
// deepest nesting of areas. Areas can contain areas, the limit only bounds the parser stack
static constexpr uint8_t kMaxNesting = 6;
// value of the index fields that do not point to a tile
static constexpr TileIndex kNoTile = 0xffffffff;
// limits of the configurable grid
static constexpr uint8_t kMaxGridCols = 8;
static constexpr uint8_t kMaxGridRows = 8;

// Size of a file of the file system, 0 while it does not exist or cannot be read. The dashboard
// polls this to notice a new version of the configuration: **an upload has to change the size of
// the file**, because the file is not read to look for changes (reading and hashing the file every
// check cost more than the whole state poll of the screen). A version that keeps the size of the
// previous one (renaming a tile from "NAME1" to "NAME2") is never loaded - add or remove a comment
// or a blank line to change the size as well
bool getFileInfo(const char *path, uint32_t &size);
// string sizes of the model
static constexpr uint8_t kEntityLength = 48;
static constexpr uint8_t kNameLength = 32;
static constexpr uint8_t kUnitLength = 12;

// refresh interval of a picture tile in seconds (the `refresh` key of the configuration
// file). The camera image is a separate request and much larger than a state response, so the
// interval is independent of hass.poll and can be as low as one second (the request task fetches
// one image at a time and never while a poll or an action of a tile is waiting)
static constexpr uint16_t kDefaultRefresh = 60;
static constexpr uint16_t kMinRefresh = 1;
static constexpr uint16_t kMaxRefresh = 3600;

// ------------------------------------------------------------------------------------------
// tile types
// ------------------------------------------------------------------------------------------
enum class TileType : uint8_t {
    SWITCH,
    LIGHT,
    SENSOR,
    BUTTON,
    DIMMER,
    CLIMATE,
    // empty space, reserves cells without drawing anything (the grid keeps its layout)
    SPACER,
    // opens a page with the tiles that are nested below it (one level of nesting)
    AREA,
    // preview of a camera entity (the `picture-entity` card of a HA dashboard). It is the only
    // type that may be two columns wide: `size: 2x2` (the default), `2x1` or `1x1`
    PICTURE,
    MAX,
};

// icons a tile can be drawn with. AUTO uses the icon of the type (the ESP32 core defines
// `DEFAULT` as a macro, so the enumerator cannot use that name)
enum class TileIcon : uint8_t {
    AUTO,
    BULB,
    PLUG,
    TOGGLE,
    THERMOMETER,
    HUMIDITY,
    BUTTON,
    DIMMER,
    RADIATOR,
    FAN,
    MOTION,
    // folder, the default icon of an area tile
    AREA,
    // appended, the values above stay stable: a flash (energy/power), a CO2 molecule and the
    // icons of a general purpose dashboard (mdi names are the values of the `icon:` key)
    FLASH,
    CO2,
    LOCK,
    GAUGE,
    CAMERA,
    REMOTE,
    LIGHTBULB_OFF,
    LIGHTBULB_ON,
    FLASH_OFF,
    HOME,
    HOME_ASSISTANT,
    // `icon: none` draws no icon at all. The tile shows the name (and the value) only, which
    // frees the room the icon needed: a value with a line break in it (a combined sensor) uses
    // it for its second line
    NONE,
    MAX,
};

const __FlashStringHelper *getTileTypeName(TileType type);
const __FlashStringHelper *getTileIconName(TileIcon icon);
bool parseTileType(const char *value, TileType &type);
bool parseTileIcon(const char *value, TileIcon &icon);

// cells the type occupies. Every tile is one column wide, the type only chooses the height: a
// dimmer (level fill) and a climate tile (+ / - bars and the readout) use two rows. The `size`
// key of the file overrides that for these two types: a 1x1 dimmer keeps the level fill and the
// drag, a 1x1 climate shows the readout without the +/- bars (the panel steps the setpoint).
// A picture tile is two cells wide and two rows high by default, a camera preview needs the
// space and it is the only type that may be two columns wide
static inline uint8_t getTileWidth(TileType type)
{
    return (type == TileType::PICTURE) ? 2 : 1;
}

static inline uint8_t getTileHeight(TileType type)
{
    return (type == TileType::DIMMER || type == TileType::CLIMATE || type == TileType::PICTURE) ? 2 : 1;
}

// true when the type may have a size of its own (see the `size` key of the configuration file)
static inline bool tileTypeHasSize(TileType type)
{
    return type == TileType::DIMMER || type == TileType::CLIMATE || type == TileType::PICTURE;
}

// largest size of a picture tile (2x2 cells), the width of the two other types stays 1
static constexpr uint8_t kMaxPictureWidth = 2;
static constexpr uint8_t kMaxPictureHeight = 2;

// ------------------------------------------------------------------------------------------
// state Home Assistant reports for a tile
// ------------------------------------------------------------------------------------------
// ON means "active": a switch/light that is on, a sensor that has a value or a climate that is not
// off
enum class TileState : uint8_t {
    UNKNOWN,
    OFF,
    ON,
    UNAVAILABLE,
};

// The state Home Assistant reported for one tile, filled from the response of the last request. It
// is part of the tile (see Tile), so the model of a tile is one object - the configuration and what
// the entity reports. The state of an action that is on its way to the entity (the pending flag and
// the values needed to confirm it) is not reported data and lives in the DRAM (Dashboard::TileFlags)
struct TileValue {
    TileState state{TileState::UNKNOWN};
    // sensor value, dimmer level in percent or target temperature of a climate
    float value{0};
    // climate: current temperature
    float current{0};
    float minTemp{15};
    float maxTemp{30};
    // climate: 0 off, 1 heat, 2 cool, 0xff unknown
    uint8_t mode{0xff};
    // climate: the action of the entity ("idle", "heating", "cooling", ...), empty when the
    // entity does not report it
    char action[16]{};
    // state of the entity as it was reported ("on", "off", "21.4", "home", ...). A sensor
    // tile shows it when the state is not a number (a binary sensor reports "on"/"off" and
    // "0" from the float conversion would be wrong). The template of a tile that asks several
    // entities at once puts the readouts in one string, separated by a line break, so the buffer is
    // the size of what such a value can be
    char text[48]{};
    // device class of the entity ("motion", "door", ...), empty when it has none
    char deviceClass[20]{};
    // unit of a sensor, from the configuration or from the entity attributes
    char unit[kUnitLength]{};
    // climate: the mode the entity reports as its state ("heat", "auto", "off", ...)
    char modeName[16]{};
};

// ------------------------------------------------------------------------------------------
// one tile of the dashboard: the configuration of the tile and the state of its entity
// ------------------------------------------------------------------------------------------
struct Tile {
    TileType type{TileType::SWITCH};
    TileIcon icon{TileIcon::AUTO};
    char entity[kEntityLength]{};
    char name[kNameLength]{};
    char unit[kUnitLength]{};
    uint8_t decimals{1};
    // grid position, 0 based, width x height cells
    uint8_t width{1};
    uint8_t height{1};
    // the `size` key was set in the file (the size of the type is used otherwise)
    bool hasSize{false};
    // grid position in the landscape layout of the file, 0 based
    uint8_t col{0};
    uint8_t row{0};
    bool hasPosition{false};
    // Position in the transposed grid a portrait display shows, 0 based. The tiles are placed in
    // the order of the file (left to right, top to down) in both grids, so a page reads the same
    // way whichever orientation is active (see Config::_placeTiles())
    uint8_t portraitCol{0};
    uint8_t portraitRow{0};
    // arc range of a dimmer (percent) or a climate (degrees)
    float min{0};
    float max{100};
    float step{1};
    // true when step was set in the file (the default depends on the type)
    bool hasStep{false};
    // picture tiles only: seconds between two images, the `refresh` key of the file
    uint16_t refresh{kDefaultRefresh};
    bool hasRefresh{false};
    // line of the file the tile was defined in (error messages)
    uint32_t line{0};
    // page the tile is drawn on: 0 = main page, 1..n = the page of an area tile
    PageIndex page{0};
    // area tiles only: the page with the tiles of the area, 0 while it has none
    PageIndex areaPage{0};
    // area tiles only: grid of the page of the area (the `grid:` block of the tile), 0 = the grid
    // of the document. The tiles of an area page are placed in this grid, the first cell is the
    // back tile of the page like on every other page
    uint8_t gridCols{0};
    uint8_t gridRows{0};
    // the state of the entity, reported by the last response (see TileValue)
    TileValue value;
};

// ------------------------------------------------------------------------------------------
// configuration
// ------------------------------------------------------------------------------------------
class Config {
public:
    // Seconds between two full updates of the visible page. The changes are **pushed** over the
    // websocket subscription of the screen (see Socket), this is only the safety net: a value that
    // changed while the connection was down is reported by it, and it proves that the subscription
    // is still alive. The default is a minute, the minimum is longer than a few seconds because a
    // resync subscribes the template again (one render on the Home Assistant side)
    static constexpr uint32_t kDefaultPollInterval = 60;
    static constexpr uint32_t kMinPollInterval = 5;
    static constexpr uint32_t kMaxPollInterval = 3600;
    static constexpr uint32_t kDefaultTimeout = 8;
    static constexpr uint8_t kDefaultGridCols = 4;
    // two rows: a cell is 113x123 px on the 480x320 panel, which is the aspect the reviewed
    // layout is drawn for (a file can still ask for more rows with "grid: rows: n")
    static constexpr uint8_t kDefaultGridRows = 2;

    Config();

    // reads and parses the file. Returns false when it is missing, unreadable or invalid, the
    // reason is in getError() (with the line number for parse errors)
    bool load(const char *path = kConfigFile);

    // true while a value was configured, regardless of the state of the file
    bool isLoaded() const {
        return _loaded;
    }
    // true while the file does not exist (or is empty)
    bool isFileMissing() const {
        return _fileMissing;
    }
    // size of the last load, 0 when the file was missing. The dashboard compares it with the file
    // to notice a new version of it (see getFileInfo())
    uint32_t getFileSize() const {
        return _fileSize;
    }
    // error of the last load, empty when it succeeded
    const String &getError() const {
        return _error;
    }
    const char *getPath() const {
        return _path.c_str();
    }

    // connection
    const char *getUrl() const {
        return _url.c_str();
    }
    const char *getToken() const {
        return _token.c_str();
    }
    uint32_t getPollInterval() const {
        return _pollInterval;
    }
    uint32_t getTimeout() const {
        return _timeout;
    }
    bool getVerify() const {
        return _verify;
    }

    // Grid of a page. The main page and the pages of the areas without a `grid:` block use the
    // grid of the document, an area can use a grid of its own (for example a 4x3 grid for a room
    // with many tiles on a dashboard that is 4x2)
    uint8_t getCols(PageIndex page = 0) const;
    uint8_t getRows(PageIndex page = 0) const;

    // tiles
    TileIndex getTileCount() const {
        return static_cast<TileIndex>(_tiles.size());
    }
    // The tile of an index, a static empty tile while the index is out of range or the model is
    // empty - the callers do not have to check the range before they read
    const Tile &getTile(TileIndex index) const {
        static const Tile empty;
        return (index < _tiles.size()) ? _tiles[index] : empty;
    }
    // the same for the writer of the reported state (the response and the actions of a tile)
    Tile &getTileMutable(TileIndex index) {
        static Tile empty;
        return (index < _tiles.size()) ? _tiles[index] : empty;
    }

    // pages: 1 = only the main page, an area adds one page each (0 = the main page)
    PageIndex getPageCount() const {
        return static_cast<PageIndex>(_pages.size());
    }
    // name of an area page, nullptr for the main page and for an index without an area
    const char *getPageName(PageIndex page) const;
    // page the back tile of that page returns to (0 = the main page)
    PageIndex getPageParent(PageIndex page) const {
        return (page < _pages.size()) ? _pages[page].parent : 0;
    }

private:
    // one page of the dashboard: the area tile that owns it (kNoTile for the main page) and the
    // page the back tile of the page returns to. _pages[0] is the main page and always exists
    struct Page {
        TileIndex areaTile{kNoTile};
        PageIndex parent{0};
    };

    // resets the model, called by load()
    void _reset();
    // parses the whole file, false on the first error
    bool _parse(const char *data, size_t length);
    // parses one tile key/value pair
    bool _parseTile(Tile &tile, const char *key, const char *keyEnd, const char *value, const char *valueEnd, uint32_t line);
    // validates the tiles and places them in the grid (both orientations)
    bool _placeTiles();
    // places the tiles of one page in its grid, `portrait` in the transposed one. Only the
    // landscape pass reports an error, a tile that cannot be placed in the transposed grid of the
    // page falls back to the order of the file
    bool _placePage(PageIndex page, bool portrait);
    // stores a placement in the field of that orientation
    static void _setPosition(Tile &tile, bool portrait, uint8_t col, uint8_t row);
    // sets _error to "line N: message"
    bool _fail(uint32_t line, const char *message);
    // "" for the main page, " of the area 'name'" otherwise (error messages)
    String _pageSuffix(PageIndex page) const;
    // true while the tile occupies the cell (used by the placement)
    bool _isCellFree(uint8_t col, uint8_t row, uint8_t width, uint8_t height, uint8_t cols, uint8_t rows) const;
    void _markCell(uint8_t col, uint8_t row, uint8_t width, uint8_t height);
    // the area tile that owns a page, nullptr while the page uses the grid of the document
    const Tile *_pageGrid(PageIndex page) const;

private:
    String _path;
    String _error;
    String _url;
    String _token;
    uint32_t _pollInterval{kDefaultPollInterval};
    uint32_t _timeout{kDefaultTimeout};
    bool _verify{false};
    uint8_t _cols{kDefaultGridCols};
    uint8_t _rows{kDefaultGridRows};
    // The tiles of the configuration, the configuration and the state Home Assistant reports for
    // every one of them. The buffer lives in the PSRAM (see PsramAllocator)
    PsramVector<Tile> _tiles;
    // one entry per page, _pages[0] is the main page
    std::vector<Page> _pages;
    // bitmask of the used cells of one row
    uint16_t _used[kMaxGridRows]{};
    // size of the loaded version of the file (see getFileSize() and getFileInfo())
    uint32_t _fileSize{0};
    bool _loaded{false};
    bool _fileMissing{false};
};

} // namespace HomeAssistant
} // namespace WeatherStation2
