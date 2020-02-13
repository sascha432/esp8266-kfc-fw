/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <SSIProxyStream.h>

#if _MSC_VER

#include <assert.h>

#define ASSERT assert

#else

#undef ASSERT
#define ASSERT(...) ;

#endif

#if DEBUG_SSI_PROXY_STREAM
#include "debug_helper_enable.h"
#else
#include "debug_helper_disable.h"
#endif

SSIProxyStream::SSIProxyStream(File &file, DataProviderInterface &provider) : _file(file), _pos(0), _length(0), _template(), _provider(provider) {
}

SSIProxyStream::~SSIProxyStream() {
}

int SSIProxyStream::_read()
{
    auto data = _peek();
    if (data != -1) {
        _pos++;
    }
    return data;
}

int SSIProxyStream::_peek()
{
    int data;
    if (_pos >= _buffer.length()) {
        _readBuffer();
    }
    if (_pos < _buffer.length()) {
        data = _buffer.get()[_pos];
        return data;
    }
    else {
        close();
        return -1;
    }
}

size_t SSIProxyStream::_read(uint8_t *buffer, size_t length)
{
    int avail;
    auto loops = std::min(8U, (length / 64) + 1);
    while ((int)length > (avail = _available()) && _readBuffer() > 0 && loops--) {
    }
    debug_printf_P(PSTR("ptr=%p read_len=%d pos=%d write=%d length=%d size=%d\n"), buffer, length, _pos, avail, _buffer.length(), _buffer.size());
    memcpy(buffer, _buffer.begin() + _pos, avail);
    _pos += avail;
    return avail;
}

int SSIProxyStream::_available() const
{
    return _buffer.length() - _pos;
}

int SSIProxyStream::_readBuffer(bool templateCheck)
{
    // _debug_printf_P(PSTR("templateCheck=%u\n"), templateCheck);
    uint8_t buf[64];
    int len = 0;
    if (_provider) {
        templateCheck = _provider.resolveRecurive();
        len = _provider.fillBuffer(buf, sizeof(buf));
        if (len < 0) {
            // retry later
            return len;
        }
        else if (len == 0) {
            _provider.end();
            debug_printf_P(PSTR("template done: start=%d end=%d (%d)\n"), _template._length, _length, _length - _template._length);

            // move pointer to end of buffer if we didn't resolve template names
            if (!templateCheck) {
                ASSERT(_template.marker == false);
                _template.pos = _buffer.end();
            }
        }
        else {
            _length += len;
        }
    }
    if (len == 0 && _file && _file.available()) {
        len = _file.readBytes((char *)buf, sizeof(buf));
        if (len <= 0) {
            debug_printf_P(PSTR("template EOF: %d\n"), _length);
            close();
            return 0;
        }
        _length += len;
    }
    if (len > 0) {
        // safe offset
        ptrdiff_t ofs = _template.pos - _buffer.begin();

        if (!_template.marker && _buffer.length()) {
            _buffer.removeAndShrink(0, _pos, sizeof(buf) * 2);
            ofs -= _pos;
            _pos = 0;
        }
        _buffer.write(buf, len);

        // restore ptr to buffer
        _template.pos = _buffer.begin() + ((ofs < 0) ? 0 : ofs);

        if (templateCheck) {
            do {
                uint8_t *start = _template.pos ? _template.pos : _buffer.begin();
                ASSERT(start >= _buffer.begin() && start < _buffer.end());
                uint8_t *ptr = (uint8_t *)memchr(start, '%', _buffer.end() - start);
                if (ptr) {
                    if (_template.marker) {
                        if (_template.pos == ptr) { // double %%
                            _template.marker = false;
                            _template.pos++;
                        }
                        else {
                            auto nameStartPtr = _template.pos;
                            auto nameEndPtr = ptr;
                            auto nameLen = nameEndPtr - nameStartPtr;
                            _template.pos = ++ptr;
                            _template.marker = false;
                            auto templateName = PrintString(nameStartPtr, nameLen);

                            // get position inside the file after the template name without %
                            size_t filePos = _file.position() - (_buffer.end() - nameEndPtr) + 1;
#if DEBUG && !ESP8266
                            {
                                // verify position
                                auto pos = _file.position();
                                _file.seek(filePos - nameLen - 1, SeekSet);
                                uint8_t tmp[128];
                                auto len = _file.read(tmp, sizeof(tmp));
                                ASSERT(memcmp(tmp, nameStartPtr, nameLen) == 0);
                                _file.seek(pos, SeekSet);
                            }
#endif
                            // check if data provider can resolve the name
                            if (_provider.begin(templateName)) {
                                debug_printf_P(PSTR("template %s started: last start=%d new start=%d (%d)\n"), templateName.c_str(), _template._length, _length);
                                _template._length = _length;
                                // remove buffer after template name starts
                                size_t buf_len = nameStartPtr - _buffer.begin() - 1;
                                ASSERT(_pos <= buf_len);
                                _buffer.remove(buf_len, -1);

                                // position file read pointer after the name end
                                _file.seek(filePos, SeekSet);
                            }
                            _template.pos = _buffer.end();
                        }
                    }
                    else {
                        _template.marker = true;
                        _template.pos = ++ptr;
                    }
                }
                else if (_template.marker) {
                    if (_buffer.end() - _template.pos > 128) { // template name length exceeded
                        _template.marker = false;
                        _template.pos = _buffer.end();
                    }
                    else {
                        auto ptr = start;
                        while (ptr < _buffer.end()) {
                            if (*ptr <= 32) { // invalid character in template name
                                _template.marker = false;
                                break;
                            }
                            ptr++;
                        }
                        _template.pos = _buffer.end();
                        if (_template.marker) {
                            // get more data until we have found the end of the template name
                            _readBuffer(false);
                        }
                    }
                }
                else {
                    // no further % in buffer
                    _template.pos = _buffer.end();
                }
            } while (_template.pos < _buffer.end());

            ASSERT(_template.marker == false);
        }
    }
    return len;
}
