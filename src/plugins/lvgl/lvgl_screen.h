/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

// Screens and the screen manager of the LVGL UI.
//
// Application plugins (for example the weather station plugin) create one class per screen
// derived from LVGLScreen, hand them to the manager of the lvgl plugin and the manager takes
// care of building/destroying the widget tree, the automatic rotation and the touch gestures:
//
//   LVGLPlugin::screens().add(&_mainScreen);
//   LVGLPlugin::screens().add(&_indoorScreen);
//   LVGLPlugin::screens().setRotationTime(10);
//
// Everything runs from the main loop (LVGL is not re-entrant), screens must not be created,
// shown or removed from another task.

#include <Arduino_compat.h>

#if IOT_LVGL_SUPPORT

#include <lvgl.h>
#include "lvgl_ui.h"
#include "lvgl_touch_lens.h"

// Direction of a swipe, see LVGLScreen::onSwipe(). The horizontal swipes have a default action of
// the manager (left = previous screen, right = next screen), the vertical ones have none - a
// screen that does not handle them leaves the gesture without an action
enum class SwipeDirection : int8_t {
    LEFT = -1,
    RIGHT = 1,
    UP = -2,
    DOWN = 2,
};

// true for a swipe up or down
inline bool isVerticalSwipe(SwipeDirection direction)
{
    return (direction == SwipeDirection::UP) || (direction == SwipeDirection::DOWN);
}

class LVGLScreen {
public:
    virtual ~LVGLScreen() = default;

    // short name, shown in the status output
    virtual const char *getName() const = 0;

    // Icon and label of the tile in the screen overview (the page with all screens). The icon is
    // drawn procedurally by LVGLUI::createIcon(), IconType::UNKNOWN shows a placeholder
    virtual LVGLUI::IconType getIcon() const {
        return LVGLUI::IconType::UNKNOWN;
    }
    // label of the screen overview tile, the short name is the default
    virtual const char *getTitle() const {
        return getName();
    }

    // builds the widget tree below parent, called every time the screen is shown.
    // parent is the active screen object of the display
    virtual void create(lv_obj_t *parent) = 0;

    // called after the screen is left (the widget tree is removed by the manager)
    virtual void release() {}

    // refresh interval in milliseconds while the screen is active, 0 = only create() is called
    virtual uint32_t getRefreshInterval() const {
        return 1000;
    }

    // time in seconds before the manager switches to the next screen, 0 = manager default,
    // LVGLScreenManager::kScreenTimeNoRotation keeps the screen until another one is selected
    virtual uint32_t getScreenTime() const {
        return 0;
    }

    // called every getRefreshInterval() milliseconds while the screen is active
    virtual void update() {}

    // false when the screen does not draw the top bar of LVGLUI::createPage(). The manager treats
    // a tap in the band of the bar as its own shortcut to the screen overview, a screen without a
    // bar has widgets of its own there (the tiles of a dashboard cover the whole display)
    virtual bool hasTopBar() const {
        return true;
    }

    // Touch gestures. A callback that returns true has handled the gesture, the manager then
    // leaves the screen alone, otherwise it runs the default action of the gesture (a tap and a
    // double tap show the screen overview, a swipe to the right goes to the next screen and a
    // swipe to the left to the previous one). Swipes up/down have no default action, a screen
    // has to handle them itself.
    //
    // A tap is not evaluated immediately, the manager waits for the double tap window
    // (kDoubleTapTime) and calls onDoubleTap() instead when a second tap follows. So the
    // single tap action only runs after the window has passed. A tap in the top bar
    // (LVGLUI::kBarHeight) is never passed to the screen, it always opens the screen overview.
    virtual bool onTap() {
        return false;
    }
    virtual bool onDoubleTap() {
        return false;
    }
    virtual bool onLongPress() {
        return false;
    }
    // direction of the swipe, see SwipeDirection. A screen that rebuilds its widget tree has to
    // ask the manager for it (LVGLPlugin::screens().reload()), the gesture is evaluated while
    // the screen is active
    virtual bool onSwipe(SwipeDirection direction) {
        return false;
    }
    // Start-point-aware hook; existing screens keep their direction-only handler.
    virtual bool onSwipe(SwipeDirection direction, const lv_point_t &)
    {
        return onSwipe(direction);
    }
};

class LVGLScreenManager {
public:
    static constexpr uint8_t kMaxScreens = 16;
    // getScreenTime() of a screen that must not be rotated away (an application screen such as a
    // dashboard stays until another screen is selected)
    static constexpr uint32_t kScreenTimeNoRotation = 0xffffffff;
    static constexpr uint32_t kDefaultRotationTime = 10; // seconds
    static constexpr uint32_t kTapMaxTime = 600; // milliseconds
    // a second tap within this time is a double tap, a single tap only runs after the window
    static constexpr uint32_t kDoubleTapTime = 300; // milliseconds
    static constexpr uint32_t kLongPressTime = 800; // milliseconds
    static constexpr lv_coord_t kTouchTolerance = 20; // pixels
    static constexpr lv_coord_t kSwipeDistance = 50; // pixels
    // the automatic rotation stops for this long after the touch screen was used
    static constexpr uint32_t kTouchRotationPause = 5 * 60 * 1000; // milliseconds

