/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>

class DumpBinary {
public:
    DumpBinary(Print &output);
    DumpBinary(const String &title, Print &output);

    // bytes per line
    DumpBinary &setPerLine(uint8_t perLine);

    // space between group of bytes
    DumpBinary &setGroupBytes(uint8_t groupBytes);

    DumpBinary &print(const String &str);

    DumpBinary &dump(const uint8_t *data, size_t length, ptrdiff_t offset = 0);

private:
    Print &_output;
    uint8_t _perLine;
    uint8_t _groupBytes;
};
