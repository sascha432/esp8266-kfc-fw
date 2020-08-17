/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <ESPAsyncWebServer.h>
#include "GFXCanvasConfig.h"
#include "GFXCanvasCompressed.h"

class AsyncBitmapStreamResponse : public AsyncAbstractResponse {
public:
    AsyncBitmapStreamResponse(GFXCanvasCompressed& canvas);
    virtual ~AsyncBitmapStreamResponse();
    bool _sourceValid() const;
    virtual size_t _fillBuffer(uint8_t* buf, size_t maxLen) override;

private:
    GFXCanvasBitmapStream _stream;
};

// use GFXCanvasCompressed::clone() to create the object
class AsyncClonedBitmapStreamResponse : public AsyncBitmapStreamResponse {
public:
    AsyncClonedBitmapStreamResponse(GFXCanvasCompressed *canvas);
    virtual ~AsyncClonedBitmapStreamResponse();

private:
    GFXCanvasCompressed *_canvasPtr;
};
