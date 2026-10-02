/**
 * Author: sascha_lammers@gmx.de
 */

#include "hass_config.h"

#include <PrintString.h>

#ifndef DEBUG_WEATHER_STATION2
#    define DEBUG_WEATHER_STATION2 1
#endif

#if DEBUG_WEATHER_STATION2
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

namespace WeatherStation2 {
namespace HomeAssistant {

// ------------------------------------------------------------------------------------------
// helpers of the parser
// ------------------------------------------------------------------------------------------
namespace {

// indentation of the file, tabs are rejected
static constexpr uint8_t kIndentStep = 2;

enum : uint8_t {
    kSectionNone = 0,
    kSectionHass,
    kSectionGrid,
    kSectionTiles,
};

// compares a range with a literal
bool _match(const char *begin, const char *end, const char *literal)
{
    const auto length = strlen(literal);
    if (static_cast<size_t>(end - begin) != length) {
        return false;
    }
    return strncmp(begin, literal, length) == 0;
}

// whitespace of the YAML subset. A carriage return is treated as whitespace so that a file
// written by a Windows editor (CRLF line endings) is parsed as well
inline bool _isWhitespace(char chr)
{
    return chr == ' ' || chr == '\t' || chr == '\r';
}

const char *_trimStart(const char *begin, const char *end)
{
    while (begin < end && _isWhitespace(*begin)) {
        begin++;
    }
    return begin;
}

const char *_trimEnd(const char *begin, const char *end)
{
    while (end > begin && _isWhitespace(end[-1])) {
        end--;
    }
    return end;
}

// first ':' of the line, nullptr when there is none
const char *_findColon(const char *begin, const char *end)
{
    for (auto ptr = begin; ptr < end; ptr++) {
        if (*ptr == ':') {
            return ptr;
        }
    }
    return nullptr;
}

// copies a value into a buffer, surrounding quotes are removed
bool _toChars(const char *begin, const char *end, char *output, size_t outputSize)
{
    if (outputSize == 0) {
        return false;
    }
    output[0] = 0;
    if (begin >= end) {
        return false;
    }
    if ((*begin == '"' || *begin == '\'') && (end - begin) >= 2 && end[-1] == *begin) {
        begin++;
        end--;
    }
    auto length = static_cast<size_t>(end - begin);
    if (length >= outputSize) {
        length = outputSize - 1;
    }
    memcpy(output, begin, length);
    output[length] = 0;
    return length != 0;
}

bool _toValueString(const char *begin, const char *end, String &output)
{
    char buffer[256];
    if (!_toChars(begin, end, buffer, sizeof(buffer))) {
        output = String();
        return false;
    }
    output = buffer;
    return true;
}

bool _toFloat(const char *begin, const char *end, float &value)
{
    if (begin >= end) {
        return false;
    }
    char buffer[24];
    auto length = static_cast<size_t>(end - begin);
    if (length > sizeof(buffer) - 1) {
        length = sizeof(buffer) - 1;
    }
    memcpy(buffer, begin, length);
    buffer[length] = 0;
    char *stop = nullptr;
    value = strtof(buffer, &stop);
    return stop && stop != buffer;
}

bool _toInt(const char *begin, const char *end, long &value)
{
    if (begin >= end) {
        return false;
    }
    char buffer[24];
    auto length = static_cast<size_t>(end - begin);
    if (length > sizeof(buffer) - 1) {
        length = sizeof(buffer) - 1;
    }
    memcpy(buffer, begin, length);
    buffer[length] = 0;
    char *stop = nullptr;
    value = strtol(buffer, &stop, 10);
    return stop && stop != buffer;
}

bool _toBool(const char *begin, const char *end, bool &value)
{
    char buffer[8];
    if (!_toChars(begin, end, buffer, sizeof(buffer))) {
        return false;
    }
    if (!strcasecmp(buffer, "true") || !strcasecmp(buffer, "yes") || !strcasecmp(buffer, "on") || !strcmp(buffer, "1")) {
        value = true;
        return true;
    }
    if (!strcasecmp(buffer, "false") || !strcasecmp(buffer, "no") || !strcasecmp(buffer, "off") || !strcmp(buffer, "0")) {
        value = false;
        return true;
    }
    return false;
}

// "[1, 2]" -> 1 based position
bool _toPosition(const char *begin, const char *end, uint8_t &col, uint8_t &row)
{
    if (begin >= end || *begin != '[' || end[-1] != ']') {
        return false;
    }
    begin++;
    end--;
    const char *comma = nullptr;
    for (auto ptr = begin; ptr < end; ptr++) {
        if (*ptr == ',') {
            comma = ptr;
            break;
        }
    }
    if (!comma) {
        return false;
    }
    long colValue = 0;
    long rowValue = 0;
    if (!_toInt(_trimStart(begin, comma), _trimEnd(begin, comma), colValue) ||
        !_toInt(_trimStart(comma + 1, end), _trimEnd(comma + 1, end), rowValue)) {
        return false;
    }
    if (colValue < 1 || rowValue < 1 || colValue > kMaxGridCols || rowValue > kMaxGridRows) {
        return false;
    }
    col = static_cast<uint8_t>(colValue - 1);
    row = static_cast<uint8_t>(rowValue - 1);
    return true;
}

// "1x2" -> 1 column, 2 rows
bool _toSize(const char *begin, const char *end, uint8_t &width, uint8_t &height)
{
    const char *separator = nullptr;
    for (auto ptr = begin; ptr < end; ptr++) {
        if (*ptr == 'x' || *ptr == 'X') {
            separator = ptr;
            break;
        }
    }
    if (!separator) {
        return false;
    }
    long widthValue = 0;
    long heightValue = 0;
    if (!_toInt(_trimStart(begin, separator), _trimEnd(begin, separator), widthValue) ||
        !_toInt(_trimStart(separator + 1, end), _trimEnd(separator + 1, end), heightValue)) {
        return false;
    }
    if (widthValue < 1 || heightValue < 1 || widthValue > kMaxGridCols || heightValue > kMaxGridRows) {
        return false;
    }
    width = static_cast<uint8_t>(widthValue);
    height = static_cast<uint8_t>(heightValue);
    return true;
}

// entity id: domain.object_id, lowercase, one dot
bool _isValidEntityId(const char *entity)
{
    const char *dot = nullptr;
    for (const char *ptr = entity; *ptr; ptr++) {
        const char chr = *ptr;
        if (chr == '.') {
            if (dot) {
                return false;
            }
            dot = ptr;
            continue;
        }
        if (!((chr >= 'a' && chr <= 'z') || (chr >= '0' && chr <= '9') || chr == '_')) {
            return false;
        }
    }
    return dot && dot != entity && dot[1] != 0;
}

// "living_room_lamp" of "switch.living_room_lamp" with the underscores replaced
void _defaultName(const char *entity, char *output, size_t outputSize)
{
    const char *name = strrchr(entity, '.');
    name = name ? name + 1 : entity;
    size_t index = 0;
    for (; name[index] && index + 1 < outputSize; index++) {
        output[index] = (name[index] == '_') ? ' ' : name[index];
    }
    output[index] = 0;
}

} // namespace

// ------------------------------------------------------------------------------------------
// names of the types and icons (status output and error messages)
// ------------------------------------------------------------------------------------------
const __FlashStringHelper *getTileTypeName(TileType type)
{
    switch (type) {
    case TileType::SWITCH:
        return F("switch");
    case TileType::LIGHT:
        return F("light");
    case TileType::SENSOR:
        return F("sensor");
    case TileType::BUTTON:
        return F("button");
    case TileType::DIMMER:
        return F("dimmer");
    case TileType::CLIMATE:
        return F("climate");
    case TileType::SPACER:
        return F("spacer");
    case TileType::AREA:
        return F("area");
    case TileType::PICTURE:
        return F("picture");
    default:
        break;
    }
    return F("unknown");
}

const __FlashStringHelper *getTileIconName(TileIcon icon)
{
    switch (icon) {
    case TileIcon::BULB:
        return F("bulb");
    case TileIcon::PLUG:
        return F("plug");
    case TileIcon::TOGGLE:
        return F("toggle");
    case TileIcon::THERMOMETER:
        return F("thermometer");
    case TileIcon::HUMIDITY:
        return F("humidity");
    case TileIcon::BUTTON:
        return F("button");
    case TileIcon::DIMMER:
        return F("dimmer");
    case TileIcon::RADIATOR:
        return F("radiator");
    case TileIcon::FAN:
        return F("fan");
    case TileIcon::MOTION:
        return F("motion");
    case TileIcon::AREA:
        return F("area");
    case TileIcon::FLASH:
        return F("flash");
    case TileIcon::CO2:
        return F("co2");
    case TileIcon::LOCK:
        return F("lock");
    case TileIcon::GAUGE:
        return F("gauge");
    case TileIcon::CAMERA:
        return F("camera");
    case TileIcon::REMOTE:
        return F("remote");
    case TileIcon::LIGHTBULB_OFF:
        return F("lightbulb-off");
    case TileIcon::LIGHTBULB_ON:
        return F("lightbulb-on");
    case TileIcon::FLASH_OFF:
        return F("flash-off");
    case TileIcon::HOME:
        return F("home");
    case TileIcon::HOME_ASSISTANT:
        return F("home-assistant");
    case TileIcon::NONE:
        return F("none");
    default:
        break;
    }
    return F("auto");
}

bool parseTileType(const char *value, TileType &type)
{
    if (!strcasecmp(value, "switch")) {
        type = TileType::SWITCH;
    }
    else if (!strcasecmp(value, "light")) {
        type = TileType::LIGHT;
    }
    else if (!strcasecmp(value, "sensor")) {
        type = TileType::SENSOR;
    }
    else if (!strcasecmp(value, "button")) {
        type = TileType::BUTTON;
    }
    else if (!strcasecmp(value, "dimmer")) {
        type = TileType::DIMMER;
    }
    else if (!strcasecmp(value, "climate")) {
        type = TileType::CLIMATE;
    }
    else if (!strcasecmp(value, "spacer")) {
        type = TileType::SPACER;
    }
    else if (!strcasecmp(value, "area")) {
        type = TileType::AREA;
    }
    else if (!strcasecmp(value, "picture") || !strcasecmp(value, "picture-entity")) {
        // `picture-entity` is the type of the card of a HA dashboard the tile is modelled on
        type = TileType::PICTURE;
    }
    else {
        return false;
    }
    return true;
}

bool parseTileIcon(const char *value, TileIcon &icon)
{
    if (!strcasecmp(value, "auto") || !strcasecmp(value, "default")) {
        icon = TileIcon::AUTO;
    }
    else if (!strcasecmp(value, "bulb")) {
        icon = TileIcon::BULB;
    }
    else if (!strcasecmp(value, "plug")) {
        icon = TileIcon::PLUG;
    }
    else if (!strcasecmp(value, "toggle")) {
        icon = TileIcon::TOGGLE;
    }
    else if (!strcasecmp(value, "thermometer")) {
        icon = TileIcon::THERMOMETER;
    }
    else if (!strcasecmp(value, "humidity")) {
        icon = TileIcon::HUMIDITY;
    }
    else if (!strcasecmp(value, "button")) {
        icon = TileIcon::BUTTON;
    }
    else if (!strcasecmp(value, "dimmer")) {
        icon = TileIcon::DIMMER;
    }
    else if (!strcasecmp(value, "radiator")) {
        icon = TileIcon::RADIATOR;
    }
    else if (!strcasecmp(value, "fan")) {
        icon = TileIcon::FAN;
    }
    else if (!strcasecmp(value, "motion")) {
        icon = TileIcon::MOTION;
    }
    else if (!strcasecmp(value, "flash")) {
        icon = TileIcon::FLASH;
    }
    else if (!strcasecmp(value, "co2")) {
        icon = TileIcon::CO2;
    }
    else if (!strcasecmp(value, "lock")) {
        icon = TileIcon::LOCK;
    }
    else if (!strcasecmp(value, "gauge")) {
        icon = TileIcon::GAUGE;
    }
    else if (!strcasecmp(value, "camera")) {
        icon = TileIcon::CAMERA;
    }
    else if (!strcasecmp(value, "remote")) {
        icon = TileIcon::REMOTE;
    }
    else if (!strcasecmp(value, "lightbulb-off")) {
        icon = TileIcon::LIGHTBULB_OFF;
    }
    else if (!strcasecmp(value, "lightbulb-on")) {
        icon = TileIcon::LIGHTBULB_ON;
    }
    else if (!strcasecmp(value, "flash-off")) {
        icon = TileIcon::FLASH_OFF;
    }
    else if (!strcasecmp(value, "home")) {
        icon = TileIcon::HOME;
    }
    else if (!strcasecmp(value, "home-assistant")) {
        icon = TileIcon::HOME_ASSISTANT;
    }
    else if (!strcasecmp(value, "area")) {
        icon = TileIcon::AREA;
    }
    else if (!strcasecmp(value, "none")) {
        // no icon, the tile shows the name (and the value) only
        icon = TileIcon::NONE;
    }
    else {
        return false;
    }
    return true;
}

// ------------------------------------------------------------------------------------------
// config
// ------------------------------------------------------------------------------------------
Config::Config()
{
    _reset();
}

void Config::_reset()
{
    _error = String();
    _url = String();
    _token = String();
    _pollInterval = kDefaultPollInterval;
    _timeout = kDefaultTimeout;
    _verify = false;
    _cols = kDefaultGridCols;
    _rows = kDefaultGridRows;
    for (auto &tile : _tiles) {
        tile = Tile();
    }
    _tileCount = 0;
    _pageCount = 1;
    memset(_pageArea, kNoTile, sizeof(_pageArea));
    memset(_pageParent, 0, sizeof(_pageParent));
    memset(_used, 0, sizeof(_used));
    _loaded = false;
    _fileMissing = false;
}

bool Config::_fail(uint8_t line, const char *message)
{
    if (line) {
        _error = PrintString(F("line %u: %s"), static_cast<unsigned>(line), message);
    }
    else {
        _error = message;
    }
    __LDBG_printf("%s: %s", _path.c_str(), _error.c_str());
    return false;
}

// Only the size of the file, the file itself is not read: the check runs every 3 seconds and
// reading + hashing the file is as expensive as the whole state poll of the screen (a stat and an
// open of the file system cost ~6 ms each, the CRC16 of 10 KB another ~9 ms). An upload that keeps
// the size of the previous version is not noticed, see the comment in hass_config.h
bool getFileInfo(const char *path, uint32_t &size)
{
    size = 0;

    KFCFS_begin();
    // no KFCFS.exists(): a failed open reports a missing file as well and saves the stat
    auto file = KFCFS.open(path, fs::FileOpenMode::read);
    if (!file) {
        return false;
    }
    size = static_cast<uint32_t>(file.size());
    file.close();
    return true;
}

bool Config::load(const char *path)
{
    _reset();
    _path = path;
    _fileSize = 0;

    KFCFS_begin();
    if (!KFCFS.exists(path)) {
        _fileMissing = true;
        _error = PrintString(F("%s is missing"), path);
        __LDBG_printf("%s", _error.c_str());
        return false;
    }

    auto file = KFCFS.open(path, fs::FileOpenMode::read);
    if (!file) {
        _fileMissing = true;
        _error = PrintString(F("cannot open %s"), path);
        __LDBG_printf("%s", _error.c_str());
        return false;
    }

    const auto size = static_cast<uint32_t>(file.size());
    _fileSize = size;
    if (size == 0) {
        _fileMissing = true;
        _error = PrintString(F("%s is empty"), path);
        file.close();
        __LDBG_printf("%s", _error.c_str());
        return false;
    }
    if (size > kMaxFileSize) {
        _error = PrintString(F("%s is too large (%u bytes, max %u)"), path, static_cast<unsigned>(size), static_cast<unsigned>(kMaxFileSize));
        file.close();
        __LDBG_printf("%s", _error.c_str());
        return false;
    }

    String data = file.readString();
    file.close();

    if (!_parse(data.c_str(), data.length())) {
        return false;
    }
    _loaded = true;
    __LDBG_printf("%s: %u tile(s) on %u page(s), %ux%u grid, poll=%us, url=%s", path, static_cast<unsigned>(_tileCount),
                  static_cast<unsigned>(_pageCount), static_cast<unsigned>(_cols), static_cast<unsigned>(_rows),
                  static_cast<unsigned>(_pollInterval), _url.c_str());
    return true;
}

bool Config::_parse(const char *data, size_t length)
{
    uint8_t section = kSectionNone;
    Tile *tile = nullptr;
    // index of the list item the keys belong to and the level it is at
    uint8_t tileIndex = kNoTile;
    uint8_t tileLevel = 0;
    // index of the area that owns a nesting level (kNoTile while the level is closed)
    uint8_t areaStack[kMaxNesting];
    memset(areaStack, kNoTile, sizeof(areaStack));
    // area tile whose `grid:` block is open and the indentation of its keys (the block is closed
    // by the next line that is not indented that far)
    Tile *gridTile = nullptr;
    int gridIndent = 0;
    uint8_t line = 0;

    // a UTF-8 byte order mark written by an editor is not part of the document
    if (length >= 3 && static_cast<uint8_t>(data[0]) == 0xef && static_cast<uint8_t>(data[1]) == 0xbb && static_cast<uint8_t>(data[2]) == 0xbf) {
        data += 3;
        length -= 3;
    }

    size_t pos = 0;
    while (pos < length) {
        size_t end = pos;
        while (end < length && data[end] != '\n') {
            end++;
        }
        const char *lineStart = data + pos;
        const char *lineEnd = data + end;
        pos = (end < length) ? end + 1 : length;
        line++;

        // leading whitespace, tabs are not allowed. A carriage return of a CRLF line ending is
        // skipped as well (it is not an indentation)
        const char *ptr = lineStart;
        int indent = 0;
        while (ptr < lineEnd && (*ptr == ' ' || *ptr == '\t' || *ptr == '\r')) {
            if (*ptr == '\t') {
                return _fail(line, "tab used for indentation, use spaces");
            }
            if (*ptr != '\r') {
                indent++;
            }
            ptr++;
        }
        if (ptr >= lineEnd) {
            continue;
        }
        if (*ptr == '#') {
            continue;
        }

        // comment at the end of the line, quotes protect a '#'
        bool inSingle = false;
        bool inDouble = false;
        for (const char *scan = ptr; scan < lineEnd; scan++) {
            if (*scan == '\'' && !inDouble) {
                inSingle = !inSingle;
            }
            else if (*scan == '"' && !inSingle) {
                inDouble = !inDouble;
            }
            else if (*scan == '#' && !inSingle && !inDouble) {
                lineEnd = scan;
                break;
            }
        }
        lineEnd = _trimEnd(ptr, lineEnd);
        if (lineEnd <= ptr) {
            continue;
        }

        // ---------------------------------------------------------------- block keys
        if (indent == 0) {
            tile = nullptr;
            tileIndex = kNoTile;
            memset(areaStack, kNoTile, sizeof(areaStack));
            const auto colon = _findColon(ptr, lineEnd);
            if (!colon) {
                return _fail(line, PrintString(F("expected 'key:', got '%.*s'"), static_cast<int>(lineEnd - ptr), ptr).c_str());
            }
            const auto keyEnd = _trimEnd(ptr, colon);
            if (_trimStart(colon + 1, lineEnd) != lineEnd) {
                return _fail(line, "block key with a value, expected 'key:'");
            }
            if (_match(ptr, keyEnd, "hass")) {
                section = kSectionHass;
            }
            else if (_match(ptr, keyEnd, "grid")) {
                section = kSectionGrid;
            }
            else if (_match(ptr, keyEnd, "tiles")) {
                section = kSectionTiles;
            }
            else {
                return _fail(line, PrintString(F("unknown key '%.*s'"), static_cast<int>(keyEnd - ptr), ptr).c_str());
            }
            continue;
        }

        if (indent % kIndentStep) {
            return _fail(line, "wrong indentation, use a multiple of 2 spaces");
        }
        const auto colon = _findColon(ptr, lineEnd);
        const auto value = colon ? _trimStart(colon + 1, lineEnd) : lineEnd;

        switch (section) {
        case kSectionHass:
            {
                if (!colon) {
                    return _fail(line, "expected 'key: value'");
                }
                const auto keyEnd = _trimEnd(ptr, colon);
                if (_match(ptr, keyEnd, "url")) {
                    if (!_toValueString(value, lineEnd, _url)) {
                        return _fail(line, "hass.url is empty");
                    }
                    // a trailing slash would produce "//api/..." for every request
                    StrWrapper(_url).rtrim('/');
                }
                else if (_match(ptr, keyEnd, "token")) {
                    if (!_toValueString(value, lineEnd, _token)) {
                        return _fail(line, "hass.token is empty");
                    }
                }
                else if (_match(ptr, keyEnd, "poll")) {
                    long number = 0;
                    if (!_toInt(value, lineEnd, number) || number < static_cast<long>(kMinPollInterval) || number > static_cast<long>(kMaxPollInterval)) {
                        return _fail(line, PrintString(F("hass.poll must be between %u and %u"), static_cast<unsigned>(kMinPollInterval), static_cast<unsigned>(kMaxPollInterval)).c_str());
                    }
                    _pollInterval = static_cast<uint32_t>(number);
                }
                else if (_match(ptr, keyEnd, "timeout")) {
                    long number = 0;
                    if (!_toInt(value, lineEnd, number) || number < 1 || number > 120) {
                        return _fail(line, "hass.timeout must be between 1 and 120");
                    }
                    _timeout = static_cast<uint32_t>(number);
                }
                else if (_match(ptr, keyEnd, "verify")) {
                    if (!_toBool(value, lineEnd, _verify)) {
                        return _fail(line, "hass.verify must be true or false");
                    }
                }
                else {
                    return _fail(line, PrintString(F("unknown key '%.*s'"), static_cast<int>(keyEnd - ptr), ptr).c_str());
                }
            }
            break;

        case kSectionGrid:
            {
                if (!colon) {
                    return _fail(line, "expected 'key: value'");
                }
                const auto keyEnd = _trimEnd(ptr, colon);
                long number = 0;
                if (_match(ptr, keyEnd, "cols")) {
                    if (!_toInt(value, lineEnd, number) || number < 1 || number > kMaxGridCols) {
                        return _fail(line, PrintString(F("grid.cols must be between 1 and %u"), static_cast<unsigned>(kMaxGridCols)).c_str());
                    }
                    _cols = static_cast<uint8_t>(number);
                }
                else if (_match(ptr, keyEnd, "rows")) {
                    if (!_toInt(value, lineEnd, number) || number < 1 || number > kMaxGridRows) {
                        return _fail(line, PrintString(F("grid.rows must be between 1 and %u"), static_cast<unsigned>(kMaxGridRows)).c_str());
                    }
                    _rows = static_cast<uint8_t>(number);
                }
                else {
                    return _fail(line, PrintString(F("unknown key '%.*s'"), static_cast<int>(keyEnd - ptr), ptr).c_str());
                }
            }
            break;

        case kSectionTiles:
            {
                const bool isItem = (*ptr == '-' && (ptr + 1 == lineEnd || ptr[1] == ' '));

                // Keys of the `grid:` block of an area tile. They are indented one step more than
                // the keys of the list item and the block ends with the next line that is not
                // indented that far (or with the next list item, which is at the same indentation
                // as the keys of the block)
                if (gridTile) {
                    if (indent > gridIndent) {
                        return _fail(line, "wrong indentation, use a multiple of 2 spaces");
                    }
                    if (indent == gridIndent && !isItem) {
                        const auto gridColon = _findColon(ptr, lineEnd);
                        if (!gridColon) {
                            return _fail(line, "expected 'key: value'");
                        }
                        const auto gridKeyEnd = _trimEnd(ptr, gridColon);
                        long number = 0;
                        if (_match(ptr, gridKeyEnd, "cols")) {
                            if (!_toInt(_trimStart(gridColon + 1, lineEnd), lineEnd, number) || number < 1 || number > kMaxGridCols) {
                                return _fail(line, PrintString(F("grid.cols must be between 1 and %u"), static_cast<unsigned>(kMaxGridCols)).c_str());
                            }
                            gridTile->gridCols = static_cast<uint8_t>(number);
                        }
                        else if (_match(ptr, gridKeyEnd, "rows")) {
                            if (!_toInt(_trimStart(gridColon + 1, lineEnd), lineEnd, number) || number < 1 || number > kMaxGridRows) {
                                return _fail(line, PrintString(F("grid.rows must be between 1 and %u"), static_cast<unsigned>(kMaxGridRows)).c_str());
                            }
                            gridTile->gridRows = static_cast<uint8_t>(number);
                        }
                        else {
                            return _fail(line, PrintString(F("unknown key '%.*s' in the grid of an area"), static_cast<int>(gridKeyEnd - ptr), ptr).c_str());
                        }
                        continue;
                    }
                    // the block ended, the line is handled as a tile key below
                    gridTile = nullptr;
                }

                // The nesting level follows from the indentation: a list item is at
                // 2 + 4 * level, the keys of that item at 4 + 4 * level (one level per area).
                const auto step = static_cast<int>(kIndentStep * 2);
                const auto base = static_cast<int>(isItem ? kIndentStep : kIndentStep * 2);
                if (indent < base || ((static_cast<int>(indent) - base) % step) != 0) {
                    return _fail(line, "wrong indentation, expected 2 spaces per nesting level");
                }
                const auto level = static_cast<uint8_t>((static_cast<int>(indent) - base) / step);
                if (level >= kMaxNesting) {
                    return _fail(line, "wrong indentation");
                }

                // a new list item, the first key can be on the same line
                if (isItem) {
                    if (level && areaStack[level - 1] == kNoTile) {
                        return _fail(line, "list item without an area above it");
                    }
                    if (_tileCount >= kMaxTiles) {
                        return _fail(line, PrintString(F("too many tiles, the maximum is %u"), static_cast<unsigned>(kMaxTiles)).c_str());
                    }
                    tileIndex = _tileCount++;
                    tile = &_tiles[tileIndex];
                    tile->line = line;
                    tile->page = (level == 0) ? 0 : _tiles[areaStack[level - 1]].areaPage;
                    // the lists below this level are finished
                    for (uint8_t i = level; i < kMaxNesting; i++) {
                        areaStack[i] = kNoTile;
                    }
                    tileLevel = level;
                    ptr = _trimStart(ptr + 1, lineEnd);
                    if (ptr >= lineEnd) {
                        continue;
                    }
                }
                else if (!tile) {
                    return _fail(line, "key without a list item ('tiles:' must be the last key of an area)");
                }
                else if (tileLevel != level) {
                    return _fail(line, "wrong indentation, the key does not belong to the list item above it");
                }

                const auto itemColon = _findColon(ptr, lineEnd);
                if (!itemColon) {
                    return _fail(line, "expected 'key: value'");
                }
                const auto itemKeyEnd = _trimEnd(ptr, itemColon);

                // the nested list of an area
                if (!isItem && _match(ptr, itemKeyEnd, "tiles")) {
                    if (tile->type != TileType::AREA) {
                        return _fail(line, PrintString(F("only an area has tiles, the type is '%s'"), getTileTypeName(tile->type)).c_str());
                    }
                    if (_trimStart(itemColon + 1, lineEnd) != lineEnd) {
                        return _fail(line, "tiles: has no value, use a block sequence");
                    }
                    if (level + 1 >= kMaxNesting) {
                        return _fail(line, PrintString(F("too many levels of areas, the maximum is %u"), static_cast<unsigned>(kMaxNesting - 1)).c_str());
                    }
                    if (_pageCount >= kMaxPages) {
                        return _fail(line, PrintString(F("too many areas, the maximum is %u"), static_cast<unsigned>(kMaxPages - 1)).c_str());
                    }
                    const auto page = _pageCount++;
                    tile->areaPage = page;
                    _pageArea[page] = tileIndex;
                    _pageParent[page] = tile->page;
                    areaStack[level] = tileIndex;
                    // the keys of the tiles below follow as their own list items
                    tile = nullptr;
                    continue;
                }
                // an area can use a grid of its own for its page, the keys of the block follow
                if (!isItem && _match(ptr, itemKeyEnd, "grid")) {
                    if (tile->type != TileType::AREA) {
                        return _fail(line, PrintString(F("only an area has a grid of its own, the type is '%s'"), getTileTypeName(tile->type)).c_str());
                    }
                    if (_trimStart(itemColon + 1, lineEnd) != lineEnd) {
                        return _fail(line, "grid: has no value, use a block with cols and rows");
                    }
                    gridTile = tile;
                    gridIndent = indent + kIndentStep;
                    continue;
                }
                if (!_parseTile(*tile, ptr, itemKeyEnd, _trimStart(itemColon + 1, lineEnd), lineEnd, line)) {
                    return false;
                }
            }
            break;

        default:
            return _fail(line, "expected 'hass:', 'grid:' or 'tiles:'");
        }
    }

    // ---------------------------------------------------------------- validation
    if (!_tileCount) {
        return _fail(0, "no tiles configured");
    }
    for (uint8_t i = 0; i < _tileCount; i++) {
        auto &item = _tiles[i];
        if (item.hasRefresh && item.type != TileType::PICTURE) {
            return _fail(item.line, "only a picture tile has a refresh interval");
        }
        switch (item.type) {
        case TileType::SPACER:
            // reserves cells, it has no entity and no label
            if (item.entity[0]) {
                return _fail(item.line, "a spacer has no entity");
            }
            break;
        case TileType::AREA:
            if (item.entity[0]) {
                return _fail(item.line, "an area has no entity");
            }
            if (!item.name[0]) {
                return _fail(item.line, "an area needs a name");
            }
            if (!item.areaPage) {
                return _fail(item.line, PrintString(F("the area '%s' has no tiles"), item.name).c_str());
            }
            // the grid of an area page is the grid of the document for the value it does not set
            if (item.gridCols && !item.gridRows) {
                item.gridRows = _rows;
            }
            else if (!item.gridCols && item.gridRows) {
                item.gridCols = _cols;
            }
            break;
        default:
            if (!item.entity[0]) {
                return _fail(item.line, "tile without an entity");
            }
            if (!_isValidEntityId(item.entity)) {
                return _fail(item.line, PrintString(F("'%s' is not a valid entity id"), item.entity).c_str());
            }
            if (item.type == TileType::PICTURE && strncasecmp(item.entity, "camera.", 7)) {
                return _fail(item.line, "a picture tile needs a camera entity (camera.<name>)");
            }
            if (!item.name[0]) {
                _defaultName(item.entity, item.name, sizeof(item.name));
            }
            break;
        }
        if (!item.hasStep) {
            // one percent for a dimmer, half a degree for a thermostat
            item.step = (item.type == TileType::CLIMATE) ? 0.5f : 1.0f;
        }
    }
    if (!_url.length()) {
        return _fail(0, "hass.url is missing");
    }
    if (!_url.startsWith(F("http://")) && !_url.startsWith(F("https://"))) {
        return _fail(0, "hass.url must start with http:// or https://");
    }
    if (!_token.length()) {
        return _fail(0, "hass.token is missing");
    }
    return _placeTiles();
}

bool Config::_parseTile(Tile &tile, const char *key, const char *keyEnd, const char *value, const char *valueEnd, uint8_t line)
{
    if (_match(key, keyEnd, "type")) {
        char buffer[16];
        if (!_toChars(value, valueEnd, buffer, sizeof(buffer)) || !parseTileType(buffer, tile.type)) {
            return _fail(line, PrintString(F("unknown tile type '%.*s'"), static_cast<int>(valueEnd - value), value).c_str());
        }
    }
    else if (_match(key, keyEnd, "entity")) {
        char buffer[kEntityLength];
        if (!_toChars(value, valueEnd, buffer, sizeof(buffer))) {
            return _fail(line, "entity is empty");
        }
        // The entity of a picture tile is the camera it shows (the image request uses it, not the
        // state of the entity). A second `entity` key with a different value is a mistake in the
        // file and not a second entity
        if (tile.entity[0] && strcasecmp(tile.entity, buffer)) {
            return _fail(line, "the tile has two different entity ids");
        }
        _toChars(value, valueEnd, tile.entity, sizeof(tile.entity));
    }
    else if (_match(key, keyEnd, "refresh")) {
        long number = 0;
        if (!_toInt(value, valueEnd, number) || number < static_cast<long>(kMinRefresh) || number > static_cast<long>(kMaxRefresh)) {
            return _fail(line, PrintString(F("refresh must be between %u and %u seconds"), static_cast<unsigned>(kMinRefresh), static_cast<unsigned>(kMaxRefresh)).c_str());
        }
        tile.refresh = static_cast<uint16_t>(number);
        tile.hasRefresh = true;
    }
    else if (_match(key, keyEnd, "name")) {
        if (!_toChars(value, valueEnd, tile.name, sizeof(tile.name))) {
            return _fail(line, "name is empty");
        }
    }
    else if (_match(key, keyEnd, "unit")) {
        if (!_toChars(value, valueEnd, tile.unit, sizeof(tile.unit))) {
            return _fail(line, "unit is empty");
        }
    }
    else if (_match(key, keyEnd, "icon")) {
        char buffer[16];
        if (!_toChars(value, valueEnd, buffer, sizeof(buffer)) || !parseTileIcon(buffer, tile.icon)) {
            return _fail(line, PrintString(F("unknown icon '%.*s'"), static_cast<int>(valueEnd - value), value).c_str());
        }
    }
    else if (_match(key, keyEnd, "position")) {
        if (!_toPosition(value, valueEnd, tile.col, tile.row)) {
            return _fail(line, "position must be '[col, row]' with 1 based values");
        }
        tile.hasPosition = true;
    }
    else if (_match(key, keyEnd, "size")) {
        // the type is checked while the tiles are placed, the keys of a tile may be in any order
        if (!_toSize(value, valueEnd, tile.width, tile.height)) {
            return _fail(line, "size must be 'columns x rows', for example 1x1 or 1x2");
        }
        tile.hasSize = true;
    }
    else if (_match(key, keyEnd, "decimals")) {
        long number = 0;
        if (!_toInt(value, valueEnd, number) || number < 0 || number > 4) {
            return _fail(line, "decimals must be between 0 and 4");
        }
        tile.decimals = static_cast<uint8_t>(number);
    }
    else if (_match(key, keyEnd, "min")) {
        if (!_toFloat(value, valueEnd, tile.min)) {
            return _fail(line, "min must be a number");
        }
    }
    else if (_match(key, keyEnd, "max")) {
        if (!_toFloat(value, valueEnd, tile.max)) {
            return _fail(line, "max must be a number");
        }
    }
    else if (_match(key, keyEnd, "step")) {
        if (!_toFloat(value, valueEnd, tile.step) || tile.step <= 0) {
            return _fail(line, "step must be a number greater than 0");
        }
        tile.hasStep = true;
    }
    else {
        return _fail(line, PrintString(F("unknown tile key '%.*s'"), static_cast<int>(keyEnd - key), key).c_str());
    }
    return true;
}

bool Config::_isCellFree(uint8_t col, uint8_t row, uint8_t width, uint8_t height, uint8_t cols, uint8_t rows) const
{
    if (col + width > cols || row + height > rows) {
        return false;
    }
    for (uint8_t r = row; r < row + height; r++) {
        for (uint8_t c = col; c < col + width; c++) {
            if (_used[r] & (1U << c)) {
                return false;
            }
        }
    }
    return true;
}

void Config::_markCell(uint8_t col, uint8_t row, uint8_t width, uint8_t height)
{
    for (uint8_t r = row; r < row + height; r++) {
        for (uint8_t c = col; c < col + width; c++) {
            _used[r] |= static_cast<uint16_t>(1U << c);
        }
    }
}

void Config::_setPosition(Tile &tile, bool portrait, uint8_t col, uint8_t row)
{
    if (portrait) {
        tile.portraitCol = col;
        tile.portraitRow = row;
        return;
    }
    tile.col = col;
    tile.row = row;
}

bool Config::_placePage(uint8_t page, bool portrait)
{
    // The grid of the page: the document grid, or the grid of the area that owns the page. A
    // portrait display shows it transposed (4x3 becomes 3x4), so the cells keep their shape
    const auto gridCols = getCols(page);
    const auto gridRows = getRows(page);
    const uint8_t cols = static_cast<uint8_t>(portrait ? gridRows : gridCols);
    const uint8_t rows = static_cast<uint8_t>(portrait ? gridCols : gridRows);
    memset(_used, 0, sizeof(_used));
    if (page) {
        // the first cell of an area page is the back tile that closes it
        _markCell(0, 0, 1, 1);
    }
    for (uint8_t i = 0; i < _tileCount; i++) {
        auto &tile = _tiles[i];
        if (tile.page != page) {
            continue;
        }
        // the span is swapped with the cells of the transposed grid: a tile that is two cells tall
        // in landscape is two cells wide in portrait
        const uint8_t width = static_cast<uint8_t>(portrait ? tile.height : tile.width);
        const uint8_t height = static_cast<uint8_t>(portrait ? tile.width : tile.height);
        if (!portrait && (tile.width > cols || tile.height > rows)) {
            return _fail(tile.line, PrintString(F("a %s tile needs a %ux%u block, the grid is %ux%u"), getTileTypeName(tile.type),
                                                static_cast<unsigned>(tile.width), static_cast<unsigned>(tile.height),
                                                static_cast<unsigned>(cols), static_cast<unsigned>(rows)).c_str());
        }
        if (tile.hasPosition) {
            if (!portrait && page && tile.col == 0 && tile.row == 0) {
                return _fail(tile.line, "the first cell of an area page is used by the back tile");
            }
            // A position names a cell of the grid that is shown, so a tile that declares one keeps
            // its column and its row in both orientations. The transposed grid can be narrower than
            // the document grid: a position that does not exist in it (or a block that is occupied
            // there) falls back to the order of the file - the file is written for landscape and
            // must not fail because of the other orientation
            if (_isCellFree(tile.col, tile.row, width, height, cols, rows)) {
                _setPosition(tile, portrait, tile.col, tile.row);
                _markCell(tile.col, tile.row, width, height);
                continue;
            }
            if (!portrait) {
                return _fail(tile.line, "position is outside the grid or overlaps another tile");
            }
        }
        bool placed = false;
        for (uint8_t row = 0; row + height <= rows && !placed; row++) {
            for (uint8_t col = 0; col + width <= cols && !placed; col++) {
                if (_isCellFree(col, row, width, height, cols, rows)) {
                    _setPosition(tile, portrait, col, row);
                    _markCell(col, row, width, height);
                    placed = true;
                }
            }
        }
        if (!placed) {
            if (!portrait) {
                return _fail(tile.line, PrintString(F("the %s tile does not fit into the %ux%u grid%s"), getTileTypeName(tile.type),
                                                    static_cast<unsigned>(cols), static_cast<unsigned>(rows), _pageSuffix(page).c_str()).c_str());
            }
            // A grid of one row is a portrait grid of one column: a tile that is two cells wide has
            // no block in it at all. It keeps the first cell instead of disappearing
            _setPosition(tile, portrait, 0, 0);
        }
    }
    return true;
}

// Validates the tiles and places them in the grid of their page - once for the landscape layout
// the file is written for and once for the transposed grid of a portrait display.
//
// The tiles are placed in the ORDER OF THE FILE in both grids (left to right, top to down), so a
// page reads the same way whichever orientation is active. Transposing the positions instead would
// put the tiles of the first row of the file down the left edge of the portrait display (the
// landscape reading order turns into a column)
bool Config::_placeTiles()
{
    // the size of a tile: the `size` key of the file wins over the default of the type (`size:
    // 1x1`), the other types are always one cell wide and cannot declare a size
    for (uint8_t i = 0; i < _tileCount; i++) {
        auto &tile = _tiles[i];
        if (!tile.hasSize) {
            tile.width = getTileWidth(tile.type);
            tile.height = getTileHeight(tile.type);
            continue;
        }
        if (!tileTypeHasSize(tile.type)) {
            return _fail(tile.line, PrintString(F("only a dimmer, a climate and a picture tile can have a size of their own, a %s tile is %ux%u"),
                                                getTileTypeName(tile.type), static_cast<unsigned>(getTileWidth(tile.type)),
                                                static_cast<unsigned>(getTileHeight(tile.type))).c_str());
        }
        if (tile.width > 1 && tile.type != TileType::PICTURE) {
            return _fail(tile.line, "every tile is one column wide, size must start with 1x");
        }
        if (tile.type == TileType::PICTURE && (tile.width > kMaxPictureWidth || tile.height > kMaxPictureHeight)) {
            return _fail(tile.line, PrintString(F("a picture tile is at most %ux%u cells, for example 2x2, 2x1 or 1x1"),
                                                static_cast<unsigned>(kMaxPictureWidth), static_cast<unsigned>(kMaxPictureHeight)).c_str());
        }
    }
    // one pass per page, every page has its own grid. The main page is 0, an area adds one page
    for (uint8_t page = 0; page < _pageCount; page++) {
        if (!_placePage(page, false)) {
            return false;
        }
        _placePage(page, true);
    }
    // the placed tiles, the size of a dimmer or a climate tile depends on the file (`size: 1x1`)
    for (uint8_t i = 0; i < _tileCount; i++) {
        const auto &tile = _tiles[i];
        const String type = getTileTypeName(tile.type);
        PrintString extra;
        if (tile.type == TileType::PICTURE) {
            extra.printf_P(PSTR(", refresh %us"), static_cast<unsigned>(tile.refresh));
        }
        else if (tile.gridCols || tile.gridRows) {
            extra.printf_P(PSTR(", grid %ux%u"), static_cast<unsigned>(tile.gridCols), static_cast<unsigned>(tile.gridRows));
        }
        __LDBG_printf("tile %u: %s '%s' %ux%u at (%u,%u) of page %u%s, portrait (%u,%u)", static_cast<unsigned>(i), type.c_str(), tile.name,
                      static_cast<unsigned>(tile.width), static_cast<unsigned>(tile.height),
                      static_cast<unsigned>(tile.col + 1), static_cast<unsigned>(tile.row + 1), static_cast<unsigned>(tile.page),
                      extra.c_str(), static_cast<unsigned>(tile.portraitCol + 1), static_cast<unsigned>(tile.portraitRow + 1));
    }
    return true;
}

// the area tile that owns a page, nullptr while the page uses the grid of the document
const Tile *Config::_pageGrid(uint8_t page) const
{
    if (page && page < _pageCount) {
        const auto index = _pageArea[page];
        if (index < _tileCount && _tiles[index].gridCols && _tiles[index].gridRows) {
            return &_tiles[index];
        }
    }
    return nullptr;
}

uint8_t Config::getCols(uint8_t page) const
{
    const auto grid = _pageGrid(page);
    return grid ? grid->gridCols : _cols;
}

uint8_t Config::getRows(uint8_t page) const
{
    const auto grid = _pageGrid(page);
    return grid ? grid->gridRows : _rows;
}

const char *Config::getPageName(uint8_t page) const
{
    if (page && page < _pageCount) {
        const auto index = _pageArea[page];
        if (index < _tileCount && _tiles[index].type == TileType::AREA) {
            return _tiles[index].name;
        }
    }
    return nullptr;
}

// "" for the main page, " of the area 'name'" otherwise (error messages)
String Config::_pageSuffix(uint8_t page) const
{
    const auto name = getPageName(page);
    return name ? PrintString(F(" of the area '%s'"), name) : String();
}

} // namespace HomeAssistant
} // namespace WeatherStation2
