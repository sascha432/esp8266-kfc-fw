/**
* Author: sascha_lammers@gmx.de
*/

#if HAVE_GFX_LIB

#include "AsyncBitmapStreamResponse.h"

AsyncBitmapStreamResponse::AsyncBitmapStreamResponse(GFXCanvasCompressed& canvas) : AsyncAbstractResponse(nullptr), _stream(canvas) {
	_code = 200;
	_contentLength = _stream.size();
	_contentType = F("image/bmp");
}

bool AsyncBitmapStreamResponse::_sourceValid() const {
	return !!_stream;
}

size_t AsyncBitmapStreamResponse::_fillBuffer(uint8_t* buf, size_t maxLen) {
    debug_printf("_fillBuffer(%p, %u): available %u\n", buf, maxLen, _stream.available());
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

AsyncClonedBitmapStreamResponse::AsyncClonedBitmapStreamResponse(GFXCanvasCompressed *canvas) : AsyncBitmapStreamResponse(*canvas), _canvasPtr(canvas) {
}

AsyncClonedBitmapStreamResponse::~AsyncClonedBitmapStreamResponse() {
    delete _canvasPtr;
}

#endif
