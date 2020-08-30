/**
* Author: sascha_lammers@gmx.de
*/

#include "DumpBinary.h"

extern "C" {

    void __dump_binary(const void *ptr, size_t len, size_t perLine, PGM_P title)
    {
        DumpBinary d(Serial);
        if (title) {
            Serial.printf_P(PSTR("%s: %p:%u\n"), title, ptr, len);
        }
        if (perLine == 0) {
            perLine = len;
        }
        d.setPerLine((uint8_t)perLine).dump(ptr, len);
    }

}

DumpBinary::DumpBinary(Print &output) : _output(output)
{
    _perLine = 16;
    _groupBytes = 2;
}

DumpBinary::DumpBinary(const String &title, Print &output) : DumpBinary(output)
{
    output.print(title);
    output.println(':');
}

DumpBinary &DumpBinary::setPerLine(uint8_t perLine)
{
    _perLine = perLine;
    return *this;
}

DumpBinary &DumpBinary::setGroupBytes(uint8_t groupBytes)
{
    _groupBytes = groupBytes;
    return *this;
}

DumpBinary &DumpBinary::print(const String &str)
{
    _output.print(str);
    return *this;
}

DumpBinary &DumpBinary::dump(const uint8_t *data, size_t length, ptrdiff_t offset)
{
    ptrdiff_t end = length + offset;
    if (end - offset < 1) {
        return *this;
    }
    uint8_t perLine = _perLine ? _perLine : (end - offset);
    ptrdiff_t pos = offset;
    while (pos < end) {
        _output.printf_P(PSTR("[%04X] "), pos);
        uint8_t j = 0;
        for (ptrdiff_t i = pos; i < end && j < perLine; i++, j++) {
            char ch = pgm_read_byte(data + i);
            if (isprint(ch)) {
                _output.print(ch);
            } else {
                _output.print('.');
            }
        }
        if (perLine == kPerLineDisabled) {
            _output.print(' ');
        }
        else {
            while (j++ <= perLine) {
                _output.print(' ');
            }
        }
        j = 0;
        for (; pos < end && j < perLine; pos++, j++) {
            int ch = pgm_read_byte(data + pos);
            _output.printf_P(SPGM(_02x, "%02x"), ch);
            if ((pos < end - 1) && ((j % _groupBytes) == 1)) {
                _output.print(' ');
            }
        }
        if (_perLine != kPerLineDisabled) {
            _output.println();
        }
    }
#if ESP8266
    delay(1);
#endif
    return *this;
}
