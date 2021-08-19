/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <SSIProxyStream.h>
#include <DumpBinary.h>

#if DEBUG_SSI_PROXY_STREAM
#    include "debug_helper_enable.h"
#else
#    include "debug_helper_disable.h"
#endif

#if !DEBUG_SSI_PROXY_STREAM
#    undef __DBG_validatePointer
#    define __DBG_validatePointer(ptr, ...) ptr
#endif

size_t SSIProxyStream::_copy(uint8_t *buffer, size_t length)
{
    __DBG_validatePointer(buffer, VP_HPS);
    __LDBG_assert(_template.marker == -1);
    if (length > _available()) {
        length = _available();
    }
    std::copy_n(_buffer.begin() + _position, length, buffer);
    //memcpy(buffer, _buffer.begin() + _position, length);
    _buffer.remove(_position, length);
    if (_template.position >= _buffer.begin() + _position) {
        _template.position -= length;
    }
    __LDBG_assert(_template.position == _buffer.end() || _template.in_buffer(_template.position));
    return length;
}

size_t SSIProxyStream::_readBuffer(bool templateCheck)
{
    // __LDBG_printf("templateCheck=%u", templateCheck);
    #define DEBUG_SSI_PROXY_STREAM_POISON_CHECK DEBUG_SSI_PROXY_STREAM
    #if DEBUG_SSI_PROXY_STREAM_POISON_CHECK
        constexpr size_t bufferSizePosionCheck = 32;
        size_t bufferSize = 512 + (bufferSizePosionCheck * 2);
    #else
        constexpr size_t bufferSize = 512;
    #endif
    auto bufferPtr = std::unique_ptr<uint32_t[]>(new uint32_t[bufferSize / sizeof(uint32_t)]);
    auto buf = reinterpret_cast<uint8_t *>(bufferPtr.get());
    if (!buf) {
        __DBG_printf_E("allocation failed size=%u", bufferSize);
        return 0;
    }
    #if DEBUG_SSI_PROXY_STREAM_POISON_CHECK
        std::fill_n(bufferPtr.get(), bufferSize / sizeof(uint32_t), 0xccccccccU);
        auto bufBegin = buf;
        auto bufEnd = buf + bufferSize - bufferSizePosionCheck;
        buf += bufferSizePosionCheck;
        bufferSize -= bufferSizePosionCheck * 2;
    #endif

    size_t len = 0;
    if (_provider) {
        len = _provider.fillBuffer(buf, bufferSize);
        if (len <= 0) {
            _provider.end();
            __LDBG_printf("template %c%s%c end @ %d length=%d", _template.delim, _template.template_name(), _template.delim, _length, _template.template_length());
            __LDBG_assert(_template.marker == -1);
            _template.position = _buffer.end();
        }
        else {
            _length += len;
        }
    }
    if (len == 0 && _file && _file.available()) {
        len = _file.readBytes(reinterpret_cast<char *>(buf), bufferSize);
        if (len <= 0) {
            __LDBG_printf("stream reached EOF @ %d, result=%d", _length, len);
            close();
            return 0;
        }
        _length += len;
    }
    if (len > 0) {
        ptrdiff_t posOffset = _template.to_offset(_template.position); // save offset
        if (_template.marker == -1 && _position) {
            // only shrink if no template marker is set
            _buffer.removeAndShrink(0, _position);
            posOffset -= _position; // move offset
            _position = 0;
        }
        _buffer.write(buf, len);

        _template.position = _template.from_offset(posOffset); // get new pointer from offset
        __LDBG_assert(_template.in_buffer(_template.position));

        if (templateCheck) {
            do {
                uint8_t *ptr = _template.start();
                uint8_t *end = _buffer.end();
                // if (ptr == end) {
                //     __DBG_printf("ptr==end template len=%u", _template.template_length());
                //     break;
                // }
                __LDBG_assert(_template.in_buffer(ptr));
                while(ptr < end) {
                    if (*ptr == '%' || *ptr == '$') {
                        _template.delim = *ptr;
                        break;
                    }
                    ptr++;
                }
                if (ptr < end) {
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
                        if (++_template.position == _buffer.end()) { // read more data if the delimiter is the last character
                            if (_readBuffer(false) <= 0) {
                                _template.eof();
                                break;
                            }
                        }
                        if (*_template.position == _template.delim) {
                            // name.len() will be zero for double delimiter and ignored
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

                    if (name.len()) { // we found the end delimiter
                        __LDBG_assert(name.begin  && *(name.begin - 1) == _template.delim);
                        __LDBG_assert(name.end && *(name.end + 0) == _template.delim);

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
                                __LDBG_assert(memcmp(tmp, name.begin, name.len()) == 0);
                                _file.seek(savePos, SeekSet);
                            }
                        #endif
                        // check if data provider can resolve the name
                        if (_provider.begin(_template.name)) {
                            _template.start_length = _length;
                            __LDBG_printf("template %c%s%c started @ %d", _template.delim, _template.template_name(), _template.delim, _template.start_length);

                            // remove buffer after template name starts
                            size_t templateStartOfs = name.begin - _buffer.begin() - 1;
                            __LDBG_assert(_buffer[templateStartOfs] == _template.delim);
                            __LDBG_assert(_position <= templateStartOfs);
                            _buffer.remove(templateStartOfs, -1);

                            // continue adding data
                            _template.position = _buffer.end();

                            // position file read pointer after the name end
                            _file.seek(filePos, SeekSet);
                        }
                        else {
                            // skip name
                            __LDBG_printf("template %c%s%c skipped", _template.delim, _template.template_name(), _template.delim);
                            _template.name = String();
                            _template.position = name.end + 1;
                        }

                    }

                }
                else {
                    // no delimter found, end loop
                    _template.position = _buffer.end();
                }

            } while (_template.position < _buffer.end());

            __LDBG_assert(_template.marker == -1);
        }
    }
    #if DEBUG_SSI_PROXY_STREAM_POISON_CHECK
        for(size_t i = 0; i < bufferSizePosionCheck; i++) {
            if (bufBegin[i] != 0xcc) {
                __DBG_printf_E("buffer overrun @bufBegin[%u]", i);
                break;
            }
            if (bufEnd[i] != 0xcc) {
                __DBG_printf_E("buffer overrun @bufEnd[%u]", i);
                break;
            }
        }
    #endif
    #undef DEBUG_SSI_PROXY_STREAM_POISON_CHECK
    #if DEBUG_SSI_PROXY_STREAM
        _ramUsage = std::min(ESP.getFreeHeap(), _ramUsage);
    #endif
    return len;
}
