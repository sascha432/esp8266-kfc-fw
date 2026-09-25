/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>

#if IOT_LVGL_SUPPORT

// Test screen for the WT32-SC01: title, resolution, color bars and a touch area.
// It is shown by the lvgl plugin after the display has been initialized.

namespace LVGLTestScreen {

    //! removes all widgets from the screen and creates the test screen
    void create();

    //! removes all widgets from the screen and clears it to black
    void clear();

    //! number of touch events received by the test screen
    uint32_t getPressCount();

    //! last touch position, returns false if no touch has been received yet
    bool getLastPoint(int32_t &x, int32_t &y);

}

#endif
