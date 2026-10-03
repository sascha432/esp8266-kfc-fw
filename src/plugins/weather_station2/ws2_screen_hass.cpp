/**
 * Author: sascha_lammers@gmx.de
 */

#include "ws2_screens.h"

#include <LoopFunctions.h>
#include <PrintString.h>
#include "devices/wt32_sc01/wt32_sc01.h"
#include "lvgl_plugin.h"

// weather configuration (the orientation of the dashboard and the lock of the quick settings)
using Plugins = KFCConfigurationClasses::PluginsType;

// Self test of the action path: toggles a test tile on a timer so the sequence of a tap, the
// requests and the responses can be followed in the log without touching the panel (see
// HassScreen::_actionTest()). Build with -D DEBUG_HASS_ACTION_TEST=1 to enable it, it is never
// part of a normal build
#ifndef DEBUG_HASS_ACTION_TEST
#    define DEBUG_HASS_ACTION_TEST 0
#endif

// The logger enables __LDBG_printf() per file (see logger.h): without this block every trace of
// this file - the gestures, the tiles and the controls of a panel - is compiled out, which makes
// the screen look like it does nothing in the log
#ifndef DEBUG_WEATHER_STATION2
#    define DEBUG_WEATHER_STATION2 1
#endif

#if DEBUG_WEATHER_STATION2
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

