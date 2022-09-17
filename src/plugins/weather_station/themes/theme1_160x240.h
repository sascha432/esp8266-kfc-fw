/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "colors.h"

#define COLORS_ANALOG_CLOCK_BACKGROUND  0x52AA
#define COLORS_BACKGROUND               COLORS_BLACK
#define COLORS_DEFAULT_TEXT             COLORS_WHITE
#define COLORS_DATE                     COLORS_WHITE
#define COLORS_TIME                     COLORS_WHITE
#define COLORS_TIMEZONE                 COLORS_YELLOW
#define COLORS_CITY                     COLORS_CYAN
#define COLORS_INDOOR_ICON              0xD6DA
#define COLORS_CURRENT_WEATHER          COLORS_YELLOW
#define COLORS_TEMPERATURE              COLORS_WHITE
#define COLORS_WEATHER_DESCR            COLORS_YELLOW
#define COLORS_SUN_AND_MOON             COLORS_YELLOW
#define COLORS_SUN_RISE_SET             COLORS_WHITE
#define COLORS_MOON_PHASE               COLORS_WHITE
#define COLORS_WORLD_CLOCK_CITY         COLORS_CYAN
#define COLORS_WORLD_CLOCK_TIME         COLORS_WHITE
#define COLORS_WORLD_CLOCK_TIMEZONE     COLORS_YELLOW
#define COLORS_INFO_TITLE               COLORS_ORANGE
#define COLORS_INFO_DETAILS             COLORS_WHITE
#define COLORS_MOON_PHASE_TOP           COLORS_CYAN
#define COLORS_MOON_PHASE_TIME          COLORS_WHITE
#define COLORS_MOON_PHASE_TIMEZONE      COLORS_YELLOW
#define COLORS_MOON_PHASE_NAME          COLORS_WHITE
#define COLORS_MOON_PHASE_ACTIVE        COLORS_CYAN
#define COLORS_MOON_PHASE_DATETIME      COLORS_YELLOW
#define COLORS_LOCATION                 COLORS_BACKGROUND, COLORS_WHITE, COLORS_YELLOW, COLORS_BLUE

#define FONTS_DEFAULT_SMALL             &DejaVuSans_5pt8b
#define FONTS_DEFAULT_MEDIUM            &Dialog_Bold_7pt8b
#define FONTS_DEFAULT_BIG               &DejaVuSans_Bold_10pt8b

#define FONTS_DATE                      &DejaVuSans_Bold_5pt8b
#define FONTS_TIME                      &DejaVuSans_Bold_10pt8b
#define FONTS_TIMEZONE                  &DejaVuSans_5pt8b

#define FONTS_TEMPERATURE               &DejaVuSans_Bold_10pt8b
#define FONTS_PRESSURE                  &DejaVuSans_Bold_7pt8b

#define FONTS_WEATHER_INDOOR            &DejaVuSans_5pt8b

#define FONTS_CITY                      &Dialog_6pt8b
#define FONTS_WEATHER_DESCR             &Dialog_6pt8b

#define FONTS_SUN_AND_MOON              &DejaVuSans_5pt8b
#define FONTS_MOON_PHASE                &moon_phases14pt7b
#define FONTS_MOON_PHASE_UPPERCASE      true

#define FONTS_MESSAGE_TITLE             &DejaVuSans_Bold_10pt8b
#define FONTS_MESSAGE_TEXT              &Dialog_Bold_7pt8b

// time

#define X_POSITION_DATE                 (TFT_WIDTH / 2) + _offsetX
#define Y_POSITION_DATE                 5 + _offsetY
#define H_POSITION_DATE                 AdafruitGFXExtension::CENTER

#define X_POSITION_TIME                 (TFT_WIDTH / 2) + _offsetX
#define Y_POSITION_TIME                 17 + _offsetY
#define H_POSITION_TIME                 AdafruitGFXExtension::CENTER

#define X_POSITION_TIMEZONE             (TFT_WIDTH / 2) + _offsetX
#define Y_POSITION_TIMEZONE             35 + _offsetY
#define H_POSITION_TIMEZONE             AdafruitGFXExtension::CENTER

// weather

#define X_POSITION_WEATHER_ICON         (2 + _offsetX)
#define Y_POSITION_WEATHER_ICON         (0 + _offsetY)

#define X_POSITION_CITY                 (TFT_WIDTH - 3 + _offsetX)
#define Y_POSITION_CITY                 (3 + _offsetY)
#define H_POSITION_CITY                 AdafruitGFXExtension::RIGHT

#define X_POSITION_TEMPERATURE          (X_POSITION_CITY - 2)
#define Y_POSITION_TEMPERATURE          (17 + _offsetY)
#define H_POSITION_TEMPERATURE          AdafruitGFXExtension::RIGHT

#define X_POSITION_WEATHER_DESCR        X_POSITION_CITY
#define Y_POSITION_WEATHER_DESCR        (35 + _offsetY)
#define H_POSITION_WEATHER_DESCR        AdafruitGFXExtension::RIGHT

