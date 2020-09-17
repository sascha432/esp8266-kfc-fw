/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include "PrintArgs.h"

// wrapper for Print objects and direct outpout to a print interface

class PrintArgsPrintWrapper : public PrintArgs
{
public:
    PrintArgsPrintWrapper(Print &output) {
        _bufferPtr = reinterpret_cast<uint8_t *>(&output);
        _position = PrintArgs::PrintObject;
    }
    PrintArgsPrintWrapper(const PrintArgsPrintWrapper &wrapper) {
        _bufferPtr = wrapper._bufferPtr;
        _position = wrapper._position;
    }
};
