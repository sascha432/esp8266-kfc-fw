/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>

double unixTimeToJulianDate(time_t time);
void calcMoon(time_t time, double &moonDay, uint8_t &moonPhase,char &moonPhaseFont, bool upperCase);
const __FlashStringHelper *moonPhaseName(uint8_t moonPhase);
