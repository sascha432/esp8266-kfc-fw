/**
 * Author: sascha_lammers@gmx.de
 */

#include "lvgl_screen.h"

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
#include "devices/wt32_sc01/wt32_sc01.h"

bool LVGLScreenManager::add(LVGLScreen *screen)
{
    if (!screen || _count >= kMaxScreens) {
        return false;
    }
    for (uint8_t i = 0; i < _count; i++) {
        if (_screens[i] == screen) {
            return false; // already registered
        }
    }
    _screens[_count++] = screen;
    __LDBG_printf("added screen '%s' (#%u)", screen->getName(), static_cast<unsigned>(_count - 1));
    return true;
}

bool LVGLScreenManager::remove(LVGLScreen *screen)
{
    if (!screen) {
        return false;
    }
    for (uint8_t i = 0; i < _count; i++) {
        if (_screens[i] != screen) {
            continue;
        }
        if (_overviewActive) {
            // the tiles of the overview refer to the registered screens, leave it first
            closeOverview();
        }
        if (_active == static_cast<int8_t>(i)) {
            _releaseActive();
            _active = -1;
        }
        for (uint8_t j = i; j + 1 < _count; j++) {
            _screens[j] = _screens[j + 1];
        }
        _screens[--_count] = nullptr;
        if (_active > static_cast<int8_t>(i)) {
            _active--;
        }
        if (_overviewReturn > static_cast<int8_t>(i)) {
            _overviewReturn--;
        }
        else if (_overviewReturn == static_cast<int8_t>(i)) {
            _overviewReturn = -1;
        }
        return true;
    }
    return false;
}

void LVGLScreenManager::removeAll()
{
    _releaseActive();
    _active = -1;
    _overviewActive = false;
    _overviewReturn = -1;
    _pressed = false;
    _tapPending = false;
    _rotationPause = 0;
    for (uint8_t i = 0; i < _count; i++) {
        _screens[i] = nullptr;
    }
    _count = 0;
}

LVGLScreen *LVGLScreenManager::get(uint8_t index) const
{
    return (index < _count) ? _screens[index] : nullptr;
}

LVGLScreen *LVGLScreenManager::getActiveScreen() const
{
    if (_overviewActive) {
        return _overview;
    }
    return (_active >= 0 && _active < static_cast<int8_t>(_count)) ? _screens[_active] : nullptr;
}

uint32_t LVGLScreenManager::getActiveTime() const
{
    if (_active < 0) {
        return 0;
    }
    return static_cast<uint32_t>(millis() - _lastSwitch) / 1000;
}

bool LVGLScreenManager::show(uint8_t index)
{
    if (!_count || !WT32_SC01::display()) {
        return false;
    }
    if (index >= _count) {
        index = 0;
    }
    if (_active == static_cast<int8_t>(index) && !_overviewActive) {
        return false;
    }
    _show(index);
    return true;
}

bool LVGLScreenManager::showNext()
{
    if (_count < 2) {
        return false;
    }
    return show(static_cast<uint8_t>((_active + 1) % _count));
}

bool LVGLScreenManager::showPrev()
{
    if (_count < 2) {
        return false;
    }
    return show(static_cast<uint8_t>((_active + _count - 1) % _count));
}

void LVGLScreenManager::_show(uint8_t index)
{
    _releaseActive();
    _overviewActive = false;
    _active = static_cast<int8_t>(index);
    _createActive();
}

void LVGLScreenManager::setOverview(LVGLScreen *overview)
{
    if (_overview == overview) {
        return;
    }
    if (_overviewActive) {
        _releaseActive();
        _overviewActive = false;
    }
    _overview = overview;
    _overviewReturn = -1;
    __LDBG_printf("overview %s", overview ? "registered" : "removed");
}

bool LVGLScreenManager::showOverview()
{
    if (!_overview || _overviewActive || !WT32_SC01::display()) {
        return false;
    }
    // remember the screen the overview is opened from
    _overviewReturn = _active;
    _releaseActive();
    _overviewActive = true;
    _createActive();
    return true;
}

bool LVGLScreenManager::closeOverview()
{
    if (!_overviewActive) {
        return false;
    }
    const auto index = _overviewReturn;
    _overviewReturn = -1;
    if (index >= 0 && index < static_cast<int8_t>(_count)) {
        _show(static_cast<uint8_t>(index));
        return true;
    }
    if (_count) {
        _show(0);
        return true;
    }
    // no screens registered
    _releaseActive();
    _overviewActive = false;
    return false;
}

