/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include <array>
#include <memory>
#include <PrintString.h>
#include <PrintColoredString.h>


#define PRINT_COLOR_ARRAY(name, str) \
    ColorCodes::ColorMap(F(_STRINGIFY(name)), F(str))

ColorCodes::ColorMapArray &&ColorCodes::initColors()
{
    return std::move(ColorMapArray({
        PRINT_COLOR_ARRAY( reset        , "\033[m"     ),
        PRINT_COLOR_ARRAY( bold         , "\033[1m"    ),
        PRINT_COLOR_ARRAY( red          , "\033[31m"   ),
        PRINT_COLOR_ARRAY( green        , "\033[32m"   ),
        PRINT_COLOR_ARRAY( yellow       , "\033[33m"   ),
        PRINT_COLOR_ARRAY( blue         , "\033[34m"   ),
        PRINT_COLOR_ARRAY( magenta      , "\033[35m"   ),
        PRINT_COLOR_ARRAY( cyan         , "\033[36m"   ),
        PRINT_COLOR_ARRAY( bold_red     , "\033[1;31m" ),
        PRINT_COLOR_ARRAY( bold_green   , "\033[1;32m" ),
        PRINT_COLOR_ARRAY( bold_yellow  , "\033[1;33m" ),
        PRINT_COLOR_ARRAY( bold_blue    , "\033[1;34m" ),
        PRINT_COLOR_ARRAY( bold_magenta , "\033[1;35m" ),
        PRINT_COLOR_ARRAY( bold_cyan    , "\033[1;36m" ),
        PRINT_COLOR_ARRAY( bg_red       , "\033[41m"   ),
        PRINT_COLOR_ARRAY( bg_green     , "\033[42m"   ),
        PRINT_COLOR_ARRAY( bg_yellow    , "\033[43m"   ),
        PRINT_COLOR_ARRAY( bg_blue      , "\033[44m"   ),
        PRINT_COLOR_ARRAY( bg_magenta   , "\033[45m"   ),
        PRINT_COLOR_ARRAY( bg_cyan      , "\033[46m"   )
    }));
}

ColorCodes::ColorMapArray ColorCodes::_codes = std::move(ColorCodes::initColors());
