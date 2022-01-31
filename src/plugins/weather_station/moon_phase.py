#
# Author: sascha_lammers@gmx.de
#

import time

# this script is to find the correct values
# see src\plugins\weather_station\moon_phase.cpp for further explation of the issue

#29.5305888531 + 0.00000021621T − 3.64×10−10T2 where T = (JD − 2451545.0)/36525 and JD

EPOCH = 2444238.5
MOON_CYCLE = 29.53058868
MOON_CYCLE = 29.5305888531
MOON_CYCLE = 29.53058770576

def T(time):
    return (36525 * time) + 2451545;

def unixTimeToJulianDate(time):
    # return (time / 86400.0) + 2440587.5;
    t = T(time)
    t = (time / 86400.0) + 2440587.5;
    return 29.5305888531 + (0.00000021621 * t) - (3.64E-10 * (t * t))

def calcMoon(time):
    moonCycle = 29.5305888531 #29.53;
    # value = (unixTimeToJulianDate(time) - 2244116.75) / moonCycle
    # value2 = int(value)
    # value -= int(value)
    # value = (unixTimeToJulianDate(time) - 2244116.75) / moonCycle
    # value -= int(value)
    value = unixTimeToJulianDate(time)
    value2 = int(value)
    #value -= int(value2)
    moonDay = value * moonCycle;                            # convert to days
    moonPhase = int((value * 8) + 0.5) & 0x07;        # convert to moon phase (0-7)
    return ((value2 * 100 / value), moonDay, moonPhase)

moon_phases = (
    "Full Moon",
    "Waning Gibbous",
    "Last Quarter",
    "Old Crescent",
    "New Moon",
    "New Crescent",
    "First Quarter",
    "Waxing Gibbous"
)

unix_time = int(time.time() - (3600 * 7));
# unix_time = 1634741760;
data = calcMoon(unix_time)

print(moon_phases[data[2] & 0x07], data)