void LVGLScreenManager::_releaseActive()
{
#if LVGL_TOUCH_FEEDBACK
    // the widget tree below the finger is about to be removed
    _lens.hide();
#endif
    auto screen = getActiveScreen();
    if (screen) {
        screen->release();
    }
    auto disp = WT32_SC01::display();
    if (disp) {
        lv_obj_clean(lv_disp_get_scr_act(disp));
    }
}

void LVGLScreenManager::_createActive()
{
    auto disp = WT32_SC01::display();
    auto screen = getActiveScreen();
    if (!disp || !screen) {
        return;
    }
    auto activeScreen = lv_disp_get_scr_act(disp);
    lv_obj_clean(activeScreen);
    screen->create(activeScreen);

    _pressed = false;
    _tapPending = false;
    auto now = millis();
    _lastSwitch = now;
    _lastUpdate = now;
    _updateInterval = screen->getRefreshInterval();
    auto screenTime = screen->getScreenTime() ? screen->getScreenTime() : _rotationTime;
    // the overview stays until a screen is selected, it is not part of the automatic rotation.
    // A screen that returns kScreenTimeNoRotation is pinned the same way (delay 0 = no rotation)
    _rotationDelay = (_overviewActive || screenTime == kScreenTimeNoRotation) ? 0 : screenTime * 1000;
    __LDBG_printf("screen '%s' active, refresh=%ums rotation=%us", screen->getName(),
                  static_cast<unsigned>(_updateInterval), static_cast<unsigned>(screenTime));
    screen->update();
}

void LVGLScreenManager::tick()
{
    if (!WT32_SC01::display()) {
        return;
    }
    // a screen registered after the display has been initialized is shown as soon as it appears
    if (_active < 0) {
        if (_count) {
            _show(0);
        }
        return;
    }
    _handleInput();
    if (!_inputEnabled) {
        // the display is turned off (power saving mode), the screen is not refreshed and the
        // automatic rotation is stopped
        return;
    }

    auto now = millis();
    _handlePendingTap(now);
    if (_rotationPause && static_cast<int32_t>(now - _rotationPause) >= 0) {
        // the pause after a touch is over, the active screen gets a full rotation time
        _rotationPause = 0;
        _lastSwitch = now;
        __LDBG_printf("auto rotation resumed");
    }

    auto screen = getActiveScreen();
    if (screen && _updateInterval && static_cast<int32_t>(now - _lastUpdate) >= static_cast<int32_t>(_updateInterval)) {
        _lastUpdate = now;
        screen->update();
    }
    if (!_rotationPause && _rotationDelay && _count > 1 && static_cast<int32_t>(now - _lastSwitch) >= static_cast<int32_t>(_rotationDelay)) {
        showNext();
    }
}

void LVGLScreenManager::release()
{
    _releaseActive();
    _pressed = false;
    _tapPending = false;
    _active = -1;
    _overviewActive = false;
    _overviewReturn = -1;
#if LVGL_TOUCH_FEEDBACK
    _lens.destroy();
#endif
}

void LVGLScreenManager::reload()
{
    if (_active < 0) {
        return;
    }
    _releaseActive();
    _createActive();
}

