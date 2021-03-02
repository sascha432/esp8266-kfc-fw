/**
 * Author: sascha_lammers@gmx.de
 */

#include "DimmerTwoWireEx.h"
#include "serial_handler.h"

#if IOT_DIMMER_MODULE_INTERFACE_UART

volatile bool DimmerTwoWireEx::_locked = false;

DimmerTwoWireEx::DimmerTwoWireEx(Stream &stream) :
    DimmerTwoWireClass(stream, SerialHandler::Wrapper::pollSerial)
{
}

#endif
