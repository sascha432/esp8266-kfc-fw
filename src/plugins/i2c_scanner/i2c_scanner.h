/**
  Author: sascha_lammers@gmx.de
*/

#if !I2CSCANNER_PLUGIN
#error Plugin not active
#endif

#pragma once

#include <Arduino.h>

void scanPorts(Print &output);
