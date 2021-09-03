/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>

float unixTimeToJulianDate(time_t time);
void calcMoon(time_t time, float &moonDay, uint8_t &moonPhase,char &moonPhaseFont, bool upperCase);
const __FlashStringHelper *moonPhaseName(uint8_t moonPhase);
