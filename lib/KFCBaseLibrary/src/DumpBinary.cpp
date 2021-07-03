/**
* Author: sascha_lammers@gmx.de
*/

#include "DumpBinary.h"

extern "C" {

    void __dump_binary(const void *ptr, int len, size_t perLine, PGM_P title, uint8_t groupBytes)
    {
        __dump_binary_to(Serial, ptr, len, perLine == DUMP_BINARY_DEFAULTS ? DumpBinary::kPerLineDefault : perLine, title, groupBytes == static_cast<uint8_t>(DUMP_BINARY_DEFAULTS) ? DumpBinary::kGroupBytesDefault : groupBytes);
    }

    void __dump_binary_to(Print &output, const void *ptr, int len, size_t perLine, PGM_P title, uint8_t groupBytes)
    {
        DumpBinary d(output, groupBytes == static_cast<uint8_t>(DUMP_BINARY_DEFAULTS) ? DumpBinary::kGroupBytesDefault : groupBytes, perLine == 0 ? static_cast<uint8_t>(len) : perLine == DUMP_BINARY_DEFAULTS ? DumpBinary::kPerLineDefault : static_cast<uint8_t>(perLine));
        if (title) {
            output.printf_P(PSTR("%s: %p:%d\n"), title, ptr, len);
        }
        d.dump(ptr, std::max(0, len));
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

DumpBinary &DumpBinary::dump(const uint8_t *data, size_t length)
{
    ptrdiff_t end = length;
    uint8_t perLine = _perLine ? _perLine : end;
    ptrdiff_t pos = 0;
    while (pos < end) {
        _output.printf_P(PSTR("[%08X] "), pos + _displayOffset);
        uint8_t j = 0;
        for (ptrdiff_t i = pos; i < end && j < perLine; i++, j++) {
            int ch = pgm_read_byte_safe(data + i);
            _output.print(ch == -1 ? '!' : isprint(ch) ? static_cast<char>(ch) : '.');
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
            int ch = pgm_read_byte_safe(data + pos);
            _output.printf_P(ch == -1 ? PSTR("??") : PSTR("%02x"), static_cast<uint8_t>(ch));
            if (
                (_groupBytes == 1) ||
                (_groupBytes && (pos < end - 1) && (j < perLine - 1) && (((j % _groupBytes) == _groupBytes - 1)))
            ) {
                _output.print(' ');
            }
        }
        if (_perLine != kPerLineDisabled) {
            _output.println();
        }
    }
    if (_perLine == kPerLineDisabled) {
        _output.println();
    }
    delayMicroseconds(100);
    return *this;
}
