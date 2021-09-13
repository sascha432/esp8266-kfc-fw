/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "../src/plugins/mqtt/mqtt_json.h"

#ifndef DEBUG_DISPLAY_PLUGIN
#    define DEBUG_DISPLAY_PLUGIN 1
#endif

#ifndef DISPLAY_ADAFRUIT
#    define DISPLAY_ADAFRUIT 0
#endif

#ifndef DISPLAY_TFT_ESPI
#    define DISPLAY_TFT_ESPI 0
#endif

#ifndef DISPLAY_PLUGIN_WIDTH
#    error DISPLAY_PLUGIN_WIDTH not defined
#endif

#ifndef DISPLAY_PLUGIN_HEIGHT
#    error DISPLAY_PLUGIN_HEIGHT not defined
#endif

#if DISPLAY_PLUGIN_ADAFRUIT
#    include <Adafruit_GFX.h>
#    include <Adafruit_SPITFT.h>
#endif

#if DISPLAY_TFT_ESPI
#    include <TFT_eSPI.h>
#endif

namespace Display {

    using UnnamedObject = MQTT::Json::UnnamedObject;

    #if DISPLAY_PLUGIN_ADAFRUIT
        using DisplayType = GFXExtension<DISPLAY_PLUGIN_TFT_TYPE>;
    #endif
    #if DISPLAY_TFT_ESPI
        using DisplayType = TFT_eSPI;
    #endif

}