namespace WeatherStation2 {

using HomeAssistant::Tile;
using HomeAssistant::TileIcon;
using HomeAssistant::TileState;
using HomeAssistant::TileType;
using HomeAssistant::TileValue;

// ------------------------------------------------------------------------------------------
// layout and helpers
// ------------------------------------------------------------------------------------------
namespace {

constexpr lv_coord_t kTileGap = 8;
constexpr lv_coord_t kTileMargin = 8;
// This screen has no top bar and no footer: the tiles use the whole display, the grid starts and
// ends at the page margin (the reviewed layout is docs/hass_layout/screen1.html). The bottom edge
// is HassScreen::_gridBottom(), the display is 480x320 in landscape and 320x480 in portrait
constexpr lv_coord_t kGridTop = kTileMargin;
// notes and errors (a failed request, a missing file) are drawn over the first row of tiles, the
// line is hidden while everything is fine
constexpr lv_coord_t kStatusY = 0;
// the three options of a climate panel (mode, preset, fan mode)
constexpr uint8_t kNumPills = 3;

// smaller of two values: the controls of a panel have fixed sizes that have to fit the column the
// orientation leaves for them (a portrait display is 320 px wide)
inline lv_coord_t minOf(lv_coord_t a, lv_coord_t b)
{
    return a < b ? a : b;
}

// ------------------------------------------------------------------------------------------
// tiles: a centered column of icon, value and name (one cell), the level fill of a dimmer and
// the bars of a climate fill two rows
// ------------------------------------------------------------------------------------------
// height of the +/- bars of a climate tile (tall enough to hit with a finger)
constexpr lv_coord_t kTileBarHeight = 44;
// height of the small icon in front of the current temperature of a climate tile
constexpr lv_coord_t kTileInlineIcon = LVGLUI::kIconSizeSmall;
// height of the name block at the bottom of a tile
constexpr lv_coord_t kTileNameHeight = 32;
// A dimmer tile changes its level only while the finger moves: a movement of less than this many
// pixels (up or down) is a tap and opens the panel of the entity instead. Without it every tap
// would jump to the level under the finger
constexpr lv_coord_t kDimmerDragTolerance = 10;
// A touch that closes the fullscreen image of a picture tile must not change to another screen as
// well. The screen manager follows the touch screen on its own and runs the swipe of that touch
// right after, this is how long the screen ignores the swipe after the image was closed
constexpr uint32_t kFullscreenGestureTime = 700;
// Tolerance of the level/setpoint that the user set and the entity has not reported yet (the hold,
// see _expect()/_expects()). It has to be **smaller than the step** of the control: a response that
// was already in flight when the action was sent carries the value from before it, which is exactly
// one step away - with a tolerance of one step or more that response ended the hold and the readout
// flipped back to the old value until the entity adopted the new one (the +/- steppers looked like
// they lag by a second). The levels are 0..100 % (smallest step 1) and the setpoints are on the
// 0.5 °C grid of a climate (smallest step 0.5)
constexpr float kLevelHoldTolerance = 0.5f;
constexpr float kSetpointHoldTolerance = 0.25f;

// The cells of the grid change with the number of rows, so the content of a one cell tile is
// centered instead of using fixed offsets: icon, value and name as one block.
//
// A page can use more rows than the two of the document (`grid:` of an area tile, for example a
// 4x3 room page with 110x92 px cells) and then a cell is too short for the large icon and the
// large value font: icon and value are drawn in the compact layout instead. There is no third
// size of the icon font, so the icon of a compact tile is the small one
constexpr lv_coord_t kCompactTileHeight = 128;
// vertical space the value needs below the icon: the line height of its font plus the gap to the
// name block
constexpr lv_coord_t kTileValueSpace = 32;        // kFontValue (28), line height 30
constexpr lv_coord_t kTileValueSpaceCompact = 24; // kFontLarge (20), line height 22

inline bool tileIsCompact(lv_coord_t height)
{
    return height < kCompactTileHeight;
}

inline lv_coord_t tileIconSize(lv_coord_t height)
{
    return tileIsCompact(height) ? LVGLUI::kIconSizeSmall : LVGLUI::kIconSizeLarge;
}

inline const lv_font_t *tileValueFont(lv_coord_t height)
{
    return tileIsCompact(height) ? LVGLUI::kFontLarge : LVGLUI::kFontValue;
}

// height of the block above the name of a compact climate tile (setpoint and action of a 1x1
// tile). The two labels are centered as one block above the name, in a tall tile they are placed
// around the middle of the tile instead
constexpr lv_coord_t kCompactClimateValueSpace = 42;

// Vertical gap between the blocks of a one cell tile that is drawn as a centered column: icon,
// value and name above each other. The name is pinned to the bottom edge of the tile, so the room
// the blocks do not need is the space above the name, split into as many equal gaps as there are -
// three while the icon is drawn (above the icon, icon to value, value to name), two while it is not
// (above the value, value to name). Without this the leftover room lands between the icon and the
// value only (the value is anchored to the name block) and the tile looks like the value was pushed
// down
inline lv_coord_t tileBlockGap(lv_coord_t height, lv_coord_t iconSize, lv_coord_t valueHeight, bool withIcon = true)
{
    const auto free = static_cast<lv_coord_t>(height - kTileNameHeight - valueHeight - (withIcon ? iconSize : 0));
    const auto gap = static_cast<lv_coord_t>(free / (withIcon ? 3 : 2));
    return (gap > 0) ? gap : 0;
}

// top of the icon of a one cell tile: icon, value and the name block as one centered column. The
// value is one line tall here, _updateTile() lays the three of them out with the height it measured
inline lv_coord_t tileIconTop(lv_coord_t height)
{
    return tileBlockGap(height, tileIconSize(height), tileIsCompact(height) ? kTileValueSpaceCompact : kTileValueSpace);
}

// an area tile and the back tile of an area page carry the icon and the name only. The area tile
// keeps the large icon while a short cell uses the compact layout for the value: the icon and the
// name fit next to each other in the 110x92 px cell of a three row page
inline lv_coord_t areaIconTop(lv_coord_t height)
{
    return static_cast<lv_coord_t>((height - (LVGLUI::kIconSizeLarge + 4 + 14)) / 2);
}

// the back tile carries the arrow only, it is centered in the tile
inline lv_coord_t iconCenterTop(lv_coord_t height)
{
    return static_cast<lv_coord_t>((height - LVGLUI::kIconSizeLarge) / 2);
}

// The glyph of a state icon: an icon that has an off state is replaced while the entity is off,
// so the state is visible without reading the text (the light bulb goes dark, the plug is pulled
// out, the motion sensor that does not detect anything). Every other icon keeps its glyph
LVGLUI::IconType stateIconType(LVGLUI::IconType type, LVGLUI::TileState state)
{
    if (state == LVGLUI::TileState::OFF) {
        switch (type) {
        case LVGLUI::IconType::TOGGLE:
            return LVGLUI::IconType::TOGGLE_OFF;
        case LVGLUI::IconType::BULB:
            return LVGLUI::IconType::LIGHTBULB_OFF;
        case LVGLUI::IconType::PLUG:
            return LVGLUI::IconType::POWER_PLUG_OFF;
        case LVGLUI::IconType::MOTION:
            return LVGLUI::IconType::MOTION_SENSOR_OFF;
        default:
            break;
        }
    }
    return type;
}

// color of a state icon: grey while the entity is off, red while it is unavailable, 0 (the theme
// color of the icon) while it is active or an action is pending
uint32_t stateIconColor(LVGLUI::TileState state)
{
    switch (state) {
    case LVGLUI::TileState::OFF:
        return LVGLUI::kColorTextMuted;
    case LVGLUI::TileState::UNAVAILABLE:
        return LVGLUI::kColorError;
    default:
        return LVGLUI::kIconColorDefault;
    }
}

// true when a tap landed on a widget: the widget itself or on one of its children (the glyph
// inside a button). The children of the buttons are not clickable, this is the second safety net
// for a tap that reaches a child anyway
bool isTapOn(lv_obj_t *target, lv_obj_t *widget)
{
    return widget && (target == widget || lv_obj_get_parent(target) == widget);
}

// true when the comma separated list of colour modes of an entity contains a name
bool hasColorMode(const HomeAssistant::Detail &detail, const char *name)
{
    const auto length = strlen(name);
    const char *ptr = detail.colorModes;
    while (ptr && *ptr) {
        if (!strncasecmp(ptr, name, length) && (ptr[length] == 0 || ptr[length] == ',')) {
            return true;
        }
        ptr = strchr(ptr, ',');
        if (ptr) {
            ptr++;
        }
    }
    return false;
}

// The buttons of a light panel depend on the entity: every light has a power switch and a level,
// only some have a color, a color temperature or effects (supported_color_modes / effect_list)
bool lightHasColor(const HomeAssistant::Detail &detail)
{
    return hasColorMode(detail, "hs") || hasColorMode(detail, "rgb") || hasColorMode(detail, "rgbw") ||
           hasColorMode(detail, "rgbww") || hasColorMode(detail, "xy");
}

bool lightHasColorTemp(const HomeAssistant::Detail &detail)
{
    return hasColorMode(detail, "color_temp") || detail.colorTemp > 0;
}

bool lightHasEffects(const HomeAssistant::Detail &detail)
{
    return detail.effectList[0] != 0;
}

// slider of a light panel, the level and the colour temperature share their look. `radius` is the
// corner radius of the track and `fillRadius` the one of the fill (the level that is filled in).
// LVGL masks the fill with the rounding of the *track* (`lv_bar_draw()` builds a radius mask from
// the main part), so a track with large corners forces the same corners on the fill - the two are
// one value for a panel slider (see the constants below)
lv_obj_t *createPanelSlider(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h,
                            int32_t min, int32_t max, lv_event_cb_t callback, void *userData,
                            lv_coord_t radius = LVGLUI::kCardRadius, lv_coord_t fillRadius = -1)
{
    auto slider = lv_slider_create(parent);
    lv_obj_set_pos(slider, x, y);
    lv_obj_set_size(slider, w, h);
    lv_slider_set_range(slider, min, max);
    lv_slider_set_value(slider, min, LV_ANIM_OFF);
    // The value of a slider is animated with the anim time of the style (lv_bar_set_value with
    // LV_ANIM_ON, lv_slider.c writes the pressed position into the track). With the default theme
    // timings the fill of a drag runs after the finger and the fill of a tap runs toward the
    // pressed position for a moment before the callback puts the value back
    lv_obj_set_style_anim_time(slider, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(slider, radius, LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, lv_color_hex(LVGLUI::kColorCardAlt), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(slider, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(slider, lv_color_hex(LVGLUI::kColorBorder), LV_PART_MAIN);
    lv_obj_set_style_radius(slider, (fillRadius < 0) ? radius : fillRadius, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, lv_color_hex(LVGLUI::kColorActive), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_INDICATOR);
    // no knob, the value label in the middle of the track is the readout
    lv_obj_set_style_bg_opa(slider, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider, 0, LV_PART_KNOB);
    lv_obj_add_event_cb(slider, callback, LV_EVENT_ALL, userData);
    return slider;
}

// measured height of the (wrapped) text of a label: LVGL breaks the text into lines at the width
// it is measured with, which is the height the object has to be
lv_coord_t textBlockHeight(const char *text, const lv_font_t *font, lv_coord_t width)
{
    lv_point_t size;
    lv_txt_get_size(&size, text ? text : "", font, 0, 0, width, LV_TEXT_FLAG_NONE);
    if (size.y <= 0) {
        // an empty text keeps the height of one line
        return static_cast<lv_coord_t>(lv_font_get_line_height(font));
    }
    return static_cast<lv_coord_t>(size.y);
}

// A label at the bottom edge of a tile: the height of the (wrapped) text is measured and the
// object is moved up by it, so a two line name grows upward and never runs into the widget below
// it. LVGL draws the text of a label from the top of its object, which is why the object moves.
// The font is the one the label is drawn with (fitTextDown() may have shrunk it) and the height
// is only written when it changed - this runs for every value that changed
void setBottomAlignedText(lv_obj_t *label, const char *text, lv_coord_t width, lv_coord_t bottomY)
{
    if (!label) {
        return;
    }
    const auto height = textBlockHeight(text, lv_obj_get_style_text_font(label, LV_PART_MAIN), width);
    const auto top = static_cast<lv_coord_t>(bottomY - height);
    if (lv_obj_get_style_height(label, LV_PART_MAIN) == height && lv_obj_get_style_y(label, LV_PART_MAIN) == top) {
        return;
    }
    lv_obj_set_height(label, height);
    lv_obj_set_y(label, top);
}

// Long mode of the value of a one cell tile. A value with a line break in it (a combined sensor,
// "25.0 °C\n57.7 %") is drawn on two lines: LVGL breaks the text into lines when it is set and
// replaces what does not fit with dots - measured against the box of the label at that moment,
// and the box of the first value of a tile is one empty line tall (the tile was built with an
// empty label). The first value therefore lost its second line to "..." until the text changed
// again. LV_LABEL_LONG_WRAP reverts the dots and draws every line - the label is anchored to the
// top of the name block and grows upward, into the space of the icon. A value without a line
// break keeps LV_LABEL_LONG_DOT and is shortened with dots when it does not fit (the font was
// already shrunk by fitTextDown(), an ellipsis keeps a long text inside the cell)
//
// The return value is true while the value is drawn on more than one line: that height decides
// whether the icon of the tile is drawn, see setTileIconVisible()
bool setValueLongMode(lv_obj_t *label, const char *text)
{
    if (!label) {
        return false;
    }
    const bool multiLine = (text && strchr(text, '\n')) ? true : false;
    lv_label_set_long_mode(label, multiLine ? LV_LABEL_LONG_WRAP : LV_LABEL_LONG_DOT);
    return multiLine;
}

// The icon of a one cell tile is part of the centered block of icon, value and name. The caller
// measures the value (a combined sensor is drawn on two lines) and hides the icon when the three
// of them do not fit into the cell - in the 110x92 px cell of a three row page there is no room
// left for the icon next to two lines, which leaves the value and the name the whole cell (the
// same layout as `icon: none`)
void setTileIconVisible(lv_obj_t *icon, bool visible)
{
    if (!icon) {
        return;
    }
    if (visible) {
        lv_obj_clear_flag(icon, LV_OBJ_FLAG_HIDDEN);
    }
    else {
        lv_obj_add_flag(icon, LV_OBJ_FLAG_HIDDEN);
    }
}

// ------------------------------------------------------------------------------------------
// panels: the left column is the width of one cell (back tile and options), the right column
// takes the rest (header, control)
// ------------------------------------------------------------------------------------------
// height of the header of a panel (name, and the current temperature of a climate)
constexpr lv_coord_t kPanelHeaderHeight = 46;
// size of the stepper buttons of a panel and the gap between them (the level of a dimmer and the
// setpoint of a climate are stepped with two of them stacked)
constexpr lv_coord_t kPanelStepSize = 64;
constexpr lv_coord_t kPanelStepGap = 8;
// width of the column the steppers of a panel use
constexpr lv_coord_t kPanelStepColumn = kPanelStepSize + 8;
// gap between the level slider and the steppers of the level (they are one block, see _buildPanel())
constexpr lv_coord_t kPanelSliderStepGap = 20;
// A stepper of a panel is a round button. LVGL draws a rectangle with a corner radius, so the
// button is square and the radius is half of it - that is the circle the reviewed layouts show.
// Never set a rectangle radius on it (kCardRadius made them 64x64 squares with 12 px corners)
lv_obj_t *createStepButton(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, LVGLUI::IconType type)
{
    auto button = LVGLUI::createIconButton(parent, x, y, kPanelStepSize, kPanelStepSize, type, LVGLUI::kIconSizeLarge);
    lv_obj_set_style_radius(button, static_cast<lv_coord_t>(kPanelStepSize / 2), LV_PART_MAIN);
    return button;
}
// height of an item of a panel list and the gap between two items (the height of one row of the
// grid of items, see _buildPanelList())
constexpr lv_coord_t kListItemHeight = 42;
constexpr lv_coord_t kListItemGap = 4;
// height of a button of the light panel and the gap between two of them
constexpr lv_coord_t kPanelButtonHeight = 40;
constexpr lv_coord_t kPanelButtonGap = 6;
// width of the level slider of the dimmer panel
constexpr lv_coord_t kPanelSliderWidth = 113;
// The sliders of a light/dimmer panel (the level and the colour temperature) look the same in both
// orientations: the track keeps rounded corners, only the edge of the fill that follows the level
// is nearly square. LVGL masks the fill with the rounding of the *track* (`lv_bar_draw()` builds a
// radius mask from the main part), so the corners of the fill at the end it starts from are the
// track's - the moving edge is the fill's own, which is the one that is seen while sliding.
// A vertical slider is what a panel uses (the band it fills is narrow and tall), so
// `kPanelSliderNarrow` is only about the width of the portrait row
constexpr uint8_t kPanelSliderNarrow = 12;                                      // percent
constexpr lv_coord_t kPanelSliderRadius = 34;                                   // track
constexpr lv_coord_t kPanelSliderFillRadius = 5;                                // fill
// size of the color wheel of the dimmer panel
constexpr lv_coord_t kPanelWheelSize = 196;
// thickness of the ring (the band) of the climate arc: the ring of a tile (kArcWidth, 18 px) is
// 22 px in the panel and 25 % more than that (27 px) - the band is what the arc of the panel is
// about, its diameter is limited by the width of the display. It is only the panel that passes
// this to createArc(), the arcs of the tiles keep kArcWidth
constexpr lv_coord_t kPanelArcRing = static_cast<lv_coord_t>(LVGLUI::kArcWidth * 5 / 4 * 5 / 4);
// gap between the blocks of the portrait panel (header, readout, control, options), see
// HassScreen::_panelLayout()
constexpr lv_coord_t kPanelPortraitGap = 12;
// The arc of a panel covers 270 degrees with the gap centred at the bottom (LVGLUI::createArc()),
// so its ring does not reach the bottom edge of its box: the lowest points of the ring are the two
// ends, r * cos(45) = 0.7071 r below the centre, and the ring reaches r above it. The height the
// ring covers is therefore 1.7071 r = 0.85355 d. The ring is what the eye measures a gap against,
// which is why a portrait panel centers the ring (not the box) in the band it has for the arc
constexpr float kArcRingHeight = 0.85355f;

// ------------------------------------------------------------------------------------------
// sensor panel: the live value of the entity, the range buttons and the history graph of its 5
// minute aggregated long term statistics (see the class Stats of the Home Assistant client)
// ------------------------------------------------------------------------------------------
// The panel of a sensor has no control of its own, so it is not laid out in the two columns of
// the other panels: the tile that closes it is a compact one in the upper left corner, the header
// and the graph use the whole width of the display. The reviewed layout is the entity card of the
// Home Assistant app (docs/temp/"When I tap on a sensor.htm")
constexpr lv_coord_t kSensorBackSize = 48;
// header: the glyph of the entity, its name and the live value
constexpr lv_coord_t kSensorHeaderGap = 6;
constexpr lv_coord_t kSensorHeaderY = kTileMargin;
// "History" and the range buttons below the header
constexpr lv_coord_t kSensorTitleY = static_cast<lv_coord_t>(kSensorHeaderY + kSensorBackSize + kTileGap);
constexpr lv_coord_t kSensorTitleHeight = 30;
constexpr lv_coord_t kSensorChipWidth = 52;
constexpr lv_coord_t kSensorChipHeight = 26;
constexpr lv_coord_t kSensorChipGap = 6;
// the card of the graph, below the title. kSensorCardGap keeps the "History" label and the range
// chips (the title row is kSensorTitleHeight tall) off the card - the 27 px kFontTitle font of the
// label used to end one pixel above the outline of the card
constexpr lv_coord_t kSensorCardX = kTileMargin;
constexpr lv_coord_t kSensorCardGap = 10;
constexpr lv_coord_t kSensorCardY = static_cast<lv_coord_t>(kSensorTitleY + kSensorTitleHeight + kSensorCardGap);
// size of the card and of the graph inside it: HassScreen::_sensorCardWidth() and the other
// metrics, the display is 480x320 in landscape and 320x480 in portrait
// inside the card: the column of the level labels (at the left of the graph), the strip of the
// time labels (below it) and the graph that uses the rest
constexpr lv_coord_t kSensorLevelWidth = 40;
constexpr lv_coord_t kSensorLevelHeight = 14;
// the strip below the graph: the time labels (kFontSmall = montserrat_12, 15 px line height, see
// lv_font_montserrat_12.c) start kSensorTimeTop below the graph and keep kSensorTimeGap to the
// bottom edge of the card (with 16 px they sat on the outline)
constexpr lv_coord_t kSensorTimeTop = 2;
constexpr lv_coord_t kSensorTimeLineHeight = 15;
constexpr lv_coord_t kSensorTimeGap = 6;
constexpr lv_coord_t kSensorTimeHeight = static_cast<lv_coord_t>(kSensorTimeTop + kSensorTimeLineHeight + kSensorTimeGap);
constexpr lv_coord_t kSensorGraphX = static_cast<lv_coord_t>(kSensorLevelWidth + 8);
constexpr lv_coord_t kSensorGraphY = static_cast<lv_coord_t>(kSensorLevelHeight + 4);
// The chart is clipped to its object: a value on the top or bottom edge loses half of the 2 px
// line and the divider on that edge is cut off (the lowest divider sat on the bottom edge when the
// auto scale snapped the minimum of the window to 0). The range is mapped inside this inset and the
// three value lines and their labels use the same coordinates
constexpr lv_coord_t kSensorGraphInset = 2;
// Interval of the grid lines of the X axis, in hours: one line every 4 hours at whole local hours
// (00:00, 04:00, ...). The lines follow the clock and the label of a line is drawn under it, see
// _drawSensorChart()
constexpr uint8_t kSensorGridHours = 4;
// The chart of LVGL 8.4 stores integers (lv_coord_t, 16 bit) and LV_CHART_POINT_NONE is
// INT16_MAX, so the span of the window is mapped to 0..kSensorChartScale and the labels show the
// range that was mapped. A fixed factor per unit (like the power screen uses) is not possible
// here: the graph is drawn for any sensor, from a temperature to a pressure in pascal
constexpr lv_coord_t kSensorChartScale = 20000;
// length of one bucket of the statistics (5 minutes, the period of the request)
constexpr uint32_t kSensorBucketSeconds = 300;

// A step of one significant digit (1..9 x 10^n) for the Y axis of the sensor graph: the three
// labels of the axis become round numbers (0 / 70 / 140) instead of the raw window plus a margin
// (-10.4 / 55.7 / 122). `raw` is Inf/NaN safe
float statsNiceStep(float raw)
{
    if (!(raw > 0.0f) || raw > 1e30f) {
        return 1.0f;
    }
    const auto exponent = floorf(log10f(raw));
    const auto base = powf(10.0f, exponent);
    auto digits = roundf(raw / base);
    if (digits < 1.0f) {
        digits = 1.0f;
    }
    return digits * base;
}

// Interval of the labels below the graph, in hours. The grid lines are one per kSensorGridHours and
// every line carries its label, only the 48 hour range labels every second one: a "14:00" of the
// 12 px font needs 32 px while the 12 lines of that range are 34 px apart in landscape and 21 px in
// portrait, they would overlap
uint8_t statsLabelStepHours(uint8_t hours)
{
    return (hours > 24) ? 8 : kSensorGridHours;
}

// Tenths of a degree one step of the climate arc is. The range and the value of an LVGL arc are
// integers, so an arc that is set up in tenths of a degree moves in 0.1 °C while it is dragged -
// the setpoint of a climate tile (default 0.5, see `step:` in hass_config.md) is that unit instead
// and the arc moves in whole steps
int16_t arcStepTenths(const Tile &tile)
{
    const auto step = static_cast<int16_t>(lroundf((tile.step > 0) ? tile.step * 10.0f : 5.0f));
    return (step < 1) ? 1 : step;
}

// value of the arc for a temperature/tenths pair (rounded to the nearest step)
int16_t arcValueOf(int32_t tenths, int16_t step)
{
    return static_cast<int16_t>((tenths + (step / 2)) / step);
}

// icon of a tile, the icon of the configuration wins over the one of the type
LVGLUI::IconType toIconType(const Tile &tile)
{
    switch (tile.icon) {
    case TileIcon::BULB:
        return LVGLUI::IconType::BULB;
    case TileIcon::PLUG:
        return LVGLUI::IconType::PLUG;
    case TileIcon::TOGGLE:
        return LVGLUI::IconType::TOGGLE;
    case TileIcon::THERMOMETER:
        return LVGLUI::IconType::THERMOMETER;
    case TileIcon::HUMIDITY:
        return LVGLUI::IconType::HUMIDITY;
    case TileIcon::BUTTON:
        return LVGLUI::IconType::BUTTON;
    case TileIcon::DIMMER:
        return LVGLUI::IconType::DIMMER;
    case TileIcon::RADIATOR:
        return LVGLUI::IconType::RADIATOR;
    case TileIcon::FAN:
        return LVGLUI::IconType::FAN;
    case TileIcon::MOTION:
        return LVGLUI::IconType::MOTION;
    case TileIcon::AREA:
        return LVGLUI::IconType::AREA;
    case TileIcon::FLASH:
        // mdi-flash, the same glyph the power screen uses
        return LVGLUI::IconType::POWER;
    case TileIcon::CO2:
        return LVGLUI::IconType::CO2;
    case TileIcon::LOCK:
        return LVGLUI::IconType::LOCK;
    case TileIcon::GAUGE:
        return LVGLUI::IconType::GAUGE;
    case TileIcon::CAMERA:
        return LVGLUI::IconType::CAMERA;
    case TileIcon::REMOTE:
        return LVGLUI::IconType::REMOTE;
    case TileIcon::LIGHTBULB_OFF:
        return LVGLUI::IconType::LIGHTBULB_OFF;
    case TileIcon::LIGHTBULB_ON:
        return LVGLUI::IconType::LIGHTBULB_ON;
    case TileIcon::FLASH_OFF:
        return LVGLUI::IconType::FLASH_OFF;
    case TileIcon::HOME:
        return LVGLUI::IconType::HOME;
    case TileIcon::HOME_ASSISTANT:
        return LVGLUI::IconType::HOME_ASSISTANT;
    default:
        break;
    }
    switch (tile.type) {
    case TileType::SWITCH:
        // the plug of the entity that is switched on/off, not the handle of a toggle
        return LVGLUI::IconType::PLUG;
    case TileType::LIGHT:
        return LVGLUI::IconType::BULB;
    case TileType::SENSOR:
        // a sensor follows the device_class of its entity (see toIconType(tile, value)), the
        // thermometer is used while the entity has none
        return LVGLUI::IconType::THERMOMETER;
    case TileType::BUTTON:
        return LVGLUI::IconType::BUTTON;
    case TileType::DIMMER:
        // the same bulb as a light, the dimmer tile is the level fill itself
        return LVGLUI::IconType::BULB;
    case TileType::AREA:
        return LVGLUI::IconType::AREA;
    case TileType::CLIMATE:
        // the inline icon of a climate tile sits in front of the current temperature, the config
        // replaces it (`icon: radiator` for the tile of a room, for example). Without this case the
        // icon fell through to UNKNOWN and the tile showed an exclamation mark
        return LVGLUI::IconType::THERMOMETER;
    default:
        break;
    }
    return LVGLUI::IconType::UNKNOWN;
}

// Everything the dashboard knows about the device_class of an entity, in one table with one
// lookup: the glyph its tile draws and the wording of the two states of a binary sensor. The glyph,
// the wording and "is this entity switched on and off" are the same fact.
//   `icon`  UNKNOWN for a class without a glyph of its own (the tile keeps the icon of its type)
//   `on`    nullptr for a class that is a reading, not a switch
struct DeviceClass {
    const char *name;
    LVGLUI::IconType icon;
    const char *on;
    const char *off;
};

const DeviceClass kDeviceClasses[] = {
    { "temperature", LVGLUI::IconType::THERMOMETER, nullptr, nullptr },
    { "humidity", LVGLUI::IconType::HUMIDITY, nullptr, nullptr },
    { "pressure", LVGLUI::IconType::GAUGE, nullptr, nullptr },
    { "carbon_dioxide", LVGLUI::IconType::CO2, nullptr, nullptr },
    { "illuminance", LVGLUI::IconType::BRIGHTNESS, nullptr, nullptr },
    { "voltage", LVGLUI::IconType::SINE_WAVE, nullptr, nullptr },
    { "current", LVGLUI::IconType::CURRENT_AC, nullptr, nullptr },
    { "power", LVGLUI::IconType::POWER, nullptr, nullptr },
    { "energy", LVGLUI::IconType::LIGHTNING_BOLT, nullptr, nullptr },
    { "motion", LVGLUI::IconType::MOTION, "Detected", "Clear" },
    { "occupancy", LVGLUI::IconType::MOTION, "Detected", "Clear" },
    // binary sensors without a glyph of their own
    { "presence", LVGLUI::IconType::UNKNOWN, "Detected", "Clear" },
    { "smoke", LVGLUI::IconType::UNKNOWN, "Detected", "Clear" },
    { "moisture", LVGLUI::IconType::UNKNOWN, "Wet", "Dry" },
    { "window", LVGLUI::IconType::UNKNOWN, "Open", "Closed" },
    { "door", LVGLUI::IconType::UNKNOWN, "Open", "Closed" },
    { "garage_door", LVGLUI::IconType::UNKNOWN, "Open", "Closed" },
    { "opening", LVGLUI::IconType::UNKNOWN, "Open", "Closed" },
};

// entry of a device_class, nullptr when the entity does not report one or the class is not above
const DeviceClass *findDeviceClass(const char *deviceClass)
{
    if (!deviceClass || !deviceClass[0]) {
        return nullptr;
    }
    for (const auto &entry : kDeviceClasses) {
        if (!strcasecmp(deviceClass, entry.name)) {
            return &entry;
        }
    }
    return nullptr;
}

// true when a state text is the state of a switched entity ("on"/"off")
bool isOnOffText(const char *text)
{
    return !strcasecmp(text, "on") || !strcasecmp(text, "off");
}

// glyph of a tile with the values of its entity: the `icon:` of the configuration wins, a sensor
// tile follows the device_class of its entity and falls back to the icon of its type (an entity
// without the attribute, or a class without a glyph of its own)
LVGLUI::IconType toIconType(const Tile &tile, const TileValue &value)
{
    if (tile.icon == TileIcon::AUTO && tile.type == TileType::SENSOR) {
        const auto *entry = findDeviceClass(value.deviceClass);
        if (entry && entry->icon != LVGLUI::IconType::UNKNOWN) {
            return entry->icon;
        }
    }
    return toIconType(tile);
}

// true when the state of an entity is a number (a value the tile can format)
bool _isNumber(const char *text)
{
    if (!text || !*text) {
        return false;
    }
    char *stop = nullptr;
    strtof(text, &stop);
    return stop != text && stop && *stop == 0;
}

// True when the entity of a tile is switched on and off, so its tile draws the off glyph of its
// icon. A switch, a light and a dimmer are switched, a binary sensor reports on/off (or the wording
// of its device_class). A tile that shows a reading is not: it is "off" in the model because it is
// not switched on, so `icon: plug` of a power meter stays a plug
bool hasTwoStates(const Tile &tile, const TileValue &value)
{
    if (tile.type == TileType::SENSOR) {
        const auto *entry = findDeviceClass(value.deviceClass);
        return isOnOffText(value.text) || (entry && entry->on);
    }
    return tile.type == TileType::SWITCH || tile.type == TileType::LIGHT || tile.type == TileType::DIMMER;
}

// glyph of a tile with the state of its entity, see hasTwoStates()
LVGLUI::IconType tileIconType(const Tile &tile, const TileValue &value, LVGLUI::TileState state)
{
    const auto type = toIconType(tile, value);
    if (state != LVGLUI::TileState::OFF || hasTwoStates(tile, value)) {
        return stateIconType(type, state);
    }
    return type;
}

// value of an entity that does not report a number
String sensorStateText(const HomeAssistant::TileValue &value)
{
    if (isOnOffText(value.text)) {
        const auto on = !strcasecmp(value.text, "on");
        const auto *entry = findDeviceClass(value.deviceClass);
        if (entry && entry->on) {
            return String(on ? entry->on : entry->off);
        }
        return String(on ? "On" : "Off");
    }
    // some integrations report the wording itself ("clear", "detected", "sunny")
    char text[sizeof(value.text)];
    strncpy(text, value.text, sizeof(text) - 1);
    text[sizeof(text) - 1] = 0;
    text[0] = static_cast<char>(::toupper(text[0]));
    return String(text);
}

// value of a sensor with its unit
String formatSensorValue(const Tile &tile, const HomeAssistant::TileValue &value)
{
    if (value.state == TileState::UNAVAILABLE) {
        return String("--");
    }
    if (value.state == TileState::UNKNOWN) {
        return String("...");
    }
    if (!_isNumber(value.text)) {
        // the entity does not report a number (a binary sensor: on/off, motion: detected/clear)
        return sensorStateText(value);
    }
    PrintString text;
    text.printf_P(PSTR("%.*f"), tile.decimals, static_cast<double>(value.value));
    const char *unit = tile.unit[0] ? tile.unit : value.unit;
    if (unit && unit[0]) {
        // a plain append, no printf: the concat is a capacity check and one memcpy
        text += ' ';
        text += unit;
    }
    return text;
}

const char *stateText(TileState state)
{
    switch (state) {
    case TileState::ON:
        return "On";
    case TileState::OFF:
        return "Off";
    case TileState::UNAVAILABLE:
        return "--";
    default:
        break;
    }
    return "...";
}

// "fan_only" -> "Fan only" (the names the API reports are lowercase)
String capitalize(const char *text)
{
    String result(text);
    result.replace('_', ' ');
    if (result.length()) {
        result.setCharAt(0, static_cast<char>(::toupper(result.charAt(0))));
    }
    return result;
}

// True while a finger presses the screen. The touch driver of the panel repeats press events and a
// release can be lost (LVGL ends a drag on LV_EVENT_PRESS_LOST as well, see lv_arc.c), so the flag a
// control sets on PRESSED/RELEASED is not enough to tell whether the user still holds it: as long as
// the input device has an active object, that control owns its value and the polled value of the
// entity must not overwrite it
bool isTouchPressed()
{
    for (auto indev = lv_indev_get_next(nullptr); indev; indev = lv_indev_get_next(indev)) {
        if (indev->proc.types.pointer.act_obj) {
            return true;
        }
    }
    return false;
}

// shows or hides an optional widget
void showWidget(lv_obj_t *obj, bool visible)
{
    if (!obj) {
        return;
    }
    if (visible) {
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
    else {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
}

// state of a climate tile: the action of the entity ("heating") wins over the name of the mode
String climateStateText(const TileValue &value)
{
    if (value.state == TileState::UNAVAILABLE) {
        return String("--");
    }
    if (value.state == TileState::UNKNOWN) {
        return String("...");
    }
    if (value.action[0]) {
        char text[sizeof(value.action)];
        strncpy(text, value.action, sizeof(text) - 1);
        text[sizeof(text) - 1] = 0;
        text[0] = static_cast<char>(::toupper(text[0]));
        return String(text);
    }
    switch (value.mode) {
    case 0:
        return String("Off");
    case 1:
        return String("Heat");
    case 2:
        return String("Cool");
    default:
        break;
    }
    return stateText(value.state);
}

// One item of a comma separated panel list: `begin`/`end` delimit it (the spaces around it are
// skipped) and `ptr` is the cursor to continue with. Empty entries are skipped - they never become
// one of the buttons of a list
static bool nextListItem(const char *&ptr, const char *&begin, const char *&end)
{
    for (;;) {
        if (!*ptr) {
            return false;
        }
        begin = ptr;
        while (*ptr && *ptr != ',') {
            ptr++;
        }
        end = ptr;
        if (*ptr == ',') {
            ptr++;
        }
        while (begin < end && (*begin == ' ' || *begin == '\t')) {
            begin++;
        }
        while (end > begin && (end[-1] == ' ' || end[-1] == '\t')) {
            end--;
        }
        if (end > begin) {
            return true;
        }
    }
}

// Number of items of a comma separated panel list (the options of a climate, the effects of a
// light): the empty entries are skipped, the counter is what the grid of items of the list is
// built from (see _buildPanelList())
uint8_t countListItems(const char *text)
{
    uint8_t count = 0;
    const char *ptr = text;
    const char *begin = nullptr;
    const char *end = nullptr;
    while (nextListItem(ptr, begin, end)) {
        count++;
    }
    return count;
}

// Item of a comma separated panel list by its ordinal (0 based): the empty entries are skipped,
// exactly like countListItems() counts and _buildPanelList() creates them
String listItemText(const char *text, uint8_t ordinal)
{
    uint8_t index = 0;
    const char *ptr = text;
    const char *begin = nullptr;
    const char *end = nullptr;
    while (nextListItem(ptr, begin, end)) {
        if (index == ordinal) {
            return String(begin, static_cast<unsigned int>(end - begin));
        }
        index++;
    }
    return String();
}

// Ordinal of an item of a panel list, -1 while the object is not one of its children.
// The name of an item cannot be read back from its label: a name that is longer than the item is
// shortened with dots and LVGL replaces the characters of the label's text with them
// (lv_label.c, LV_LABEL_LONG_DOT saves them and only restores them when the text changes). A tap
// read "...Spectrum Rai" instead of "Spectrum Rainbow Bars" and the entity ignored the effect it
// does not know - short names like "Solid" or "Fire" fit into the item and worked, which is why
// only the long ones looked broken. The item is looked up by its ordinal in the text of the list
int16_t listItemOrdinal(lv_obj_t *list, const lv_obj_t *item)
{
    for (uint32_t i = 0;; i++) {
        auto child = lv_obj_get_child(list, i);
        if (!child) {
            return -1;
        }
        if (child == item) {
            return static_cast<int16_t>(i);
        }
    }
}

// ------------------------------------------------------------------------------------------
// quick settings: geometry, presets and helpers
//
// The sheet is one panel that fills the display with a kTileMargin margin, split into three rows:
// the header (clock, date, WiFi signal and the button that closes it), the content and the row at
// the bottom (the button that leaves the screen, or "Done" in an editor). The content row has a
// darker fill than the two rows around it, the rows are separated by 1 px dividers and clipped to
// the rounded corners of the panel. Landscape keeps the header in one row and puts the six tiles
// in three columns, a portrait display stacks the header and uses two columns - everything is
// derived from the panel size, so the four orientations (two sizes) are covered by the same code.
// The reviewed layout is docs/hass_layout/quick_settings2.html
// ------------------------------------------------------------------------------------------
struct SettingsGeometry {
    lv_coord_t panelX{0};
    lv_coord_t panelY{0};
    lv_coord_t panelW{0};
    lv_coord_t panelH{0};
    // A row is a child of the panel and drawn inside its 1 px border, so a row is rowW wide and the
    // rows start at y 0 (the header), centerY (the content) and bottomY (the action)
    lv_coord_t rowW{0};
    lv_coord_t topH{0};
    lv_coord_t centerY{0};
    lv_coord_t centerH{0};
    lv_coord_t bottomY{0};
    lv_coord_t bottomH{0};
    // padding of the center row (the other two rows use it as the inset of their content too)
    lv_coord_t pad{0};
    // square cell at the right end of the header with the button that closes the sheet
    lv_coord_t closeX{0};
    lv_coord_t closeSize{0};
    // header: the clock, the date and the WiFi signal
    lv_coord_t timeX{0};
    lv_coord_t timeY{0};
    lv_coord_t dateX{0};
    lv_coord_t dateY{0};
    lv_coord_t signalX{0};
    lv_coord_t signalY{0};
    // center row: the row of the screen brightness above the tiles (relative to the center row)
    lv_coord_t brightLabelX{0};
    lv_coord_t brightLabelY{0};
    lv_coord_t brightLabelW{0};
    lv_coord_t brightSliderX{0};
    lv_coord_t brightSliderY{0};
    lv_coord_t brightSliderW{0};
    lv_coord_t brightValueX{0};
    lv_coord_t brightValueY{0};
    lv_coord_t brightValueW{0};
    // center row: the grid of the tiles (relative to the center row)
    lv_coord_t tilesX{0};
    lv_coord_t tilesY{0};
    lv_coord_t tileW{0};
    lv_coord_t tileH{0};
    lv_coord_t tileCols{3};
    // center row: the editor of one setting (relative to the center row)
    lv_coord_t editTitleX{0};
    lv_coord_t editTitleY{0};
    lv_coord_t editValueY{0};
    lv_coord_t editControlX{0};
    lv_coord_t editControlY{0};
    lv_coord_t editControlW{0};
    lv_coord_t editControlH{0};
};

// gap between two tiles of the sheet
constexpr lv_coord_t kSettingsTileGap = 8;
// distance between the edge of the center row and its content, and between two blocks of it
constexpr lv_coord_t kSettingsPad = 10;
constexpr lv_coord_t kSettingsBlockGap = 14;
// height of the track of a slider of the sheet
constexpr lv_coord_t kSettingsSliderHeight = 24;
// height of the row at the top and at the bottom in landscape, the portrait sheet stacks the header
// in two lines and gets taller rows
constexpr lv_coord_t kSettingsTopHeight = 52;
constexpr lv_coord_t kSettingsBottomHeight = 44;
// height of the round steppers of an editor (the same size the panels use)
constexpr lv_coord_t kSettingsStepSize = 64;
// Height of the block inside a tile: the icon (20), the value (kFontMedium, line 18) and the label
// (kFontSmall, line 15) above each other with a gap of 6 and 2 px. A tile without a value (the
// display sleep one) is just the icon and the label. The block is centered in the tile
constexpr lv_coord_t kSettingsTileBlockH = 61;
constexpr lv_coord_t kSettingsTileBlockHShort = 41;
constexpr lv_coord_t kSettingsValueOffset = 26;
constexpr lv_coord_t kSettingsLabelOffset = 46;
// offset of the big value of an editor below its title
constexpr lv_coord_t kSettingsValueTop = 34;
// width reserved for the clock in the header (the date follows it, the text is not measured)
constexpr lv_coord_t kSettingsTimeWidth = 96;
// gap between the WiFi glyph and the divider of the cell that closes the sheet. The date label
// stops at the same edge
constexpr lv_coord_t kSettingsSignalGap = 14;

// The baseline of a label that starts at the top of its box: LVGL draws the first line's baseline
// `line_height - base_line` below it (lv_font_t). The clock and the date of the header are put on
// the same baseline with it
static inline lv_coord_t labelBaseline(const lv_font_t *font)
{
    return static_cast<lv_coord_t>(font->line_height - font->base_line);
}

SettingsGeometry settingsGeometry(lv_coord_t width, lv_coord_t height, bool portrait, uint8_t tiles)
{
    SettingsGeometry g;
    g.panelX = kTileMargin;
    g.panelY = kTileMargin;
    g.panelW = static_cast<lv_coord_t>(width - 2 * kTileMargin);
    g.panelH = static_cast<lv_coord_t>(height - 2 * kTileMargin);
    g.rowW = static_cast<lv_coord_t>(g.panelW - 2);
    g.pad = kSettingsPad;
    // A portrait display is 320 px wide and the date does not fit next to the clock there, so the
    // header puts it on a second line and the row is taller
    g.topH = portrait ? 88 : kSettingsTopHeight;
    g.bottomH = portrait ? 64 : kSettingsBottomHeight;
    g.centerY = g.topH;
    g.centerH = static_cast<lv_coord_t>(g.panelH - 2 - g.topH - g.bottomH);
    g.bottomY = static_cast<lv_coord_t>(g.topH + g.centerH);

    // the button that closes the sheet is a square cell at the right end of the header, separated
    // by a 1 px divider (the whole cell is the hit area)
    g.closeSize = g.topH;
    g.closeX = static_cast<lv_coord_t>(g.rowW - g.closeSize);
    g.timeX = g.pad;
    g.timeY = portrait ? 8 : 4;
    g.dateX = portrait ? g.pad : static_cast<lv_coord_t>(g.pad + kSettingsTimeWidth);
    // The clock and the date are drawn beside each other in landscape: the top of the date is
    // derived from the two fonts so both baselines (the bottom of the digits) are the same line.
    // The portrait sheet stacks them, the date keeps its own row there
    g.dateY = portrait ? 46 : static_cast<lv_coord_t>(g.timeY + labelBaseline(LVGLUI::kFontHuge) - labelBaseline(LVGLUI::kFontSmall));
    g.signalX = static_cast<lv_coord_t>(g.closeX - kSettingsSignalGap - LVGLUI::kIconSizeSmall);
    g.signalY = static_cast<lv_coord_t>((g.topH - LVGLUI::kIconSizeSmall) / 2);

    // the screen brightness above the tiles. The portrait sheet writes the label of the slider on a
    // row of its own above the track, the landscape one beside it
    const auto usableW = static_cast<lv_coord_t>(g.rowW - 2 * g.pad);
    g.brightLabelX = g.pad;
    g.brightLabelY = portrait ? g.pad : static_cast<lv_coord_t>(g.pad + 4);
    g.brightLabelW = static_cast<lv_coord_t>(portrait ? (usableW / 2) : 72);
    g.brightSliderX = static_cast<lv_coord_t>(portrait ? g.pad : (g.pad + 80));
    g.brightSliderY = portrait ? static_cast<lv_coord_t>(g.pad + kSettingsSliderHeight) : g.pad;
    g.brightSliderW = static_cast<lv_coord_t>(portrait ? usableW : (usableW - 80 - 72));
    g.brightValueX = static_cast<lv_coord_t>(portrait ? (g.pad + usableW / 2) : (g.rowW - g.pad - 68));
    g.brightValueY = portrait ? static_cast<lv_coord_t>(g.pad - 2) : static_cast<lv_coord_t>(g.pad + 4);
    g.brightValueW = static_cast<lv_coord_t>(portrait ? (usableW / 2) : 68);

    // the tiles fill the band between the brightness row and the bottom of the content
    g.tilesX = g.pad;
    g.tilesY = static_cast<lv_coord_t>(g.brightSliderY + kSettingsSliderHeight + kSettingsBlockGap);
    g.tileCols = portrait ? 2 : 3;
    const auto rows = static_cast<lv_coord_t>((tiles + g.tileCols - 1) / g.tileCols);
    const auto tilesH = static_cast<lv_coord_t>(g.centerH - g.tilesY - g.pad);
    g.tileH = static_cast<lv_coord_t>((tilesH - (rows - 1) * kSettingsTileGap) / rows);
    g.tileW = static_cast<lv_coord_t>((usableW - (g.tileCols - 1) * kSettingsTileGap) / g.tileCols);

    // the editor: the title, the big value and the control, centered in the band below the value
    g.editTitleX = g.pad;
    g.editTitleY = g.pad;
    g.editValueY = static_cast<lv_coord_t>(g.pad + kSettingsValueTop);
    g.editControlX = g.pad;
    g.editControlW = usableW;
    g.editControlH = kSettingsStepSize;
    const auto bandTop = static_cast<lv_coord_t>(g.editValueY + lv_font_get_line_height(LVGLUI::kFontHuge));
    const auto bandBottom = static_cast<lv_coord_t>(g.centerH - g.pad);
    g.editControlY = static_cast<lv_coord_t>(bandTop + ((bandBottom - bandTop) - g.editControlH) / 2);
    return g;
}

// top of the icon, the value and the label of a tile, the block is centered in the tile. A tile
// without a value keeps its label where the value would be
void settingsTileTops(const SettingsGeometry &g, bool withValue, lv_coord_t &iconTop, lv_coord_t &valueTop, lv_coord_t &labelTop)
{
    const auto blockH = withValue ? kSettingsTileBlockH : kSettingsTileBlockHShort;
    const auto top = static_cast<lv_coord_t>((g.tileH - blockH) / 2);
    iconTop = top;
    valueTop = static_cast<lv_coord_t>(top + kSettingsValueOffset);
    labelTop = static_cast<lv_coord_t>(withValue ? (top + kSettingsLabelOffset) : (top + kSettingsValueOffset));
}

// The step size of a timeout grows with its value: one minute steps up to 15 minutes, five minute
// steps up to an hour and 15 minute steps above that (a linear scale over the whole range of up to
// 24 hours cannot hit a value the user wants). The value is stored in seconds
constexpr uint32_t kTimeoutFineMinutes = 15;
constexpr uint32_t kTimeoutCoarseMinutes = 60;
constexpr uint32_t kTimeoutStepFine = 1;
constexpr uint32_t kTimeoutStepMedium = 5;
constexpr uint32_t kTimeoutStepCoarse = 15;
constexpr uint32_t kTimeoutMinMinutes = 1;
constexpr uint32_t kTimeoutMaxMinutes = 24 * 60;

// "45 s", "5 min", "1 h 30 min", "24 h"
String formatTimeout(uint32_t seconds)
{
    if (seconds < 60) {
        return PrintString(F("%u s"), static_cast<unsigned>(seconds));
    }
    if (seconds < 3600) {
        return PrintString(F("%u min"), static_cast<unsigned>(seconds / 60));
    }
    const auto hours = seconds / 3600;
    const auto minutes = (seconds % 3600) / 60;
    if (minutes) {
        return PrintString(F("%u h %u min"), static_cast<unsigned>(hours), static_cast<unsigned>(minutes));
    }
    return PrintString(F("%u h"), static_cast<unsigned>(hours));
}

// value of a timeout one step up or down, in seconds. The value is snapped to the nearest minute
// first, the steps walk the scale 1..15 min (1 min), 20..60 min (5 min) and 75 min..24 h (15 min),
// so 15 min is reached from both sides (15 -> 20 up, 20 -> 15 down)
uint32_t stepTimeout(uint32_t seconds, bool up)
{
    auto minutes = (seconds + 30) / 60; // round to the nearest minute
    if (minutes < kTimeoutMinMinutes) {
        minutes = kTimeoutMinMinutes;
    }
    else if (minutes > kTimeoutMaxMinutes) {
        minutes = kTimeoutMaxMinutes;
    }
    if (up) {
        if (minutes < kTimeoutFineMinutes) {
            minutes += kTimeoutStepFine;
        }
        else if (minutes < kTimeoutCoarseMinutes) {
            minutes += kTimeoutStepMedium;
        }
        else {
            minutes += kTimeoutStepCoarse;
        }
    }
    else {
        if (minutes <= kTimeoutFineMinutes) {
            minutes -= kTimeoutStepFine;
        }
        else if (minutes <= kTimeoutCoarseMinutes) {
            minutes -= kTimeoutStepMedium;
        }
        else {
            minutes -= kTimeoutStepCoarse;
        }
    }
    if (minutes < kTimeoutMinMinutes) {
        minutes = kTimeoutMinMinutes;
    }
    else if (minutes > kTimeoutMaxMinutes) {
        minutes = kTimeoutMaxMinutes;
    }
    return minutes * 60;
}

// name of an orientation, short enough for a tile (HassScreen::getOrientationName() is the long one
// of the status page)
const char *rotationName(uint8_t rotation)
{
    switch (rotation) {
    case static_cast<uint8_t>(WT32_SC01::Rotation::PORTRAIT):
        return "Portrait";
    case static_cast<uint8_t>(WT32_SC01::Rotation::LANDSCAPE_FLIPPED):
        return "Landscape 180";
    case static_cast<uint8_t>(WT32_SC01::Rotation::PORTRAIT_FLIPPED):
        return "Portrait 180";
    default:
        break;
    }
    return "Landscape";
}

// WiFi state of the header. The glyph tells it, there is no signal strength reading: associated,
// connecting (associating or waiting for the DHCP lease), an error and the interface switched off
LVGLUI::IconType settingsSignalIcon()
{
    if (WiFi.getMode() == WIFI_OFF) {
        return LVGLUI::IconType::WIFI_OFF;
    }
    switch (WiFi.status()) {
    case WL_CONNECTED:
        return LVGLUI::IconType::WIFI;
    case WL_NO_SSID_AVAIL:
    case WL_CONNECT_FAILED:
    case WL_CONNECTION_LOST:
        return LVGLUI::IconType::WIFI_ALERT;
    default:
        break;
    }
    return LVGLUI::IconType::WIFI_REMOVE;
}

} // namespace

// ------------------------------------------------------------------------------------------
// screen
// ------------------------------------------------------------------------------------------
void HassScreen::create(lv_obj_t *parent)
{
    // the display is rotated before anything is built: the whole layout below uses the metrics of
    // the orientation that is active (_width/_height and the values derived from them)
    _applyOrientation();
    // no top bar and no footer on this screen, the tiles use the whole display. createPage() is
    // only used by the pages that show a title and the clock
    _page = LVGLUI::PageRefs();
    // The widget tree of the last visit was removed by the screen manager: the fullscreen image
    // is gone with it (a camera preview that is still on screen is released below)
    _fullRoot = nullptr;
    _fullImage = nullptr;
    _fullTile = -1;
    _pendingFullscreen = -1;
    _pendingFullscreenClose = false;
    // the quick settings sheet is a child of the screen as well, it is gone with the tree
    _settingsRefs = SettingsRefs();
    _settingsOpen = false;
    _settingsPending = false;
    _settingsPendingView = -1;
    _settingsAction = SettingsAction::NONE;
    _settingsView = SettingsView::MAIN;
    _releaseImages();
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(parent, lv_color_hex(LVGLUI::kColorBackground), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(parent, 0, LV_PART_MAIN);
    // the widget tree is built by _rebuild() below, it fills _tiles
    _tiles.clear();
    _configLoaded = false;
    _configGeneration = 0;
    _lastTileClick = 0;
    _areaPage = 0;
    _pendingPage = -1;
    _back = TileRefs();
    _panelRefs = PanelRefs();
    _panel = Panel::NONE;
    _panelTile = -1;
    _panelView = PanelView::ARC;
    _listView = -1;
    _panelDetail = 0;
    _pendingPanel = -1;
    _pendingPanelClose = false;
    _pendingTileTap = -1;
    _pendingTileTapTime = 0;
    _controlPressed = false;
    _expectedItem[0] = 0;

    // request errors and notes (for example a missing file while the test configuration is used)
    _status = LVGLUI::addLabel(parent, kTileMargin, kStatusY, "", LVGLUI::kFontSmall, LVGLUI::kColorAlert,
                               static_cast<lv_coord_t>(_width - 2 * kTileMargin));
    lv_obj_add_flag(_status, LV_OBJ_FLAG_HIDDEN);

    // shown instead of the grid while there is no configuration
    _message = LVGLUI::addLabel(parent, kTileMargin, static_cast<lv_coord_t>(kGridTop + 40), "", LVGLUI::kFontMedium,
                                LVGLUI::kColorAlert, static_cast<lv_coord_t>(_width - 2 * kTileMargin), LV_TEXT_ALIGN_CENTER);
    lv_obj_add_flag(_message, LV_OBJ_FLAG_HIDDEN);

    // the grid lives in a container of its own, it is rebuilt when the configuration changes
    _grid = LVGLUI::createContainer(parent, 0, 0, _width, _height);

    // the screen is visible now, the values are only polled while it is
    _dashboard.setActive(true);
    _dashboard.update();
    _rebuild();
}

void HassScreen::release()
{
    _dashboard.setActive(false);
    // the manager removes the widget tree right after this call
    _fullTile = -1;
    _pendingFullscreen = -1;
    _pendingFullscreenClose = false;
    // the pointers of the quick settings sheet and its state
    _settingsRefs = SettingsRefs();
    _settingsOpen = false;
    _settingsPending = false;
    _settingsPendingView = -1;
    _settingsAction = SettingsAction::NONE;
    _releaseImages();
    _fullRoot = nullptr;
    _fullImage = nullptr;
    _tiles.clear();
    _back = TileRefs();
    _areaPage = 0;
    _pendingPage = -1;
    _grid = nullptr;
    _status = nullptr;
    _message = nullptr;
    _configLoaded = false;
    _panel = Panel::NONE;
    _panelTile = -1;
    _panelRefs = PanelRefs();
    // the graph of an open sensor panel would be requested while the screen is not visible
    _dashboard.closeStats();
    // a tap that waits for its second one must not open the panel when the screen comes back
    _pendingPanel = -1;
    _pendingTileTap = -1;
    _pendingTileTapTime = 0;
    // The other screens of the plugin (and the screen overview) are laid out for landscape. The
    // rotation is applied again by create(), this keeps the display in the orientation the rest of
    // the UI is built for
    WT32_SC01::setRotation(WT32_SC01::Rotation::LANDSCAPE);
}

// ------------------------------------------------------------------------------------------
// orientation
// ------------------------------------------------------------------------------------------
void HassScreen::setOrientation(uint8_t rotation)
{
    if (rotation > static_cast<uint8_t>(WT32_SC01::Rotation::PORTRAIT_FLIPPED)) {
        rotation = static_cast<uint8_t>(WT32_SC01::Rotation::LANDSCAPE);
    }
    if (rotation == _rotation) {
        return;
    }
    _rotation = rotation;
    _portrait = _rotation == static_cast<uint8_t>(WT32_SC01::Rotation::PORTRAIT) ||
                _rotation == static_cast<uint8_t>(WT32_SC01::Rotation::PORTRAIT_FLIPPED);
    // the display is only touched from the main loop (create() applies the rotation there, this
    // runs while the dashboard may not even be on screen)
    _orientationPending = true;
    __LDBG_printf("hass> orientation %u (%s)", static_cast<unsigned>(_rotation), _portrait ? "portrait" : "landscape");
}

void HassScreen::_applyOrientation()
{
    _orientationPending = false;
    _width = _portrait ? kScreenHeight : kScreenWidth;
    _height = _portrait ? kScreenWidth : kScreenHeight;
    // The grid container and the two labels above it are created by create() and survive a
    // _rebuild(), so they still have the size of the orientation that was active when the screen
    // was built. Without resizing them here the tiles of a new orientation are clipped by the
    // container (the landscape layout of a 480 px wide design cut off at a 320 px wide container,
    // and the other way around) - a screen reload hides it, a live change does not
    if (_grid) {
        lv_obj_set_size(_grid, _width, _height);
    }
    if (_status) {
        lv_obj_set_width(_status, static_cast<lv_coord_t>(_width - 2 * kTileMargin));
    }
    if (_message) {
        lv_obj_set_width(_message, static_cast<lv_coord_t>(_width - 2 * kTileMargin));
    }
    // The fullscreen view of a picture tile is created with the size of the display and lives next
    // to the grid (a child of the screen, a rebuild of the grid does not remove it). While the tree
    // is live it is dropped here, so the next _openFullscreen() creates it again with the new size.
    // create() runs with the tree removed (release() cleared the pointers) and _rebuild() closes an
    // open view anyway
    if (_grid && _fullRoot) {
        lv_obj_del(_fullRoot);
        _fullRoot = nullptr;
        _fullImage = nullptr;
        _fullTile = -1;
        _pendingFullscreen = -1;
        _pendingFullscreenClose = false;
    }
    // the quick settings sheet is created with the size of the display as well: the tree that is
    // live is dropped here and built again by update() (the view that is shown stays selected)
    if (_grid && _settingsRefs.root) {
        lv_obj_del(_settingsRefs.root);
        _settingsRefs = SettingsRefs();
        _settingsPendingView = static_cast<int8_t>(_settingsView);
    }
    WT32_SC01::setRotation(static_cast<WT32_SC01::Rotation>(_rotation));
}

const char *HassScreen::getOrientationName() const
{
    switch (static_cast<WT32_SC01::Rotation>(_rotation)) {
    case WT32_SC01::Rotation::PORTRAIT:
        return "portrait (320x480)";
    case WT32_SC01::Rotation::LANDSCAPE_FLIPPED:
        return "landscape (480x320), turned 180 degrees";
    case WT32_SC01::Rotation::PORTRAIT_FLIPPED:
        return "portrait (320x480), turned 180 degrees";
    default:
        return "landscape (480x320)";
    }
}

// ------------------------------------------------------------------------------------------
// motion sensor (MPU-6050 of the sensor plugin)
// ------------------------------------------------------------------------------------------
void HassScreen::setSensorRotation(uint16_t rotation)
{
    // the sensor reports 0/90/180/270 in 90 degree steps, see _applySensorRotation() for how the
    // value is turned into an orientation of the display
    if (rotation > 270 || (rotation % 90) != 0) {
        return;
    }
    if (static_cast<int16_t>(rotation) == _sensorRotation) {
        return;
    }
    _sensorRotation = static_cast<int16_t>(rotation);
    __LDBG_printf("hass> motion sensor reports %u degrees", static_cast<unsigned>(rotation));
    _applySensorRotation();
}

void HassScreen::_applySensorRotation()
{
    if (_sensorRotation < 0) {
        return;
    }
    if (Plugins::WeatherStation::getHassRotationLock()) {
        __LDBG_printf("hass> motion sensor rotation %d degrees ignored (the rotation is locked)", static_cast<int>(_sensorRotation));
        return;
    }
    // The module sits mirrored on the back of the panel, so its steps run opposite to the rotation
    // of the display driver: 0 and 180 (landscape) agree, 90 and 270 would be 180 degrees off and
    // the dashboard would be upside down in portrait. Mirroring the value turns the reported step
    // into the orientation index of the display (`WeatherStation::HassRotation`), so a Rotation
    // Offset in the sensor form turns the dashboard the other way round
    setOrientation(static_cast<uint8_t>((360 - _sensorRotation) % 360 / 90));
}

lv_coord_t HassScreen::_gridBottom() const
{
    return static_cast<lv_coord_t>(_height - kTileMargin);
}

lv_coord_t HassScreen::_sensorCardWidth() const
{
    return static_cast<lv_coord_t>(_width - 2 * kTileMargin);
}

lv_coord_t HassScreen::_sensorCardHeight() const
{
    return static_cast<lv_coord_t>(_height - kTileMargin - kSensorCardY);
}

lv_coord_t HassScreen::_sensorGraphWidth() const
{
    return static_cast<lv_coord_t>(_sensorCardWidth() - kSensorGraphX - 6);
}

lv_coord_t HassScreen::_sensorGraphHeight() const
{
    return static_cast<lv_coord_t>(_sensorCardHeight() - kSensorGraphY - kSensorTimeHeight);
}

const lv_font_t *HassScreen::_panelValueFont() const
{
    return _portrait ? LVGLUI::kFontHuge : LVGLUI::kFontValue;
}

void HassScreen::_releaseImages()
{
    // the pixels are released below, nothing may point at them any more (the fullscreen image
    // shows the same buffer as the tile)
    if (_fullImage) {
        lv_img_set_src(_fullImage, nullptr);
    }
    for (auto &picture : _pictures) {
        if (picture.buffer) {
            free(picture.buffer);
        }
        picture = Picture();
    }
}

HassScreen::Picture *HassScreen::_picture(HomeAssistant::TileIndex index)
{
    for (auto &picture : _pictures) {
        if (picture.tile == index) {
            return &picture;
        }
    }
    return nullptr;
}

HassScreen::Picture *HassScreen::_pictureFor(HomeAssistant::TileIndex index)
{
    auto found = _picture(index);
    if (found) {
        return found;
    }
    for (auto &picture : _pictures) {
        if (picture.tile == kNoTile) {
            picture.tile = index;
            return &picture;
        }
    }
    return nullptr;
}

// A tap on a picture tile shows the image over the whole display. The widget tree of the overlay
// is created once (a child of the screen, so that a rebuild of the grid does not remove it) and
// only shown and hidden afterwards. The request task fetches the next image in the size of the
// display while it is open: the tile sized frame that is stored is centered until it arrives
void HassScreen::_openFullscreen(HomeAssistant::TileIndex index)
{
    if (_fullTile >= 0 || index >= _dashboard.getTileCount()) {
        return;
    }
    const auto &tile = _dashboard.getConfig().getTile(index);
    if (tile.type != TileType::PICTURE) {
        return;
    }
    if (!_fullRoot) {
        auto parent = _grid ? lv_obj_get_parent(_grid) : nullptr;
        if (!parent) {
            return;
        }
        _fullRoot = LVGLUI::createContainer(parent, 0, 0, _width, _height);
        lv_obj_set_style_bg_color(_fullRoot, lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(_fullRoot, LV_OPA_COVER, LV_PART_MAIN);
        // the whole area returns to the dashboard and not only the image itself, so any tap or
        // swipe of the touch screen closes it (see _fullCallback())
        lv_obj_add_event_cb(_fullRoot, _fullCallback, LV_EVENT_ALL, this);
        _fullImage = LVGLUI::createImage(_fullRoot, 0, 0, _width, _height);
        lv_obj_center(_fullImage);
        lv_obj_add_flag(_fullRoot, LV_OBJ_FLAG_HIDDEN);
    }
    _fullTile = static_cast<int32_t>(index);
    _fullscreenTime = millis();
    auto picture = _picture(index);
    if (picture && picture->buffer) {
        // the frame that is stored is shown right away
        lv_img_set_src(_fullImage, &picture->dsc);
        lv_obj_set_size(_fullImage, picture->dsc.header.w, picture->dsc.header.h);
    }
    else {
        lv_img_set_src(_fullImage, nullptr);
    }
    lv_obj_center(_fullImage);
    lv_obj_move_foreground(_fullRoot);
    lv_obj_clear_flag(_fullRoot, LV_OBJ_FLAG_HIDDEN);
    // the tile behind the overlay draws the same pixels: it would show the image of the display
    // clipped to its corners
    if (picture && picture->image) {
        lv_obj_add_flag(picture->image, LV_OBJ_FLAG_HIDDEN);
    }
    // the next image is requested in the size of the display
    _dashboard.setPictureBox(index, static_cast<uint16_t>(_width), static_cast<uint16_t>(_height));
    __LDBG_printf("hass> fullscreen image of tile %u '%s'", static_cast<unsigned>(index), tile.name);
}

void HassScreen::_closeFullscreen()
{
    if (_fullTile < 0) {
        return;
    }
    const auto index = static_cast<HomeAssistant::TileIndex>(_fullTile);
    _fullTile = -1;
    _fullscreenTime = millis();
    if (_fullRoot) {
        lv_obj_add_flag(_fullRoot, LV_OBJ_FLAG_HIDDEN);
    }
    if (_fullImage) {
        lv_img_set_src(_fullImage, nullptr);
    }
    // The frame that is stored has the size of the display and the tile would show its top left
    // corner only. It shows the placeholder until its own frame arrives (the box of the tile
    // below makes the request task fetch it right away)
    auto picture = _picture(index);
    if (picture && picture->image) {
        lv_img_set_src(picture->image, nullptr);
        lv_obj_add_flag(picture->image, LV_OBJ_FLAG_HIDDEN);
    }
    showWidget(_widgets(index).refs.value, true);
    if (index < _dashboard.getTileCount()) {
        const auto &tile = _dashboard.getConfig().getTile(index);
        if (tile.type == TileType::PICTURE) {
            lv_coord_t x;
            lv_coord_t y;
            lv_coord_t w;
            lv_coord_t h;
            _tileGeometry(tile, x, y, w, h);
            _dashboard.setPictureBox(index, static_cast<uint16_t>(w), static_cast<uint16_t>(h));
        }
    }
    __LDBG_printf("hass> fullscreen image of tile %u closed", static_cast<unsigned>(index));
}

void HassScreen::_fullCallback(lv_event_t *event)
{
    auto self = static_cast<HassScreen *>(lv_event_get_user_data(event));
    if (!self || self->_fullTile < 0) {
        return;
    }
    switch (lv_event_get_code(event)) {
    case LV_EVENT_CLICKED:
    case LV_EVENT_RELEASED:
    case LV_EVENT_PRESS_LOST:
        // any tap or swipe on the image returns to the dashboard
        self->_pendingFullscreenClose = true;
        break;
    default:
        break;
    }
}

void HassScreen::_showPage(HomeAssistant::PageIndex page)
{
    if (page >= _dashboard.getConfig().getPageCount()) {
        page = 0;
    }
    if (page == _areaPage && _configLoaded) {
        return;
    }
    _areaPage = page;
    _rebuild();
}

void HassScreen::_rebuild()
{
    if (!_grid) {
        return;
    }
    _configLoaded = _dashboard.isLoaded();
    _configGeneration = _dashboard.getConfigGeneration();
    // the page or the panel changed: a tap that is still waiting for its second one is dropped
    _pendingTileTap = -1;
    // the configuration may have fewer pages than the one that is open
    if (_areaPage >= _dashboard.getConfig().getPageCount()) {
        _areaPage = 0;
    }
    // the camera previews of the page that is shown are fetched, the ones of the other pages not
    _dashboard.setVisiblePage(_areaPage);
    // an open fullscreen image is closed: the pixels and the tile it belongs to are released
    // below (the overlay itself lives next to the grid and is kept)
    _closeFullscreen();
    // the widget tree is removed first: the lv_img of a picture tile points into the pixel
    // buffer, which is released right after (the camera images of the page that is left)
    lv_obj_clean(_grid);
    _releaseImages();
    _tiles.clear();
    _back = TileRefs();
    _panelRefs = PanelRefs();
    if (!_configLoaded) {
        // no configuration: the message replaces the grid
        lv_obj_clear_flag(_message, LV_OBJ_FLAG_HIDDEN);
        const auto text = _dashboard.getScreenStatus();
        LVGLUI::setText(_message, text.c_str(), LVGLUI::kFontMedium, LVGLUI::kColorAlert);
        return;
    }
    lv_obj_add_flag(_message, LV_OBJ_FLAG_HIDDEN);
    // the panel of a tile that is gone (a new version of the file) is closed
    if (_panel != Panel::NONE && (_panelTile < 0 || _panelTile >= _dashboard.getTileCount())) {
        _panel = Panel::NONE;
        _panelTile = -1;
        _dashboard.closeDetail();
        _dashboard.closeStats();
    }
    if (_panel == Panel::NONE) {
        _buildGrid();
    }
    else {
        _buildPanel();
    }
    // the new entries of _tiles start with arcState 0xff, the widgets do not show a value yet
}

// Position and size of a block of cells in pixels. `page` selects the grid: the grid of a page is
// the grid of the document, or the grid of the area that owns the page (an area can use one of its
// own, see the `grid:` key of an area tile). The panels are laid out in the grid of the document
//
// A portrait display shows the grid of the configuration transposed: the file is written for the
// landscape layout, so 4x3 columns/rows become 3x4 and a cell keeps its shape. The tiles are placed
// in the transposed grid in the order of the file (see Config::_placeTiles()) and the span of a tile
// is swapped with the cells (see _tileGeometry())
void HassScreen::_cellGeometry(HomeAssistant::PageIndex page, uint8_t col, uint8_t row, uint8_t width, uint8_t height, lv_coord_t &x, lv_coord_t &y, lv_coord_t &w, lv_coord_t &h) const
{
    const auto gridCols = _dashboard.getConfig().getCols(page);
    const auto gridRows = _dashboard.getConfig().getRows(page);
    const auto cols = static_cast<uint8_t>(_portrait ? gridRows : gridCols);
    const auto rows = static_cast<uint8_t>(_portrait ? gridCols : gridRows);
    const auto usableWidth = static_cast<lv_coord_t>(_width - 2 * kTileMargin - (cols - 1) * kTileGap);
    const auto usableHeight = static_cast<lv_coord_t>(_gridBottom() - kGridTop - (rows - 1) * kTileGap);
    const auto cellWidth = static_cast<lv_coord_t>(usableWidth / cols);
    const auto cellHeight = static_cast<lv_coord_t>(usableHeight / rows);

    w = static_cast<lv_coord_t>(cellWidth * width + (width - 1) * kTileGap);
    h = static_cast<lv_coord_t>(cellHeight * height + (height - 1) * kTileGap);
    x = static_cast<lv_coord_t>(kTileMargin + col * (cellWidth + kTileGap));
    y = static_cast<lv_coord_t>(kGridTop + row * (cellHeight + kTileGap));
}

// The areas of a light/dimmer or climate panel. In landscape the back tile and the options are the
// two cells of the left column and the header and the control use the rest of the display (the
// reviewed layout). A portrait display transposes the cells of the document grid into four rows,
// where that column is only half as tall as the display, so the portrait panel is stacked in rows
// over the whole width instead:
//
//   climate                            light (dimmer)
//   +------------------------------+   +------------------------------+
//   | [back]  name of the entity   |   | [back]  name of the entity   |
//   |         the arc              |   |         the level, in %      |
//   |         - +                  |   |  -  [ the slider ]        +  |
//   |         the options          |   |         the options          |
//   +------------------------------+   +------------------------------+
//
// The arc is centered on its *ring* (the 270 degree arc does not draw into the bottom of its box)
// and the steppers of a light are centered on the slider, which fills the band it is in
HassScreen::PanelLayout HassScreen::_panelLayout() const
{
    PanelLayout layout;
    // the two cells of the left column of a landscape panel
    lv_coord_t x;
    lv_coord_t y;
    lv_coord_t w;
    lv_coord_t h;
    lv_coord_t ox;
    lv_coord_t oy;
    lv_coord_t ow;
    lv_coord_t oh;
    _cellGeometry(0, 0, 0, 1, 1, x, y, w, h);
    _cellGeometry(0, 0, 1, 1, 1, ox, oy, ow, oh);

    if (_portrait) {
        // the header: the tile that closes the panel and the name of the entity beside it
        layout.backX = kTileMargin;
        layout.backY = kGridTop;
        layout.backW = kSensorBackSize;
        layout.backH = kSensorBackSize;
        layout.headerX = static_cast<lv_coord_t>(kTileMargin + kSensorBackSize + kSensorHeaderGap);
        layout.headerY = layout.backY;
        layout.headerW = static_cast<lv_coord_t>(_width - kTileMargin - layout.headerX);
        // the options are the last row, above the bottom margin
        layout.optionsX = kTileMargin;
        layout.optionsW = static_cast<lv_coord_t>(_width - 2 * kTileMargin);
        layout.optionsH = kPanelButtonHeight;
        layout.optionsY = static_cast<lv_coord_t>(_gridBottom() - layout.optionsH);

        const auto headerBottom = static_cast<lv_coord_t>(layout.backY + layout.backH);

        if (_panel == Panel::CLIMATE) {
            // the row of the steppers sits above the options and the arc has the band between the
            // header and that row (the arc is centered on its ring in it, see _buildPanel())
            layout.stepsX = kTileMargin;
            layout.stepsH = kPanelStepSize;
            layout.stepsY = static_cast<lv_coord_t>(layout.optionsY - kPanelPortraitGap - layout.stepsH);
            layout.controlX = kTileMargin;
            layout.controlW = layout.optionsW;
            layout.controlY = static_cast<lv_coord_t>(headerBottom + kPanelPortraitGap);
            layout.controlH = static_cast<lv_coord_t>(layout.stepsY - kPanelPortraitGap - layout.controlY);
            // The arc and the steppers are hidden while a list is shown: the list takes the whole
            // band down to the buttons (the stepper row would be an empty strip above them)
            layout.listX = layout.controlX;
            layout.listW = layout.controlW;
            layout.listY = layout.controlY;
            layout.listH = static_cast<lv_coord_t>(layout.optionsY - kPanelPortraitGap - layout.listY);
            return layout;
        }

        // the light: the readout is a row of its own under the header (as tall as its text, so the
        // gap above and below it is the plain block gap) and the slider fills the band between it
        // and the options (the steppers of the level are centered on the slider). The list of the
        // effects starts at the readout row: the readout is hidden while the list is shown, so the
        // list uses that row and there is no empty row between the name and the effects
        layout.valueY = static_cast<lv_coord_t>(headerBottom + kPanelPortraitGap);
        layout.valueH = static_cast<lv_coord_t>(lv_font_get_line_height(_panelValueFont()));
        layout.controlX = kTileMargin;
        layout.controlW = layout.optionsW;
        layout.controlY = static_cast<lv_coord_t>(layout.valueY + layout.valueH + kPanelPortraitGap);
        layout.controlH = static_cast<lv_coord_t>(layout.optionsY - kPanelPortraitGap - layout.controlY);
        layout.listX = layout.controlX;
        layout.listW = layout.controlW;
        layout.listY = layout.valueY;
        layout.listH = static_cast<lv_coord_t>(layout.optionsY - kPanelPortraitGap - layout.listY);
        return layout;
    }

    layout.backX = x;
    layout.backY = y;
    layout.backW = w;
    layout.backH = h;
    layout.optionsX = ox;
    layout.optionsY = oy;
    layout.optionsW = ow;
    layout.optionsH = oh;
    layout.headerX = static_cast<lv_coord_t>(x + w + kTileGap);
    layout.headerY = y;
    layout.headerW = static_cast<lv_coord_t>(_width - kTileMargin - layout.headerX);
    layout.controlX = layout.headerX;
    layout.controlY = static_cast<lv_coord_t>(y + kPanelHeaderHeight);
    layout.controlW = layout.headerW;
    layout.controlH = static_cast<lv_coord_t>(oh + h + kTileGap - kPanelHeaderHeight);
    // the list replaces the control, it uses the same area of the right column
    layout.listX = layout.controlX;
    layout.listY = layout.controlY;
    layout.listW = layout.controlW;
    layout.listH = layout.controlH;
    return layout;
}

void HassScreen::_tileGeometry(const Tile &tile, lv_coord_t &x, lv_coord_t &y, lv_coord_t &w, lv_coord_t &h) const
{
    // the grid of the page the tile belongs to, which is not the page that is shown while a panel
    // of another page is built
    if (_portrait) {
        // The placement of the portrait grid (the tiles are in the order of the file there as well,
        // see Config::_placeTiles()) and the span swapped with the cells: a tile that is two cells
        // tall in landscape is two cells wide in portrait
        _cellGeometry(tile.page, tile.portraitCol, tile.portraitRow, tile.height, tile.width, x, y, w, h);
    }
    else {
        _cellGeometry(tile.page, tile.col, tile.row, tile.width, tile.height, x, y, w, h);
    }
}

void HassScreen::_buildGrid()
{
    lv_coord_t x;
    lv_coord_t y;
    lv_coord_t w;
    lv_coord_t h;

    // an area page starts with the tile that closes it again (an arrow, no text - the layout of
    // the reviewed design, see docs/hass_layout)
    if (_areaPage) {
        _cellGeometry(_areaPage, 0, 0, 1, 1, x, y, w, h);
        _back.tile = LVGLUI::createTile(_grid, x, y, w, h);
        lv_obj_add_event_cb(_back.tile, _tileCallback, LV_EVENT_CLICKED, this);
        _back.icon = LVGLUI::createIcon(_back.tile, LVGLUI::IconType::BACK, static_cast<lv_coord_t>((w - LVGLUI::kIconSizeLarge) / 2), iconCenterTop(h), LVGLUI::kIconSizeLarge);
        LVGLUI::clearClickable(_back.icon);
    }

    const auto &config = _dashboard.getConfig();
    for (HomeAssistant::TileIndex i = 0; i < config.getTileCount(); i++) {
        const auto &tile = config.getTile(i);
        if (tile.page != _areaPage) {
            // the tile belongs to another page
            continue;
        }
        if (tile.type == TileType::SPACER) {
            // empty space, the cells stay free and nothing is drawn
            continue;
        }
        _buildTile(i);
    }
}

void HassScreen::_buildTile(HomeAssistant::TileIndex index)
{
    const auto &tile = _dashboard.getConfig().getTile(index);
    lv_coord_t x;
    lv_coord_t y;
    lv_coord_t w;
    lv_coord_t h;
    _tileGeometry(tile, x, y, w, h);

    // one entry per tile of the page that is built, it carries the index of the tile in the
    // configuration (the entry is looked up by it, see _widgets()). Grow the buffer by 16 entries
    // instead of letting the vector double it (a page has at most 64 tiles)
    if (_tiles.size() == _tiles.capacity()) {
        _tiles.reserve(_tiles.size() + 16);
    }
    _tiles.emplace_back();
    auto &refs = _tiles.back().refs;
    _tiles.back().globalTile = index;
    refs.tile = LVGLUI::createTile(_grid, x, y, w, h);
    lv_obj_add_event_cb(refs.tile, _tileCallback, LV_EVENT_CLICKED, this);

    // the name is the last row of every tile, the value sits above it
    const auto nameY = static_cast<lv_coord_t>(h - kTileNameHeight);
    // Number of cells the tile is tall in the grid that is shown. The layout of the configuration
    // is transposed for a portrait display, so a tile that is two cells tall in landscape is two
    // cells wide in portrait (_tileGeometry())
    const auto spanHeight = static_cast<uint8_t>(_portrait ? tile.width : tile.height);

    if (tile.type == TileType::DIMMER) {
        // 1x2: the level fills the tile from the bottom edge to the top, the percentage and the
        // name are drawn on top of it. The fill has the size of the tile and is clipped by its
        // rounded corners, so there is no gap at the top or the bottom (the reviewed layout has the
        // fill flush with the tile). The whole tile receives the drag (up/down changes the level, a
        // tap opens the panel of the entity)
        lv_obj_set_style_clip_corner(refs.tile, true, LV_PART_MAIN);
        refs.fill = LVGLUI::createLevelFill(refs.tile, 0, 0, w, h);
        // the name sits at the bottom edge and is centered, the percentage in the middle. Both
        // are white: the name is drawn on top of the level fill
        refs.name = LVGLUI::addLabel(refs.tile, 6, 0, tile.name, LVGLUI::kFontSmall,
                                     LVGLUI::kColorText, static_cast<lv_coord_t>(w - 12), LV_TEXT_ALIGN_CENTER);
        setBottomAlignedText(refs.name, tile.name, static_cast<lv_coord_t>(w - 12), static_cast<lv_coord_t>(h - 6));
        refs.value = LVGLUI::addLabel(refs.tile, 6, static_cast<lv_coord_t>(h / 2 - 18), "", LVGLUI::kFontValue,
                                      LVGLUI::kColorText, static_cast<lv_coord_t>(w - 12), LV_TEXT_ALIGN_CENTER);
        refs.drag = LVGLUI::createContainer(refs.tile, 0, 0, w, h);
        lv_obj_add_event_cb(refs.drag, _dragCallback, LV_EVENT_ALL, this);
        return;
    }

    if (tile.type == TileType::CLIMATE) {
        if (spanHeight <= 1) {
            // 1x1 (`size: 1x1`): the readout of the 1x2 without the +/- bars. The setpoint is
            // stepped in the panel, a tap on the tile opens it. The name sits in the bottom row
            // like the name of any other one cell tile. A short cell draws the setpoint with the
            // smaller font and puts the action right below it, otherwise the two labels run into
            // the name (a page with three rows, same as the sensor tiles)
            const auto valueSpace = tileIsCompact(h) ? kCompactClimateValueSpace : 0;
            const auto valueTop = tileIsCompact(h) ? static_cast<lv_coord_t>((h - kTileNameHeight - valueSpace) / 2)
                                                   : static_cast<lv_coord_t>(h / 2 - 24);
            const auto actionTop = tileIsCompact(h) ? static_cast<lv_coord_t>(valueTop + 26)
                                                    : static_cast<lv_coord_t>(h / 2 + 8);
            refs.value = LVGLUI::addLabel(refs.tile, 4, valueTop, "", tileValueFont(h),
                                          LVGLUI::kColorText, static_cast<lv_coord_t>(w - 8), LV_TEXT_ALIGN_CENTER);
            refs.action = LVGLUI::addLabel(refs.tile, 4, actionTop, "", LVGLUI::kFontNormal,
                                           LVGLUI::kColorTextValue, static_cast<lv_coord_t>(w - 8), LV_TEXT_ALIGN_CENTER);
            refs.name = LVGLUI::addLabel(refs.tile, 4, nameY, tile.name, LVGLUI::kFontSmall, LVGLUI::kColorTextLabel,
                                         static_cast<lv_coord_t>(w - 8), LV_TEXT_ALIGN_CENTER);
            lv_label_set_long_mode(refs.name, LV_LABEL_LONG_WRAP);
            return;
        }
        // 1x2: the + and - bars at the top and the bottom step the setpoint, the rest is the action
        // ("Heating"), the setpoint and the current temperature with the thermometer in front of it.
        // The three of them are one group that is centered in the room the bars and the name block
        // leave, with the same gap between each other - the setpoint keeps the large font while the
        // group fits with it and uses the smaller one otherwise (a 4x3 page gives the tile 192 px
        // and 84 px of room, the large font needs 86). Before they sat around the vertical middle
        // with the readouts of a compact tile crowded together and the name far below them
        const auto roomTop = static_cast<lv_coord_t>(kTileBarHeight + 2);
        const auto nameHeight = textBlockHeight(tile.name, LVGLUI::kFontSmall, static_cast<lv_coord_t>(w - 8));
        const auto roomBottom = static_cast<lv_coord_t>(h - kTileBarHeight - 4 - nameHeight);
        const auto room = static_cast<lv_coord_t>(roomBottom - roomTop);
        const auto readoutGap = static_cast<lv_coord_t>(8);
        const auto actionHeight = static_cast<lv_coord_t>(lv_font_get_line_height(LVGLUI::kFontNormal));
        const auto stateHeight = static_cast<lv_coord_t>(lv_font_get_line_height(LVGLUI::kFontMedium));
        auto valueFont = tileValueFont(h);
        auto valueHeight = static_cast<lv_coord_t>(lv_font_get_line_height(valueFont));
        if ((actionHeight + readoutGap + valueHeight + readoutGap + stateHeight) > room) {
            valueFont = LVGLUI::kFontLarge;
            valueHeight = static_cast<lv_coord_t>(lv_font_get_line_height(valueFont));
        }
        const auto groupHeight = static_cast<lv_coord_t>(actionHeight + readoutGap + valueHeight + readoutGap + stateHeight);
        const auto groupTop = static_cast<lv_coord_t>(roomTop + ((room > groupHeight) ? ((room - groupHeight) / 2) : 0));
        const auto actionTop = groupTop;
        const auto valueTop = static_cast<lv_coord_t>(actionTop + actionHeight + readoutGap);
        const auto stateTop = static_cast<lv_coord_t>(valueTop + valueHeight + readoutGap);
        refs.stepUp = LVGLUI::createButton(refs.tile, 2, 2, static_cast<lv_coord_t>(w - 4), kTileBarHeight, "+", LVGLUI::kFontMedium);
        lv_obj_add_event_cb(refs.stepUp, _stepCallback, LV_EVENT_CLICKED, this);
        LVGLUI::setButtonActive(refs.stepUp, true);
        refs.action = LVGLUI::addLabel(refs.tile, 4, actionTop, "", LVGLUI::kFontNormal, LVGLUI::kColorTextValue,
                                      static_cast<lv_coord_t>(w - 8), LV_TEXT_ALIGN_CENTER);
        refs.value = LVGLUI::addLabel(refs.tile, 4, valueTop, "", valueFont, LVGLUI::kColorText,
                                     static_cast<lv_coord_t>(w - 8), LV_TEXT_ALIGN_CENTER);
        // the current temperature with a small thermometer in front of it
        refs.icon = LVGLUI::createIcon(refs.tile, LVGLUI::IconType::THERMOMETER, static_cast<lv_coord_t>(w / 2 - 32),
                                       static_cast<lv_coord_t>(stateTop - 1), kTileInlineIcon);
        LVGLUI::clearClickable(refs.icon);
        refs.state = LVGLUI::addLabel(refs.tile, static_cast<lv_coord_t>(w / 2 - 8), stateTop, "", LVGLUI::kFontMedium,
                                     LVGLUI::kColorTextValue, static_cast<lv_coord_t>(w / 2 + 4));
        refs.name = LVGLUI::addLabel(refs.tile, 4, 0, tile.name, LVGLUI::kFontSmall, LVGLUI::kColorTextLabel,
                                     static_cast<lv_coord_t>(w - 8), LV_TEXT_ALIGN_CENTER);
        // above the bottom bar: a name that wraps into a second line grows upward
        setBottomAlignedText(refs.name, tile.name, static_cast<lv_coord_t>(w - 8), static_cast<lv_coord_t>(h - kTileBarHeight - 4));
        refs.stepDown = LVGLUI::createButton(refs.tile, 2, static_cast<lv_coord_t>(h - kTileBarHeight - 2),
                                            static_cast<lv_coord_t>(w - 4), kTileBarHeight, "-", LVGLUI::kFontMedium);
        lv_obj_add_event_cb(refs.stepDown, _stepCallback, LV_EVENT_CLICKED, this);
        LVGLUI::setButtonActive(refs.stepDown, true);
        return;
    }

    if (tile.type == TileType::PICTURE) {
        // Camera preview: the image fills the tile and is clipped by its rounded corners, the name
        // sits on a translucent strip at the bottom edge. The first frame arrives from the request
        // task (see update()), until then the tile shows a placeholder. A tap opens the image
        // fullscreen (see _openFullscreen())
        lv_obj_set_style_clip_corner(refs.tile, true, LV_PART_MAIN);
        auto picture = _pictureFor(index);
        if (picture) {
            picture->width = static_cast<uint16_t>(w);
            picture->height = static_cast<uint16_t>(h);
            picture->image = LVGLUI::createImage(refs.tile, 0, 0, w, h);
        }
        else {
            __LDBG_printf("hass> no free picture slot for tile %u (the maximum is %u)", static_cast<unsigned>(index), static_cast<unsigned>(kMaxPictures));
        }
        refs.value = LVGLUI::addLabel(refs.tile, 0, static_cast<lv_coord_t>(h / 2 - 18), "--", LVGLUI::kFontValue,
                                      LVGLUI::kColorTextMuted, w, LV_TEXT_ALIGN_CENTER);
        refs.name = LVGLUI::addLabel(refs.tile, 0, static_cast<lv_coord_t>(h - kTileNameHeight), tile.name, LVGLUI::kFontSmall,
                                     LVGLUI::kColorText, w, LV_TEXT_ALIGN_CENTER);
        // a strip behind the name, an image can be bright where the text is
        lv_obj_set_style_bg_color(refs.name, lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(refs.name, LV_OPA_50, LV_PART_MAIN);
        lv_obj_set_style_pad_all(refs.name, 4, LV_PART_MAIN);
        // The text of a label starts at the top of its box, with the padding of the four sides
        // the gap below the text would be the rest of the strip. The text is centered between the
        // two edges of the strip instead (the same gap above and below)
        const auto namePadding = static_cast<lv_coord_t>((kTileNameHeight - lv_font_get_line_height(LVGLUI::kFontSmall)) / 2);
        lv_obj_set_style_pad_top(refs.name, namePadding > 0 ? namePadding : 0, LV_PART_MAIN);
        lv_obj_set_style_pad_bottom(refs.name, namePadding > 0 ? namePadding : 0, LV_PART_MAIN);
        lv_obj_set_style_radius(refs.name, 0, LV_PART_MAIN);
        lv_obj_set_height(refs.name, kTileNameHeight);
        lv_label_set_long_mode(refs.name, LV_LABEL_LONG_DOT);
        // the request task needs the pixel box of the tile to ask for the right size
        _dashboard.setPictureBox(index, static_cast<uint16_t>(w), static_cast<uint16_t>(h));
        return;
    }

    if (tile.type == TileType::AREA) {
        // an area tile only carries the icon and the name, it opens a page of its own. The icon
        // is the large one, it fits into the cell of a short page as well
        const auto iconTop = areaIconTop(h);
        if (tile.icon != TileIcon::NONE) {
            refs.icon = LVGLUI::createIcon(refs.tile, toIconType(tile), static_cast<lv_coord_t>((w - LVGLUI::kIconSizeLarge) / 2),
                                           iconTop, LVGLUI::kIconSizeLarge);
        }
        refs.name = LVGLUI::addLabel(refs.tile, 4, static_cast<lv_coord_t>(iconTop + LVGLUI::kIconSizeLarge + 4), tile.name, LVGLUI::kFontSmall,
                                     LVGLUI::kColorTextLabel, static_cast<lv_coord_t>(w - 8), LV_TEXT_ALIGN_CENTER);
        LVGLUI::clearClickable(refs.icon);
        return;
    }

    // one cell: icon, value and name in a centered column. A cell that is too short for the large
    // icon and the large value font (a page with three or more rows) uses the compact layout.
    // `icon: none` draws no icon, the value and the name then use the whole cell
    if (tile.icon != TileIcon::NONE) {
        const auto iconSize = tileIconSize(h);
        refs.icon = LVGLUI::createIcon(refs.tile, toIconType(tile), static_cast<lv_coord_t>((w - iconSize) / 2),
                                       tileIconTop(h), iconSize);
        LVGLUI::clearClickable(refs.icon);
    }
    refs.value = LVGLUI::addLabel(refs.tile, 4, 0, "", tileValueFont(h),
                                  LVGLUI::kColorText, static_cast<lv_coord_t>(w - 8), LV_TEXT_ALIGN_CENTER);
    // The value sits above the name block: it is anchored to the bottom edge of the name block and
    // grows upward, into the space below the icon. The label is empty while the tile is built, the
    // position and the height of its box are set with the first value (_updateTile() calls
    // setBottomAlignedText() for that), the alignment here is only the same bottom edge for the
    // first frame after the tile was built
    lv_obj_align(refs.value, LV_ALIGN_BOTTOM_MID, 0, -kTileNameHeight);
    refs.name = LVGLUI::addLabel(refs.tile, 4, nameY, tile.name, LVGLUI::kFontSmall, LVGLUI::kColorTextLabel,
                                 static_cast<lv_coord_t>(w - 8), LV_TEXT_ALIGN_CENTER);
    // the name of a tile can be long, it wraps into the second line of the block
    lv_label_set_long_mode(refs.name, LV_LABEL_LONG_WRAP);
}

void HassScreen::_setTextIfChanged(lv_obj_t *label, const String &text, const lv_font_t *font, uint32_t color)
{
    if (!label) {
        return;
    }
    const auto current = lv_label_get_text(label);
    if (current && !strcmp(current, text.c_str())) {
        return;
    }
    LVGLUI::setText(label, text.c_str(), font, color);
    LVGLUI::fitTextDown(label, text.c_str());
}

// chip of the range selector (the channel chips of the power screen have the same look, the round
// background and the label are drawn by hand)
lv_obj_t *createStatsChip(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, uint8_t hours, lv_obj_t **label)
{
    auto obj = lv_obj_create(parent);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, kSensorChipWidth, kSensorChipHeight);
    lv_obj_set_style_radius(obj, static_cast<lv_coord_t>(kSensorChipHeight / 2), LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN);
    lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);

    char text[8];
    snprintf(text, sizeof(text), "%u h", static_cast<unsigned>(hours));
    // the label is not clickable, LVGL routes the click to the chip below it
    const auto lineHeight = lv_font_get_line_height(LVGLUI::kFontSmall);
    *label = LVGLUI::addLabel(obj, 0, static_cast<lv_coord_t>((kSensorChipHeight - lineHeight) / 2), text, LVGLUI::kFontSmall,
                              LVGLUI::kColorTextMuted, kSensorChipWidth, LV_TEXT_ALIGN_CENTER);
    return obj;
}

// ------------------------------------------------------------------------------------------
// panels: the page a light/dimmer or a climate tile opens (back tile, header, options and the
// control of the option that is selected)
// ------------------------------------------------------------------------------------------
void HassScreen::_buildPanel()
{
    if (_panelTile < 0 || !_grid) {
        return;
    }
    const auto index = static_cast<HomeAssistant::TileIndex>(_panelTile);

    // the panel of a sensor is the entity card of the Home Assistant app: a header with the live
    // value, the range buttons and the history graph. It has none of the controls of the other two
    // panels, so it is built by a function of its own
    if (_panel == Panel::SENSOR) {
        _buildSensorPanel(index);
        return;
    }

    const auto &tile = _dashboard.getConfig().getTile(index);
    // the panel is the only tile that is built, one entry carries its widgets (the grid was
    // removed by the caller)
    _tiles.clear();
    _tiles.emplace_back();
    _tiles.back().globalTile = index;
    auto &refs = _panelRefs;
    auto &widgets = _tiles.back();
    auto &refsTile = widgets.refs;
    const auto layout = _panelLayout();

    // The controls of the panel have fixed sizes that fit the landscape column. A portrait panel
    // leaves ~300 px for them but a wheel of 196 px would run over the bottom edge of a short
    // control area, so they shrink with the area instead
    const auto levelSliderWidth = minOf(kPanelSliderWidth, static_cast<lv_coord_t>(layout.controlW - kPanelSliderStepGap - kPanelStepSize));
    const auto wheelSize = minOf(kPanelWheelSize, minOf(layout.controlW, layout.controlH));

    // The level slider and the steppers of the level are one block, it is centered in the control
    // area (before the block started at the left edge and left a wide empty band at the right,
    // the reviewed layout has the control in the middle). The color temperature slider has no
    // steppers and is centered on its own
    const auto levelSliderX = static_cast<lv_coord_t>(
        layout.controlX + (layout.controlW - (levelSliderWidth + kPanelSliderStepGap + kPanelStepSize)) / 2);
    const auto tempSliderX = static_cast<lv_coord_t>(layout.controlX + (layout.controlW - levelSliderWidth) / 2);
    const auto levelStepX = static_cast<lv_coord_t>(levelSliderX + levelSliderWidth + kPanelSliderStepGap);

    // the tile that closes the panel (an arrow, see the back tile of an area page)
    _back.tile = LVGLUI::createTile(_grid, layout.backX, layout.backY, layout.backW, layout.backH);
    lv_obj_add_event_cb(_back.tile, _tileCallback, LV_EVENT_CLICKED, this);
    _back.icon = LVGLUI::createIcon(_back.tile, LVGLUI::IconType::BACK,
                                    static_cast<lv_coord_t>((layout.backW - LVGLUI::kIconSizeLarge) / 2),
                                    iconCenterTop(layout.backH), LVGLUI::kIconSizeLarge);
    LVGLUI::clearClickable(_back.icon);

    // The name of the entity is the header of the panel: it uses the whole width of its area (only
    // half of it made a name like "Lyric Thermostat" wrap into a second line, which ran into the
    // pills below). A name that is longer than the area shrinks the font instead. The landscape
    // header is the top of the control column, the name of a portrait one is centered in the
    // header row beside the back tile
    const auto titleY = _portrait
                            ? static_cast<lv_coord_t>(layout.headerY +
                                                      (layout.backH - lv_font_get_line_height(LVGLUI::kFontTitle)) / 2)
                            : static_cast<lv_coord_t>(layout.headerY + 4);
    refs.title = LVGLUI::addLabel(_grid, layout.headerX, titleY, tile.name, LVGLUI::kFontTitle,
                                  LVGLUI::kColorText, layout.headerW, LV_TEXT_ALIGN_LEFT);
    LVGLUI::fitTextDown(refs.title, tile.name, layout.headerW);

    if (_panel == Panel::CLIMATE) {
        // the three options of the thermostat card: a column in the left cell of a landscape panel,
        // a row under the header of a portrait one
        static const LVGLUI::IconType kPillIcons[kNumPills] = { LVGLUI::IconType::FLAME, LVGLUI::IconType::RECORD, LVGLUI::IconType::FAN };
        const auto pillW = _portrait
                               ? static_cast<lv_coord_t>((layout.optionsW - (kNumPills - 1) * kTileGap) / kNumPills)
                               : layout.optionsW;
        const auto pillH = _portrait
                               ? layout.optionsH
                               : static_cast<lv_coord_t>((layout.optionsH - (kNumPills - 1) * kTileGap) / kNumPills);
        for (uint8_t i = 0; i < kNumPills; i++) {
            const auto px = _portrait ? static_cast<lv_coord_t>(layout.optionsX + i * (pillW + kTileGap)) : layout.optionsX;
            const auto py = _portrait ? layout.optionsY : static_cast<lv_coord_t>(layout.optionsY + i * (pillH + kTileGap));
            refs.pills[i] = LVGLUI::createButton(_grid, px, py, pillW, pillH, nullptr);
            auto icon = LVGLUI::createIcon(refs.pills[i], kPillIcons[i], 6,
                                           static_cast<lv_coord_t>((pillH - LVGLUI::kIconSizeSmall) / 2), LVGLUI::kIconSizeSmall);
            LVGLUI::clearClickable(icon);
            refs.pillValue[i] = LVGLUI::addLabel(refs.pills[i], 30, static_cast<lv_coord_t>(pillH / 2 - 9), "", LVGLUI::kFontNormal,
                                                 LVGLUI::kColorText, static_cast<lv_coord_t>(pillW - 34));
            lv_obj_add_event_cb(refs.pills[i], _panelCallback, LV_EVENT_CLICKED, this);
        }
        // The arc with the action and the setpoint inside. In landscape the steppers share the
        // control column with it (stacked at its right edge) and the arc fills the rest down to the
        // bottom. In portrait the arc uses the whole width and its *ring* (not its box, the 270
        // degree arc does not draw into the bottom of the box) is centered in the band between the
        // header and the row of the steppers, so the gap the eye sees above and below it is the same
        lv_coord_t arcSize;
        lv_coord_t arcX;
        lv_coord_t arcY;
        if (_portrait) {
            arcSize = layout.controlW;
            const auto ringHeight = static_cast<lv_coord_t>(arcSize * kArcRingHeight);
            arcX = layout.controlX;
            arcY = static_cast<lv_coord_t>(layout.controlY + (layout.controlH - ringHeight) / 2);
        }
        else {
            const auto maxArc = static_cast<lv_coord_t>(layout.controlW - kPanelStepColumn - kTileGap);
            arcSize = minOf(static_cast<lv_coord_t>(layout.controlH - 4), maxArc);
            arcX = static_cast<lv_coord_t>(layout.controlX + (layout.controlW - kPanelStepColumn - arcSize) / 2);
            arcY = static_cast<lv_coord_t>(layout.controlY + layout.controlH - arcSize);
        }
        refsTile.arc = LVGLUI::createArc(_grid, arcX, arcY, arcSize, kPanelArcRing);
        lv_obj_add_event_cb(refsTile.arc, _arcCallback, LV_EVENT_ALL, this);
        refsTile.action = LVGLUI::addLabel(_grid, arcX, static_cast<lv_coord_t>(arcY + arcSize / 2 - 34), "", LVGLUI::kFontNormal,
                                           LVGLUI::kColorActive, arcSize, LV_TEXT_ALIGN_CENTER);
        refsTile.value = LVGLUI::addLabel(_grid, arcX, static_cast<lv_coord_t>(arcY + arcSize / 2 - 10), "", LVGLUI::kFontTitle,
                                          LVGLUI::kColorText, arcSize, LV_TEXT_ALIGN_CENTER);
        // the current temperature below the setpoint: "Current temperature" and the value, both
        // centered under the setpoint inside the arc
        refs.label = LVGLUI::addLabel(_grid, arcX, static_cast<lv_coord_t>(arcY + arcSize / 2 + 24), "Current temperature", LVGLUI::kFontSmall,
                                      LVGLUI::kColorTextLabel, arcSize, LV_TEXT_ALIGN_CENTER);
        refs.headerValue = LVGLUI::addLabel(_grid, arcX, static_cast<lv_coord_t>(arcY + arcSize / 2 + 38), "", LVGLUI::kFontMedium,
                                            LVGLUI::kColorText, arcSize, LV_TEXT_ALIGN_CENTER);
        widgets.arcMin = 0;
        widgets.arcMax = 0;
        widgets.arcValue = -1;
    }
    else {
        // light/dimmer: the buttons of the control bar are stacked in the left column, the control
        // they select fills the right one. Power and brightness are always there, the color, the
        // color temperature and the effects depend on the entity and appear when its attributes
        // arrived (see _layoutPanelButtons())
        static const LVGLUI::IconType kButtonIcons[static_cast<uint8_t>(LightButton::COUNT)] = {
            LVGLUI::IconType::POWER, LVGLUI::IconType::BRIGHTNESS, LVGLUI::IconType::PALETTE,
            LVGLUI::IconType::COLOR_TEMP, LVGLUI::IconType::EFFECT,
        };
        for (uint8_t i = 0; i < static_cast<uint8_t>(LightButton::COUNT); i++) {
            refs.buttons[i] = LVGLUI::createIconButton(_grid, layout.optionsX, layout.optionsY, layout.optionsW,
                                                       kPanelButtonHeight, kButtonIcons[i], LVGLUI::kIconSizeSmall);
            lv_obj_add_event_cb(refs.buttons[i], _panelCallback, LV_EVENT_CLICKED, this);
        }
        // the power button is white with a dark glyph, the selectors are cards that show which
        // control is on screen (setTileState() fills the one of the control that is shown)
        lv_obj_set_style_bg_color(refs.buttons[static_cast<uint8_t>(LightButton::POWER)], lv_color_hex(LVGLUI::kColorText), LV_PART_MAIN);
        LVGLUI::setIconColor(lv_obj_get_child(refs.buttons[static_cast<uint8_t>(LightButton::POWER)], 0), LVGLUI::kColorBackground);
        // The buttons the entity offers are known from the state poll (the capabilities of the
        // tile), so the panel is complete when it appears. Only when they are not available yet
        // the detail response refines them (a bit later, the layout changes once then).
        const auto capabilities = _dashboard.getCapabilities(index);
        _panelButtons = capabilities ? capabilities : static_cast<uint8_t>(HomeAssistant::kCapPower | HomeAssistant::kCapLevel);
        _layoutPanelButtons();

        // The level slider fills the band between the readout and the options in portrait, the two
        // steppers are centered on it at its ends (the same place a climate puts its steppers). It
        // is 12 % narrower than the space between them and stays centered between them, so the gap
        // on either side of it is about 21 px instead of 12 - evenly. The rounding of the track and
        // of the fill is the same in both orientations (the panel has one look for its sliders)
        lv_coord_t sliderW = levelSliderWidth;
        lv_coord_t sliderX = levelSliderX;
        lv_coord_t sliderY = layout.controlY;
        lv_coord_t sliderH = layout.controlH;
        if (_portrait) {
            const auto rowW = static_cast<lv_coord_t>(layout.controlW - 2 * (kPanelStepSize + kPanelPortraitGap));
            sliderW = static_cast<lv_coord_t>(rowW * (100 - kPanelSliderNarrow) / 100);
            sliderX = static_cast<lv_coord_t>(layout.controlX + kPanelStepSize +
                                              (layout.controlW - 2 * kPanelStepSize - sliderW) / 2);
        }
        refs.slider = createPanelSlider(_grid, sliderX, sliderY, sliderW, sliderH, 0, 100, _sliderCallback, this,
                                        kPanelSliderRadius, kPanelSliderFillRadius);
        // the color temperature slider, in kelvin (the range comes from the entity)
        refs.tempSlider = createPanelSlider(_grid, _portrait ? sliderX : tempSliderX, sliderY, sliderW, sliderH, 2000, 6500,
                                            _tempSliderCallback, this, kPanelSliderRadius, kPanelSliderFillRadius);

        // The readout of the level and the one of the colour temperature are two labels at the
        // same place, only the one of the control that is selected is visible (see _updatePanel()):
        // in portrait the row above the slider, where the whole width belongs to the value, in
        // landscape on the track of the slider of that control - the colour temperature has none
        // of the steppers, so its slider is centered in the control area and the readout has to
        // follow it (it was centered on the level slider and reached out of the track)
        const auto valueFont = _panelValueFont();
        const auto valueHeight = static_cast<lv_coord_t>(lv_font_get_line_height(valueFont));
        const lv_coord_t valueY = _portrait
                                      ? static_cast<lv_coord_t>(layout.valueY + (layout.valueH - valueHeight) / 2)
                                      : static_cast<lv_coord_t>(sliderY + sliderH / 2 - valueHeight / 2);
        const auto levelValueWidth = _portrait ? layout.controlW : sliderW;
        const auto levelValueX = _portrait ? layout.controlX : sliderX;
        const auto tempValueWidth = _portrait ? layout.controlW : levelSliderWidth;
        const auto tempValueX = _portrait ? layout.controlX : tempSliderX;
        refsTile.value = LVGLUI::addLabel(_grid, levelValueX, valueY, "", valueFont, LVGLUI::kColorText, levelValueWidth, LV_TEXT_ALIGN_CENTER);
        refs.label = LVGLUI::addLabel(_grid, tempValueX, valueY, "", valueFont, LVGLUI::kColorText, tempValueWidth, LV_TEXT_ALIGN_CENTER);

        // the color wheel
        refs.wheel = lv_colorwheel_create(_grid, true);
        lv_obj_set_pos(refs.wheel, static_cast<lv_coord_t>(layout.controlX + (layout.controlW - wheelSize) / 2),
                       static_cast<lv_coord_t>(layout.controlY + (layout.controlH - wheelSize) / 2));
        lv_obj_set_size(refs.wheel, wheelSize, wheelSize);
        // the wheel draws its hue ring and the knob: no background of its own (a background would
        // fill the disc and hide the ring) and a white ring around the knob, so the color that is
        // set is visible on every hue (knob_recolor of the constructor fills it)
        lv_obj_set_style_bg_opa(refs.wheel, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(refs.wheel, 0, LV_PART_MAIN);
        lv_obj_set_style_arc_width(refs.wheel, 24, LV_PART_MAIN);
        lv_obj_set_style_border_width(refs.wheel, 3, LV_PART_KNOB);
        lv_obj_set_style_border_color(refs.wheel, lv_color_hex(LVGLUI::kColorText), LV_PART_KNOB);
        lv_obj_add_event_cb(refs.wheel, _wheelCallback, LV_EVENT_ALL, this);
    }

    if (_panel == Panel::DIMMER) {
        if (_portrait) {
            // the steppers are centered on the slider, one at each end of it (minus at the left,
            // like the bars of a dimmer tile)
            const auto stepY = static_cast<lv_coord_t>(layout.controlY + (layout.controlH - kPanelStepSize) / 2);
            refsTile.stepDown = createStepButton(_grid, layout.controlX, stepY, LVGLUI::IconType::MINUS);
            refsTile.stepUp = createStepButton(_grid, static_cast<lv_coord_t>(layout.controlX + layout.controlW - kPanelStepSize),
                                               stepY, LVGLUI::IconType::PLUS);
        }
        else {
            // the steppers of the level are right of the slider, stacked and centered in the control
            // area
            const auto stepsH = static_cast<lv_coord_t>(2 * kPanelStepSize + kPanelStepGap);
            const auto stepY = static_cast<lv_coord_t>(layout.controlY + (layout.controlH - stepsH) / 2);
            refsTile.stepUp = createStepButton(_grid, levelStepX, stepY, LVGLUI::IconType::PLUS);
            refsTile.stepDown = createStepButton(_grid, levelStepX, static_cast<lv_coord_t>(stepY + kPanelStepSize + kPanelStepGap), LVGLUI::IconType::MINUS);
        }
    }
    else {
        // The steppers of the setpoint: stacked at the right edge of the control area in landscape
        // (the top one raises, "+ above -"), a row under the arc in portrait - "-" left of "+",
        // the same order the bars of the tile have
        if (_portrait) {
            const auto stepRowX = static_cast<lv_coord_t>(layout.controlX + (layout.controlW - (2 * kPanelStepSize + kPanelStepGap)) / 2);
            refsTile.stepDown = createStepButton(_grid, stepRowX, layout.stepsY, LVGLUI::IconType::MINUS);
            refsTile.stepUp = createStepButton(_grid, static_cast<lv_coord_t>(stepRowX + kPanelStepSize + kPanelStepGap), layout.stepsY,
                                               LVGLUI::IconType::PLUS);
        }
        else {
            const auto stepsH = static_cast<lv_coord_t>(2 * kPanelStepSize + kPanelStepGap);
            const auto stepX = static_cast<lv_coord_t>(layout.controlX + layout.controlW - kPanelStepSize);
            const auto stepY = static_cast<lv_coord_t>(layout.controlY + layout.controlH - stepsH);
            refsTile.stepUp = createStepButton(_grid, stepX, stepY, LVGLUI::IconType::PLUS);
            refsTile.stepDown = createStepButton(_grid, stepX, static_cast<lv_coord_t>(stepY + kPanelStepSize + kPanelStepGap), LVGLUI::IconType::MINUS);
        }
    }
    if (refsTile.stepUp) {
        lv_obj_add_event_cb(refsTile.stepUp, _stepCallback, LV_EVENT_CLICKED, this);
        lv_obj_add_event_cb(refsTile.stepDown, _stepCallback, LV_EVENT_CLICKED, this);
    }

    // The list of the options (climate) and of the effects (light panel) replaces the control. Its
    // items are buttons on the background of the panel, no card around them (they look like the
    // tiles of the grid, see _buildPanelList()). The right padding keeps the scrollbar off them.
    refs.list = LVGLUI::createList(_grid, layout.listX, layout.listY, layout.listW, layout.listH);
    lv_obj_set_style_bg_opa(refs.list, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(refs.list, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(refs.list, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_right(refs.list, 14, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(refs.list, 8, LV_PART_MAIN);
    lv_obj_add_flag(refs.list, LV_OBJ_FLAG_HIDDEN);
    _listView = -1;
}

// ------------------------------------------------------------------------------------------
// sensor panel: the header with the live value, the range buttons and the history graph
// ------------------------------------------------------------------------------------------
uint8_t HassScreen::statsRangeHours(StatsRange range)
{
    switch (range) {
    case StatsRange::HOURS_48:
        return 48;
    case StatsRange::HOURS_12:
        return 12;
    default:
        return 24;
    }
}

HassScreen::StatsRange HassScreen::statsRangeOf(uint8_t hours)
{
    switch (hours) {
    case 48:
        return StatsRange::HOURS_48;
    case 12:
        return StatsRange::HOURS_12;
    default:
        return StatsRange::HOURS_24;
    }
}

void HassScreen::_buildSensorPanel(HomeAssistant::TileIndex index)
{
    const auto &tile = _dashboard.getConfig().getTile(index);
    auto &refs = _panelRefs;
    auto &sensor = _sensor;
    sensor = SensorRefs();

    // the tile that closes the panel: a compact one in the upper left corner (the header and the
    // graph use the whole width, the other two panels use the first cell of the grid)
    _back.tile = LVGLUI::createTile(_grid, kTileMargin, kSensorHeaderY, kSensorBackSize, kSensorBackSize);
    lv_obj_add_event_cb(_back.tile, _tileCallback, LV_EVENT_CLICKED, this);
    _back.icon = LVGLUI::createIcon(_back.tile, LVGLUI::IconType::BACK,
                                    static_cast<lv_coord_t>((kSensorBackSize - LVGLUI::kIconSizeSmall) / 2),
                                    static_cast<lv_coord_t>((kSensorBackSize - LVGLUI::kIconSizeSmall) / 2), LVGLUI::kIconSizeSmall);
    LVGLUI::clearClickable(_back.icon);

    // Header: the glyph of the entity, its name and the live value. The value is polled with the
    // states of the screen (every `hass.poll`), so the panel needs no request of its own for it -
    // only the graph is a request of its own (once a minute)
    const auto iconX = static_cast<lv_coord_t>(kTileMargin + kSensorBackSize + kSensorHeaderGap);
    sensor.icon = LVGLUI::createIcon(_grid, toIconType(tile), iconX, kSensorHeaderY, LVGLUI::kIconSizeLarge);
    const auto nameX = static_cast<lv_coord_t>(iconX + LVGLUI::kIconSizeLarge + kSensorHeaderGap);
    // the value is right aligned at the edge, the name gets what is left of the header. A portrait
    // display has ~300 px for the header (the back tile and the icon take 110 of them), so the
    // value column is a third of the width instead of the fixed landscape size
    const auto valueWidth = minOf(static_cast<lv_coord_t>(150), static_cast<lv_coord_t>(_width / 3));
    const auto nameWidth = static_cast<lv_coord_t>(_width - kTileMargin - valueWidth - kSensorHeaderGap - nameX);
    const auto headerCenter = static_cast<lv_coord_t>(kSensorHeaderY + kSensorBackSize / 2);
    refs.title = LVGLUI::addLabel(_grid, nameX, static_cast<lv_coord_t>(headerCenter - lv_font_get_line_height(LVGLUI::kFontTitle) / 2),
                                  tile.name, LVGLUI::kFontTitle, LVGLUI::kColorText, nameWidth);
    LVGLUI::fitTextDown(refs.title, tile.name, nameWidth);
    refs.headerValue = LVGLUI::addLabel(_grid, static_cast<lv_coord_t>(_width - kTileMargin - valueWidth),
                                        static_cast<lv_coord_t>(headerCenter - lv_font_get_line_height(LVGLUI::kFontValue) / 2), "",
                                        LVGLUI::kFontValue, LVGLUI::kColorText, valueWidth, LV_TEXT_ALIGN_RIGHT);

    // "History" and the range buttons, the one that is selected is filled (see _updateSensorPanel)
    sensor.title = LVGLUI::addLabel(_grid, kTileMargin, static_cast<lv_coord_t>(kSensorTitleY + 2), "History", LVGLUI::kFontTitle,
                                    LVGLUI::kColorText);
    auto chipX = static_cast<lv_coord_t>(_width - kTileMargin -
                                         (kStatsRangeCount * kSensorChipWidth + (kStatsRangeCount - 1) * kSensorChipGap));
    for (uint8_t i = 0; i < kStatsRangeCount; i++) {
        sensor.chips[i] = createStatsChip(_grid, chipX, static_cast<lv_coord_t>(kSensorTitleY + (kSensorTitleHeight - kSensorChipHeight) / 2),
                                          statsRangeHours(static_cast<StatsRange>(i)), &sensor.chipLabels[i]);
        // PRESSED instead of CLICKED: the chips are small and CLICKED is only sent while the finger
        // did not move (a scroll cancels it), a selection is reliable on the press
        lv_obj_add_event_cb(sensor.chips[i], _panelCallback, LV_EVENT_PRESSED, this);
        chipX = static_cast<lv_coord_t>(chipX + kSensorChipWidth + kSensorChipGap);
    }

    // the card of the graph with the unit above the level labels
    auto card = LVGLUI::createCard(_grid, kSensorCardX, kSensorCardY, _sensorCardWidth(), _sensorCardHeight());
    sensor.unit = LVGLUI::addLabel(card, 2, 2, "", LVGLUI::kFontSmall, LVGLUI::kColorTextMuted,
                                   static_cast<lv_coord_t>(kSensorLevelWidth - 4), LV_TEXT_ALIGN_RIGHT);
    // The three value lines are the dividers of the chart (3 divisions put them on the top, the
    // middle and the bottom of the content area - the object minus the inset the values are mapped
    // into) and the labels sit on the same lines
    const auto levelTop = static_cast<lv_coord_t>(kSensorGraphY + kSensorGraphInset);
    const auto levelHeight = static_cast<lv_coord_t>(_sensorGraphHeight() - 2 * kSensorGraphInset);
    const lv_coord_t levelY[3] = {
        levelTop,
        static_cast<lv_coord_t>(levelTop + levelHeight / 2 - kSensorLevelHeight / 2),
        static_cast<lv_coord_t>(levelTop + levelHeight - kSensorLevelHeight),
    };
    for (uint8_t i = 0; i < 3; i++) {
        sensor.levels[i] = LVGLUI::addLabel(card, 2, levelY[i], "", LVGLUI::kFontSmall, LVGLUI::kColorTextMuted,
                                            static_cast<lv_coord_t>(kSensorLevelWidth - 4), LV_TEXT_ALIGN_RIGHT);
    }

    // The vertical grid lines of the X axis: one 1 px line per tick, positioned by
    // _drawSensorChart(). lv_chart can only place its dividers evenly over the object, which cannot
    // follow the clock. They are created before the chart, so the curve is drawn over them
    const auto gridY = static_cast<lv_coord_t>(kSensorGraphY + kSensorGraphInset);
    const auto gridHeight = static_cast<lv_coord_t>(_sensorGraphHeight() - 2 * kSensorGraphInset);
    for (auto &line : sensor.grid) {
        line = LVGLUI::createContainer(card, kSensorGraphX, gridY, 1, gridHeight);
        lv_obj_set_style_bg_color(line, lv_color_hex(LVGLUI::kColorBorder), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(line, LV_OPA_COVER, LV_PART_MAIN);
        LVGLUI::clearClickable(line);
        showWidget(line, false);
    }

    // The graph: a line chart with the three value lines of the app layout, the curve is drawn over
    // the grid lines of the X axis
    sensor.chart = lv_chart_create(card);
    lv_obj_set_pos(sensor.chart, kSensorGraphX, kSensorGraphY);
    lv_obj_set_size(sensor.chart, _sensorGraphWidth(), _sensorGraphHeight());
    lv_obj_set_style_bg_opa(sensor.chart, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(sensor.chart, 0, LV_PART_MAIN);
    // The chart maps the value range to its content area (object minus padding) and clips the
    // drawing to the object, see kSensorGraphInset
    lv_obj_set_style_pad_all(sensor.chart, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_top(sensor.chart, kSensorGraphInset, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(sensor.chart, kSensorGraphInset, LV_PART_MAIN);
    lv_obj_set_style_line_color(sensor.chart, lv_color_hex(LVGLUI::kColorBorder), LV_PART_MAIN);
    lv_obj_set_style_width(sensor.chart, 0, LV_PART_INDICATOR);
    lv_obj_set_style_height(sensor.chart, 0, LV_PART_INDICATOR);
    lv_obj_set_style_line_width(sensor.chart, 2, LV_PART_ITEMS);
    lv_obj_set_style_line_color(sensor.chart, lv_color_hex(LVGLUI::kColorAccent), LV_PART_ITEMS);
    lv_chart_set_type(sensor.chart, LV_CHART_TYPE_LINE);
    lv_chart_set_div_line_count(sensor.chart, 3, 0);
    lv_chart_set_range(sensor.chart, LV_CHART_AXIS_PRIMARY_Y, 0, kSensorChartScale);
    // one column per bucket of the window, a bucket without data stays empty (LV_CHART_POINT_NONE)
    sensor.buckets = statsRangeBuckets(_statsHours);
    lv_chart_set_point_count(sensor.chart, sensor.buckets);
    sensor.series = lv_chart_add_series(sensor.chart, lv_color_hex(LVGLUI::kColorAccent), LV_CHART_AXIS_PRIMARY_Y);
    if (sensor.series) {
        // lv_chart_add_series() leaves the x-pointers of a non scatter series uninitialized and
        // lv_chart_remove_series() frees them - the same trap the power screen avoids, never remove
        // a series of a chart (LVGL 8.4, lv_chart.c)
        sensor.series->x_points = nullptr;
        sensor.series->x_ext_buf_assigned = 0;
        lv_chart_set_all_value(sensor.chart, sensor.series, LV_CHART_POINT_NONE);
    }

    // The labels below the graph, one per grid line of the X axis. They are created hidden: the
    // position, the width and the text come from the tick in _drawSensorChart(), a tick whose label
    // does not fit inside the card is not drawn at all
    const auto timeY = static_cast<lv_coord_t>(kSensorGraphY + _sensorGraphHeight() + kSensorTimeTop);
    for (auto &label : sensor.times) {
        label = LVGLUI::addLabel(card, kSensorGraphX, timeY, "", LVGLUI::kFontSmall, LVGLUI::kColorTextMuted);
        // the width is set per tick, the alignment keeps the text centered on its line
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        showWidget(label, false);
    }

    // the message that replaces the graph while it is empty ("loading", "no history", an error)
    sensor.info = LVGLUI::addLabel(card, kSensorGraphX, static_cast<lv_coord_t>(kSensorGraphY + _sensorGraphHeight() / 2 - 8), "",
                                   LVGLUI::kFontMedium, LVGLUI::kColorTextMuted, _sensorGraphWidth(), LV_TEXT_ALIGN_CENTER);

    // the tree is new, the graph has to be filled (and its labels set) with the next update
    _statsGeneration = 0xffffffff;
}

void HassScreen::_updateSensorPanel()
{
    if (_panelTile < 0) {
        return;
    }
    const auto index = static_cast<HomeAssistant::TileIndex>(_panelTile);
    const auto &tile = _dashboard.getConfig().getTile(index);
    const auto &value = _dashboard.getValue(index);
    auto &refs = _panelRefs;
    auto &sensor = _sensor;

    // the live value of the entity, formatted like its tile (the state poll refreshes it, the graph
    // only once a minute)
    auto stateColor = LVGLUI::kColorText;
    auto iconState = LVGLUI::TileState::OFF;
    switch (value.state) {
    case TileState::ON:
        iconState = LVGLUI::TileState::ON;
        break;
    case TileState::UNAVAILABLE:
        iconState = LVGLUI::TileState::UNAVAILABLE;
        stateColor = LVGLUI::kColorError;
        break;
    case TileState::UNKNOWN:
        stateColor = LVGLUI::kColorTextMuted;
        break;
    default:
        break;
    }
    if (sensor.icon) {
        LVGLUI::setIcon(sensor.icon, tileIconType(tile, value, iconState), LVGLUI::kIconSizeLarge, stateIconColor(iconState));
    }
    _setTextIfChanged(refs.headerValue, formatSensorValue(tile, value), LVGLUI::kFontValue, stateColor);

    // the range that is selected is filled, the other two are plain cards
    const auto selected = statsRangeOf(_statsHours);
    for (uint8_t i = 0; i < kStatsRangeCount; i++) {
        const auto active = (static_cast<StatsRange>(i) == selected);
        if (sensor.chips[i]) {
            lv_obj_set_style_bg_color(sensor.chips[i], lv_color_hex(active ? LVGLUI::kColorAccent : LVGLUI::kColorCardAlt), LV_PART_MAIN);
            lv_obj_set_style_border_color(sensor.chips[i], lv_color_hex(active ? LVGLUI::kColorAccent : LVGLUI::kColorBorder), LV_PART_MAIN);
            // the label of the selected chip is dark on the accent background
            lv_obj_set_style_text_color(sensor.chipLabels[i], lv_color_hex(active ? LVGLUI::kColorBackground : LVGLUI::kColorTextMuted),
                                        LV_PART_MAIN);
        }
    }

    // The graph is only filled again when a new response arrived (or the range changed): the
    // request runs once a minute, drawing the buckets again on every update would be wasted work
    const auto generation = _dashboard.getStatsGeneration();
    if (sensor.drawnHours != _statsHours || sensor.drawnGeneration != generation) {
        sensor.drawnHours = _statsHours;
        sensor.drawnGeneration = generation;
        _drawSensorChart(tile);
    }
}

void HassScreen::_drawSensorChart(const HomeAssistant::Tile &tile)
{
    auto &sensor = _sensor;
    if (!sensor.chart || !sensor.series) {
        return;
    }
    const auto hours = _statsHours;
    const auto count = _dashboard.getStatsCount();
    const auto points = _dashboard.getStatsPoints();
    // The window of the graph: the one of the last response. While the first one is on its way the
    // labels are built from the clock of the device - the request covers the same window
    auto start = _dashboard.getStatsStart();
    auto end = _dashboard.getStatsEnd();
    if (!start || end <= start) {
        end = static_cast<uint32_t>(time(nullptr));
        start = static_cast<uint32_t>(end - static_cast<uint32_t>(hours) * 3600);
    }
    // the unit of the value, the same one the tile shows
    const char *unit = tile.unit[0] ? tile.unit : _dashboard.getValue(static_cast<HomeAssistant::TileIndex>(_panelTile)).unit;
    LVGLUI::setText(sensor.unit, unit ? unit : "", LVGLUI::kFontSmall, LVGLUI::kColorTextMuted);

    // The X axis: one grid line every kSensorGridHours at whole local hours (00:00, 04:00, ...) and
    // the label of the tick under its line. The ticks are stepped in local time (tm_hour += 4 and
    // mktime() resolve a DST change), so the labels keep reading whole hours wherever the window
    // sits in the year
    const auto span = end - start;
    const auto labelStep = statsLabelStepHours(hours);
    const auto timeY = static_cast<lv_coord_t>(kSensorGraphY + _sensorGraphHeight() + kSensorTimeTop);
    struct tm tick;
    {
        const auto value = static_cast<time_t>(start);
        localtime_r(&value, &tick);
        tick.tm_min = 0;
        tick.tm_sec = 0;
        tick.tm_hour = ((tick.tm_hour + kSensorGridHours - 1) / kSensorGridHours) * kSensorGridHours;
        tick.tm_isdst = -1;
    }
    uint8_t lines = 0;
    uint8_t labels = 0;
    for (auto time = static_cast<uint32_t>(mktime(&tick)); time <= end;) {
        String text;
        _formatStatsTime(time, text);
        const auto width = static_cast<lv_coord_t>(lv_txt_get_width(text.c_str(), text.length(), LVGLUI::kFontSmall, 0, LV_TEXT_FLAG_NONE));
        const auto x = static_cast<lv_coord_t>(kSensorGraphX + (static_cast<int64_t>(_sensorGraphWidth()) * (time - start)) / span);
        // A tick whose label does not fit inside the card is dropped (line and label): a label that
        // is moved inside the card sits beside its line instead of under it
        if (x - width / 2 >= 2 && x + width / 2 <= static_cast<lv_coord_t>(_sensorCardWidth() - 2)) {
            if (lines < kSensorTimeTicks && sensor.grid[lines]) {
                lv_obj_set_pos(sensor.grid[lines], x, static_cast<lv_coord_t>(kSensorGraphY + kSensorGraphInset));
                showWidget(sensor.grid[lines], true);
                lines++;
            }
            if ((tick.tm_hour % labelStep) == 0 && labels < kSensorTimeTicks && sensor.times[labels]) {
                auto *label = sensor.times[labels];
                // centered on the line of the tick, the box is 2 px wider than the text so the text
                // can never wrap into a second line
                lv_obj_set_pos(label, static_cast<lv_coord_t>(x - width / 2), timeY);
                lv_obj_set_width(label, static_cast<lv_coord_t>(width + 2));
                LVGLUI::setText(label, text.c_str(), LVGLUI::kFontSmall, LVGLUI::kColorTextMuted);
                showWidget(label, true);
                labels++;
            }
        }
        tick.tm_hour += kSensorGridHours;
        tick.tm_isdst = -1;
        time = static_cast<uint32_t>(mktime(&tick));
    }
    for (uint8_t i = lines; i < kSensorTimeTicks; i++) {
        if (sensor.grid[i]) {
            showWidget(sensor.grid[i], false);
        }
    }
    for (uint8_t i = labels; i < kSensorTimeTicks; i++) {
        if (sensor.times[i]) {
            showWidget(sensor.times[i], false);
        }
    }

    if (!count) {
        // no buckets: the request is on its way, it failed, or the entity has no long term
        // statistics at all (a combined template sensor, for example)
        lv_chart_set_all_value(sensor.chart, sensor.series, LV_CHART_POINT_NONE);
        lv_chart_refresh(sensor.chart);
        for (auto &level : sensor.levels) {
            LVGLUI::setText(level, "", LVGLUI::kFontSmall, LVGLUI::kColorTextMuted);
        }
        String message;
        if (_dashboard.isStatsPending()) {
            message = String("Loading history...");
        }
        else if (_dashboard.getStatsError().length()) {
            message = _dashboard.getStatsError();
        }
        else {
            message = String("No history for this entity");
        }
        LVGLUI::setText(sensor.info, message.c_str(), LVGLUI::kFontMedium, LVGLUI::kColorTextMuted);
        showWidget(sensor.info, true);
        return;
    }
    showWidget(sensor.info, false);

    // auto scale from the window plus a margin of 10 %, like the graph of the power screen. A flat
    // line keeps a small span instead of filling the whole graph
    auto min = points[0].mean;
    auto max = min;
    for (uint16_t i = 1; i < count; i++) {
        const auto value = points[i].mean;
        if (value < min) {
            min = value;
        }
        if (value > max) {
            max = value;
        }
    }
    // Auto scale from the window, snapped to a "nice" step of one significant digit: the three
    // labels of the axis are round numbers (0 / 70 / 140) instead of the window plus a margin
    // (-10.4 / 55.7 / 122). A flat line keeps a small span around its value
    if ((max - min) <= 0.0f) {
        const auto flat = (fabsf(max) > 0.01f) ? fabsf(max) * 0.1f : 1.0f;
        min -= flat / 2.0f;
        max += flat / 2.0f;
    }
    // two intervals between the bottom and the top label, a single significant digit per step
    const auto step = statsNiceStep((max - min) / 2.0f);
    auto low = floorf(min / step) * step;
    auto high = low + 2.0f * step;
    if (high < max) {
        high += step * ceilf((max - high) / step);
    }
    // the span of the window is mapped to the integer range of the chart (see kSensorChartScale)
    const auto factor = (high > low) ? (static_cast<float>(kSensorChartScale) / (high - low)) : 1.0f;
    lv_chart_set_range(sensor.chart, LV_CHART_AXIS_PRIMARY_Y, 0, kSensorChartScale);
    lv_chart_set_all_value(sensor.chart, sensor.series, LV_CHART_POINT_NONE);

    // One column per bucket of the window, placed by the time of the bucket: the recorder omits a
    // bucket without data and the graph shows that gap instead of moving the curve
    for (uint16_t i = 0; i < count; i++) {
        const auto time = points[i].time;
        if (!time || time < start) {
            continue;
        }
        const auto bucket = (time - start) / kSensorBucketSeconds;
        if (bucket >= sensor.buckets) {
            continue;
        }
        auto value = static_cast<int32_t>(lroundf((points[i].mean - low) * factor));
        if (value < 0) {
            value = 0;
        }
        else if (value > kSensorChartScale) {
            value = kSensorChartScale;
        }
        lv_chart_set_value_by_id(sensor.chart, sensor.series, static_cast<uint16_t>(bucket), static_cast<lv_coord_t>(value));
    }
    lv_chart_refresh(sensor.chart);

    // the levels of the window, all three in the format of the step (mixing "140" and "70.0"
    // looks broken)
    const auto decimals = (step >= 1.0f) ? 0 : ((step >= 0.1f) ? 1 : 2);
    LVGLUI::setText(sensor.levels[0], _formatStatsValue(high, decimals).c_str(), LVGLUI::kFontSmall, LVGLUI::kColorTextMuted);
    LVGLUI::setText(sensor.levels[1], _formatStatsValue((high + low) / 2, decimals).c_str(), LVGLUI::kFontSmall, LVGLUI::kColorTextMuted);
    LVGLUI::setText(sensor.levels[2], _formatStatsValue(low, decimals).c_str(), LVGLUI::kFontSmall, LVGLUI::kColorTextMuted);

    __LDBG_printf("hass> history of tile %u: %u bucket(s) of %u, %.4f..%.4f drawn as %.4f..%.4f", static_cast<unsigned>(_panelTile),
                  static_cast<unsigned>(count), static_cast<unsigned>(sensor.buckets), static_cast<double>(min), static_cast<double>(max),
                  static_cast<double>(low), static_cast<double>(high));
}

String HassScreen::_formatStatsValue(float value, uint8_t decimals) const
{
    // a level of the graph has a column of 40 px, so a large value is drawn without decimals. The
    // number of decimals comes from the step of the axis so all three labels are formatted alike
    return PrintString(F("%.*f"), decimals, static_cast<double>(value));
}

void HassScreen::_formatStatsTime(uint32_t time, String &output) const
{
    struct tm tm;
    const auto value = static_cast<time_t>(time);
    localtime_r(&value, &tm);
    char buffer[32];
    const auto format = _data.isTimeFormat24h() ? "%H:%M" : "%I:%M %p";
    if (!strftime(buffer, sizeof(buffer), format, &tm)) {
        output = String();
        return;
    }
    output = buffer;
    // strftime pads the hour of the 12 hour format with a zero ("04:00 PM"), the labels of the
    // layout have none. The hour of the 24 hour format keeps its zero ("00:30" is a valid time)
    if (!_data.isTimeFormat24h() && output.length() > 1 && output[0] == '0') {
        output.remove(0, 1);
    }
}

// Stacks the buttons of a light panel in the left column and gives the rest of the column to the
// back tile. The buttons the entity offers are known before the panel is built (the capabilities
// of the state poll), so the stack has no gaps: the tile grows when the entity has fewer controls
// and the arrow of the back tile stays centered in it.
void HassScreen::_layoutPanelButtons()
{
    auto &refs = _panelRefs;
    const auto layout = _panelLayout();

    uint8_t count = 0;
    for (uint8_t i = 0; i < static_cast<uint8_t>(LightButton::COUNT); i++) {
        if (_panelButtons & (1 << i)) {
            count++;
        }
    }
    if (!count) {
        count = 1;
    }

    // the options of a portrait panel are a row under the header (see _panelLayout()): the
    // buttons share its width and the back tile keeps its square size
    if (_portrait) {
        const auto buttonWidth = static_cast<lv_coord_t>((layout.optionsW - (count - 1) * kPanelButtonGap) / count);
        uint8_t visible = 0;
        for (uint8_t i = 0; i < static_cast<uint8_t>(LightButton::COUNT); i++) {
            auto button = refs.buttons[i];
            if (!button) {
                continue;
            }
            if (!(_panelButtons & (1 << i))) {
                lv_obj_add_flag(button, LV_OBJ_FLAG_HIDDEN);
                continue;
            }
            lv_obj_clear_flag(button, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_pos(button, static_cast<lv_coord_t>(layout.optionsX + visible * (buttonWidth + kPanelButtonGap)), layout.optionsY);
            lv_obj_set_size(button, buttonWidth, kPanelButtonHeight);
            // the button is round ended and its glyph is centered again after the resize
            lv_obj_set_style_radius(button, static_cast<lv_coord_t>(kPanelButtonHeight / 2), LV_PART_MAIN);
            auto icon = lv_obj_get_child(button, 0);
            if (icon) {
                lv_obj_set_pos(icon, static_cast<lv_coord_t>((buttonWidth - LVGLUI::kIconSizeSmall) / 2),
                               static_cast<lv_coord_t>((kPanelButtonHeight - LVGLUI::kIconSizeSmall) / 2));
            }
            visible++;
        }
        return;
    }

    const auto columnH = static_cast<lv_coord_t>(layout.optionsH + layout.backH + kTileGap);
    const auto stackH = static_cast<lv_coord_t>(count * kPanelButtonHeight + (count - 1) * kPanelButtonGap);
    const auto backH = static_cast<lv_coord_t>(columnH - stackH - kPanelButtonGap);

    if (_back.tile) {
        lv_obj_set_size(_back.tile, layout.backW, backH);
        if (_back.icon) {
            lv_obj_set_pos(_back.icon, static_cast<lv_coord_t>((layout.backW - LVGLUI::kIconSizeLarge) / 2),
                           static_cast<lv_coord_t>((backH - LVGLUI::kIconSizeLarge) / 2));
        }
    }

    auto buttonY = static_cast<lv_coord_t>(layout.backY + backH + kPanelButtonGap);
    for (uint8_t i = 0; i < static_cast<uint8_t>(LightButton::COUNT); i++) {
        auto button = refs.buttons[i];
        if (!button) {
            continue;
        }
        if (!(_panelButtons & (1 << i))) {
            lv_obj_add_flag(button, LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_clear_flag(button, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(button, layout.backX, buttonY);
        lv_obj_set_size(button, layout.backW, kPanelButtonHeight);
        // the button is round ended and its glyph is centered again after the resize
        lv_obj_set_style_radius(button, static_cast<lv_coord_t>(kPanelButtonHeight / 2), LV_PART_MAIN);
        auto icon = lv_obj_get_child(button, 0);
        if (icon) {
            lv_obj_set_pos(icon, static_cast<lv_coord_t>((layout.backW - LVGLUI::kIconSizeSmall) / 2),
                           static_cast<lv_coord_t>((kPanelButtonHeight - LVGLUI::kIconSizeSmall) / 2));
        }
        buttonY = static_cast<lv_coord_t>(buttonY + kPanelButtonHeight + kPanelButtonGap);
    }
}

bool HassScreen::_expects(ExpectedValue &expected, const char *what, float value, float value2, float tolerance)
{
    if (!expected.active) {
        return false;
    }
    const auto confirmed = (fabsf(value - expected.value) <= tolerance) &&
                           ((value2 < 0) || (fabsf(value2 - expected.value2) <= tolerance));
    if (confirmed || static_cast<int32_t>(millis() - expected.deadline) >= 0) {
        expected.active = false;
        __LDBG_printf("hass> %s: entity reports %.1f (held was %.1f, %s)", what, static_cast<double>(value),
                      static_cast<double>(expected.value), confirmed ? "released" : "timeout");
        return false;
    }
    return true;
}

void HassScreen::_expect(ExpectedValue &expected, const char *what, float value, float value2)
{
    expected.active = true;
    expected.value = value;
    expected.value2 = value2;
    expected.deadline = millis() + kExpectedTimeout;
    __LDBG_printf("hass> %s: user set %.1f (kept until the entity reports it)", what, static_cast<double>(value));
}

void HassScreen::_expectItem(PanelView view, const char *item)
{
    if (!item || !*item) {
        return;
    }
    strncpy(_expectedItem, item, sizeof(_expectedItem) - 1);
    _expectedItem[sizeof(_expectedItem) - 1] = 0;
    _expectedItemView = static_cast<uint8_t>(view);
    _expectedItemTime = millis();
    // the list is rebuilt by the next update, the tapped item is marked right away
    _listView = -1;
    __LDBG_printf("hass> option list marks '%s' until the detail response reports it", _expectedItem);
}

// The item an option list marks: the one the user tapped, until the detail response reports it as
// the active one (the entity took it) or kExpectedTimeout reverts it (an action that failed or was
// ignored). The reported item is the truth in both cases.
const char *HassScreen::_expectedItemOf(PanelView view, const char *reported)
{
    if (!_expectedItem[0] || _expectedItemView != static_cast<uint8_t>(view)) {
        return reported;
    }
    if (!strcasecmp(reported, _expectedItem)) {
        _expectedItem[0] = 0;
        return reported;
    }
    if (static_cast<uint32_t>(millis() - _expectedItemTime) >= kExpectedTimeout) {
        __LDBG_printf("hass> option list '%s' was not confirmed, showing '%s' again", _expectedItem, reported);
        _expectedItem[0] = 0;
        return reported;
    }
    return _expectedItem;
}

// The items of the option list the open panel shows, nullptr while the view has no list. Only the
// panel that is open has a list: the effects of a light, the mode, the preset and the fan mode of a
// climate. The lists are comma separated and point into the buffer of the detail slot of the
// dashboard (see Detail)
const char *HassScreen::_panelListItems() const
{
    const auto &detail = _dashboard.getDetail();
    if (_panel == Panel::CLIMATE) {
        switch (_panelView) {
        case PanelView::OPTION_1:
            return detail.modeList;
        case PanelView::OPTION_2:
            return detail.presetList;
        case PanelView::OPTION_3:
            return detail.fanModeList;
        default:
            break;
        }
    }
    else if (_panelView == PanelView::EFFECTS) {
        return detail.effectList;
    }
    return nullptr;
}

void HassScreen::_buildPanelList()
{
    auto &refs = _panelRefs;
    if (!refs.list || _panelTile < 0) {
        return;
    }
    const auto &detail = _dashboard.getDetail();

    // the list of the view that is shown (the effects of a light, the mode, the preset or the fan
    // mode of a climate) and the item it marks: the one the user tapped until the detail response
    // reports it, else the one the entity reports
    const auto items = _panelListItems();
    const char *current = nullptr;
    switch (_panelView) {
    case PanelView::OPTION_1:
        current = _expectedItemOf(PanelView::OPTION_1, detail.mode);
        break;
    case PanelView::OPTION_2:
        current = _expectedItemOf(PanelView::OPTION_2, detail.preset);
        break;
    case PanelView::OPTION_3:
        current = _expectedItemOf(PanelView::OPTION_3, detail.fanMode);
        break;
    case PanelView::EFFECTS:
        current = _expectedItemOf(PanelView::EFFECTS, detail.effect);
        break;
    default:
        break;
    }
    if (!items || !*items) {
        return;
    }
    // The list is only built again when the items or the marked one changed: the response comes
    // every poll and rebuilding the items each time makes the list flicker
    String content(items);
    content += '|';
    content += current ? current : "";
    if (_listView == static_cast<int8_t>(_panelView) && _listContent == content) {
        return;
    }
    lv_obj_clean(refs.list);

    // The items are buttons on the background of the panel, the list has no card of its own (see
    // _buildPanel()). A list that fits into a single column is drawn as a centered column, a
    // longer one is placed in two columns - like the effects of a dimmer panel - and scrolls.
    const auto borderWidth = lv_obj_get_style_border_width(refs.list, LV_PART_MAIN);
    const auto listWidth = static_cast<lv_coord_t>(lv_obj_get_style_width(refs.list, LV_PART_MAIN) -
                                                   lv_obj_get_style_pad_left(refs.list, LV_PART_MAIN) -
                                                   lv_obj_get_style_pad_right(refs.list, LV_PART_MAIN) -
                                                   2 * borderWidth);
    const auto listHeight = static_cast<lv_coord_t>(lv_obj_get_style_height(refs.list, LV_PART_MAIN) -
                                                    lv_obj_get_style_pad_top(refs.list, LV_PART_MAIN) -
                                                    lv_obj_get_style_pad_bottom(refs.list, LV_PART_MAIN) -
                                                    2 * borderWidth);
    // the rows of items that fit into the list without scrolling (5 on this display)
    const uint8_t rowsFit = static_cast<uint8_t>((listHeight + kListItemGap) / (kListItemHeight + kListItemGap));
    const auto itemCount = countListItems(items);
    const uint8_t columns = (itemCount <= rowsFit) ? 1 : 2;
    // an item keeps its width (half of the list, the width of the effects of a dimmer) when the
    // list is a single column, it is only centered then
    const auto itemWidth = static_cast<lv_coord_t>((listWidth - kListItemGap) / 2);
    const auto gridWidth = static_cast<lv_coord_t>(columns * itemWidth + (columns - 1) * kListItemGap);
    const uint8_t rows = static_cast<uint8_t>((itemCount + columns - 1) / columns);
    const auto gridHeight = static_cast<lv_coord_t>(rows * kListItemHeight + (rows - 1) * kListItemGap);
    // the single column is centered vertically and horizontally, a two column grid that fills the
    // list (or scrolls) starts at the top left corner
    const auto offsetX = static_cast<lv_coord_t>((columns == 1) ? (listWidth - gridWidth) / 2 : 0);
    const auto offsetY = static_cast<lv_coord_t>((columns == 1 && gridHeight < listHeight) ? (listHeight - gridHeight) / 2 : 0);

    uint8_t i = 0;
    lv_obj_t *marked = nullptr;
    lv_coord_t markedY = 0;
    const char *cursor = items;
    const char *begin = nullptr;
    const char *end = nullptr;
    while (nextListItem(cursor, begin, end)) {
        const String item(begin, static_cast<unsigned int>(end - begin));
        const auto col = static_cast<lv_coord_t>(offsetX + (i % columns) * (itemWidth + kListItemGap));
        const auto row = static_cast<lv_coord_t>(offsetY + (i / columns) * (kListItemHeight + kListItemGap));
        auto obj = LVGLUI::addListItem(refs.list, col, row, itemWidth, kListItemHeight, item.c_str());
        const auto active = current && detail.valid && !strcasecmp(current, item.c_str());
        LVGLUI::setListItemActive(obj, active);
        if (active) {
            marked = obj;
            markedY = row;
        }
        lv_obj_add_event_cb(obj, _panelCallback, LV_EVENT_CLICKED, this);
        i++;
    }
    // The list is built again when an item was tapped (the marked one changed) or when the items
    // arrived: the scroll position jumps back to the top then and the marker can end up below the
    // rows that fit - the effects of a dimmer panel are more than one screen. The marked item is
    // scrolled into the middle of the visible rows (LVGL clamps the position to the content, so
    // the first and the last rows stay reachable)
    if (marked) {
        lv_obj_update_layout(refs.list);
        lv_obj_scroll_to(refs.list, 0, static_cast<lv_coord_t>(markedY - (listHeight - kListItemHeight) / 2), LV_ANIM_OFF);
    }
    _listView = static_cast<int8_t>(_panelView);
    _listContent = content;
    _panelDetail = detail.generation;
}

void HassScreen::_updatePanel()
{
    if (_panel == Panel::NONE || _panelTile < 0) {
        return;
    }
    // the sensor panel has none of the controls below, only its header and the graph
    if (_panel == Panel::SENSOR) {
        _updateSensorPanel();
        return;
    }
    const auto index = static_cast<HomeAssistant::TileIndex>(_panelTile);
    const auto &tile = _dashboard.getConfig().getTile(index);
    const auto &value = _dashboard.getValue(index);
    const auto &detail = _dashboard.getDetail();
    // a control the finger holds owns its value until the finger is up (see isTouchPressed())
    const auto touchPressed = isTouchPressed();
    auto &refs = _panelRefs;
    auto &widgets = _widgets(index);
    auto &refsTile = widgets.refs;

    if (_panel == Panel::CLIMATE) {
        _setTextIfChanged(refs.headerValue, PrintString(F("%.1f °C"), static_cast<double>(value.current)),
                          LVGLUI::kFontMedium, LVGLUI::kColorText);
        // the pills show what the entity reports, "--" until the first detail response. The item
        // the user tapped is shown until the detail response confirms it (see _expectedItemOf())
        for (uint8_t i = 0; i < kNumPills; i++) {
            const auto view = static_cast<PanelView>(static_cast<uint8_t>(PanelView::OPTION_1) + i);
            const char *text = nullptr;
            switch (i) {
            case 0:
                text = _expectedItemOf(view, detail.mode);
                break;
            case 1:
                text = _expectedItemOf(view, detail.preset);
                break;
            default:
                text = _expectedItemOf(view, detail.fanMode);
                break;
            }
            _setTextIfChanged(refs.pillValue[i], text[0] ? capitalize(text) : String("--"), LVGLUI::kFontNormal, LVGLUI::kColorText);
        }
        // the arc follows the setpoint while it is not dragged
        if (refsTile.arc) {
            const auto step = arcStepTenths(tile);
            auto minValue = arcValueOf(static_cast<int32_t>(lroundf(value.minTemp * 10)), step);
            auto maxValue = arcValueOf(static_cast<int32_t>(lroundf(value.maxTemp * 10)), step);
            if (tile.min > 0 && tile.max > tile.min) {
                minValue = arcValueOf(static_cast<int32_t>(lroundf(tile.min * 10)), step);
                maxValue = arcValueOf(static_cast<int32_t>(lroundf(tile.max * 10)), step);
            }
            if (maxValue <= minValue) {
                maxValue = static_cast<int16_t>(minValue + 1);
            }
            if (minValue != widgets.arcMin || maxValue != widgets.arcMax) {
                lv_arc_set_range(refsTile.arc, minValue, maxValue);
                widgets.arcMin = minValue;
                widgets.arcMax = maxValue;
                widgets.arcValue = -1;
            }
            // The finger owns the arc while it is dragged: the polled setpoint is not drawn, the big
            // number is written by _arcCallback() and follows the finger. An else branch here that
            // draws the value of the entity made the number flip back to the setpoint of Home
            // Assistant with every refresh - it looked like the arc was not dragging at all
            if (!widgets.arcPressed && !touchPressed) {
                // the setpoint the user stepped or dragged is kept until the entity reports it: the
                // responses that were already in flight carry the setpoint from before the action
                const auto held = _expects(_expectedSetpoint, "setpoint", value.value, -1, kSetpointHoldTolerance);
                const auto setpoint = held ? _expectedSetpoint.value : value.value;
                const auto arcValue = arcValueOf(static_cast<int32_t>(lroundf(setpoint * 10)), step);
                if (arcValue != widgets.arcValue) {
                    __LDBG_printf("hass> arc %u from the response: %.1f (pressed %u, touch %u)", static_cast<unsigned>(index),
                                  static_cast<double>(setpoint), static_cast<unsigned>(widgets.arcPressed), static_cast<unsigned>(touchPressed));
                    LVGLUI::setArcValue(refsTile.arc, arcValue, LVGLUI::kColorActive);
                    widgets.arcValue = arcValue;
                }
                _setTextIfChanged(refsTile.value, (value.mode == 0) ? String("Off") : PrintString(F("%.1f°"), static_cast<double>(setpoint)),
                                  LVGLUI::kFontTitle, LVGLUI::kColorText);
            }
            else {
                // the arc is held: the polled setpoint is ignored (proof for the log that the value
                // of the entity does not reach the readout while the finger is on the arc)
                __LDBG_printf("hass> arc %u held (pressed %u, touch %u), polled %.1f ignored", static_cast<unsigned>(index),
                              static_cast<unsigned>(widgets.arcPressed), static_cast<unsigned>(touchPressed),
                              static_cast<double>(value.value));
            }
            _setTextIfChanged(refsTile.action, climateStateText(value), LVGLUI::kFontNormal, LVGLUI::kColorActive);
        }
    }
    else if (_panel == Panel::DIMMER) {
        // The buttons the entity offers are known from the state poll. Only while that did not
        // arrive yet (the first poll after a boot) the detail response refines them
        uint8_t available = _dashboard.getCapabilities(index);
        if (!available) {
            available = static_cast<uint8_t>(HomeAssistant::kCapPower | HomeAssistant::kCapLevel);
            if (lightHasColor(detail)) {
                available |= HomeAssistant::kCapColor;
            }
            if (lightHasColorTemp(detail)) {
                available |= HomeAssistant::kCapColorTemp;
            }
            if (lightHasEffects(detail)) {
                available |= HomeAssistant::kCapEffects;
            }
        }
        if (available != _panelButtons) {
            _panelButtons = available;
            _layoutPanelButtons();
        }
        // the button of the control on screen is filled, the others are plain cards (the power
        // button keeps its white background)
        static const PanelView kViewOfButton[static_cast<uint8_t>(LightButton::COUNT)] = {
            PanelView::LEVEL, PanelView::LEVEL, PanelView::COLOR, PanelView::COLOR_TEMP, PanelView::EFFECTS,
        };
        for (uint8_t i = 1; i < static_cast<uint8_t>(LightButton::COUNT); i++) {
            auto button = refs.buttons[i];
            if (!button) {
                continue;
            }
            const auto selected = (_panelView == kViewOfButton[i]);
            LVGLUI::setTileState(button, selected ? LVGLUI::TileState::ON : LVGLUI::TileState::OFF, selected);
            LVGLUI::setIconColor(lv_obj_get_child(button, 0), selected ? LVGLUI::kColorText : LVGLUI::kColorTextValue);
        }

        // The power button shows the state of the entity: white with a dark glyph while it is on,
        // a card with a light grey glyph while it is off
        if (auto power = refs.buttons[static_cast<uint8_t>(LightButton::POWER)]) {
            const auto on = (value.state == TileState::ON);
            lv_obj_set_style_bg_color(power, lv_color_hex(on ? LVGLUI::kColorText : LVGLUI::kColorCardAlt), LV_PART_MAIN);
            LVGLUI::setIconColor(lv_obj_get_child(power, 0), on ? LVGLUI::kColorBackground : LVGLUI::kColorTextMuted);
        }

        // Every control follows the entity, but a running drag owns its readout and a control that
        // was just set keeps the value of the user until the entity reports it (the expected value
        // of that control). That value is drawn as well, otherwise the slider, the percentage and
        // the +/- steppers only move when the entity answers (they felt laggy)
        if (!touchPressed && !_controlPressed) {
            const auto held = _expects(_expectedLevel, "level", value.value, -1, kLevelHoldTolerance);
            const auto level = static_cast<int32_t>(lroundf(held ? _expectedLevel.value : value.value));
            if (refs.slider && lv_slider_get_value(refs.slider) != level) {
                lv_slider_set_value(refs.slider, level, LV_ANIM_OFF);
            }
            _setTextIfChanged(refsTile.value, PrintString(F("%d %%"), static_cast<int>(level)), _panelValueFont(), LVGLUI::kColorText);
        }
        if (!touchPressed && !_controlPressed && !_expects(_expectedColor, "color", detail.hue, detail.saturation, 4.0f)) {
            if (refs.wheel && detail.valid) {
                lv_color_hsv_t hsv;
                hsv.h = static_cast<uint16_t>(detail.hue > 0 ? detail.hue : 0);
                hsv.s = static_cast<uint8_t>((detail.saturation * 255.0f) / 100.0f);
                hsv.v = 255;
                lv_colorwheel_set_hsv(refs.wheel, hsv);
            }
        }
        if (!touchPressed && !_controlPressed && !_expects(_expectedTemp, "color temp", detail.colorTemp, -1, 50.0f)) {
            if (refs.tempSlider) {
                auto minTemp = static_cast<int32_t>(detail.minColorTemp);
                auto maxTemp = static_cast<int32_t>(detail.maxColorTemp);
                if (minTemp <= 0) {
                    minTemp = 2000;
                }
                if (maxTemp <= minTemp) {
                    maxTemp = 6500;
                }
                if (lv_slider_get_min_value(refs.tempSlider) != minTemp || lv_slider_get_max_value(refs.tempSlider) != maxTemp) {
                    lv_slider_set_range(refs.tempSlider, minTemp, maxTemp);
                }
                const auto kelvin = static_cast<int32_t>(lroundf(detail.colorTemp));
                if (kelvin >= minTemp && lv_slider_get_value(refs.tempSlider) != kelvin) {
                    lv_slider_set_value(refs.tempSlider, kelvin, LV_ANIM_OFF);
                }
                _setTextIfChanged(refs.label, (kelvin > 0) ? PrintString(F("%d K"), kelvin) : String("--"),
                                  _panelValueFont(), LVGLUI::kColorText);
            }
        }
    }

    // one control at a time on the right side of the panel: the arc, the level slider, the color
    // wheel or the colour temperature slider, replaced by the list of the selected option
    const bool isClimate = (_panel == Panel::CLIMATE);
    const bool listVisible = isClimate ? (_panelView != PanelView::ARC) : (_panelView == PanelView::EFFECTS);
    const auto showArc = isClimate && (_panelView == PanelView::ARC);
    const auto showSlider = !isClimate && (_panelView == PanelView::LEVEL);
    const auto showWheel = !isClimate && (_panelView == PanelView::COLOR);
    const auto showTemp = !isClimate && (_panelView == PanelView::COLOR_TEMP);
    showWidget(refs.list, listVisible);
    showWidget(refs.slider, showSlider);
    showWidget(refs.wheel, showWheel);
    showWidget(refs.tempSlider, showTemp);
    showWidget(refsTile.arc, showArc);
    showWidget(refsTile.action, showArc);
    showWidget(refs.label, showArc || showTemp);
    showWidget(refs.headerValue, showArc);
    // The readout belongs to the control that is selected: the level in percent for the slider of
    // the level, the colour temperature in kelvin for the one of the temperature - and to neither
    // of them for the colour wheel or the list of the effects (the readout of the level would sit
    // in the row the list of the effects starts in, which is the gap between the name and the
    // effects). The readout of a climate (the setpoint) sits inside the arc
    showWidget(refsTile.value, showArc || showSlider);
    showWidget(refsTile.stepUp, showArc || showSlider);
    showWidget(refsTile.stepDown, showArc || showSlider);
    if (listVisible) {
        _buildPanelList();
    }
}

// Fonts the value of a one cell tile is shrunk through while the icon of the configuration has to
// fit into the cell, from the large to the small one (the ladder LVGLUI::setText() uses)
static const lv_font_t *const kTileValueLadder[] = {
    LVGLUI::kFontValue, LVGLUI::kFontTitle, LVGLUI::kFontLarge, LVGLUI::kFontMedium, LVGLUI::kFontNormal, LVGLUI::kFontSmall,
};
constexpr uint8_t kTileValueLadderSize = static_cast<uint8_t>(sizeof(kTileValueLadder) / sizeof(kTileValueLadder[0]));

// Places the icon and the value of a one cell tile: icon, value and name are one centered column,
// see tileBlockGap(). The name is pinned to the bottom edge of the tile and the room above it is
// split into equal gaps. The value keeps the height LVGL gives it (the one of its text) - a height
// of its own clipped the text away (LVGL draws a label inside its box) - only the top edge it is
// aligned to moves. The icon is the one of the configuration, so a value that leaves no room for it
// is drawn with a smaller font instead of dropping the icon - only the smallest font of the ladder
// gives up and returns false, the caller hides the icon then
bool HassScreen::_layoutCenteredColumn(TileRefs &refs, lv_coord_t tileHeight, const String &text)
{
    if (!refs.value) {
        return false;
    }
    const auto width = static_cast<lv_coord_t>(lv_obj_get_style_width(refs.value, LV_PART_MAIN));
    const auto iconSize = tileIconSize(tileHeight);
    auto font = lv_obj_get_style_text_font(refs.value, LV_PART_MAIN);
    auto valueHeight = textBlockHeight(text.c_str(), font, width);
    auto iconFits = (refs.icon != nullptr) && ((iconSize + valueHeight + kTileNameHeight + 2) <= tileHeight);
    if (refs.icon && !iconFits) {
        uint8_t index = 0;
        while (index < kTileValueLadderSize && kTileValueLadder[index] != font) {
            index++;
        }
        for (index++; index < kTileValueLadderSize; index++) {
            const auto candidate = kTileValueLadder[index];
            const auto candidateHeight = textBlockHeight(text.c_str(), candidate, width);
            if ((iconSize + candidateHeight + kTileNameHeight + 2) <= tileHeight) {
                // _setTextIfChanged() set the font of the tile on the text it changed (setText()
                // takes the largest one that fits the width), this one is smaller to leave the room
                // for the icon and the next text change walks the same way again
                lv_obj_set_style_text_font(refs.value, candidate, LV_PART_MAIN);
                valueHeight = candidateHeight;
                iconFits = true;
                break;
            }
        }
    }
    const auto gap = tileBlockGap(tileHeight, iconSize, valueHeight, iconFits);
    if (iconFits && lv_obj_get_style_y(refs.icon, LV_PART_MAIN) != gap) {
        lv_obj_set_y(refs.icon, gap);
    }
    auto valueTop = static_cast<lv_coord_t>(tileHeight - kTileNameHeight - gap - valueHeight);
    if (valueTop < 0) {
        // a value that is taller than the room even with the smallest font (a long combined value
        // wraps into more lines than the cell has): the top edge stops at the tile, the value is
        // drawn inside of it instead of above it, where the label would be clipped away
        valueTop = 0;
    }
    if (lv_obj_get_style_y(refs.value, LV_PART_MAIN) != valueTop) {
        lv_obj_align(refs.value, LV_ALIGN_TOP_LEFT, 4, valueTop);
    }
    return iconFits;
}

void HassScreen::_updateTile(HomeAssistant::TileIndex index)
{
    auto &widgets = _widgets(index);
    auto &refs = widgets.refs;
    if (!refs.tile) {
        return;
    }
    const auto &tile = _dashboard.getConfig().getTile(index);
    const auto &value = _dashboard.getValue(index);

    if (tile.type == TileType::SPACER || tile.type == TileType::AREA) {
        // nothing to read, the name and the icon of an area are static
        return;
    }
    if (tile.type == TileType::PICTURE) {
        // The camera preview is the image itself, its state does not change what the tile shows
        // (a camera that goes away keeps the last frame, the placeholder stays until the first
        // one arrived)
        return;
    }
    // The height of the tile decides the sizes of the icon and of the value (see _buildTile): a
    // short cell draws both of them smaller
    lv_coord_t tileHeight = 0;
    {
        lv_coord_t x;
        lv_coord_t y;
        lv_coord_t w;
        lv_coord_t h;
        _tileGeometry(tile, x, y, w, h);
        tileHeight = h;
    }

    // a dimmer and a climate tile show their state with the level fill or the bars, only a one
    // cell tile is filled with the active color
    // Only a switch, a light or a button shows its active state with a filled tile. A sensor is a
    // readout - a binary sensor reports on/off but the tile is not an actuator, so it stays a plain
    // card (the value and the color of the glyph show the state) and a dimmer/climate tile shows
    // its state with the level fill or with the bars
    const auto filled = (tile.type == TileType::SWITCH || tile.type == TileType::LIGHT || tile.type == TileType::BUTTON);

    // The state of the entity decides the glyph and the colors. A pending action only marks the
    // tile (darker card): it must not change what the entity reports, otherwise the tile flips
    // back and forth while the responses of the requests that are in flight arrive
    auto entityState = LVGLUI::TileState::OFF;
    auto stateColor = LVGLUI::kColorText;
    switch (value.state) {
    case TileState::ON:
        entityState = LVGLUI::TileState::ON;
        break;
    case TileState::UNAVAILABLE:
        entityState = LVGLUI::TileState::UNAVAILABLE;
        stateColor = LVGLUI::kColorError;
        break;
    case TileState::UNKNOWN:
        stateColor = LVGLUI::kColorTextMuted;
        break;
    default:
        break;
    }
    const auto pending = _dashboard.isPending(index);
    auto tileState = entityState;
    if (pending) {
        // the next response confirms the action, until then the tile is marked
        tileState = LVGLUI::TileState::PENDING;
    }

    const auto active = (entityState == LVGLUI::TileState::ON) && filled;
    // Everything the fill and the glyph are built from. The state the entity reports, the mark of
    // an action that is on its way and whether the tile is filled are not independent: a pending
    // tile of an entity that is on is filled and shows the glyph of "on", the same tile of an
    // entity that is off is not. Only the reported state as the key lets a tile keep the fill and
    // the glyph of a state it does not have any more (the entity reports off while an action is on
    // its way: the text says off, the tile stays lit)
    const auto stateKey = static_cast<uint8_t>(entityState) | static_cast<uint8_t>(pending ? 0x10 : 0) |
                          static_cast<uint8_t>(active ? 0x20 : 0);
    if (stateKey != widgets.arcState) {
        // `active` also selects the fill of a pending tile: the state of an action that is on its
        // way was applied to the model right away (see Dashboard::toggle())
        LVGLUI::setTileState(refs.tile, tileState, active);
        widgets.arcState = stateKey;
        // The glyph follows the state of the entity (a switch shows its handle) and the color is
        // white on a filled tile, grey while the entity is off and red while it is unavailable
        __LDBG_printf("hass> draw tile %u '%s': state %u (entity %u, pending %u), active %u", static_cast<unsigned>(index), tile.name,
                      static_cast<unsigned>(tileState), static_cast<unsigned>(entityState), static_cast<unsigned>(pending),
                      static_cast<unsigned>(active));
        if (refs.icon) {
            const auto size = (tile.type == TileType::CLIMATE) ? kTileInlineIcon : tileIconSize(tileHeight);
            LVGLUI::setIcon(refs.icon, tileIconType(tile, value, entityState), size,
                            active ? LVGLUI::kColorText : stateIconColor(entityState));
        }
        // the name of a dimmer sits on the level fill and stays white, the name of a one cell
        // tile is greyed out while it is not active
        if (refs.name && tile.type != TileType::DIMMER) {
            lv_obj_set_style_text_color(refs.name, lv_color_hex(active ? LVGLUI::kColorText : LVGLUI::kColorTextLabel), LV_PART_MAIN);
        }
    }

    switch (tile.type) {
    case TileType::SENSOR:
        {
            // a value with a line break in it (a combined sensor, "25.0 °C\n57.7 %") is drawn on
            // two lines and takes the space below the icon, see setValueLongMode()
            const auto text = formatSensorValue(tile, value);
            _setTextIfChanged(refs.value, text, tileValueFont(tileHeight), active ? LVGLUI::kColorText : stateColor);
            setValueLongMode(refs.value, text.c_str());
            // The icon of the configuration is drawn when the tile is built, the device_class of
            // the entity arrives with the first response. A state change is not what a sensor
            // reports (a reading is "off" like every entity that is not switched on), so the
            // glyph is refreshed here - the block above only follows the states
            if (refs.icon) {
                const auto icon = tileIconType(tile, value, entityState);
                if (!LVGLUI::isIcon(refs.icon, icon)) {
                    LVGLUI::setIcon(refs.icon, icon, tileIconSize(tileHeight), stateIconColor(entityState));
                }
            }
            // The icon is the one of the configuration and the value is shrunk until it fits next to
            // it (see _layoutCenteredColumn()). Only a value that leaves no room even for the
            // smallest font drops the icon, the value and the name use the whole cell then, like
            // `icon: none`
            setTileIconVisible(refs.icon, _layoutCenteredColumn(refs, tileHeight, text));
        }
        break;

    case TileType::DIMMER:
        {
            // The level the user dragged or stepped is kept until the entity reports it (a response
            // that is in flight carries the level from before the action) and it is drawn right
            // away: waiting for the response of the service call feels laggy
            const auto held = _expects(_expectedLevel, "level", value.value, -1, kLevelHoldTolerance);
            const auto level = static_cast<int>(lroundf(held ? _expectedLevel.value : value.value));
            const auto width = static_cast<lv_coord_t>(lv_obj_get_style_width(refs.tile, LV_PART_MAIN));
            const auto height = static_cast<lv_coord_t>(lv_obj_get_style_height(refs.tile, LV_PART_MAIN));
            LVGLUI::setLevelFill(refs.fill, 0, 0, width, height, static_cast<uint8_t>(level), LVGLUI::kColorActive);
            _setTextIfChanged(refs.value, PrintString(F("%d %%"), level), LVGLUI::kFontValue, LVGLUI::kColorText);
        }
        break;

    case TileType::CLIMATE:
        // an entity that is off does not report a setpoint, the tile shows the word instead of
        // the "0.0 °C" of the missing attribute
        if (value.mode == 0) {
            _setTextIfChanged(refs.value, String("Off"), tileValueFont(tileHeight), LVGLUI::kColorText);
        }
        else {
            // the setpoint the user stepped is kept until the entity reports it
            const auto held = _expects(_expectedSetpoint, "setpoint", value.value, -1, kSetpointHoldTolerance);
            _setTextIfChanged(refs.value, PrintString(F("%.1f °C"), static_cast<double>(held ? _expectedSetpoint.value : value.value)),
                              tileValueFont(tileHeight), LVGLUI::kColorText);
        }
        _setTextIfChanged(refs.action, climateStateText(value), LVGLUI::kFontNormal, LVGLUI::kColorTextValue);
        _setTextIfChanged(refs.state, PrintString(F("%.1f °C"), static_cast<double>(value.current)), LVGLUI::kFontMedium, LVGLUI::kColorTextValue);
        break;

    default:
        // switch, light and button only carry the state
        {
            const auto text = (tile.type == TileType::BUTTON) ? String() : String(stateText(value.state));
            _setTextIfChanged(refs.value, text, tileValueFont(tileHeight), stateColor);
            setValueLongMode(refs.value, text.c_str());
            setTileIconVisible(refs.icon, _layoutCenteredColumn(refs, tileHeight, text));
        }
        break;
    }
}

void HassScreen::update()
{
    // The configured orientation changed while the dashboard is on screen (the form was saved):
    // switch the display and rebuild the whole widget tree for the new size. A rebuild outside an
    // LVGL event callback, the same rule the page and the panels follow
    if (_orientationPending) {
        _applyOrientation();
        _rebuild();
    }
#if DEBUG_HASS_ACTION_TEST
    _actionTest();
#endif
    _dashboard.update();

#if DEBUG_LVGL_SCREENSHOT
    // The debug screen of the lvgl plugin can switch screens but not pages, so a page is opened
    // by pushing it (see /lvgl-screen and the setValue callback of the plugin)
    const auto debugPage = _data.getDebugHassPage();
    if (debugPage != 0xffffffff) {
        _data.clearDebugHassPage();
        __LDBG_printf("hass> debug page %u requested", static_cast<unsigned>(debugPage));
        _pendingPage = static_cast<int32_t>(debugPage);
    }
    // ... and a tap on a picture tile cannot be reached over the network either: the same key
    // opens the fullscreen image of that tile and closes it again
    const auto debugFullscreen = _data.getDebugHassFullscreen();
    if (debugFullscreen != 0xffffffff) {
        _data.clearDebugHassFullscreen();
        __LDBG_printf("hass> debug fullscreen image of tile %u requested", static_cast<unsigned>(debugFullscreen));
        _pendingFullscreen = static_cast<int32_t>(debugFullscreen);
    }
    // a tap on a light/dimmer/climate/sensor tile opens its panel, "hasspanel:<tile>" does the same.
    // The tile whose panel is already open closes it again (like the fullscreen image above)
    const auto debugPanel = _data.getDebugHassPanel();
    if (debugPanel != 0xffffffff) {
        _data.clearDebugHassPanel();
        if (_panel != Panel::NONE && _panelTile == static_cast<int32_t>(debugPanel)) {
            __LDBG_printf("hass> debug panel of tile %u closes it", static_cast<unsigned>(debugPanel));
            _pendingPanelClose = true;
        }
        else {
            __LDBG_printf("hass> debug panel of tile %u requested", static_cast<unsigned>(debugPanel));
            _pendingPanel = static_cast<int32_t>(debugPanel);
        }
    }
    // the control a panel shows is selected with the buttons of the panel, "hassview:<n>" does the
    // same (0 = the level slider of a light, the arc of a climate, 1..3 = the color wheel, the
    // colour temperature and the effect list / the option lists of a climate). Applied after the
    // panel handling below, so a panel that is opened in the same tick already shows it
    const auto debugView = _data.getDebugHassView();
    if (debugView != 0xff) {
        _data.clearDebugHassView();
        __LDBG_printf("hass> debug view %u requested", static_cast<unsigned>(debugView));
        _pendingPanelView = static_cast<int8_t>(debugView);
    }
    // The range of the history graph of the sensor panel is selected with the buttons of the panel,
    // "hassrange:<hours>" does the same (the buttons cannot be reached over the network)
    const auto debugRange = _data.getDebugHassRange();
    if (debugRange != 0xff) {
        _data.clearDebugHassRange();
        const auto range = statsRangeOf(static_cast<uint8_t>(debugRange));
        if (statsRangeHours(range) == debugRange) {
            __LDBG_printf("hass> debug range %u requested", static_cast<unsigned>(debugRange));
            _pendingStatsRange = static_cast<int8_t>(range);
        }
        else {
            __LDBG_printf("hass> debug range %u is not a range of the panel (12, 24 or 48)", static_cast<unsigned>(debugRange));
        }
    }
    // The quick settings sheet is opened with a swipe, "hasssettings:<view>" does the same
    // (0 = close it, 1 = the tiles, 2..4 = the editor of a setting)
    const auto debugSettings = _data.getDebugHassSettings();
    if (debugSettings != 0xff) {
        _data.clearDebugHassSettings();
        if (debugSettings == 0) {
            __LDBG_printf("hass> debug: quick settings closed");
            _settingsAction = SettingsAction::CLOSE;
        }
        else {
            __LDBG_printf("hass> debug: quick settings view %u requested", static_cast<unsigned>(debugSettings));
            _settingsPending = true;
            _settingsPendingView = static_cast<int8_t>(debugSettings - 1);
        }
    }
    // The orientation of the dashboard is a stored setting (a change from the web form needs a
    // reboot), "hassrot:<0..3>" applies it right away. The value is not stored, a reboot restores
    // the configured orientation
    const auto debugRotation = _data.getDebugHassRotation();
    if (debugRotation != 0xff) {
        _data.clearDebugHassRotation();
        __LDBG_printf("hass> debug: orientation %u requested", static_cast<unsigned>(debugRotation));
        setOrientation(debugRotation);
        // applied here, so the grid and the sheet are built for the new size in this update()
        _applyOrientation();
        _rebuild();
    }
#endif

    // A new camera image of a picture tile (the request task decodes it in the background). The
    // frame is handed over with its PSRAM buffer: the image points at the new one before the
    // buffer of the previous frame is released, an lv_img must never point at freed pixels
    HomeAssistant::TileIndex imageTile = 0;
    uint16_t *imageData = nullptr;
    uint16_t imageWidth = 0;
    uint16_t imageHeight = 0;
    uint32_t imageStamp = 0;
    while (_dashboard.takeImage(imageTile, imageData, imageWidth, imageHeight, imageStamp)) {
        // the preview of the tile the image belongs to (a tile of a page that is not shown has
        // none, the frame is dropped)
        auto picture = _picture(imageTile);
        // The frame is for the fullscreen image of the tile (its box is the display) or for the
        // tile itself. A frame of a box the tile does not have any more is dropped: the box
        // changed while the request was on its way (the fullscreen image was closed, or the page
        // was built again)
        lv_obj_t *target = nullptr;
        if (picture && _fullTile == static_cast<int32_t>(imageTile)) {
            target = _fullImage;
        }
        else if (picture && picture->image && imageWidth == picture->width && imageHeight == picture->height) {
            target = picture->image;
        }
        if (!picture || !imageData || !target) {
            free(imageData);
            continue;
        }
        auto previous = picture->buffer;
        picture->buffer = imageData;
        picture->dsc.header.always_zero = 0;
        picture->dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
        picture->dsc.header.w = imageWidth;
        picture->dsc.header.h = imageHeight;
        picture->dsc.data_size = static_cast<uint32_t>(imageWidth) * imageHeight * sizeof(lv_color_t);
        picture->dsc.data = reinterpret_cast<const uint8_t *>(imageData);
        // the pixels changed but the descriptor did not: the cached decoder entry of this source
        // would keep drawing the released buffer of the previous frame
        lv_img_cache_invalidate_src(&picture->dsc);
        lv_img_set_src(target, &picture->dsc);
        if (target == _fullImage) {
            // the frame of the display fills it, a tile sized frame that is still on its way
            // is centered
            lv_obj_set_size(target, imageWidth, imageHeight);
            lv_obj_center(target);
        }
        else {
            lv_obj_clear_flag(target, LV_OBJ_FLAG_HIDDEN);
            showWidget(_widgets(imageTile).refs.value, false);
        }
        if (previous) {
            free(previous);
        }
        __LDBG_printf("hass> image of tile %u shown (%ux%u, stamp %u)", static_cast<unsigned>(imageTile),
                      static_cast<unsigned>(imageWidth), static_cast<unsigned>(imageHeight), static_cast<unsigned>(imageStamp));
    }

    // The tap window of a tile passed without a second tap: the panel of the entity opens now (a
    // double tap switched the entity instead, see _tapTile())
    if (_pendingTileTap >= 0 && static_cast<uint32_t>(millis() - _pendingTileTapTime) > kTileDoubleTapTime) {
        _pendingPanel = _pendingTileTap;
        _pendingTileTap = -1;
    }

    // a tile opened a panel or the back tile of a panel closed it. The tree is rebuilt here,
    // never inside an LVGL event callback
    if (_pendingPanelClose) {
        _pendingPanelClose = false;
        if (_panel != Panel::NONE) {
            _panel = Panel::NONE;
            _panelTile = -1;
            _dashboard.closeDetail();
            _dashboard.closeStats();
            _rebuild();
        }
    }
    else if (_pendingPanel >= 0) {
        const auto index = static_cast<uint8_t>(_pendingPanel);
        _pendingPanel = -1;
        if (index < _dashboard.getTileCount()) {
            const auto type = _dashboard.getConfig().getTile(index).type;
            _panel = (type == TileType::CLIMATE) ? Panel::CLIMATE : (type == TileType::SENSOR) ? Panel::SENSOR : Panel::DIMMER;
            _panelTile = static_cast<int32_t>(index);
            // a light panel starts with the level, the climate panel with the arc, the sensor panel
            // with a 24 hour history
            _panelView = (type == TileType::CLIMATE) ? PanelView::ARC : PanelView::LEVEL;
            _listView = -1;
            _panelDetail = 0;
            _panelButtons = 0;
            if (type == TileType::SENSOR) {
                // The live value of the entity comes from the state poll of the screen, so the
                // panel does not request the detail attributes of its entity (the graph is a
                // request of its own, see Dashboard::requestStats()). The attributes of a panel
                // that is left are dropped by closeDetail() and closeStats() of the tile that
                // opens the next one
                _statsHours = HomeAssistant::Dashboard::kStatsDefaultHours;
                // a range that was selected in the same tick (the debug key sets the panel and the
                // range together) is used right away, otherwise opening the panel would drop it
                if (_pendingStatsRange >= 0) {
                    _statsHours = statsRangeHours(static_cast<StatsRange>(_pendingStatsRange));
                    _pendingStatsRange = -1;
                }
                _dashboard.closeDetail();
                _dashboard.requestStats(index, _statsHours);
            }
            else {
                _dashboard.closeStats();
                _dashboard.requestDetail(index);
            }
            _rebuild();
        }
    }

    // the range of the history graph: the answer of the new window is requested right away (the
    // tree is rebuilt, the graph has a column per bucket of the range)
    if (_pendingStatsRange >= 0) {
        const auto range = static_cast<StatsRange>(_pendingStatsRange);
        _pendingStatsRange = -1;
        if (_panel == Panel::SENSOR && _panelTile >= 0) {
            _statsHours = statsRangeHours(range);
            _dashboard.requestStats(static_cast<HomeAssistant::TileIndex>(_panelTile), _statsHours);
            _rebuild();
        }
    }

    // The control a panel shows is selected with the buttons of the panel, "hassview" records the
    // same request. Applied after the panel was opened above, so a request that opens a panel and
    // selects a control in one tick ends on that control
    if (_pendingPanelView >= 0) {
        const auto requested = static_cast<uint8_t>(_pendingPanelView);
        _pendingPanelView = -1;
        if (_panel == Panel::CLIMATE || _panel == Panel::DIMMER) {
            const auto base = (_panel == Panel::CLIMATE) ? PanelView::ARC : PanelView::LEVEL;
            if (requested < 4) {
                _panelView = static_cast<PanelView>(static_cast<uint8_t>(base) + requested);
                _listView = -1;
                __LDBG_printf("hass> panel of tile %i shows control %u", static_cast<int>(_panelTile), static_cast<unsigned>(requested));
            }
        }
    }

    // an area tile only records the page it wants, the tree is rebuilt here (LVGL events are
    // dispatched by the same task, a rebuild inside a callback would delete the event target)
    if (_pendingPage >= 0) {
        const auto page = static_cast<uint8_t>(_pendingPage);
        _pendingPage = -1;
        _showPage(page);
    }

    // A tap on a picture tile opens the image fullscreen, a tap or a swipe on it closes it again.
    // The overlay and the tile that is behind it are only shown and hidden here, never inside an
    // LVGL event callback
    if (_pendingFullscreen >= 0) {
        const auto index = static_cast<uint8_t>(_pendingFullscreen);
        _pendingFullscreen = -1;
        // a tap cannot reach the image of a tile that is already open (the overlay covers the
        // grid), so this is the debug key of that tile: it closes the image again
        if (_fullTile == static_cast<int32_t>(index)) {
            _closeFullscreen();
        }
        else {
            _openFullscreen(index);
        }
    }
    else if (_pendingFullscreenClose) {
        _pendingFullscreenClose = false;
        _closeFullscreen();
    }

    // the file disappeared, or a new version was uploaded (a different configuration needs a new
    // widget tree, the tile of the old one does not match the new list)
    if (_dashboard.isLoaded() != _configLoaded || _dashboard.getConfigGeneration() != _configGeneration) {
        _rebuild();
    }

    // Quick settings: the swipe only records the request, the tree is built here (LVGL must not be
    // touched from inside the gesture of the screen manager, which calls onSwipe()). The view, the
    // action and the rebuild after an orientation change are applied the same way
    if (_settingsPending) {
        _settingsPending = false;
        _openSettings();
    }
    if (_settingsPendingView >= 0) {
        const auto view = static_cast<SettingsView>(_settingsPendingView);
        _settingsPendingView = -1;
        if (_settingsOpen) {
            _showSettingsView(view);
        }
    }
    if (_settingsAction != SettingsAction::NONE) {
        _applySettingsAction();
    }

    const auto status = _dashboard.getScreenStatus();
    if (status.length() && !_settingsOpen) {
        // a note or an error is drawn over the first row of tiles; the sheet covers the display, so
        // it is hidden while it is open (it would show through the layer below it)
        lv_obj_clear_flag(_status, LV_OBJ_FLAG_HIDDEN);
        _setTextIfChanged(_status, status, LVGLUI::kFontSmall, LVGLUI::kColorAlert);
    }
    else {
        lv_obj_add_flag(_status, LV_OBJ_FLAG_HIDDEN);
    }

    if (!_configLoaded) {
        // the message may have changed (another error, or the file appeared)
        _setTextIfChanged(_message, _dashboard.getScreenStatus(), LVGLUI::kFontMedium, LVGLUI::kColorAlert);
        return;
    }
    if (_panel != Panel::NONE) {
        _updatePanel();
        return;
    }
    // the sheet is drawn over the grid, the tiles behind it keep their values
    if (_settingsOpen) {
        _updateSettings();
    }
    // only the tiles of the page that is built have widgets (the model may hold many more)
    for (auto &entry : _tiles) {
        _updateTile(entry.globalTile);
    }
}

// ------------------------------------------------------------------------------------------
// quick settings
//
// The sheet is an overlay of the screen: it is created as a child of the object the grid lives in
// (like the fullscreen image of a picture tile), so a rebuild of the grid - a page change or a new
// configuration - does not remove it and the tiles behind it keep their values. The tree is built
// for one view at a time and a view change deletes it again, which keeps the widgets and the
// pointers that have to be tracked small. The reviewed layout is
// docs/hass_layout/quick_settings_preview.html
// ------------------------------------------------------------------------------------------
void HassScreen::_openSettings()
{
    if (_settingsOpen || !_grid) {
        return;
    }
    _settingsOpen = true;
    _settingsView = SettingsView::MAIN;
    __LDBG_printf("hass> quick settings opened");
    _buildSettings();
}

void HassScreen::_closeSettings()
{
    if (!_settingsOpen) {
        return;
    }
    _settingsOpen = false;
    _settingsPending = false;
    _settingsPendingView = -1;
    _settingsAction = SettingsAction::NONE;
    if (_settingsRefs.root) {
        lv_obj_del(_settingsRefs.root);
    }
    _settingsRefs = SettingsRefs();
    __LDBG_printf("hass> quick settings closed");
}

// label of a tile of the sheet. The value is on the tile, the label tells what it is
const char *HassScreen::_settingsTitle(SettingsTile tile)
{
    switch (tile) {
    case SettingsTile::IDLE_BRIGHTNESS:
        return "Idle brightness";
    case SettingsTile::IDLE_TIMEOUT:
        return "Idle timeout";
    case SettingsTile::STANDBY_TIMEOUT:
        return "Standby timeout";
    case SettingsTile::ROTATE:
        return "Rotate";
    case SettingsTile::ROTATION_LOCK:
        return "Rotation lock";
    default:
        break;
    }
    return "Display sleep";
}

// the tile of the rotation is drawn with LVGLUI::createRotateIcon() (the MDI subset fonts have no
// glyph for a circular arrow), every other tile uses this one
LVGLUI::IconType HassScreen::_settingsIcon(SettingsTile tile)
{
    switch (tile) {
    case SettingsTile::IDLE_BRIGHTNESS:
        return LVGLUI::IconType::BRIGHTNESS;
    case SettingsTile::IDLE_TIMEOUT:
        return LVGLUI::IconType::CLOCK;
    case SettingsTile::STANDBY_TIMEOUT:
        return LVGLUI::IconType::POWER_SYMBOL;
    case SettingsTile::ROTATION_LOCK:
        return LVGLUI::IconType::LOCK;
    case SettingsTile::SLEEP:
        return LVGLUI::IconType::POWER;
    default:
        break;
    }
    return LVGLUI::IconType::GLOBE;
}

// value of a setting as it is shown on its tile and in its editor
String HassScreen::_settingsValue(SettingsTile tile)
{
    switch (tile) {
    case SettingsTile::IDLE_BRIGHTNESS:
        return PrintString(F("%u %%"), static_cast<unsigned>(LVGLPlugin::getPowerSavingLevel()));
    case SettingsTile::IDLE_TIMEOUT:
        return formatTimeout(LVGLPlugin::getPowerSavingTimeout());
    case SettingsTile::STANDBY_TIMEOUT:
        return formatTimeout(LVGLPlugin::getStandbyTimeout());
    case SettingsTile::ROTATE:
        return String(rotationName(Plugins::WeatherStation::getHassRotation()));
    case SettingsTile::ROTATION_LOCK:
        return String(Plugins::WeatherStation::getHassRotationLock() ? "On" : "Off");
    default:
        break;
    }
    return String();
}

uint8_t HassScreen::_settingsTileAt(const lv_obj_t *object) const
{
    for (uint8_t i = 0; i < kSettingsTiles; i++) {
        const auto tile = _settingsRefs.tiles[i];
        // the tile itself or one of its children (the icon container is cleared, a callback that
        // arrives on it anyway must not be treated as a tap beside the tiles)
        if (tile && (object == tile || lv_obj_get_parent(object) == tile)) {
            return i;
        }
    }
    return kSettingsTiles;
}

void HassScreen::_showSettingsView(SettingsView view)
{
    _settingsView = view;
    __LDBG_printf("hass> quick settings view %u", static_cast<unsigned>(view));
    _settingsUpdate = 0;
    _buildSettings();
}

void HassScreen::_buildSettings()
{
    if (!_settingsOpen || !_grid) {
        return;
    }
    // the tree of the view that was shown before is dropped, the sheet is built for one view only
    if (_settingsRefs.root) {
        lv_obj_del(_settingsRefs.root);
    }
    _settingsRefs = SettingsRefs();
    auto parent = lv_obj_get_parent(_grid);
    if (!parent) {
        return;
    }
    const auto g = settingsGeometry(_width, _height, _portrait, kSettingsTiles);
    auto &refs = _settingsRefs;

    // The layer below the sheet is clickable, so a tap that misses a control does not reach the
    // tiles of the grid (LVGL does not bubble a click to a covered sibling)
    refs.root = LVGLUI::createContainer(parent, 0, 0, _width, _height);
    lv_obj_set_style_bg_color(refs.root, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(refs.root, LV_OPA_60, LV_PART_MAIN);
    lv_obj_add_event_cb(refs.root, _settingsCallback, LV_EVENT_CLICKED, this);

    // The panel: three rows inside a 1 px border, clipped to the rounded corners. The content row
    // has a darker fill than the two rows around it, the rows are separated by 1 px dividers
    refs.panel = LVGLUI::createContainer(refs.root, g.panelX, g.panelY, g.panelW, g.panelH);
    lv_obj_set_style_bg_color(refs.panel, lv_color_hex(LVGLUI::kColorCard), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(refs.panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_side(refs.panel, LV_BORDER_SIDE_FULL, LV_PART_MAIN);
    lv_obj_set_style_border_color(refs.panel, lv_color_hex(LVGLUI::kColorBorder), LV_PART_MAIN);
    lv_obj_set_style_border_width(refs.panel, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(refs.panel, LVGLUI::kCardRadius, LV_PART_MAIN);
    lv_obj_set_style_clip_corner(refs.panel, true, LV_PART_MAIN);

    // header row: the clock, the date, the WiFi signal and the cell that closes the sheet
    refs.top = LVGLUI::createContainer(refs.panel, 0, 0, g.rowW, g.topH);
    lv_obj_set_style_border_side(refs.top, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
    lv_obj_set_style_border_color(refs.top, lv_color_hex(LVGLUI::kColorBorder), LV_PART_MAIN);
    lv_obj_set_style_border_width(refs.top, 1, LV_PART_MAIN);
    refs.time = LVGLUI::addLabel(refs.top, g.timeX, g.timeY, "", LVGLUI::kFontHuge, LVGLUI::kColorText, kSettingsTimeWidth);
    refs.date = LVGLUI::addLabel(refs.top, g.dateX, g.dateY, "", LVGLUI::kFontSmall, LVGLUI::kColorTextLabel,
                                 static_cast<lv_coord_t>(g.closeX - kSettingsSignalGap - g.dateX));
    // the glyph of the WiFi state, it is redrawn when the state changes (see _updateSettings())
    _settingsSignal = static_cast<uint8_t>(settingsSignalIcon());
    refs.signal = LVGLUI::createIcon(refs.top, static_cast<LVGLUI::IconType>(_settingsSignal), g.signalX, g.signalY,
                                    LVGLUI::kIconSizeSmall);
    LVGLUI::clearClickable(refs.signal);
    // the whole cell is the hit area of the close button, the divider is its left border
    refs.close = LVGLUI::createContainer(refs.top, g.closeX, 0, g.closeSize, g.topH);
    lv_obj_set_style_border_side(refs.close, LV_BORDER_SIDE_LEFT, LV_PART_MAIN);
    lv_obj_set_style_border_color(refs.close, lv_color_hex(LVGLUI::kColorBorder), LV_PART_MAIN);
    lv_obj_set_style_border_width(refs.close, 1, LV_PART_MAIN);
    lv_obj_add_event_cb(refs.close, _settingsCallback, LV_EVENT_CLICKED, this);
    auto closeLabel = LVGLUI::addLabel(refs.close, 0, 0, "X", LVGLUI::kFontLarge, LVGLUI::kColorText);
    lv_obj_center(closeLabel);

    // content row
    refs.center = LVGLUI::createContainer(refs.panel, 0, g.centerY, g.rowW, g.centerH);
    lv_obj_set_style_bg_color(refs.center, lv_color_hex(LVGLUI::kColorCardAlt), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(refs.center, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_side(refs.center, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
    lv_obj_set_style_border_color(refs.center, lv_color_hex(LVGLUI::kColorBorder), LV_PART_MAIN);
    lv_obj_set_style_border_width(refs.center, 1, LV_PART_MAIN);

    // the row at the bottom leaves the screen, or closes the editor
    refs.action = LVGLUI::createContainer(refs.panel, 0, g.bottomY, g.rowW, g.bottomH);
    lv_obj_add_event_cb(refs.action, _settingsCallback, LV_EVENT_CLICKED, this);
    refs.actionLabel = LVGLUI::addLabel(refs.action, 0, 0, "", LVGLUI::kFontSmall, LVGLUI::kColorText, g.rowW, LV_TEXT_ALIGN_CENTER);
    lv_obj_center(refs.actionLabel);

    if (_settingsView == SettingsView::MAIN) {
        LVGLUI::setText(refs.actionLabel, "LEAVE HOME ASSISTANT CONTROL", LVGLUI::kFontSmall, LVGLUI::kColorText);

        // screen brightness: the slider follows the finger, the value is stored when it is released
        LVGLUI::addLabel(refs.center, g.brightLabelX, g.brightLabelY, "Brightness", LVGLUI::kFontSmall,
                         LVGLUI::kColorTextLabel, g.brightLabelW);
        refs.brightnessSlider = createPanelSlider(refs.center, g.brightSliderX, g.brightSliderY, g.brightSliderW,
                                                  kSettingsSliderHeight, 0, 100, _settingsCallback, this);
        lv_slider_set_value(refs.brightnessSlider, LVGLPlugin::getConfiguredBrightness(), LV_ANIM_OFF);
        // the track is darker than the content row it sits on (the panel slider is built for a card)
        lv_obj_set_style_bg_color(refs.brightnessSlider, lv_color_hex(LVGLUI::kColorBackground), LV_PART_MAIN);
        refs.brightnessValue = LVGLUI::addLabel(refs.center, g.brightValueX, g.brightValueY, "", LVGLUI::kFontNormal,
                                                LVGLUI::kColorText, g.brightValueW, LV_TEXT_ALIGN_RIGHT);

        // one tile per setting, laid out in the columns of the orientation
        for (uint8_t i = 0; i < kSettingsTiles; i++) {
            const auto tile = static_cast<SettingsTile>(i);
            const auto col = static_cast<lv_coord_t>(i % g.tileCols);
            const auto row = static_cast<lv_coord_t>(i / g.tileCols);
            const auto x = static_cast<lv_coord_t>(g.tilesX + col * (g.tileW + kSettingsTileGap));
            const auto y = static_cast<lv_coord_t>(g.tilesY + row * (g.tileH + kSettingsTileGap));
            const auto iconX = static_cast<lv_coord_t>((g.tileW - LVGLUI::kIconSizeSmall) / 2);
            const auto withValue = (tile != SettingsTile::SLEEP);
            lv_coord_t iconTop;
            lv_coord_t valueTop;
            lv_coord_t labelTop;
            settingsTileTops(g, withValue, iconTop, valueTop, labelTop);
            refs.tiles[i] = LVGLUI::createTile(refs.center, x, y, g.tileW, g.tileH);
            lv_obj_add_event_cb(refs.tiles[i], _settingsCallback, LV_EVENT_CLICKED, this);
            refs.tileIcons[i] = (tile == SettingsTile::ROTATE)
                                    ? LVGLUI::createRotateIcon(refs.tiles[i], iconX, iconTop, LVGLUI::kIconSizeSmall)
                                    : LVGLUI::createIcon(refs.tiles[i], _settingsIcon(tile), iconX, iconTop, LVGLUI::kIconSizeSmall);
            LVGLUI::clearClickable(refs.tileIcons[i]);
            if (withValue) {
                refs.tileValues[i] = LVGLUI::addLabel(refs.tiles[i], 4, valueTop, "", LVGLUI::kFontMedium,
                                                      LVGLUI::kColorText, static_cast<lv_coord_t>(g.tileW - 8), LV_TEXT_ALIGN_CENTER);
            }
            refs.tileLabels[i] = LVGLUI::addLabel(refs.tiles[i], 4, labelTop, _settingsTitle(tile),
                                                  LVGLUI::kFontSmall, LVGLUI::kColorTextLabel,
                                                  static_cast<lv_coord_t>(g.tileW - 8), LV_TEXT_ALIGN_CENTER);
        }
    }
    else {
        // editor of one setting: the title, the big value and the control
        LVGLUI::setText(refs.actionLabel, "DONE", LVGLUI::kFontSmall, LVGLUI::kColorText);
        const auto tile = _settingsTileOfView();
        refs.editTitle = LVGLUI::addLabel(refs.center, g.editTitleX, g.editTitleY, _settingsTitle(tile),
                                          LVGLUI::kFontSmall, LVGLUI::kColorTextMuted);
        refs.editValue = LVGLUI::addLabel(refs.center, g.editControlX, g.editValueY, "", LVGLUI::kFontHuge,
                                          LVGLUI::kColorText, g.editControlW, LV_TEXT_ALIGN_CENTER);
        if (_settingsView == SettingsView::IDLE_BRIGHTNESS) {
            // a level is set with a slider
            const auto sliderY = static_cast<lv_coord_t>(g.editControlY + (g.editControlH - kSettingsSliderHeight) / 2);
            refs.editSlider = createPanelSlider(refs.center, g.editControlX, sliderY, g.editControlW, kSettingsSliderHeight,
                                                1, 100, _settingsCallback, this);
            lv_slider_set_value(refs.editSlider, LVGLPlugin::getPowerSavingLevel(), LV_ANIM_OFF);
            lv_obj_set_style_bg_color(refs.editSlider, lv_color_hex(LVGLUI::kColorBackground), LV_PART_MAIN);
        }
        else {
            // a timeout steps through its scale with two round buttons (a slider cannot hit a second)
            refs.editDown = createStepButton(refs.center, g.editControlX, g.editControlY, LVGLUI::IconType::MINUS);
            refs.editUp = createStepButton(refs.center, static_cast<lv_coord_t>(g.editControlX + g.editControlW - g.editControlH),
                                           g.editControlY, LVGLUI::IconType::PLUS);
            lv_obj_add_event_cb(refs.editDown, _settingsCallback, LV_EVENT_CLICKED, this);
            lv_obj_add_event_cb(refs.editUp, _settingsCallback, LV_EVENT_CLICKED, this);
        }
    }
    // the tree is new, every label is written by the first refresh
    _settingsUpdate = 0;
    _updateSettings();
    lv_obj_move_foreground(refs.root);
}

void HassScreen::_updateSettings()
{
    if (!_settingsOpen || !_settingsRefs.root) {
        return;
    }
    const auto now = millis();
    if (_settingsUpdate && static_cast<uint32_t>(now - _settingsUpdate) < 1000) {
        return;
    }
    _settingsUpdate = now;
    auto &refs = _settingsRefs;

    // the clock, the date and the WiFi signal of the header
    String date;
    String time;
    String zone;
    LVGLUI::formatClock(_data.isTimeFormat24h(), date, time, zone);
    _setTextIfChanged(refs.time, time, LVGLUI::kFontHuge, LVGLUI::kColorText);
    // the date is drawn in upper case, like the reviewed layout
    date.toUpperCase();
    _setTextIfChanged(refs.date, date, LVGLUI::kFontSmall, LVGLUI::kColorTextLabel);
    // The glyph tells the state of the WiFi interface, there is no signal strength reading. The
    // state changes rarely, so the icon is only redrawn when the glyph differs
    const auto signal = static_cast<uint8_t>(settingsSignalIcon());
    if (refs.signal && signal != _settingsSignal) {
        _settingsSignal = signal;
        LVGLUI::setIcon(refs.signal, static_cast<LVGLUI::IconType>(signal), LVGLUI::kIconSizeSmall);
    }

    if (_settingsView != SettingsView::MAIN) {
        // the editor shows the value of the setting it edits
        _setTextIfChanged(refs.editValue, _settingsValue(_settingsTileOfView()), LVGLUI::kFontHuge, LVGLUI::kColorText);
        return;
    }

    // The readout of the brightness slider. Not while the finger is on the slider: the slider
    // writes the label itself then (the value of the configuration is the one from before the drag)
    if (!isTouchPressed()) {
        _setTextIfChanged(refs.brightnessValue, PrintString(F("%u %%"), static_cast<unsigned>(LVGLPlugin::getConfiguredBrightness())),
                          LVGLUI::kFontNormal, LVGLUI::kColorText);
    }

    // the values of the tiles, and the fill of the rotation lock while it is set. A filled tile
    // draws its texts dark, like an active tile of the dashboard
    const auto rotationLocked = Plugins::WeatherStation::getHassRotationLock();
    for (uint8_t i = 0; i < kSettingsTiles; i++) {
        const auto tile = static_cast<SettingsTile>(i);
        const auto active = (tile == SettingsTile::ROTATION_LOCK) && rotationLocked;
        auto valueColor = LVGLUI::kColorText;
        auto labelColor = LVGLUI::kColorTextLabel;
        if (active) {
            valueColor = LVGLUI::kColorBackground;
            labelColor = LVGLUI::kColorBackground;
        }
        _setTextIfChanged(refs.tileValues[i], _settingsValue(tile), LVGLUI::kFontMedium, valueColor);
        if (refs.tileLabels[i]) {
            lv_obj_set_style_text_color(refs.tileLabels[i], lv_color_hex(labelColor), LV_PART_MAIN);
        }
        if (refs.tiles[i]) {
            LVGLUI::setTileState(refs.tiles[i], active ? LVGLUI::TileState::ON : LVGLUI::TileState::OFF, active);
        }
        if (refs.tileIcons[i]) {
            LVGLUI::setIconColor(refs.tileIcons[i], active ? LVGLUI::kColorBackground : LVGLUI::kColorTextValue);
        }
    }
}

// the tile of the setting the open editor belongs to
HassScreen::SettingsTile HassScreen::_settingsTileOfView() const
{
    switch (_settingsView) {
    case SettingsView::IDLE_TIMEOUT:
        return SettingsTile::IDLE_TIMEOUT;
    case SettingsView::STANDBY_TIMEOUT:
        return SettingsTile::STANDBY_TIMEOUT;
    default:
        break;
    }
    return SettingsTile::IDLE_BRIGHTNESS;
}

void HassScreen::_stepSettingsTimeout(bool up)
{
    if (_settingsView == SettingsView::IDLE_TIMEOUT) {
        LVGLPlugin::setPowerSavingTimeout(stepTimeout(LVGLPlugin::getPowerSavingTimeout(), up));
    }
    else if (_settingsView == SettingsView::STANDBY_TIMEOUT) {
        LVGLPlugin::setStandbyTimeout(stepTimeout(LVGLPlugin::getStandbyTimeout(), up));
    }
    else {
        return;
    }
    // the readout follows the new value right away
    _settingsUpdate = 0;
    _updateSettings();
}

void HassScreen::_applySettingsValue()
{
    if (_settingsView != SettingsView::IDLE_BRIGHTNESS || !_settingsRefs.editSlider) {
        return;
    }
    LVGLPlugin::setPowerSavingLevel(static_cast<uint8_t>(lv_slider_get_value(_settingsRefs.editSlider)));
}

void HassScreen::_applySettingsAction()
{
    const auto action = _settingsAction;
    _settingsAction = SettingsAction::NONE;
    switch (action) {
    case SettingsAction::ROTATE:
        {
            // the four orientations of the display driver, in a circle
            const auto rotation = static_cast<uint8_t>((Plugins::WeatherStation::getHassRotation() + 1) & 0x03);
            Plugins::WeatherStation::setHassRotation(rotation);
            __LDBG_printf("hass> rotating the dashboard to %u", static_cast<unsigned>(rotation));
            // applied by the next update(), which also builds the sheet again for the new size
            setOrientation(rotation);
        }
        break;
    case SettingsAction::LEAVE:
        _closeSettings();
        __LDBG_printf("hass> leaving the dashboard (screen list)");
        // The manager releases this screen and builds the overview, which must not happen while the
        // frame of this update() is still running - it is deferred to the next loop iteration
        LoopFunctions::callOnce([]() {
            LVGLPlugin::screens().showOverview();
        });
        break;
    case SettingsAction::SLEEP:
        _closeSettings();
        __LDBG_printf("hass> turning the display off");
        // same as the leave button: the backlight is only touched by the main loop
        LoopFunctions::callOnce([]() {
            LVGLPlugin::sleep();
        });
        break;
    case SettingsAction::CLOSE:
        _closeSettings();
        break;
    default:
        break;
    }
}

void HassScreen::_settingsCallback(lv_event_t *event)
{
    auto self = static_cast<HassScreen *>(lv_event_get_user_data(event));
    if (!self || !self->_settingsOpen) {
        return;
    }
    const auto target = lv_event_get_target(event);
    const auto code = lv_event_get_code(event);
    auto &refs = self->_settingsRefs;
    self->_lastTileClick = millis();

    // the slider of the screen brightness: it follows the finger and is stored when it is released
    if (target == refs.brightnessSlider) {
        if (code == LV_EVENT_VALUE_CHANGED || code == LV_EVENT_RELEASED) {
            const auto percent = static_cast<uint8_t>(lv_slider_get_value(target));
            if (code == LV_EVENT_VALUE_CHANGED) {
                // live: the backlight follows the finger without a fade
                LVGLPlugin::setBrightness(percent);
            }
            else {
                // stored in the display configuration, the level fades to it
                LVGLPlugin::setConfiguredBrightness(percent);
            }
            if (refs.brightnessValue) {
                LVGLUI::setText(refs.brightnessValue, PrintString(F("%u %%"), static_cast<unsigned>(percent)).c_str(),
                                LVGLUI::kFontNormal, LVGLUI::kColorText);
            }
        }
        return;
    }
    // the slider of the editor of the idle brightness
    if (target == refs.editSlider) {
        if (code == LV_EVENT_VALUE_CHANGED || code == LV_EVENT_RELEASED) {
            const auto percent = static_cast<uint8_t>(lv_slider_get_value(target));
            if (code == LV_EVENT_RELEASED) {
                self->_applySettingsValue();
            }
            if (refs.editValue) {
                LVGLUI::setText(refs.editValue, PrintString(F("%u %%"), static_cast<unsigned>(percent)).c_str(),
                                LVGLUI::kFontHuge, LVGLUI::kColorText);
            }
        }
        return;
    }
    if (code != LV_EVENT_CLICKED) {
        return;
    }
    // the cell of the header closes the sheet, the row at the bottom leaves the screen or closes
    // the editor
    if (isTapOn(target, refs.close)) {
        self->_settingsAction = SettingsAction::CLOSE;
        return;
    }
    if (isTapOn(target, refs.action)) {
        if (self->_settingsView == SettingsView::MAIN) {
            self->_settingsAction = SettingsAction::LEAVE;
        }
        else {
            self->_settingsPendingView = static_cast<int8_t>(SettingsView::MAIN);
        }
        return;
    }
    // the steppers of a timeout
    if (isTapOn(target, refs.editDown) || isTapOn(target, refs.editUp)) {
        self->_stepSettingsTimeout(isTapOn(target, refs.editUp));
        return;
    }
    const auto index = self->_settingsTileAt(target);
    if (index == kSettingsTiles) {
        // a tap beside the controls (the layer below the sheet) does nothing
        return;
    }
    const auto tile = static_cast<SettingsTile>(index);
    __LDBG_printf("hass> quick settings tile %u tapped", static_cast<unsigned>(index));
    switch (tile) {
    case SettingsTile::IDLE_BRIGHTNESS:
        self->_settingsPendingView = static_cast<int8_t>(SettingsView::IDLE_BRIGHTNESS);
        break;
    case SettingsTile::IDLE_TIMEOUT:
        self->_settingsPendingView = static_cast<int8_t>(SettingsView::IDLE_TIMEOUT);
        break;
    case SettingsTile::STANDBY_TIMEOUT:
        self->_settingsPendingView = static_cast<int8_t>(SettingsView::STANDBY_TIMEOUT);
        break;
    case SettingsTile::ROTATE:
        // the lock only stops the motion sensor, the tile rotates the dashboard while it is set too
        self->_settingsAction = SettingsAction::ROTATE;
        break;
    case SettingsTile::ROTATION_LOCK:
        {
            const auto locked = !Plugins::WeatherStation::getHassRotationLock();
            Plugins::WeatherStation::setHassRotationLock(locked);
            __LDBG_printf("hass> rotation lock %s", locked ? "set" : "cleared");
            if (!locked) {
                // the device may have been rotated while the lock was on: the dashboard follows
                // the motion sensor again right away
                self->refreshSensorRotation();
            }
            // the tile is redrawn (filled) by the next refresh
            self->_settingsUpdate = 0;
            self->_updateSettings();
        }
        break;
    case SettingsTile::SLEEP:
        self->_settingsAction = SettingsAction::SLEEP;
        break;
    default:
        break;
    }
}

// ------------------------------------------------------------------------------------------
// touch
// ------------------------------------------------------------------------------------------
HomeAssistant::TileIndex HassScreen::_findTile(const lv_obj_t *object) const
{
    if (_back.tile == object) {
        return kBackTile;
    }
    for (const auto &entry : _tiles) {
        const auto &refs = entry.refs;
        if (refs.tile == object || refs.arc == object || refs.drag == object || refs.fill == object ||
            refs.stepDown == object || refs.stepUp == object) {
            return entry.globalTile;
        }
    }
    return kNoTile;
}

void HassScreen::_tileCallback(lv_event_t *event)
{
    auto self = static_cast<HassScreen *>(lv_event_get_user_data(event));
    if (!self) {
        return;
    }
    const auto index = self->_findTile(lv_event_get_target(event));
    if (index == kBackTile) {
        self->_lastTileClick = millis();
        if (self->_panel != Panel::NONE) {
            // the back tile of a panel closes it
            self->_pendingPanelClose = true;
        }
        else {
            // back to the area that contains this one (the main page for a top level area)
            self->_pendingPage = static_cast<int32_t>(self->_dashboard.getConfig().getPageParent(self->_areaPage));
        }
        return;
    }
    if (index == kNoTile || self->_panel != Panel::NONE) {
        return;
    }
    // Only the tile itself opens the panel: a tap on one of its widgets (+ / - of a climate,
    // the drag area of a dimmer) has its own callback and must not be treated as a tap on the tile
    if (lv_event_get_target(event) != self->_widgets(index).refs.tile) {
        return;
    }
    self->_lastTileClick = millis();
    const auto &tile = self->_dashboard.getConfig().getTile(index);
    __LDBG_printf("hass> tap on tile %u '%s' (type %u)", static_cast<unsigned>(index), tile.name, static_cast<unsigned>(tile.type));
    switch (tile.type) {
    case TileType::AREA:
        // open the page of the area
        if (tile.areaPage) {
            self->_pendingPage = static_cast<int32_t>(tile.areaPage);
        }
        break;

    case TileType::SWITCH:
    case TileType::LIGHT:
    case TileType::BUTTON:
        // a light is switched on and off like a switch, it does not open a panel (the `dimmer`
        // tile type is the one that opens the light panel and switches on a double tap)
        self->_dashboard.toggle(index);
        break;

    case TileType::CLIMATE:
        // the panel of the entity (the climate panel)
        self->_pendingPanel = static_cast<int32_t>(index);
        break;

    case TileType::SENSOR:
        // the panel of the entity: its live value and the history graph of its long term
        // statistics. There is nothing to operate on a sensor, the panel is a readout
        self->_pendingPanel = static_cast<int32_t>(index);
        break;

    case TileType::DIMMER:
        // the level fill can be switched as well: a double tap switches the entity, a single tap
        // opens the panel (see _tapTile())
        self->_tapTile(index);
        break;

    case TileType::PICTURE:
        // the image is shown over the whole display (any tap or swipe on it returns)
        self->_pendingFullscreen = static_cast<int32_t>(index);
        break;

    default:
        // a switch, a light or a button is switched, a spacer does not exist as a widget
        break;
    }
}

// A tap on a dimmer tile: the first tap is held back, a second tap inside kTileDoubleTapTime
// switches the entity (on/off, the same action as the power button of its panel) and the panel
// only opens when no second tap follows. Without the delay a double tap would open the panel of
// the first tap and the second tap would land on that panel
void HassScreen::_tapTile(HomeAssistant::TileIndex index)
{
    const auto now = millis();
    if (_pendingTileTap == static_cast<int32_t>(index) &&
        static_cast<uint32_t>(now - _pendingTileTapTime) <= kTileDoubleTapTime) {
        const auto &tile = _dashboard.getConfig().getTile(index);
        _pendingTileTap = -1;
        __LDBG_printf("hass> double tap on tile %u '%s': toggle", static_cast<unsigned>(index), tile.name);
        _dashboard.toggle(index);
        return;
    }
    __LDBG_printf("hass> tap on tile %u: waiting for a second tap (panel in %ums)", static_cast<unsigned>(index),
                  static_cast<unsigned>(kTileDoubleTapTime));
    _pendingTileTap = static_cast<int32_t>(index);
    _pendingTileTapTime = now;
}

// the whole tile of a dimmer receives the drag: moving up/down changes the level, a tap without a
// movement opens the panel
void HassScreen::_dragCallback(lv_event_t *event)
{
    auto self = static_cast<HassScreen *>(lv_event_get_user_data(event));
    if (!self) {
        return;
    }
    const auto index = self->_findTile(lv_event_get_target(event));
    if (index >= self->_dashboard.getTileCount() || !self->_widgets(index).refs.drag) {
        return;
    }
    self->_lastTileClick = millis();
    auto &widgets = self->_widgets(index);

    const auto code = lv_event_get_code(event);
    const auto tileY = static_cast<lv_coord_t>(lv_obj_get_style_y(widgets.refs.tile, LV_PART_MAIN));
    const auto trackHeight = static_cast<lv_coord_t>(lv_obj_get_style_height(widgets.refs.tile, LV_PART_MAIN));

    if (code == LV_EVENT_PRESSED) {
        lv_point_t point;
        lv_indev_get_point(lv_indev_get_act(), &point);
        widgets.dragStartY = point.y;
        widgets.dragLevel = static_cast<uint8_t>(lroundf(self->_dashboard.getValue(index).value));
        return;
    }

    if (code == LV_EVENT_PRESSING || code == LV_EVENT_RELEASED) {
        lv_point_t point;
        lv_indev_get_point(lv_indev_get_act(), &point);
        // a tap does not move the finger: the level stays where it is and the release opens the panel
        const auto distance = static_cast<lv_coord_t>(point.y - widgets.dragStartY);
        if (distance < kDimmerDragTolerance && distance > -kDimmerDragTolerance) {
            if (code == LV_EVENT_RELEASED) {
                self->_tapTile(index);
            }
            return;
        }
        // the level follows the finger, the bottom of the track is 0 %. A movement also ends a tap
        // that is still waiting for its second one: the finger drags, it does not tap
        self->_pendingTileTap = -1;
        auto level = static_cast<int>((static_cast<int32_t>(tileY + trackHeight - point.y) * 100) / trackHeight);
        if (level < 0) {
            level = 0;
        }
        else if (level > 100) {
            level = 100;
        }
        // the fill and the label follow the finger
        LVGLUI::setLevelFill(widgets.refs.fill, 0, 0, static_cast<lv_coord_t>(lv_obj_get_style_width(widgets.refs.tile, LV_PART_MAIN)),
                             trackHeight, static_cast<uint8_t>(level), LVGLUI::kColorActive);
        char levelText[16];
        snprintf_P(levelText, sizeof(levelText), PSTR("%d %%"), level);
        LVGLUI::setText(widgets.refs.value, levelText, LVGLUI::kFontValue, LVGLUI::kColorText);
        if (code == LV_EVENT_RELEASED) {
            self->_expect(self->_expectedLevel, "level", level, -1);
            self->_dashboard.setLevel(index, static_cast<uint8_t>(level));
        }
    }
}

// the + and - bars of a climate tile and the stepper buttons of the panels
void HassScreen::_stepCallback(lv_event_t *event)
{
    auto self = static_cast<HassScreen *>(lv_event_get_user_data(event));
    if (!self) {
        return;
    }
    const auto target = lv_event_get_target(event);
    const auto index = self->_findTile(target);
    if (index >= self->_dashboard.getTileCount()) {
        return;
    }
    self->_lastTileClick = millis();

    const auto &tile = self->_dashboard.getConfig().getTile(index);
    const auto &value = self->_dashboard.getValue(index);
    // The readout is drawn right away instead of with the next refresh (200 ms): the value the user
    // stepped is known, only the entity has not reported it yet (see _expect())
    const auto redraw = [self, index]() {
        if (self->_panel != Panel::NONE) {
            self->_updatePanel();
        }
        else {
            self->_updateTile(index);
        }
    };

    if (tile.type == TileType::DIMMER) {
        // the level works in percent (the step is a percentage as well). It starts from the level
        // the user set while the entity has not reported it yet, otherwise every tap would step
        // from the level of the response that is still in flight and the readout would not move
        auto step = static_cast<int32_t>(lroundf(tile.step));
        if (step < 1) {
            step = 1;
        }
        const auto held = self->_expects(self->_expectedLevel, "level", value.value, -1, kLevelHoldTolerance);
        auto level = static_cast<int32_t>(lroundf(held ? self->_expectedLevel.value : value.value)) +
                     (isTapOn(target, self->_widgets(index).refs.stepUp) ? step : -step);
        if (level < 0) {
            level = 0;
        }
        else if (level > 100) {
            level = 100;
        }
        self->_expect(self->_expectedLevel, "level", static_cast<float>(level), -1);
        self->_dashboard.setLevel(index, static_cast<uint8_t>(level));
        redraw();
        return;
    }

    // the setpoint works with a tenth of a degree, the step of the tile (climate default 0.5)
    auto step = static_cast<int16_t>(lroundf(tile.step * 10.0f));
    if (step < 1) {
        step = 1;
    }
    // the range comes from the configuration or from the attributes of the entity
    auto minValue = static_cast<int32_t>(lroundf(value.minTemp * 10));
    auto maxValue = static_cast<int32_t>(lroundf(value.maxTemp * 10));
    if (tile.min > 0 && tile.max > tile.min) {
        minValue = static_cast<int32_t>(lroundf(tile.min * 10));
        maxValue = static_cast<int32_t>(lroundf(tile.max * 10));
    }
    if (maxValue <= minValue) {
        maxValue = minValue + 1;
    }
    // An entity that is off does not report a setpoint. Stepping from 0 would start at the bottom
    // of the range, so the current temperature (or the middle of the range) is the base then. The
    // base is the setpoint the user set while the entity has not reported it yet (the expected
    // value), so repeated steps add up instead of stepping from a response in flight
    const auto held = self->_expects(self->_expectedSetpoint, "setpoint", value.value, -1, kSetpointHoldTolerance);
    const auto reported = static_cast<int32_t>(lroundf((held ? self->_expectedSetpoint.value : value.value) * 10.0f));
    auto temperature = reported;
    if (temperature < minValue || temperature > maxValue) {
        temperature = static_cast<int32_t>(lroundf(value.current * 10.0f));
        if (temperature < minValue || temperature > maxValue) {
            temperature = (minValue + maxValue) / 2;
        }
    }
    temperature += (isTapOn(target, self->_widgets(index).refs.stepUp)) ? step : -step;
    if (temperature < minValue) {
        temperature = minValue;
    }
    else if (temperature > maxValue) {
        temperature = maxValue;
    }
    if (temperature == reported) {
        return;
    }
    __LDBG_printf("hass> step tile %u: %s from %.1f to %.1f", static_cast<unsigned>(index),
                  isTapOn(target, self->_widgets(index).refs.stepUp) ? "up" : "down", static_cast<double>(reported) / 10.0,
                  static_cast<double>(temperature) / 10.0);
    self->_expect(self->_expectedSetpoint, "setpoint", static_cast<float>(temperature) / 10.0f, -1);
    self->_dashboard.setTemperature(index, static_cast<float>(temperature) / 10.0f);
    redraw();
}

// the arc of the climate panel. The range and the value of the arc are counted in steps
// (arcStepTenths()), so dragging moves the setpoint in whole steps - 0.5 °C by default
void HassScreen::_arcCallback(lv_event_t *event)
{
    auto self = static_cast<HassScreen *>(lv_event_get_user_data(event));
    if (!self) {
        return;
    }
    const auto index = self->_findTile(lv_event_get_target(event));
    if (index >= self->_dashboard.getTileCount() || !self->_widgets(index).refs.arc) {
        return;
    }
    self->_lastTileClick = millis();
    auto &widgets = self->_widgets(index);
    const auto &tile = self->_dashboard.getConfig().getTile(index);
    const auto step = arcStepTenths(tile);
    const auto value = static_cast<int16_t>(lv_arc_get_value(widgets.refs.arc));
    const auto setpoint = static_cast<float>(value) * step / 10.0f;

    switch (lv_event_get_code(event)) {
    case LV_EVENT_PRESSED:
        widgets.arcPressed = true;
        widgets.arcPressValue = value;
        __LDBG_printf("hass> arc %u pressed at %.1f", static_cast<unsigned>(index), static_cast<double>(setpoint));
        break;

    case LV_EVENT_VALUE_CHANGED:
        // the setpoint follows the finger
        char setpointText[16];
        snprintf_P(setpointText, sizeof(setpointText), PSTR("%.1f°"), static_cast<double>(setpoint));
        LVGLUI::setText(widgets.refs.value, setpointText, LVGLUI::kFontTitle, LVGLUI::kColorText);
        break;

    case LV_EVENT_PRESS_LOST:
        // the finger left the arc or the drag was taken over (LVGL ends the drag here too, see
        // lv_arc.c): without this the arc would keep its flag forever and never follow the entity
        widgets.arcPressed = false;
        __LDBG_printf("hass> arc %u press lost at %.1f", static_cast<unsigned>(index), static_cast<double>(setpoint));
        break;

    case LV_EVENT_RELEASED:
        widgets.arcPressed = false;
        __LDBG_printf("hass> arc %u released at %.1f (pressed at %.1f)", static_cast<unsigned>(index),
                      static_cast<double>(setpoint), static_cast<double>(widgets.arcPressValue) * step / 10.0f);
        if (value != widgets.arcPressValue) {
            self->_expect(self->_expectedSetpoint, "setpoint", setpoint, -1);
            self->_dashboard.setTemperature(index, setpoint);
        }
        break;

    default:
        break;
    }
}

// the option pills and the buttons of the climate and light panel, and the items of the list
void HassScreen::_panelCallback(lv_event_t *event)
{
    auto self = static_cast<HassScreen *>(lv_event_get_user_data(event));
    if (!self || self->_panel == Panel::NONE || self->_panelTile < 0) {
        return;
    }
    self->_lastTileClick = millis();
    const auto target = lv_event_get_target(event);
    const auto index = static_cast<HomeAssistant::TileIndex>(self->_panelTile);
    auto &refs = self->_panelRefs;

    if (self->_panel == Panel::SENSOR) {
        // the range buttons of the history graph. The tree is not rebuilt from inside an LVGL
        // event callback: the selection is recorded and applied by update()
        for (uint8_t i = 0; i < kStatsRangeCount; i++) {
            if (isTapOn(target, self->_sensor.chips[i])) {
                self->_pendingStatsRange = static_cast<int8_t>(i);
                __LDBG_printf("hass> history range %uh selected", static_cast<unsigned>(statsRangeHours(static_cast<StatsRange>(i))));
                return;
            }
        }
        return;
    }

    if (self->_panel == Panel::DIMMER) {
        // the buttons of the light panel: power toggles the entity, the others select the control
        for (uint8_t i = 0; i < static_cast<uint8_t>(LightButton::COUNT); i++) {
            if (!isTapOn(target, refs.buttons[i])) {
                continue;
            }
            switch (static_cast<LightButton>(i)) {
            case LightButton::POWER:
                self->_dashboard.toggle(index);
                return;
            case LightButton::BRIGHTNESS:
                self->_panelView = PanelView::LEVEL;
                break;
            case LightButton::COLOR:
                self->_panelView = PanelView::COLOR;
                break;
            case LightButton::COLOR_TEMP:
                self->_panelView = PanelView::COLOR_TEMP;
                break;
            default:
                self->_panelView = PanelView::EFFECTS;
                break;
            }
            self->_listView = -1;
            return;
        }
    }
    else {
        for (uint8_t i = 0; i < kNumPills; i++) {
            if (isTapOn(target, refs.pills[i])) {
                // a tap on a pill shows the list of that option, another tap goes back to the control
                const auto view = static_cast<PanelView>(static_cast<uint8_t>(PanelView::OPTION_1) + i);
                self->_panelView = (self->_panelView == view) ? PanelView::ARC : view;
                self->_listView = -1;
                return;
            }
        }
    }

    if (refs.list && lv_obj_get_parent(target) == refs.list) {
        // An item of the list sets the value and goes back to the control. The name is taken from
        // the list, never from the label of the item: a name that is longer than the item is
        // shortened with dots and LVGL replaced the characters of the label with them
        // (see listItemOrdinal())
        const auto items = self->_panelListItems();
        const auto ordinal = listItemOrdinal(refs.list, target);
        if (!items || ordinal < 0) {
            return;
        }
        const auto text = listItemText(items, static_cast<uint8_t>(ordinal));
        if (!text.length()) {
            return;
        }
        if (self->_panel == Panel::CLIMATE) {
            if (self->_panelView == PanelView::OPTION_1) {
                self->_expectItem(PanelView::OPTION_1, text.c_str());
                self->_dashboard.setMode(index, text.c_str());
            }
            else if (self->_panelView == PanelView::OPTION_2) {
                self->_expectItem(PanelView::OPTION_2, text.c_str());
                self->_dashboard.setPreset(index, text.c_str());
            }
            else if (self->_panelView == PanelView::OPTION_3) {
                self->_expectItem(PanelView::OPTION_3, text.c_str());
                self->_dashboard.setFanMode(index, text.c_str());
            }
            self->_panelView = PanelView::ARC;
        }
        else if (self->_panelView == PanelView::EFFECTS) {
            // the list stays open, the tapped effect is marked until the response reports it
            self->_expectItem(PanelView::EFFECTS, text.c_str());
            self->_dashboard.setEffect(index, text.c_str());
        }
        self->_listView = -1;
    }
}

// the color temperature slider of the light panel, the value is in kelvin
void HassScreen::_tempSliderCallback(lv_event_t *event)
{
    auto self = static_cast<HassScreen *>(lv_event_get_user_data(event));
    if (!self || self->_panelTile < 0 || !self->_panelRefs.tempSlider) {
        return;
    }
    const auto index = static_cast<HomeAssistant::TileIndex>(self->_panelTile);
    const auto code = lv_event_get_code(event);
    self->_lastTileClick = millis();

    if (code == LV_EVENT_PRESSED) {
        self->_controlPressed = true;
        return;
    }
    if (code == LV_EVENT_VALUE_CHANGED || code == LV_EVENT_RELEASED) {
        const auto kelvin = static_cast<int32_t>(lv_slider_get_value(self->_panelRefs.tempSlider));
        char kelvinText[16];
        snprintf_P(kelvinText, sizeof(kelvinText), PSTR("%d K"), static_cast<int>(kelvin));
        LVGLUI::setText(self->_panelRefs.label, kelvinText, self->_panelValueFont(), LVGLUI::kColorText);
        if (code == LV_EVENT_RELEASED) {
            self->_controlPressed = false;
            self->_expect(self->_expectedTemp, "color temp", static_cast<float>(kelvin), -1);
            self->_dashboard.setColorTemp(index, static_cast<float>(kelvin));
        }
    }
}

// The level slider of the dimmer panel. A tap on the track switches the entity on and off (the
// same action as the power button of the panel), only a movement changes the level. The level of a
// light is a value the finger cannot hit by a tap, and LVGL writes the value of the pressed
// position into the slider while the finger is on it (lv_slider.c, LV_EVENT_PRESSING), which made
// the level of the entity jump before. The value the slider showed is put back until the finger
// moved by more than kDimmerDragTolerance
void HassScreen::_sliderCallback(lv_event_t *event)
{
    auto self = static_cast<HassScreen *>(lv_event_get_user_data(event));
    if (!self || self->_panelTile < 0) {
        return;
    }
    const auto index = static_cast<HomeAssistant::TileIndex>(self->_panelTile);
    const auto code = lv_event_get_code(event);
    self->_lastTileClick = millis();

    if (code == LV_EVENT_PRESSED) {
        self->_controlPressed = true;
        self->_sliderMoved = false;
        // the level the panel shows: the one the user set and the entity did not report yet, else
        // the level of the entity
        const auto &value = self->_dashboard.getValue(index);
        self->_sliderLevel = static_cast<int32_t>(lroundf(self->_expectedLevel.active ? self->_expectedLevel.value : value.value));
        // LVGL animates the value of the track to the pressed position (lv_slider -> lv_bar with
        // LV_ANIM_ON, the default theme animates over ~200 ms). Without LV_ANIM_OFF the fill and the
        // percentage of a tap ran toward the pressed position for a moment until the next PRESSING
        // arrived and put the value back
        lv_slider_set_value(self->_panelRefs.slider, self->_sliderLevel, LV_ANIM_OFF);
        lv_indev_t *indev = lv_indev_get_act();
        if (indev) {
            lv_point_t point;
            lv_indev_get_point(indev, &point);
            self->_sliderPressY = point.y;
        }
        return;
    }
    if (code == LV_EVENT_PRESSING && !self->_sliderMoved) {
        lv_indev_t *indev = lv_indev_get_act();
        lv_point_t point;
        lv_indev_get_point(indev, &point);
        if (abs(static_cast<int32_t>(point.y) - static_cast<int32_t>(self->_sliderPressY)) < kDimmerDragTolerance) {
            // a movement of less than the tolerance is a tap: the level stays where it was. The
            // value is written on every cycle, which also cancels the animation LVGL started
            lv_slider_set_value(self->_panelRefs.slider, self->_sliderLevel, LV_ANIM_OFF);
            return;
        }
        self->_sliderMoved = true;
        __LDBG_printf("hass> level slider of tile %u dragged", static_cast<unsigned>(index));
    }
    if (code == LV_EVENT_VALUE_CHANGED) {
        // the value follows the finger, a tap keeps the level of the entity ("%d %%" of the value)
        const auto level = self->_sliderMoved ? lv_slider_get_value(self->_panelRefs.slider) : self->_sliderLevel;
        char levelText[16];
        snprintf_P(levelText, sizeof(levelText), PSTR("%d %%"), static_cast<int>(level));
        LVGLUI::setText(self->_widgets(index).refs.value, levelText, LVGLUI::kFontValue, LVGLUI::kColorText);
        return;
    }
    if (code == LV_EVENT_RELEASED) {
        self->_controlPressed = false;
        if (!self->_sliderMoved) {
            // a tap: the entity is switched (the level of the entity does not change)
            lv_slider_set_value(self->_panelRefs.slider, self->_sliderLevel, LV_ANIM_OFF);
            char levelText[16];
            snprintf_P(levelText, sizeof(levelText), PSTR("%d %%"), static_cast<int>(self->_sliderLevel));
            LVGLUI::setText(self->_widgets(index).refs.value, levelText, LVGLUI::kFontValue, LVGLUI::kColorText);
            const auto &tile = self->_dashboard.getConfig().getTile(index);
            __LDBG_printf("hass> tap on the level slider of tile %u '%s': toggle", static_cast<unsigned>(index), tile.name);
            self->_dashboard.toggle(index);
            return;
        }
        const auto level = static_cast<uint8_t>(lv_slider_get_value(self->_panelRefs.slider));
        __LDBG_printf("hass> level slider of tile %u released: %u %%", static_cast<unsigned>(index), static_cast<unsigned>(level));
        self->_expect(self->_expectedLevel, "level", level, -1);
        self->_dashboard.setLevel(index, level);
    }
}

// the color wheel of the dimmer panel
void HassScreen::_wheelCallback(lv_event_t *event)
{
    auto self = static_cast<HassScreen *>(lv_event_get_user_data(event));
    if (!self || self->_panelTile < 0 || !self->_panelRefs.wheel) {
        return;
    }
    const auto index = static_cast<HomeAssistant::TileIndex>(self->_panelTile);
    const auto code = lv_event_get_code(event);
    self->_lastTileClick = millis();

    if (code == LV_EVENT_PRESSED) {
        self->_controlPressed = true;
        return;
    }
    if (code == LV_EVENT_RELEASED) {
        self->_controlPressed = false;
        const auto hsv = lv_colorwheel_get_hsv(self->_panelRefs.wheel);
        self->_expect(self->_expectedColor, "color", hsv.h, (hsv.s * 100.0f) / 255.0f);
        self->_dashboard.setColor(index, hsv.h, (hsv.s * 100.0f) / 255.0f);
    }
}

bool HassScreen::onTap()
{
    // The dashboard is the whole screen and a stray tap must not replace it with the overview, so
    // the tap is always consumed. The screen is left with the "Leave HASS" button of the quick
    // settings (see onSwipe()) - the top bar of the screen manager is the second way out
    return true;
}

bool HassScreen::onDoubleTap()
{
    // same as onTap(): the double tap has no action of its own on this screen
    return true;
}

bool HassScreen::onSwipe(SwipeDirection direction)
{
    // A swipe on the fullscreen image returns to the dashboard as well: the image is closed by
    // the touch itself (see _fullCallback()) and the manager runs the swipe of that same touch
    // right after, so a closed image still consumes it (kFullscreenGestureTime covers the gap)
    if (_fullTile >= 0 || static_cast<uint32_t>(millis() - _fullscreenTime) < kFullscreenGestureTime) {
        return true;
    }
    // While a panel is open a drag on a control must not leave the screen: the color wheel and the
    // arc are dragged horizontally as well
    if (_panel != Panel::NONE) {
        return true;
    }
    // The dashboard is left with the button of the quick settings, not with a swipe: a swipe to the
    // left or to the right opens the sheet and the gesture is consumed, so the manager never
    // switches to the next screen. A swipe while the sheet is open is consumed as well
    if (!_settingsOpen) {
        __LDBG_printf("hass> swipe opens the quick settings");
        _settingsPending = true;
    }
    return true;
}

#if DEBUG_HASS_ACTION_TEST
// Temporary self test of the action path: the tile whose name contains "test" is toggled, one tap
// every 4 seconds and every fifth cycle a burst of three taps 250 ms apart (the quick tapping that
// showed the flicker). The trace of the dashboard and of the client shows what the firmware does
// with them. Enabled with -D DEBUG_HASS_ACTION_TEST=1.
void HassScreen::_actionTest()
{
    static uint8_t index = 0xff;
    static uint8_t step = 0;
    static uint8_t singleTaps = 0;
    static uint32_t next = 0;

    if (index == 0xff) {
        for (uint8_t i = 0; i < _dashboard.getTileCount(); i++) {
            const auto &tile = _dashboard.getConfig().getTile(i);
            if (strstr(tile.name, "switchtest") || strstr(tile.name, "test")) {
                index = i;
                __LDBG_printf("hass> self test: tile %u '%s' (%s)", static_cast<unsigned>(i), tile.name, tile.entity);
                break;
            }
        }
        if (index == 0xff) {
            return;
        }
    }

    const auto now = millis();
    if (static_cast<int32_t>(now - next) < 0) {
        return;
    }

    if (step == 0) {
        singleTaps++;
        if ((singleTaps % 5) == 0) {
            __LDBG_printf("hass> self test: burst of 3 taps on tile %u, reported state %u", static_cast<unsigned>(index),
                          static_cast<unsigned>(_dashboard.getValue(index).state));
            step = 2;
            next = now + 250;
            _dashboard.toggle(index);
            return;
        }
        __LDBG_printf("hass> self test: single tap on tile %u, reported state %u", static_cast<unsigned>(index),
                      static_cast<unsigned>(_dashboard.getValue(index).state));
        _dashboard.toggle(index);
        next = now + 4000;
        return;
    }

    __LDBG_printf("hass> self test: burst tap %u of 3", static_cast<unsigned>(3 - step));
    _dashboard.toggle(index);
    step = static_cast<uint8_t>(step - 1);
    next = now + 250;
}
#endif

} // namespace WeatherStation2
