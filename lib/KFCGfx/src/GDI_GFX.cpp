/**
* Author: sascha_lammers@gmx.de
*/

#ifdef _WIN32

#include "GFXCanvas.h"
#include "GDI_GFX.h"

GDI_GFX::GDI_GFX(uint16_t w, uint16_t h) : Adafruit_GFX(w, h)
{
    HDC hDC = GetDC(NULL);
    HDC hDCMem = CreateCompatibleDC(hDC);

    BITMAPINFO bi;
    memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h; // flip horizontally
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biCompression = BI_RGB;
    bi.bmiHeader.biBitCount = 24;

    _hBitmap = CreateDIBSection(hDCMem, &bi, DIB_RGB_COLORS, (VOID**)&_buffer, NULL, 0);

    ReleaseDC(NULL, hDC);
}

GDI_GFX::~GDI_GFX()
{
    //free(_buffer);
    DeleteObject(_hBitmap);
}

void GDI_GFX::drawPixel(int16_t x, int16_t y, uint16_t color)
{
    if ((uint16_t)x < (uint16_t)_width && (uint16_t)y < (uint16_t)_height) {
        auto ptr = &_buffer[(x + y * _width) * 3];

#if 0
        // create test image
        // 10 rows red, then green, then blue, repeat
        // decreasing brightness slowly
        auto n = (x + y * _width);
        uint32_t rgb = (n / (_width * 10) % 3 == 0 ? 0xff : 0) | ((n / (_width * 10) % 3 == 1 ? 0xff : 0) << 8) | ((n / (_width * 10) % 3 == 2 ? 0xff : 0) << 16);
        ptr[2] = rgb & 0xff;
        ptr[1] = (rgb >> 8) & 0xff;
        ptr[0] = (rgb >> 16) & 0xff;
#endif
#if 0
        auto n = (x + y * _width);
        uint32_t rgb = (n / (_width * 10) % 3 == 0 ? 0xff : 0) | ((n / (_width * 10) % 3 == 1 ? 0xff : 0) << 8) | ((n / (_width * 10) % 3 == 2 ? 0xff : 0) << 16);
        ptr[2] = rgb & 0xff;
        ptr[1] = (rgb >> 8) & 0xff;
        ptr[0] = (rgb >> 16) & 0xff;
#endif
        GFXCanvas::convertToRGB(color, ptr[2], ptr[1], ptr[0]); // _buffer/CreateDIBSection is BGR
    }
}

void GDI_GFX::drawBitmap(HDC hDC, uint16_t x, uint16_t y)
{
    HDC       hDCBits;
    BITMAP    Bitmap;
    BOOL      bResult;

    if (hDC && _hBitmap) {

#if 0
        // create test image
        // 10 rows red, then green, then blue, repeat
        // decreasing brightness slowly
        for (int i = 0; i < _width * _height; i++) {
            for (int j = 0; j < 3; j++) {
                _buffer[i * 3 + 2 - j] = i / (_width * 10) % 3 == j ? (0xff - 0xff * i / (_width * _height)) : 0;
            }
        }
#endif

        hDCBits = CreateCompatibleDC(hDC);
        GetObject(_hBitmap, sizeof(BITMAP), (LPSTR)&Bitmap);
        SelectObject(hDCBits, _hBitmap);

        bResult = BitBlt(hDC, x, y, Bitmap.bmWidth, Bitmap.bmHeight, hDCBits, 0, 0, SRCCOPY);
        DeleteDC(hDCBits);
    }
}

#endif
