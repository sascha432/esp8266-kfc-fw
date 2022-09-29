/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <ESPAsyncWebServer.h>
#include "GFXCanvasConfig.h"
#include "GFXCanvasCompressed.h"


extern const char content_type_image_bmp[] PROGMEM;

class AsyncBitmapStreamResponse : public AsyncAbstractResponse {
public:
    AsyncBitmapStreamResponse(GFXCanvasCompressed &canvas, const __FlashStringHelper *contentType = FPSTR(content_type_image_bmp));
    virtual ~AsyncBitmapStreamResponse();
    bool _sourceValid() const;
    virtual size_t _fillBuffer(uint8_t* buf, size_t maxLen) override;

private:
    GFXCanvasBitmapStream _stream;
};

inline AsyncBitmapStreamResponse::AsyncBitmapStreamResponse(GFXCanvasCompressed &canvas, const __FlashStringHelper *contentType) :
    AsyncAbstractResponse(nullptr),
    _stream(canvas)
{
	_code = 200;
	_contentLength = _stream.available();
	_contentType = contentType;
}

inline AsyncBitmapStreamResponse::~AsyncBitmapStreamResponse()
{
}

inline bool AsyncBitmapStreamResponse::_sourceValid() const
{
	return !!_stream;
}

