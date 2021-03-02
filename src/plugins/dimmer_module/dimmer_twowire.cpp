/**
 * Author: sascha_lammers@gmx.de
 */

#include "dimmer_twowire.h"
#include "serial_handler.h"

#if IOT_DIMMER_MODULE_INTERFACE_UART

namespace Dimmer {

    volatile bool Dimmer::TwoWireEx::_locked = false;

    TwoWireEx::TwoWireEx(Stream &stream) :
        TwoWireBase(stream, SerialHandler::Wrapper::pollSerial)
    {
    }

}

#endif
