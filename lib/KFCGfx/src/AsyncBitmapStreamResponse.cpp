/**
* Author: sascha_lammers@gmx.de
*/

#include "AsyncBitmapStreamResponse.h"
#include "GFXCanvasConfig.h"
#include <debug_helper_disable.h>

const char content_type_image_bmp[] PROGMEM = { "image/bmp" };

size_t AsyncBitmapStreamResponse::_fillBuffer(uint8_t* buf, size_t maxLen)
{
    __LDBG_printf("buf=%p len=%u avail=%u", buf, maxLen, _stream.available());
    size_t available = _stream.available();
    if (available > maxLen) {
        available = maxLen;
    }
    maxLen = available;
    while (available--) {
        *buf++ = _stream.read();
    }
    return maxLen;
}