// sun and moon

#define X_POSITION_SUN_TITLE            2
#define Y_POSITION_SUN_TITLE            0 + _offsetY
#define H_POSITION_SUN_TITLE            AdafruitGFXExtension::LEFT

#define X_POSITION_MOON_PHASE_NAME      (TFT_WIDTH - X_POSITION_SUN_TITLE)
#define Y_POSITION_MOON_PHASE_NAME      Y_POSITION_SUN_TITLE
#define H_POSITION_MOON_PHASE_NAME      AdafruitGFXExtension::RIGHT

#define X_POSITION_MOON_PHASE_DAYS      X_POSITION_MOON_PHASE_NAME
#define Y_POSITION_MOON_PHASE_DAYS      (21 + _offsetY)
#define H_POSITION_MOON_PHASE_DAYS      H_POSITION_MOON_PHASE_NAME

#define Y_POSITION_MOON_PHASE_PCT       (12 + _offsetY)

#define X_POSITION_SUN_RISE_ICON        (4)
#define Y_POSITION_SUN_RISE_ICON        (10 + _offsetY)

#define X_POSITION_SUN_SET_ICON         X_POSITION_SUN_RISE_ICON
#define Y_POSITION_SUN_SET_ICON         (21 + _offsetY)

#define X_POSITION_SUN_RISE             (TFT_WIDTH / 3 + 2)
#define Y_POSITION_SUN_RISE             (12 + _offsetY)
#define H_POSITION_SUN_RISE             AdafruitGFXExtension::RIGHT

#define X_POSITION_SUN_SET              X_POSITION_SUN_RISE
#define Y_POSITION_SUN_SET              (23 + _offsetY)
#define H_POSITION_SUN_SET              H_POSITION_SUN_RISE

#define X_POSITION_MOON_PHASE           (TFT_WIDTH / 2)
#define Y_POSITION_MOON_PHASE           (12 + _offsetY)
#define H_POSITION_MOON_PHASE           AdafruitGFXExtension::LEFT


#define Y_START_POSITION_TIME           (2)
#define Y_END_POSITION_TIME             (45 + Y_START_POSITION_TIME)

// main
#define Y_START_POSITION_WEATHER        (Y_END_POSITION_TIME + 2)
#define Y_END_POSITION_WEATHER          (70 + Y_START_POSITION_WEATHER)

// indoor climate
#define Y_START_POSITION_INDOOR         (Y_END_POSITION_TIME + 2)
#define Y_END_POSITION_INDOOR           (70 + Y_START_POSITION_WEATHER)

// weather forecast
#define Y_START_POSITION_FORECAST       (Y_END_POSITION_TIME + 2)
#define Y_END_POSITION_FORECAST         (TFT_HEIGHT - 1)

// multi timezone display
#define Y_START_POSITION_WORLD_CLOCK    (Y_END_POSITION_TIME + 2)
#define Y_END_POSITION_WORLD_CLOCK      (TFT_HEIGHT - 1)

// moon phase screen
#define Y_START_POSITION_MOON_PHASE     (2)
#define Y_END_POSITION_MOON_PHASE       (TFT_HEIGHT - 1)

// analog clock
#define Y_START_POSITION_ANALOG_CLOCK   (2)
#define Y_END_POSITION_ANALOG_CLOCK     (Y_START_POSITION_INDOOR_BOTTOM) // (70 + Y_START_POSITION_ANALOG_CLOCK)

// system info
#define Y_START_POSITION_INFO           (Y_END_POSITION_TIME + 2)
#define Y_END_POSITION_INFO             (TFT_HEIGHT - 1)

// moon phase
#define Y_START_MOON_PHASE              (Y_END_POSITION_TIME + 2)
#define Y_END_MOON_PHASE                (TFT_HEIGHT - 1)

// debug screen
#define Y_START_POSITION_DEBUG          0
#define Y_END_POSITION_DEBUG            (TFT_HEIGHT - 1)

// sun and moon @ bottom
#define Y_START_POSITION_SUN_MOON       (Y_END_POSITION_WEATHER + 2)
#define Y_END_POSITION_SUN_MOON         (TFT_HEIGHT - 1)

// indoor climate bottom
#define Y_START_POSITION_INDOOR_BOTTOM     (60 + Y_START_POSITION_WEATHER)
#define Y_END_POSITION_INDOOR_BOTTOM       (Y_START_POSITION_SUN_MOON - 1)
#define X_POSITION_WEATHER_INDOOR_TEMP     (_offsetX)
#define X_POSITION_WEATHER_INDOOR_HUMIDITY (TFT_WIDTH / 2 - (3 + _offsetX))
#define X_POSITION_WEATHER_INDOOR_PRESSURE (128 - _offsetX)
#define Y_POSITION_WEATHER_INDOOR_TEMP     (_offsetY)
#define Y_POSITION_WEATHER_INDOOR_HUMIDITY (_offsetY)
#define Y_POSITION_WEATHER_INDOOR_PRESSURE (_offsetY)