    // registers a screen, screens are not owned by the manager
    bool add(LVGLScreen *screen);
    bool remove(LVGLScreen *screen);
    void removeAll();

    uint8_t count() const {
        return _count;
    }
    LVGLScreen *get(uint8_t index) const;
    LVGLScreen *getActiveScreen() const;
    // index of the active screen, -1 before the first screen has been shown. While the overview is
    // active it is the index of the screen the overview was opened from
    int8_t getActiveIndex() const {
        return _active;
    }
    // seconds since the active screen has been shown
    uint32_t getActiveTime() const;
    // true if at least one screen is registered
    bool isReady() const {
        return _count != 0;
    }

    // shows a screen, index is wrapped, the widget tree is built and the first update runs
    bool show(uint8_t index);
    bool showNext();
    bool showPrev();

    // automatic rotation, 0 turns it off, a screen can override it with getScreenTime()
    void setRotationTime(uint32_t seconds) {
        _rotationTime = seconds;
    }
    uint32_t getRotationTime() const {
        return _rotationTime;
    }
    // true while the automatic rotation is stopped after a touch (kTouchRotationPause)
    bool isRotationPaused() const {
        return _rotationPause != 0;
    }

    // The screen overview ("all screens" page, LVGLScreenOverview): one tile per registered screen.
    // It is not part of the automatic rotation and it is the default action of the tap and double
    // tap gestures, a screen that handles the gesture itself never reaches it. The manager does
    // not own the object
    void setOverview(LVGLScreen *overview);
    LVGLScreen *getOverview() const {
        return _overview;
    }
    bool isOverviewActive() const {
        return _overviewActive;
    }
    // shows the overview, returns false if none is set or when it is already active
    bool showOverview();
    // leaves the overview and shows the screen it was opened from again, returns false if the
    // overview is not active
    bool closeOverview();

    // time format of the clock in the top bar of the screens, the pages have no configuration
    void setTimeFormat24h(bool format24h) {
        _timeFormat24h = format24h;
    }
    bool getTimeFormat24h() const {
        return _timeFormat24h;
    }

    // true while the input is enabled, the power saving mode turns it off while the display is
    // off and the touch that wakes it up is not passed to the screens
    bool getInputEnabled() const {
        return _inputEnabled;
    }

    // main loop tick: rotation timer, per screen refresh and touch gestures. Call it from the
    // main loop, before WT32_SC01::loop()
    void tick();

    // rebuilds the widget tree of the active screen (after values changed that only create()
    // applies), keeps the rotation timer in sync
    void reload();

    // removes the widget tree of the active screen and calls release(), the registered screens
    // stay registered (used by LVGLPlugin::shutdown())
    void release();

private:
    void _releaseActive();
    void _createActive();
    void _show(uint8_t index);

    // observes the input device and evaluates the gestures on the press/release edges.
    // Polling the device works for every screen and does not depend on the clickable flag
    // or the hit testing of the widgets. Screens that handle a gesture themselves return
    // true from the callback, the manager then leaves the screen alone
    void _handleInput();
    // runs the single tap action once the double tap window has passed
    void _handlePendingTap(uint32_t now);
    // stops the automatic rotation for kTouchRotationPause milliseconds
    void _pauseRotation();

private:
#if LVGL_TOUCH_FEEDBACK
    // touch indicator on the top layer, follows the finger while the panel is touched
    LVGLTouchLens _lens;
#endif
    LVGLScreen *_screens[kMaxScreens]{};
    uint8_t _count{0};
    int8_t _active{-1};
    // page with all screens, shown by the default action of the tap and double tap gestures
    LVGLScreen *_overview{nullptr};
    bool _overviewActive{false};
    // index of the screen the overview was opened from, -1 if there was none
    int8_t _overviewReturn{-1};
    // time format of the clock in the top bar, the pages have no configuration of their own
    bool _timeFormat24h{true};
    bool _pressed{false};
    // cached state of WT32_SC01::getInputEnabled(), used to drop a gesture that was in progress
    // when the input is turned off
    bool _inputEnabled{true};
    lv_point_t _pressPoint{};
    lv_point_t _lastPoint{};
    uint32_t _pressTime{0};
    // a released tap waits for kDoubleTapTime before its action runs
    bool _tapPending{false};
    uint32_t _tapTime{0};
    // position of the pending tap, a tap in the top bar is not passed to the screen
    lv_point_t _tapPoint{};
    uint32_t _rotationTime{kDefaultRotationTime};
    uint32_t _rotationDelay{0};
    // millis() when the automatic rotation resumes, 0 = not paused
    uint32_t _rotationPause{0};
    uint32_t _updateInterval{0};
    uint32_t _lastUpdate{0};
    uint32_t _lastSwitch{0};
};

#endif
