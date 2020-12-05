/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include <kfc_fw_config.h>
#include <WiFiUdp.h>
#include "remote_action.h"

#if DEBUG_IOT_REMOTE_CONTROL
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using namespace RemoteControl;

void ActionUDP::execute(Callback callback)
{
    WiFiUDP udp;

    if (udp.beginPacket(getResolved(), _port)) {
        if (udp.write(_payload.data(), _payload.size()) == _payload.size()) {
            if (udp.endPacket()) {
                callback(true);
                return;
            }
        }
    }
    callback(false);
}
