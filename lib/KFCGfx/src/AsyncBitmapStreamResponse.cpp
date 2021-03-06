/**
* Author: sascha_lammers@gmx.de
*/

#include "AsyncBitmapStreamResponse.h"

#include "GFXCanvasConfig.h"

#include <debug_helper_disable.h>

AsyncBitmapStreamResponse::AsyncBitmapStreamResponse(GFXCanvasCompressed& canvas, Callback callback) : AsyncAbstractResponse(nullptr), _stream(canvas), _callback(callback)
{
	_code = 200;
	_contentLength = _stream.size();
	_contentType = F("image/bmp");
}

AsyncBitmapStreamResponse::~AsyncBitmapStreamResponse()
{
    if (_callback) {
        _callback();
    }
}

bool AsyncBitmapStreamResponse::_sourceValid() const
{
	return !!_stream;
}

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

AsyncClonedBitmapStreamResponse::AsyncClonedBitmapStreamResponse(GFXCanvasCompressed *canvas) : AsyncBitmapStreamResponse(*canvas), _canvasPtr(canvas)
{
}

AsyncClonedBitmapStreamResponse::~AsyncClonedBitmapStreamResponse()
{
    delete _canvasPtr;
}
