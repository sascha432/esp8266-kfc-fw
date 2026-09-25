/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

// LVGL display plugin for the WT32-SC01 (panel + touch driver in src/plugins/lvgl/devices/wt32_sc01)
//
//  - setup()     initializes the panel and LVGL and shows the test screen
//  - shutdown()  clears the display
//  - getStatus() reports the state of the display, the touch controller and the brightness
//  - form        "lvgl", display brightness 0-100%
//
// The LVGL handler runs from the main loop, the brightness is stored in the display
// configuration (KFCConfigurationClasses::Plugins::DisplayConfigNS).

#if IOT_LVGL_SUPPORT

#ifndef DEBUG_LVGL
#    define DEBUG_LVGL 0
#endif

#include <Arduino_compat.h>
#include "plugins.h"
#include <kfc_fw_config.h>

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

    //! backlight brightness in percent, 0-100
    static uint8_t getBrightness();

private:
    //! initializes the panel and LVGL, returns true if the display is ready to use
    bool _initialize();

    //! reads the brightness from the display configuration and applies it to the backlight
    void _applyBrightness();

private:
    static uint8_t _brightness; // percent
    static uint8_t _backlight; // 0-255
};

#if DEBUG_LVGL
#    include <debug_helper_disable.h>
#endif

#endif
