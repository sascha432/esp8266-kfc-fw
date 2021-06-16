/**
* Author: sascha_lammers@gmx.de
*/

#include <PrintString.h>
#include "debug_helper.h"
#include "Buffer.h"

#if DEBUG_BUFFER
#include "debug_helper_enable.h"
#else
#include "debug_helper_disable.h"
#endif

bool Buffer::_changeBuffer(size_t newSize)
{
    if (newSize == 0 && _size) {
        clear();
    }
    else {
        auto resize = _alignSize(newSize);
        if (resize != _size) {
            // __LDBG_printf("size=%d", _fp_size);
            if (!_buffer) {
                _buffer = reinterpret_cast<uint8_t *>(malloc(resize));
                if (!_buffer) {
                    // __LDBG_printf("alloc failed");
                    return false;
                }
            }
            else {
                _buffer = reinterpret_cast<uint8_t *>(realloc(_buffer, resize));
                if (!_buffer) {
                    // __LDBG_printf("alloc failed");
                    _size = 0;
                    _length = 0;
                    return false;
                }
                if (_length > newSize) {
                    _length = newSize;
                }
            }
            _size = resize;
#if BUFFER_ZERO_FILL
            std::fill(_data_end(), _buffer_end(), 0);
#endif
        }
    }
    // __LDBG_printf("length=%d size=%d", _length, _fp_size);
    return true;
}
