/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <SSIProxyStream.h>
#include <DumpBinary.h>

#if DEBUG_SSI_PROXY_STREAM
#include "debug_helper_enable.h"
#else
#include "debug_helper_disable.h"
#endif

SSIProxyStream::SSIProxyStream(File &file, DataProviderInterface &provider) : _template({ *this, -1, nullptr, 0, String() }), _file(file), _position(0), _length(0), _provider(provider) {
}

SSIProxyStream::~SSIProxyStream() {
}

int SSIProxyStream::_read()
{
    auto data = _peek();
    if (data != -1) {
        _position++;
        _template.position--;
    }
    return data;
}

int SSIProxyStream::_peek()
{
    int data;
    if (_position >= _buffer.length()) {
        _readBuffer();
    }
    if (_position < _buffer.length()) {
        data = _buffer.get()[_position];
        return data;
    }
    else {
        close();
        return -1;
    }
}

size_t SSIProxyStream::_copy(uint8_t *buffer, size_t length)
{
    DEBUG_ASSERT(_template.marker == -1);

    if (length > _available()) {
        length = _available();
    }

    memcpy(buffer, _buffer.begin() + _position, length);
    _buffer.remove(_position, length);
    if (_template.position >= _buffer.begin() + _position) {
        _template.position -= length;
    }
    DEBUG_ASSERT(_template.position == _buffer.end() || _template.in_buffer(_template.position));
    return length;
}

size_t SSIProxyStream::read(uint8_t *buffer, size_t length)
{
    //_debug_printf_P(PSTR("read_len=%d pos=%d length=%d size=%d\n"), length, _position, _buffer.length(), _buffer.size());
    auto ptr = buffer;
    while (length && _available()) {
        auto copied = _copy(ptr, length);
        ptr += copied;
        length -= copied;
    }
    return ptr - buffer;
}

size_t SSIProxyStream::_available()
{
    if (_position >= _buffer.length()) {
        _readBuffer();
    }
    return _buffer.length() - _position;
}

size_t SSIProxyStream::_readBuffer(bool templateCheck)
{
    // _debug_printf_P(PSTR("templateCheck=%u\n"), templateCheck);
    uint8_t buf[128];
    size_t len = 0;
    if (_provider) {
        len = _provider.fillBuffer(buf, sizeof(buf));
        if (len <= 0) {
            _provider.end();
            _debug_printf_P(PSTR("template %%%s%% end @ %d length=%d\n"), _template.template_name(), _length, _template.template_length());

            DEBUG_ASSERT(_template.marker == -1);
            _template.position = _buffer.end();
        }
        else {
            _length += len;
        }
    }
    if (len == 0 && _file && _file.available()) {
        len = _file.readBytes((char *)buf, sizeof(buf));
        if (len <= 0) {
            _debug_printf_P(PSTR("stream reached EOF @ %d, result=%d\n"), _length, len);
            close();
            return 0;
        }
        _length += len;
    }
    if (len > 0) {
        //DumpBinary dump(Serial);

        ptrdiff_t posOffset = _template.to_offset(_template.position); // safe offset

        if (_template.marker == -1 && _position) {
            // only shrink if no template marker is set
            //if (_position) dump.setPerLine(len).print("remove: ").dump(_buffer.begin(), _position);
            _buffer.removeAndShrink(0, _position);
            posOffset -= _position;
            _position = 0;
        }
        _buffer.write(buf, len);
        //dump.setPerLine(255).print("buffer: ").dump(_buffer.begin(), _buffer.length() - len).dump(_buffer.begin(), len, _buffer.length() - len).print("\n");

        //_debug_printf_P(PSTR("buffer size=%d length=%d position=%d\n"), _buffer.size(), _buffer.length(), _position);

        _template.position = _template.from_offset(posOffset); // update pointer from offset
        DEBUG_ASSERT(_template.in_buffer(_template.position));

        if (templateCheck) {
            do {
                uint8_t *start = _template.start();
                DEBUG_ASSERT(_template.in_buffer(start));
                uint8_t *ptr = (uint8_t *)memchr(start, '%', _buffer.end() - start);
                if (ptr) {
                    struct {
                        constexpr size_t len() const {
                            return end - begin;
                        }
                        PrintString toString() const {
                            return PrintString(begin, len());
                        }
                        uint8_t *begin;
                        uint8_t *end;
                    } name = {};

                    _template.marker = _template.to_offset(ptr);
                    _template.position = ptr;
                    size_t count = 0;
                    do {
                        if (++_template.position == _buffer.end()) { // read more data if the % is the last character
                            if (_readBuffer(false) <= 0) {
                                _template.eof();
                                break;
                            }
                        }
                        if (*_template.position == '%') {
                            // name.len() will be for double %% and ignored
                            name.begin = _template.from_offset(_template.marker) + 1;
                            name.end = _template.position++;
                            _template.marker = -1;
                            break;
                        }

                        else if (++count >= maxTemplateNameSize || !isValidChar(*_template.position)) { // template name is too long or contains invalid characters
                            _template.marker = -1;
                            break;
                        }
                    } while (true);

                    if (name.len()) { // we found the end %
                        DEBUG_ASSERT(name.begin  && *(name.begin - 1) == '%');
                        DEBUG_ASSERT(name.end && *(name.end + 0) == '%');

                        _template.position = name.end + 1;
                        _template.name = name.toString();

                        // get position inside the file after the template name without %
                        size_t filePos = _file.position() - (_buffer.end() - name.end) + 1;
#if HAVE_DEBUG_ASSERT && _WIN32
                        if (name.begin) {
                            // verify position
                            auto savePos = _file.position();
                            _file.seek(filePos - name.len() - 1, SeekSet);
                            uint8_t tmp[128];
                            auto len = _file.readBytes(tmp, sizeof(tmp));
                            DEBUG_ASSERT(memcmp(tmp, name.begin, name.len()) == 0);
                            _file.seek(savePos, SeekSet);
                        }
#endif
                        // check if data provider can resolve the name
                        if (_provider.begin(_template.name)) {
                            _template.start_length = _length;
                            _debug_printf_P(PSTR("template %%%s%% started @ %d\n"), _template.template_name(), _template.start_length);

                            // remove buffer after template name starts
                            size_t templateStartOfs = name.begin - _buffer.begin() - 1;
                            DEBUG_ASSERT(_buffer[templateStartOfs] == '%');
                            DEBUG_ASSERT(_position <= templateStartOfs);
                            _buffer.remove(templateStartOfs, -1);

                            // continue adding data
                            _template.position = _buffer.end();

                            // position file read pointer after the name end
                            _file.seek(filePos, SeekSet);
                        }
                        else {
                            // skip name
                            _debug_printf_P(PSTR("template %%%s%% skipped\n"), _template.template_name());
                            _template.name = String();
                            _template.position = name.end + 1;
                        }

                    }

                }
                else {
                    // no % found, end loop
                    _template.position = _buffer.end();
                }

            } while (_template.position < _buffer.end());

            DEBUG_ASSERT(_template.marker == -1);
        }
    }
    return len;
}
