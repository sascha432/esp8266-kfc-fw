/**
 * Author: sascha_lammers@gmx.de
 */

#include "EnvComp.h"
#include <math.h>

double EnvComp::getCompensatedRH(double T, double RH, double realT)
{
    double TD = getTD(T, RH);
    return getRH(TD, realT);
}

double EnvComp::getTD(double T, double RH)
{
    return 243.04 * (log(RH / 100) + ((17.625 * T) / (243.04 + T))) / (17.625 - log(RH / 100) - ((17.625 * T) / (243.04 + T)));
}

double EnvComp::getT(double TD, double RH)
{
    return 243.04 * (((17.625 * TD) / (243.04 + TD)) - log(RH / 100)) / (17.625 + log(RH / 100) - ((17.625 * TD) / (243.04 + TD)));
}

double EnvComp::getRH(double TD, double T)
{
    return 100.0 * (exp((17.625 * TD) / (243.04 + TD)) / exp((17.625 * T) / (243.04 + T)));
}
