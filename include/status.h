 /**
 * Author: sascha_lammers@gmx.de
 */

#ifndef __STATUS_H_INCLUDED
#define __STATUS_H_INCLUDED

#include <Arduino_compat.h>
#include <Buffer.h>
#include "misc.h"

void WiFi_Station_SSID(Print &out);
void WiFi_SoftAP_SSID(Print &out);
void WiFi_get_address(Print &out);
void WiFi_get_status(Print &out);

#endif
