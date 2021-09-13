/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if ILI9341_DRIVER

#define COLORS_YELLOW                   ILI9341_YELLOW
#define COLORS_RED                      ILI9341_RED
#define COLORS_WHITE                    ILI9341_WHITE
#define COLORS_BLACK                    ILI9341_BLACK
#define COLORS_CYAN                     ILI9341_CYAN
#define COLORS_ORANGE                   ILI9341_ORANGE
#define COLORS_BLUE                     ILI9341_BLUE

#else

#define COLORS_YELLOW                   ST77XX_YELLOW
#define COLORS_RED                      ST77XX_RED
#define COLORS_WHITE                    ST77XX_WHITE
#define COLORS_BLACK                    ST77XX_BLACK
#define COLORS_CYAN                     ST77XX_CYAN
#define COLORS_ORANGE                   ST77XX_ORANGE
#define COLORS_BLUE                     ST77XX_BLUE

#endif
