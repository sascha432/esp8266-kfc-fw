/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

// The screen overview: every screen registered with LVGLScreenManager as a tile with an icon and
// a label, a tap opens it directly.
//
// The page is not part of the automatic rotation. It is registered with
// LVGLScreenManager::setOverview() and shown by the default action of the tap and double tap
// gestures - a screen that handles a gesture itself never reaches it. A tap next to the tiles
// goes back to the screen the overview was opened from.
//
// The tiles are real LVGL widgets and the click is handled by an LV_EVENT_CLICKED callback: the
// manager observes the touch as a gesture without coordinates, the tap is consumed by onTap() so
// that no default action fires. The icon comes from LVGLScreen::getIcon(), the label from
// LVGLScreen::getTitle().
//
// Layout (WT32-SC01, 480x320): 4 columns x 2 rows of 110x91 px tiles, icon 44 px, label font 12,
// 2 pages for up to 16 screens (kMaxScreens of the manager), the second page is reached with a
// swipe left/right. The footer has two buttons: "Auto scroll" (left, turns the automatic screen
// rotation on/off, the label shows the state and the rotation time) and "Back" (right, returns to
// the screen the page was opened from). A tap next to the tiles does the same as the Back button.

#include <Arduino_compat.h>

#if IOT_LVGL_SUPPORT

#include "lvgl_screen.h"
#include "lvgl_ui.h"

class LVGLScreenOverview : public LVGLScreen {
public:
    static constexpr uint8_t kColumns = 4;
    static constexpr uint8_t kRows = 2;
    static constexpr uint8_t kTilesPerPage = kColumns * kRows;
    static constexpr uint8_t kMaxPages = 2;
    // gap between the tiles and to the page edge
    static constexpr lv_coord_t kTileGap = 8;
    // size of the icon inside a tile (the large icon font of LVGLUI)
    static constexpr lv_coord_t kIconSize = LVGLUI::kIconSizeLarge;
    // y of the icon and of the label inside a tile (91 px high, the block is centered)
    static constexpr lv_coord_t kIconTop = 11;
    static constexpr lv_coord_t kLabelTop = 65;
    // first pixel row of the grid, below the title of the page
    static constexpr lv_coord_t kGridTop = 78;
    // space between the bottom of the grid and the footer
    static constexpr lv_coord_t kGridBottomGap = 6;
    // size of the footer buttons and their distance to the page edge
    static constexpr lv_coord_t kButtonHeight = 30;
    static constexpr lv_coord_t kButtonMargin = 8;
    static constexpr lv_coord_t kBackButtonWidth = 80;
    static constexpr lv_coord_t kAutoScrollButtonWidth = 175;
    // rotation time used when the automatic screen rotation was off and is turned on again
    static constexpr uint32_t kDefaultAutoScrollTime = 10; // seconds

    explicit LVGLScreenOverview(LVGLScreenManager &manager) : _manager(manager) {}

    virtual const char *getName() const override {
        return "OVERVIEW";
    }
    virtual const char *getTitle() const override {
        return "Screens";
    }
    virtual LVGLUI::IconType getIcon() const override {
        return LVGLUI::IconType::CALENDAR;
    }
    virtual void create(lv_obj_t *parent) override;
    virtual void update() override;
    // the taps are handled by the tiles and by the page background (LV_EVENT_CLICKED)
    virtual bool onTap() override {
        return true;
    }
    // a swipe left/right pages the grid, with a single page the manager shows the next screen
    virtual bool onSwipe(SwipeDirection direction) override;
private:
    // number of pages the registered screens need, at least 1
    uint8_t _pageCount() const;
    // (re)builds the tiles of the current page
    void _createTiles();
    // adds the icon and the label to a tile
    void _fillTile(lv_obj_t *tile, LVGLScreen *screen, lv_coord_t tileWidth, bool active);
    // "Auto scroll" (left) and "Back" (right) in the footer
    void _createButtons(lv_obj_t *footer, lv_coord_t width);
    // label of the auto scroll button: state and rotation time
    void _updateAutoScrollButton();
    // turns the automatic rotation of the manager on/off
    void _toggleAutoScroll();
    // updates the page indicator of the top bar ("1/2")
    void _updatePageInfo();

    static void _tileEventCb(lv_event_t *event);
    static void _backEventCb(lv_event_t *event);
    static void _autoScrollEventCb(lv_event_t *event);
    // a tap next to the tiles, the second way back next to the Back button
    static void _pageEventCb(lv_event_t *event);

private:
    LVGLScreenManager &_manager;
    LVGLUI::PageRefs _page;
    // full page layer below the tiles
    lv_obj_t *_background{nullptr};
    lv_obj_t *_autoScrollButton{nullptr};
    lv_obj_t *_autoScrollLabel{nullptr};
    // state of the auto scroll button, the rotation time before it was turned off
    bool _autoScroll{true};
    uint32_t _autoScrollTime{0};
    lv_obj_t *_tiles[kTilesPerPage]{};
    // registered screen behind a tile, -1 for an empty slot
    int8_t _tileScreen[kTilesPerPage]{};
    uint8_t _pageIndex{0};
    uint8_t _pages{1};
};

#endif
