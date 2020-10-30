/**
 * Author: sascha_lammers@gmx.de
 */

#include "DimmerTwoWireEx.h"
#include "serial_handler.h"

#if IOT_DIMMER_MODULE_INTERFACE_UART

DimmerTwoWireEx::DimmerTwoWireEx(Stream &stream) :
    DimmerTwoWireClass(stream, SerialHandler::Wrapper::pollSerial), _locked(false)
{

}

#endif