void LVGLScreenManager::_handleInput()
{
    // The power saving mode of the plugin turns the input off while the display is off
    // (WT32_SC01::setInputEnabled), the touch that wakes the display up must not reach the UI. A
    // gesture that was in progress is dropped, the input state is read directly from the panel
    // and it keeps reporting the touch while the input is off
    if (!WT32_SC01::getInputEnabled()) {
        if (_inputEnabled) {
            _inputEnabled = false;
            _pressed = false;
            _tapPending = false;
#if LVGL_TOUCH_FEEDBACK
            _lens.hide();
#endif
            __LDBG_printf("input disabled, the display is off");
        }
        return;
    }

    if (!_inputEnabled) {
        // the display was turned on again, the active screen gets a full refresh/rotation time
        _inputEnabled = true;
        auto now = millis();
        _lastSwitch = now;
        _lastUpdate = now;
        __LDBG_printf("input enabled");
    }

    // the input device is read by LVGL (LV_INDEV_DEF_READ_PERIOD), the manager only observes
    // the state of the device. LVGL 8.4 has no lv_indev_get_state(), so the raw state comes
    // from the panel driver - this also keeps the gestures independent of the widget tree
    int32_t x;
    int32_t y;
    if (WT32_SC01::isTouched() && WT32_SC01::getLastTouch(x, y)) {
        lv_point_t point;
        point.x = static_cast<lv_coord_t>(x);
        point.y = static_cast<lv_coord_t>(y);
        if (!_pressed) {
            _pressed = true;
            _pressTime = millis();
            _pressPoint = point;
#if LVGL_TOUCH_FEEDBACK
            // show the touch indicator below the finger
            _lens.press(point);
#endif
        }
#if LVGL_TOUCH_FEEDBACK
        else {
            _lens.track(point);
        }
#endif
        _lastPoint = point;
        return;
    }
    if (!_pressed) {
        return;
    }
    _pressed = false;

#if LVGL_TOUCH_FEEDBACK
    // fade the lens out, a swipe that changes the screen hides it right away
    _lens.release();
#endif

    // the touch screen was used, the automatic rotation stops for a while
    _pauseRotation();

    const auto now = millis();
    const auto dx = _lastPoint.x - _pressPoint.x;
    const auto dy = _lastPoint.y - _pressPoint.y;
    const auto duration = static_cast<uint32_t>(now - _pressTime);
    auto screen = getActiveScreen();

    if (LV_ABS(dx) < kTouchTolerance && LV_ABS(dy) < kTouchTolerance) {
        if (duration >= kLongPressTime) {
            __LDBG_printf("long press at %d,%d (%ums)", static_cast<int>(_pressPoint.x), static_cast<int>(_pressPoint.y), static_cast<unsigned>(duration));
            _tapPending = false;
            if (screen && screen->onLongPress()) {
                return;
            }
        }
        else if (duration <= kTapMaxTime) {
            // the tap is only evaluated after the double tap window has passed, a second tap
            // within the window is a double tap and runs its reserved action instead
            if (_tapPending && static_cast<uint32_t>(now - _tapTime) <= kDoubleTapTime) {
                _tapPending = false;
                __LDBG_printf("double tap at %d,%d (%ums)", static_cast<int>(_pressPoint.x), static_cast<int>(_pressPoint.y), static_cast<unsigned>(duration));
                if (screen && screen->onDoubleTap()) {
                    return;
                }
                // the default action of a double tap is the screen overview as well
                if (_overview && !_overviewActive) {
                    showOverview();
                }
            }
            else {
                _tapPending = true;
                _tapTime = now;
                _tapPoint = _pressPoint;
                __LDBG_printf("tap at %d,%d (%ums), waiting for a double tap", static_cast<int>(_pressPoint.x), static_cast<int>(_pressPoint.y), static_cast<unsigned>(duration));
            }
        }
        return;
    }

    // a swipe ends a pending tap
    _tapPending = false;

    if (LV_ABS(dx) >= kSwipeDistance && LV_ABS(dx) > LV_ABS(dy)) {
        const auto direction = (dx < 0) ? SwipeDirection::LEFT : SwipeDirection::RIGHT;
        __LDBG_printf("swipe %s (%d,%d)", (direction == SwipeDirection::LEFT) ? "left" : "right", static_cast<int>(dx), static_cast<int>(dy));
        if (screen && screen->onSwipe(direction)) {
            return;
        }
        // a swipe to the right goes to the next screen, to the left to the previous one
        if (direction == SwipeDirection::RIGHT) {
            showNext();
        }
        else {
            showPrev();
        }
        return;
    }

    if (LV_ABS(dy) >= kSwipeDistance) {
        // a swipe up/down has no default action, only the screen can handle it
        const auto direction = (dy < 0) ? SwipeDirection::UP : SwipeDirection::DOWN;
        const auto name = (direction == SwipeDirection::UP) ? "up" : "down";
        __LDBG_printf("swipe %s (%d,%d)", name, static_cast<int>(dx), static_cast<int>(dy));
        if (!screen || !screen->onSwipe(direction)) {
            __LDBG_printf("swipe %s ignored", name);
        }
    }
}

void LVGLScreenManager::_handlePendingTap(uint32_t now)
{
    // the double tap window has passed and no second tap followed, run the single tap action
    if (!_tapPending || static_cast<uint32_t>(now - _tapTime) <= kDoubleTapTime) {
        return;
    }
    _tapPending = false;

    // The top bar is the shortcut to the screen overview: the screen cannot consume a tap there
    // (the title and the clock belong to the manager, not to the widgets of a screen). A screen
    // without a top bar (hasTopBar() == false) gets the tap, it has tiles of its own there.
    auto screen = getActiveScreen();
    const bool header = (_tapPoint.y < LVGLUI::kBarHeight) && (!screen || screen->hasTopBar());
    if (!header && screen && screen->onTap()) {
        return;
    }
    // the default action of a single tap is the screen overview, without an overview it is the
    // same as a swipe to the right
    if (_overview && !_overviewActive) {
        showOverview();
        return;
    }
    if (!_overviewActive) {
        showNext();
    }
}

void LVGLScreenManager::_pauseRotation()
{
    _rotationPause = millis() + kTouchRotationPause;
    __LDBG_printf("auto rotation paused for %us", static_cast<unsigned>(kTouchRotationPause / 1000));
}

#endif
