/**
* Author: sascha_lammers@gmx.de
*/

#include "DumpBinary.h"

DumpBinary::DumpBinary(Print &output) : _output(output) {
    _perLine = 16;
    _groupBytes = 2;
}

void DumpBinary::setPerLine(uint8_t perLine) {
    _perLine = perLine;
}

void DumpBinary::setGroupBytes(uint8_t groupBytes) {
    _groupBytes = groupBytes;
}

void DumpBinary::dump(const uint8_t * data, size_t length) {
    uint16_t pos = 0;
    while (pos < length) {
        _output.printf_P(PSTR("[%04X] "), pos);
        uint8_t j = 0;
        for (uint16_t i = pos; i < length && j < _perLine; i++, j++) {
            if (isprint(data[i])) {
                _output.print((char)data[i]);
            } else {
                _output.print('.');
            }
        }
        while (j++ <= _perLine) {
            _output.print(' ');
        }
        j = 0;
        for (; pos < length && j < _perLine; pos++, j++) {
            _output.printf_P(PSTR("%02X"), (int)data[pos]);
            if (j % _groupBytes == 1) {
                _output.print(' ');
            }
        }
        _output.println();
    }
}
