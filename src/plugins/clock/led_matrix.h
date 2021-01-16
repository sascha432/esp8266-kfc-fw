/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if IOT_LED_MATRIX

class ClockPlugin;

namespace LEDMatrix {

    class LoopOptionsType
    {
    public:
        LoopOptionsType(ClockPlugin &plugin);

        // time since the display was updated
        uint32_t getTimeSinceLastUpdate() const;
        // returns the value of millis() when the object was created
        uint32_t getMillis() const;
        // returns true if _updateRate milliseconds have been passed since last update
        bool doUpdate() const;
        // returns true that the display needs to be redrawn
        // call once per object instance/loop, a second call returns false
        bool doRedraw();

        time_t getNow() const {
            return time(nullptr);
        }

    private:
        ClockPlugin &_plugin;
        uint32_t _millis;
        uint32_t _millisSinceLastUpdate;
    };

};

#endif
