/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

// LVGL display plugin for the WT32-SC01 (panel + touch driver in src/plugins/lvgl/devices/wt32_sc01)
//
//  - setup()     initializes the panel and LVGL and shows the test screen
//  - shutdown()  clears the display
//  - getStatus() reports the state of the display, the touch controller, the brightness and the
//                power saving mode
//  - form        "lvgl", display brightness 0-100% and the power saving mode
//
// The LVGL handler runs from the main loop, the brightness is stored in the display
// configuration (KFCConfigurationClasses::Plugins::DisplayConfigNS).
//
// Every change of the brightness is faded to the new level, the full 0-100% range takes 3 seconds.
// After 'power_saving_timeout' seconds without user activity the backlight is dimmed to
// 'power_saving_level' and after 'standby_timeout' seconds it is turned off. A touch or any other
// wakeup source (see notifyActivity()) brings it back, the touch that wakes the display up is not
// passed to the UI.

#if IOT_LVGL_SUPPORT

#ifndef DEBUG_LVGL
#    define DEBUG_LVGL 0
#endif

#include <Arduino_compat.h>
#include "plugins.h"
#include <kfc_fw_config.h>
#include "lvgl_screen.h"

#if DEBUG_LVGL
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

class LVGLPlugin : public PluginComponent {
public:
    LVGLPlugin();

    virtual void setup(SetupModeType mode, const PluginComponents::DependenciesPtr &dependencies) override;
    virtual void shutdown() override;
    virtual void reconfigure(const String &source) override;

    virtual void getStatus(Print &output) override;
    virtual void createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request) override;

    // backlight brightness in percent, 0-100 (the level that is displayed right now)
    static uint8_t getBrightness();

    // true while the backlight is dimmed / turned off because there was no user activity
    static bool isDimmed();
    static bool isStandby();

    // Reports user activity, it leaves the power saving mode and fades the display back to the
    // configured brightness. The touch screen calls it and another plugin can use it for any other
    // wakeup source (a motion or presence sensor for example)
    static void notifyActivity();

    // applies a brightness immediately without fading, used by the debug screenshot tool
    static void setBrightness(uint8_t percent);

    // Screen brightness in percent as it is stored in the display configuration. getBrightness()
    // returns the level that is on screen right now, which is the dimmed one while the power
    // saving mode is active - the quick settings of an application screen edit this value
    static uint8_t getConfiguredBrightness();
    // stores the screen brightness and applies it (fades to the new level)
    static void setConfiguredBrightness(uint8_t percent);

    // Power saving settings of the display configuration (the "Power Saving" group of the form),
    // the same values the quick settings edit. Every setter stores the value and restarts the
    // idle timers, so the new timeout is counted from now
    static uint8_t getPowerSavingLevel();
    static void setPowerSavingLevel(uint8_t percent);
    static uint32_t getPowerSavingTimeout();
    static void setPowerSavingTimeout(uint32_t seconds);
    static uint32_t getStandbyTimeout();
    static void setStandbyTimeout(uint32_t seconds);

    // Turns the display off at once, the standby state of the power saving mode: the backlight is
    // faded out and the touch that wakes the display up is not passed to the UI
    static void sleep();

    // screen manager of the LVGL UI, application plugins register their screens here
    // (only call it from the main loop)
    static LVGLScreenManager &screens();

    virtual void createMenu();

private:
    // initializes the panel and LVGL, returns true if the display is ready to use
    bool _initialize();

    // reads the brightness and the power saving settings from the display configuration and
    // applies the backlight. fade=false applies it without fading, used during the setup when the
    // panel is not ready yet and the main loop is not running. Static, so the setters above (which
    // are static as well) can re-read and apply the value they stored
    static void _applyBrightness(bool fade = true);
};

#if DEBUG_LVGL
#    include <debug_helper_disable.h>
#endif

#endif
