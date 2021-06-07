/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>

class DumpBinary {
public:
    static constexpr uint8_t kPerLineDefault = 16;
    static constexpr uint8_t kGroupBytesDefault = 2;
    static constexpr uint8_t kPerLineDisabled = 0xff;

public:
    DumpBinary(Print &output, uint8_t groupBytes = kGroupBytesDefault, uint8_t perLine = kPerLineDefault) : _output(output), _perLine(perLine), _groupBytes(groupBytes) {
    }
    DumpBinary(const String &title, Print &output, uint8_t groupBytes = kGroupBytesDefault, uint8_t perLine = kPerLineDefault) : DumpBinary(output, groupBytes, perLine) {
        if (title.endsWith('\n')) {
            output.print(title);
        }
        else if (title.endsWith(':')) {
            output.println(title);
        }
        else {
            output.print(title);
            output.println(':');
        }
    }

    // bytes per line
    DumpBinary &setPerLine(uint8_t perLine);
    DumpBinary &disablePerLine() {
        return setPerLine(kPerLineDisabled);
    }

    // space between group of bytes
    DumpBinary &setGroupBytes(uint8_t groupBytes);

    DumpBinary &print(const String &str);

    DumpBinary &dump(const uint8_t *data, size_t length, ptrdiff_t offset = 0);

    inline DumpBinary &dump(const void *data, size_t length, ptrdiff_t offset = 0) {
        return dump(reinterpret_cast<const uint8_t *>(data), length, offset);
    }

private:
    Print &_output;
    uint8_t _perLine;
    uint8_t _groupBytes;
};
