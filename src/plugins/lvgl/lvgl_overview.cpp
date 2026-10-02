/**
 * Author: sascha_lammers@gmx.de
 */

#include "lvgl_overview.h"

#if IOT_LVGL_SUPPORT

#ifndef DEBUG_LVGL
#    define DEBUG_LVGL 0
#endif

#if DEBUG_LVGL
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

#include <lvgl.h>

// Every LVGL object is clickable by default on its own. Only the tile should react to the click,
// so the flag is cleared on the icon (and its widgets) - LVGL passes the click to the tile then.
// Labels are not clickable, they are never in the way
static void _clearClickable(lv_obj_t *obj)
{
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(obj); i++) {
        _clearClickable(lv_obj_get_child(obj, i));
    }
}

// flat button with a centered label, the click has to be added by the caller
static lv_obj_t *_button(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h,
                         const char *text, uint32_t color)
{
    auto button = LVGLUI::createCard(parent, x, y, w, h, nullptr);
    lv_obj_set_style_radius(button, 6, LV_PART_MAIN);
    lv_obj_set_style_bg_color(button, lv_color_hex(LVGLUI::kColorCardAlt), LV_PART_MAIN);
    lv_obj_set_style_bg_color(button, lv_color_hex(LVGLUI::kColorCard), LV_STATE_PRESSED);
    lv_obj_set_style_border_color(button, lv_color_hex(LVGLUI::kColorAccent), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(button, 2, LV_STATE_PRESSED);
    LVGLUI::addLabel(button, 0, static_cast<lv_coord_t>((h - lv_font_get_line_height(LVGLUI::kFontSmall)) / 2),
                     text, LVGLUI::kFontSmall, color, w, LV_TEXT_ALIGN_CENTER);
    return button;
}

uint8_t LVGLScreenOverview::_pageCount() const
{
    auto pages = static_cast<uint8_t>((_manager.count() + kTilesPerPage - 1) / kTilesPerPage);
    if (pages == 0) {
        pages = 1;
    }
    if (pages > kMaxPages) {
        pages = kMaxPages;
    }
    return pages;
}

void LVGLScreenOverview::create(lv_obj_t *parent)
{
    const auto width = lv_disp_get_hor_res(nullptr);
    const auto height = lv_disp_get_ver_res(nullptr);

    _pages = _pageCount();
    _pageIndex = 0;

    char info[16];
    info[0] = 0;
    if (_pages > 1) {
        snprintf_P(info, sizeof(info), PSTR("%u/%u"), 1U, static_cast<unsigned>(_pages));
    }
    _page = LVGLUI::createPage(parent, getTitle(), info[0] ? info : nullptr);

    // full page layer below the tiles: a tap on a tile only reaches the tile (an event does not
    // bubble to the parent) and every other tap on the page goes back
    _background = LVGLUI::createContainer(parent, 0, 0, width, height);
    lv_obj_add_event_cb(_background, _pageEventCb, LV_EVENT_CLICKED, this);

    auto footer = LVGLUI::createFooter(parent);
    // the footer is above the layer, a tap on it that does not hit a button falls through to it
    lv_obj_clear_flag(footer, LV_OBJ_FLAG_CLICKABLE);
    _createButtons(footer, width);

    _createTiles();
    LVGLUI::setClock(_page, _manager.getTimeFormat24h());
    __LDBG_printf("overview created, %u screen(s), %u page(s)", static_cast<unsigned>(_manager.count()), static_cast<unsigned>(_pages));
}

void LVGLScreenOverview::_createButtons(lv_obj_t *footer, lv_coord_t width)
{
    const auto y = static_cast<lv_coord_t>((LVGLUI::kFooterHeight - kButtonHeight) / 2);

    // left: automatic screen rotation on/off, the label shows the state and the rotation time
    _autoScrollButton = _button(footer, kButtonMargin, y, kAutoScrollButtonWidth, kButtonHeight, "",
                                LVGLUI::kColorText);
    lv_obj_add_event_cb(_autoScrollButton, _autoScrollEventCb, LV_EVENT_CLICKED, this);
    // the label is replaced by _updateAutoScrollButton(), the placeholder is not clickable
    _autoScrollLabel = lv_obj_get_child(_autoScrollButton, 0);

    // right: back to the screen the overview was opened from
    auto back = _button(footer, static_cast<lv_coord_t>(width - kButtonMargin - kBackButtonWidth), y,
                        kBackButtonWidth, kButtonHeight, "Back", LVGLUI::kColorText);
    lv_obj_add_event_cb(back, _backEventCb, LV_EVENT_CLICKED, this);

    _autoScroll = _manager.getRotationTime() != 0;
    if (_autoScroll) {
        _autoScrollTime = _manager.getRotationTime();
    }
    _updateAutoScrollButton();
}

void LVGLScreenOverview::_updateAutoScrollButton()
{
    if (!_autoScrollLabel) {
        return;
    }
    char text[40];
    if (_autoScroll) {
        snprintf_P(text, sizeof(text), PSTR("Auto scroll: on (%us)"), static_cast<unsigned>(_manager.getRotationTime()));
    }
    else {
        snprintf_P(text, sizeof(text), PSTR("Auto scroll: off"));
    }
    lv_label_set_text(_autoScrollLabel, text);
    lv_obj_set_style_text_color(_autoScrollLabel,
                                lv_color_hex(_autoScroll ? LVGLUI::kColorTextValue : LVGLUI::kColorTextMuted), LV_PART_MAIN);
    LVGLUI::fitTextDown(_autoScrollLabel, text, static_cast<lv_coord_t>(kAutoScrollButtonWidth - 12));
    __LDBG_printf("auto scroll %s, rotation=%us", _autoScroll ? "on" : "off", static_cast<unsigned>(_manager.getRotationTime()));
}

void LVGLScreenOverview::_toggleAutoScroll()
{
    _autoScroll = !_autoScroll;
    if (_autoScroll) {
        // the rotation time is not restored when it was off before the page was created
        _manager.setRotationTime(_autoScrollTime ? _autoScrollTime : kDefaultAutoScrollTime);
    }
    else {
        // remember the time to restore it when the auto scroll is turned on again
        _autoScrollTime = _manager.getRotationTime() ? _manager.getRotationTime() : kDefaultAutoScrollTime;
        _manager.setRotationTime(0);
    }
    _updateAutoScrollButton();
}

void LVGLScreenOverview::_createTiles()
{
    const auto width = lv_disp_get_hor_res(nullptr);
    const auto height = lv_disp_get_ver_res(nullptr);
    const auto gridBottom = static_cast<lv_coord_t>(height - LVGLUI::kFooterHeight - kGridBottomGap);
    const auto tileWidth = static_cast<lv_coord_t>((width - (kColumns + 1) * kTileGap) / kColumns);
    const auto tileHeight = static_cast<lv_coord_t>((gridBottom - kGridTop - kTileGap) / kRows);
    const auto screenCount = _manager.count();
    const auto active = _manager.getActiveIndex();

    for (uint8_t slot = 0; slot < kTilesPerPage; slot++) {
        _tiles[slot] = nullptr;
        _tileScreen[slot] = -1;

        const auto index = static_cast<uint8_t>(_pageIndex * kTilesPerPage + slot);
        auto screen = (index < screenCount) ? _manager.get(index) : nullptr;
        if (!screen) {
            continue;
        }
        const auto column = static_cast<uint8_t>(slot % kColumns);
        const auto row = static_cast<uint8_t>(slot / kColumns);
        const auto x = static_cast<lv_coord_t>(kTileGap + column * (tileWidth + kTileGap));
        const auto y = static_cast<lv_coord_t>(kGridTop + row * (tileHeight + kTileGap));

        // the tile of the screen that is shown is drawn like an active (filled) tile of a
        // dashboard, the others are plain cards. The reviewed layout (docs/screens/
        // lvgl_13_screen_overview_480x320.svg) draws a 1 px kColorBorder outline here, so this
        // page uses createCard() and not the borderless createTile() of the dashboards
        const auto isActive = (static_cast<int8_t>(index) == active);
        auto tile = LVGLUI::createCard(_background, x, y, tileWidth, tileHeight, nullptr);
        LVGLUI::setTileState(tile, isActive ? LVGLUI::TileState::ON : LVGLUI::TileState::OFF);
        // highlight the tile while it is pressed
        lv_obj_set_style_border_color(tile, lv_color_hex(LVGLUI::kColorAccent), LV_STATE_PRESSED);
        lv_obj_set_style_bg_color(tile, lv_color_hex(LVGLUI::kColorCardAlt), LV_STATE_PRESSED);
        lv_obj_add_event_cb(tile, _tileEventCb, LV_EVENT_CLICKED, this);
        _fillTile(tile, screen, tileWidth, isActive);

        _tiles[slot] = tile;
        _tileScreen[slot] = static_cast<int8_t>(index);
    }
}

void LVGLScreenOverview::_fillTile(lv_obj_t *tile, LVGLScreen *screen, lv_coord_t tileWidth, bool active)
{
    auto icon = LVGLUI::createIcon(tile, screen->getIcon(), static_cast<lv_coord_t>((tileWidth - kIconSize) / 2), kIconTop, kIconSize);
    // a tap on the icon has to reach the tile
    _clearClickable(icon);
    if (active) {
        // the glyph of a filled tile is white
        LVGLUI::setIconColor(icon, LVGLUI::kColorText);
    }

    const auto labelWidth = static_cast<lv_coord_t>(tileWidth - 12);
    auto label = LVGLUI::addLabel(tile, 6, kLabelTop, screen->getTitle(), LVGLUI::kFontSmall,
                                  active ? LVGLUI::kColorText : LVGLUI::kColorTextLabel, labelWidth,
                                  LV_TEXT_ALIGN_CENTER);
    // the label is one line of a fixed tile, a longer title shrinks instead of wrapping
    LVGLUI::fitTextDown(label, screen->getTitle(), labelWidth);
}

void LVGLScreenOverview::_updatePageInfo()
{
    if (!_page.info) {
        return;
    }
    if (_pages < 2) {
        lv_obj_add_flag(_page.info, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    char info[16];
    snprintf_P(info, sizeof(info), PSTR("%u/%u"), static_cast<unsigned>(_pageIndex + 1), static_cast<unsigned>(_pages));
    lv_label_set_text(_page.info, info);
    lv_obj_clear_flag(_page.info, LV_OBJ_FLAG_HIDDEN);
}

void LVGLScreenOverview::update()
{
    LVGLUI::setClock(_page, _manager.getTimeFormat24h());
}

bool LVGLScreenOverview::onSwipe(SwipeDirection direction)
{
    if (_pages < 2) {
        // let the manager show the previous/next screen
        return false;
    }
    uint8_t page = _pageIndex;
    if (direction == SwipeDirection::RIGHT) {
        page = static_cast<uint8_t>((page + 1) % _pages);
    }
    else if (direction == SwipeDirection::LEFT) {
        page = static_cast<uint8_t>((page + _pages - 1) % _pages);
    }
    else {
        return false;
    }
    if (page == _pageIndex) {
        return false;
    }
    _pageIndex = page;
    // the background itself stays, only the tiles are rebuilt
    lv_obj_clean(_background);
    _createTiles();
    _updatePageInfo();
    return true;
}

void LVGLScreenOverview::_tileEventCb(lv_event_t *event)
{
    auto self = static_cast<LVGLScreenOverview *>(lv_event_get_user_data(event));
    auto target = lv_event_get_target(event);
    for (uint8_t slot = 0; slot < kTilesPerPage; slot++) {
        if (self->_tiles[slot] == target && self->_tileScreen[slot] >= 0) {
            __LDBG_printf("tile #%u selected, showing screen %u", static_cast<unsigned>(slot), static_cast<unsigned>(self->_tileScreen[slot]));
            self->_manager.show(static_cast<uint8_t>(self->_tileScreen[slot]));
            return;
        }
    }
}

void LVGLScreenOverview::_backEventCb(lv_event_t *event)
{
    // shows the screen the overview was opened from
    auto self = static_cast<LVGLScreenOverview *>(lv_event_get_user_data(event));
    if (!self->_manager.closeOverview()) {
        __LDBG_printf("overview has no screen to return to");
    }
}

void LVGLScreenOverview::_autoScrollEventCb(lv_event_t *event)
{
    static_cast<LVGLScreenOverview *>(lv_event_get_user_data(event))->_toggleAutoScroll();
}

void LVGLScreenOverview::_pageEventCb(lv_event_t *event)
{
    // a tap next to the tiles, the same as the Back button
    auto self = static_cast<LVGLScreenOverview *>(lv_event_get_user_data(event));
    if (!self->_manager.closeOverview()) {
        __LDBG_printf("overview has no screen to return to");
    }
}

#endif
