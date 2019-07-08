/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>

class DumpBinary {
public:
    DumpBinary(Print &output);

    // bytes per line
    void setPerLine(uint8_t perLine);

    // space between group of bytes
    void setGroupBytes(uint8_t groupBytes);

    void dump(const uint8_t *data, size_t length);

private:
    Print &_output;
    uint8_t _perLine;
    uint8_t _groupBytes;
};
