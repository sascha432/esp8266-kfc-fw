/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

// class to convert temperature, relative humidty and due point

#include <Arduino_compat.h>

class EnvComp {
public:
    static double getCompensatedRH(double T, double RH, double realT);

    static double getTD(double T, double RH);
    static double getT(double TD, double RH);
    static double getRH(double TD, double T);
};
