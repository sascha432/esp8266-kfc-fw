/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <PrintString.h>

#define PRINT_COLOR_METHOD(name, str) \
    size_t name() { \
        return print(F(str)); \
    }

class ColorCodes {
public:
    class ColorMap {
    public:
        ColorMap() = default;
        ColorMap(const __FlashStringHelper *name, const __FlashStringHelper *sequence) : _name(name), _sequence(sequence) {}
        const __FlashStringHelper *_name;
        const __FlashStringHelper *_sequence;
    };
    using ColorMapArray = std::array<ColorMap, 20>;

    static ColorMapArray &&initColors();

public:
    static ColorMapArray _codes;
};


template<class Ta>
class PrintColored : public Ta, public ColorCodes {
public:
    PrintColored()  : _escape{'{', '}', '\\'}, _escaped(false), _resetNewLine(true) {}

    void setEscape(char begin, char end, char escape) {
        _escape[0] = begin;
        _escape[1] = end;
        _escape[2] = escape;
    }

    void setResetNewLine(bool state) {
        _resetNewLine = state;
    }

    virtual size_t write(uint8_t data) override
    {
        if (data == '\r') {
            return Ta::write(data);
        }
        else if (data == '\n') {
            size_t len = 0;
            if (_resetNewLine) {
                _escaped = false;
                _colorCode = String();
                len += Ta::write_P((PGM_P)_codes[0]._sequence, strlen_P((PGM_P)_codes[0]._sequence));
            }
            return len + Ta::write('\n');
        }
        else if (_colorCode.length()) {
            if (data == _escape[1]) { // end of color code
                _colorCode.remove(0, 1);
                for (auto &color : _codes) {
                    if (_colorCode.equals(color._name)) {
                        // Color found
                        size_t len = Ta::write_P((PGM_P)color._sequence, strlen_P((PGM_P)color._sequence));
                        _colorCode = String();
                        return len;
                    }
                }
                // color not found, print raw output
                size_t len = Ta::write(_escape[0]);
                len += Ta::write(_colorCode.c_str(), _colorCode.length());
                len += Ta::write(_escape[1]);
                return len;
            }
            else {
                _colorCode += (char)data;
                return 0;
            }
        }
        else if (!_escaped) {
            if (data == _escape[0]) { // unescaped {, start
                _colorCode = String('_');
                return 0;
            }
            else if (data == _escape[2]) { // }, end
                _escaped = true;
                return 0;
            }
        }
        else if (_escaped && data != _escape[0]) { // escape characters but not {, print both
            _escaped = false;
            size_t len = Ta::write('\\');
            len += Ta::write(data);
            return len;
        }
        _escaped = false;
        return Ta::write(data);
    }

    virtual size_t write(const uint8_t *buf, size_t size) override
    {
        size_t written = 0;
        while (size--) {
            written += write(*buf++);
        }
        return written;
    }

private:
    char _escape[3];
    bool _escaped : 1;
    bool _resetNewLine : 1;
    String _colorCode;
};

class PrintColoredString : public PrintColored<PrintString> {
public:
    using PrintString::print;

    PRINT_COLOR_METHOD(reset, "\033[m")
    PRINT_COLOR_METHOD(bold, "\033[1m")
    PRINT_COLOR_METHOD(red, "\033[31m")
    PRINT_COLOR_METHOD(green, "\033[32m")
    PRINT_COLOR_METHOD(yellow, "\033[33m")
    PRINT_COLOR_METHOD(blue, "\033[34m")
    PRINT_COLOR_METHOD(magenta, "\033[35m")
    PRINT_COLOR_METHOD(cyan, "\033[36m")
    PRINT_COLOR_METHOD(bold_red, "\033[1;31m")
    PRINT_COLOR_METHOD(bold_green, "\033[1;32m")
    PRINT_COLOR_METHOD(bold_yellow, "\033[1;33m")
    PRINT_COLOR_METHOD(bold_blue, "\033[1;34m")
    PRINT_COLOR_METHOD(bold_magenta, "\033[1;35m")
    PRINT_COLOR_METHOD(bold_cyan, "\033[1;36m")
    PRINT_COLOR_METHOD(bg_red, "\033[41m")
    PRINT_COLOR_METHOD(bg_green, "\033[42m")
    PRINT_COLOR_METHOD(bg_yellow, "\033[43m")
    PRINT_COLOR_METHOD(bg_blue, "\033[44m")
    PRINT_COLOR_METHOD(bg_magenta, "\033[45m")
    PRINT_COLOR_METHOD(bg_cyan, "\033[46m")
};

class StreamBase : public Stream {
public:
    StreamBase() : _stream(nullptr) {}

    void setStream(Stream *stream) {
        _stream = stream;
    }

    size_t write_P(PGM_P buf, size_t size) {
        size_t written = 0;
        while (size--) {
            written += _stream->write((char)pgm_read_byte(buf++));
        }
        return written;
    }

    size_t write(const char *str, size_t len) {
        return _stream->write(str, len);
    }

    size_t write(uint8_t data) {
        return _stream->write(data);
    }

    size_t write(const uint8_t *buf, size_t size) {
        return _stream->write(buf, size);
    }

private:
    Stream *_stream;
};

class SerialColor : public PrintColored<StreamBase> {
public:
    using PrintColored<StreamBase>::write;

    SerialColor(Stream &serial) : _serial(serial) {
        StreamBase::setStream(&serial);
    }

    virtual int available() {
        return _serial.available();
    }
    virtual int read() {
        return _serial.read();
    }
    virtual int peek() {
        return _serial.peek();
    }

    virtual size_t readBytes(char *buffer, size_t length) {
        return _serial.readBytes(buffer, length);
    }
    virtual size_t readBytes(uint8_t *buffer, size_t length) {
        return _serial.readBytes(buffer, length);
    }
    virtual void flush() {
        _serial.flush();
    }
    virtual String readString() {
        return _serial.readString();
    }

private:
    Stream &_serial;
};
