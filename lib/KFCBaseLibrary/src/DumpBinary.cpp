/**
* Author: sascha_lammers@gmx.de
*/

#include "DumpBinary.h"

extern "C" {

    void __dump_binary(const void *ptr, size_t len, size_t perLine, PGM_P title, uint8_t groupBytes)
    {
        __dump_binary_to(Serial, ptr, len, perLine == DUMP_BINARY_DEFAULTS ? DumpBinary::kPerLineDefault : perLine, title, groupBytes == static_cast<uint8_t>(DUMP_BINARY_DEFAULTS) ? DumpBinary::kGroupBytesDefault : groupBytes);
    }

    void __dump_binary_to(Print &output, const void *ptr, size_t len, size_t perLine, PGM_P title, uint8_t groupBytes)
    {
        DumpBinary d(output, groupBytes == static_cast<uint8_t>(DUMP_BINARY_DEFAULTS) ? DumpBinary::kGroupBytesDefault : groupBytes, perLine == 0 ? len : perLine == DUMP_BINARY_DEFAULTS ? DumpBinary::kPerLineDefault : perLine);
        if (title) {
            output.printf_P(PSTR("%s: %p:%u\n"), title, ptr, len);
        }
        d.dump(ptr, len);
    }

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
            auto ch = (uint8_t)pgm_read_byte(data + i);
            if (isprint(ch)) {
                _output.print((char)ch);
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
            uint8_t ch = pgm_read_byte(data + pos);
            _output.printf_P(PSTR("%02x"), ch);
            if ((pos < end - 1) && (((j % _groupBytes) == 1) || (_groupBytes == 1))) {
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
