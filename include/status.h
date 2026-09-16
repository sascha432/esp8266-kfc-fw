 /**
 * Author: sascha_lammers@gmx.de
 */

#ifndef __STATUS_H_INCLUDED
#define __STATUS_H_INCLUDED

#include <Arduino_compat.h>
#include <Buffer.h>
#include "misc.h"

class WiFiStatus {
public:
	static void stationSSID(Print &out);
	static void softAPSSID(Print &out);
	static void getAddress(Print &out);
	static void getStatus(Print &out);

private:
    static const __FlashStringHelper *getTxPowerStr();
};

#endif
