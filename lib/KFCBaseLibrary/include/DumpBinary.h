/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>

class DumpBinary {
public:
    static constexpr uint8_t kPerLineDefault = 16;
    static constexpr uint8_t kGroupBytesDefault = 4;
    static constexpr uint8_t kPerLineDisabled = 0xff;

public:
    DumpBinary(Print &output, uint8_t groupBytes = kGroupBytesDefault, uint8_t perLine = kPerLineDefault, uintptr_t displayOffset = 0) :
        _output(output),
        _newLine(String('\n')),
        _displayOffset(displayOffset),
        _perLine(perLine),
        _groupBytes(groupBytes)
    {
    }

    DumpBinary(const String &title, Print &output, uint8_t groupBytes = kGroupBytesDefault, uint8_t perLine = kPerLineDefault, uintptr_t displayOffset = 0) :
        DumpBinary(output, groupBytes, perLine, displayOffset)
    {
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

    DumpBinary &setDisplayOffset(intptr_t ofs) {
        _displayOffset = ofs;
        return *this;
    }

    DumpBinary &setNewLine(const String &newLine) {
        _newLine = newLine;
        return *this;
    }

    // space between group of bytes
    DumpBinary &setGroupBytes(uint8_t groupBytes);

    DumpBinary &print(const String &str);

    DumpBinary &dump(const uint8_t *data, size_t length);

    inline DumpBinary &dump(const void *data, size_t length) {
        return dump(reinterpret_cast<const uint8_t *>(data), length);
    }

private:
    Print &_output;
    String _newLine;
    uintptr_t _displayOffset;
    uint8_t _perLine;
    uint8_t _groupBytes;
};
