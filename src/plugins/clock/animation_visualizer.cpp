/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_LED_MATRIX_ENABLE_UDP_VISUALIZER

#include "clock.h"
#include "animation.h"
#include "stl_ext/algorithm.h"

#if false
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

// most code is from
// https://github.com/NimmLor/esp8266-fastled-iot-webserver/blob/master/esp8266-fastled-iot-webserver.ino
//
// windows software
// https://github.com/NimmLor/IoT-Audio-Visualization-Center
//
// thanks for the great software :)

static inline void convertRGB565ToRGB(uint32_t color, uint8_t& r, uint8_t& g, uint8_t& b)
{
    r = ((color >> 11) * 527 + 23) >> 6;
    g = (((color >> 5) & 0x3f) * 259 + 33) >> 6;
    b = ((color & 0x1f) * 527 + 23) >> 6;
}

static inline uint32_t convertRGB565ToRGB(uint32_t color)
{
    uint8_t r, g, b;
    convertRGB565ToRGB(color, r, g, b);
    return (r << 16) | (g << 8) | b;
}

using namespace Clock;

WiFiUDP Clock::VisualizerAnimation::_udp;
int Clock::VisualizerAnimation::_udpUsageCounter = 0;

bool VisualizerAnimation::VideoType::isValid(const VisualizerAnimation::VideoHeaderType &header)
{
    // __DBG_printf("frame %d,%d data %d=%d pxpb=%d tsz=%d", _header._frameId, header._frameId, _data.size(), header.getPosition(), header.getBytesPerPixel(), kNumPixels * header.getBytesPerPixel());
    _header = header;
    return true;
    // if (_header._frameId == header._frameId) {
    //     if (_data.size() == header.getPosition()) {
    //         return true;
    //     }
    //     clear();
    //     __DBG_printf("1");
    //     return false;
    // }
    // else if (_data.empty() && header._position == 0) {
    //     if (header._frameId <= _header._frameId) {
    //         clear();
    //         return false;
    //     }
    //     _header._frameId = header._frameId;
    //     __DBG_printf("2");
    //     return true;
    // }
    // else {
    //     __DBG_printf("3");
    //     clear();
    // }
    // __DBG_printf("4");
    // return false;
}

CRGB VisualizerAnimation::VideoType::getRGB()
{
    uint8_t r = 128, g = 0, b = 0;
    switch(_header._format) {
        case ColorFormatType::RGB565: {
            uint16_t color;
            color = *_position++ << 8;
            color = *_position++;
            convertRGB565ToRGB(color, r, g, b);
        }
        break;
        case ColorFormatType::RGB24: {
            r = *_position++;
            g = *_position++;
            b = *_position++;
        }
        break;
        case ColorFormatType::BGR24: {
            b = *_position++;
            g = *_position++;
            r = *_position++;
        }
        break;
    }
    return CRGB(r, g, b);
}

void VisualizerAnimation::begin()
{
    _udp.stop();

    if (_cfg.get_enum_orientation(_cfg) == OrientationType::HORIZONTAL) {
        _colsInterpolation = kCols / static_cast<float>(kVisualizerPacketSize);
        _rowsInterpolation = kRows / kVisualizerMaxPacketValue;
    }
    else {
        _colsInterpolation = kRows / static_cast<float>(kVisualizerPacketSize);
        _rowsInterpolation = kCols / kVisualizerMaxPacketValue;

    }
    _storedLoudness = 0;
    _storedData.fill(0);
    _storedPeaks.fill(PeakType());
    _video.clear();

    // make sure the UDP socket is listening after a reconnect
    WiFiCallbacks::add(WiFiCallbacks::EventType::CONNECTED, [this](WiFiCallbacks::EventType event, void *payload) {
        this->_listen();
    }, this);

    _listen();
}

void VisualizerAnimation::_listen()
{
    if (_cfg.multicast) {
        auto ip = WiFi.localIP();
        auto multicastIp = WiFi.broadcastIP();
        #if ESP32
            auto result = _udp.beginMulticast(multicastIp, _cfg.port);
        #else
            auto result = _udp.beginMulticast(ip, multicastIp, _cfg.port);
        #endif
        (void)result;
        __LDBG_printf("ip=%s multi=%s port=%u begin_multicast=%u", ip.toString().c_str(), multicastIp.toString().c_str(), _cfg.port, result);
    }
    else {
        auto result = _udp.begin(_cfg.port);
        (void)result;
        __LDBG_printf("ip=* port=%u begin=%u", _cfg.port, result);
    }
}

void VisualizerAnimation::_parseUdp()
{
    size_t size;
    while((size = _udp.parsePacket()) != 0) {
        if (!_udp.available()) {
            break;
        }
        if (size >= 128) {
            VideoHeaderType header;

            if (_udp.readBytes(header, header.size()) != header.size()) {
                _video.clear();
                continue;
            }
            if (!_video.isValid(header)) {
                // invalid video id or position (lost UDP packet, wrong order)
                __DBG_printf("invalid header");
                continue;
            }
            if (_video.isReady()) {
                // skip this frame, the old one is not processed yet
                // _video._header.invalidate();
                return;
            }

            size -= header.size();
            auto ptr = _video.allocateItemBuffer(size);
            if (!ptr) {
                // out of ram
                _video.clear();
                continue;
            }

            if (_udp.readBytes(ptr, size) != size) {
                _video.clear();
                continue;
            }

            if (_video.isReady()) {
                return;
            }
        }
        else if (size == kVisualizerPacketSize) {
            // full packet, read it directly into the buffer and clean the end
            auto i = _udp.readBytes(_storedData.data(), kVisualizerPacketSize);
            if (i < kVisualizerPacketSize) {
                std::fill(_storedData.begin() + i, _storedData.end(), 0);
            }
        }
        else if (size == kVisualizerPacketSize / 4) {
            // half the size, just double all bytes
            for(int i = 0; i < kVisualizerPacketSize; i++) {
                auto data = _udp.read();
                if (data == -1) {
                    std::fill(_storedData.begin() + i, _storedData.end(), 0);
                    break;
                }
                _storedData[i++] = data;
                _storedData[i++] = data;
                _storedData[i++] = data;
                _storedData[i] = data;
            }
        }
        else if (size == kVisualizerPacketSize / 2) {
            // half the size, just double all bytes
            for(int i = 0; i < kVisualizerPacketSize; i++) {
                auto data = _udp.read();
                if (data == -1) {
                    std::fill(_storedData.begin() + i, _storedData.end(), 0);
                    break;
                }
                _storedData[i++] = data;
                _storedData[i] = data;
            }
        }
        else if (size == kVisualizerPacketSize * 2) {
            // double the size, take average of 2 values
            uint8_t buffer[2];
            for(int i = 0; i < kVisualizerPacketSize; i++) {
                auto len = _udp.readBytes(buffer, 2);
                if (len != 2) {
                    std::fill(_storedData.begin() + i, _storedData.end(), 0);
                    break;
                }
                _storedData[i] = (static_cast<int>(buffer[0]) + static_cast<int>(buffer[1])) / 2;
            }
        }
        else if (size == 1) {
            auto data = _udp.read();
            if (data == -1) {
                _storedData.fill(0);
                continue;
            }
            _storedData.fill(static_cast<uint8_t>(data));
        }
        else {
            continue;
        }
        // we have a valid packet in _storedData
        auto timeMillis = millis();

        // calculate loudness
        uint16_t loudness = 0;
        uint8_t i = 0;
        for(uint8_t &value: _storedData) {
            value = std::clamp<int16_t>(value - 1, 0, kVisualizerMaxPacketValue); // change values from 1-255 to 0-254
            loudness += value;
            _storedPeaks[i++].add(value, timeMillis);
        }
        loudness /= _storedData.size();
        if (loudness != _storedLoudness) {
            _storedLoudness = loudness;
        }

        // TODO calculate peak values using the timestamp of the last packet etc...
    }
    return;
}

void VisualizerAnimation::loop(uint32_t millisValue)
{
    // read udp in every loop
    _parseUdp();
}

#if 0

int VisualizerAnimation::_getVolume(uint8_t vals[], int start, int end, double factor)
{
    double result = 0;
    int iter = 0;
    // int cnt = 0;
    __DBG_printf("Nr: %d, %d, start: %d, end: %d", iter, vals[iter], start, end);
    for (iter = start; iter <= end && vals[iter] != '\0'; iter++) {
        __DBG_printf("Nr: %d, %d, start: %d, end: %d", iter, vals[iter], start, end);
        result += ((double)vals[iter]*factor)/(end-start + 1);
    }
    __DBG_printf("Result: %f", stdex::clamp_signed<int>(result, 1, 255));
    // if (result <= 1) result = 0;
    // if (result > 255)result = 255;
    return stdex::clamp_signed<int>(result, 1, 255);
}

void VisualizerAnimation::_BrightnessVisualizer()
{
    int currentVolume = 0;
    int avgVolume = 0;
    double cd = 0;
    // CHSV toSend;

    if (!*_incomingPacket) {
        return;
    }

    currentVolume = _getVolume(_incomingPacket, kBandStart, kBandEnd, 1);
    avgVolume = _getVolume(_lastVals.data(), 0, kAvgArraySize - 1, 1);
    if (avgVolume != 0) {
        cd = ((double)currentVolume) / ((double)avgVolume);
    }
    if (currentVolume < 30) {
        cd = 0;
    }
    if (!_lastFinished) {
        _lastFinished = _FadeUp(_lastCol, 0, kNumPixels, map(_speed, 0, 255, 1, 15), 50, false);
        return;
    };

    int decay = map(_speed, 0, 255, 8, 200);
    int b = std::clamp<int>(map(cd * 100, 30, 130, 0, 255), 0, 255);
    if (b == 0) {
        fadeToBlackBy(_leds, kNumPixels, decay);
    }
    _lastBrightness = b;

    if (++_iPos >= kAvgArraySize) {
        _iPos = 0;
    }
    _lastVals[_iPos] = currentVolume;
}


bool VisualizerAnimation::_FadeUp(CRGB c, int start, int end, int update_rate, int starting_brightness, bool isNew)
{
    static uint8_t b = 0; // start out at 0
    if (isNew) {
        b = starting_brightness;
    }
    bool retval = false;

    // slowly increase the brightness
  //EVERY_N_MILLISECONDS(update_rate)
  //{
    if (b < 255) {
        b += update_rate;
    }
    else {
        retval = true;
    }

    CRGB color((int)(c.r * ((double)(b / 255.0))), (int)(c.g * ((double)(b / 255.0))), (int)(c.b * ((double)(b / 255.0))));
    fill_solid(_leds + start, end - start, color);
    return retval;
}

void VisualizerAnimation::_ShiftLeds(int shiftAmount)
{
    int cnt = shiftAmount;
    for (int i = _parent._display.size() - 1; i >= cnt; i--) {
        _leds[i] = _leds[i - cnt];
    }
    // memmove(&_leds[cnt], &_leds[0], (_parent._display.size() - cnt) * sizeof(_leds[0]));
    // std::copy(std::begin(_leds) + shiftAmount, std::begin(_leds), std::end(_leds));
}

void VisualizerAnimation::_TrailingBulletsVisualizer()
{
    int currentVolume = 0;
    int avgVolume = 0;
    double cd = 0;
    CHSV toSend;

    if (*_incomingPacket) {
        _ShiftLeds(1);
        return;
    }

    currentVolume = _getVolume(_incomingPacket, kBandStart, kBandEnd, 1);
    avgVolume = _getVolume(_lastVals.data(), 0, kAvgArraySize - 1, 1);
    if (avgVolume != 0)  {
        cd = ((double)currentVolume) / ((double)avgVolume);
    }
    if (currentVolume < 25) {
        cd = 0;
    }
    if (currentVolume > 230) {
        cd += 0.15;
    }

    toSend = _getVisualizerBulletValue(cd);

    int update_rate = map(_speed, 0, 255, 1, 15);
    _ShiftLeds(update_rate);
    _SendTrailingLeds(toSend, update_rate);
    _ShiftLeds(update_rate / 2);
    _SendTrailingLeds(CHSV(0, 0, 0), update_rate);
    if (++_iPos >= kAvgArraySize) {
        _iPos = 0;
    }
    _lastVals[_iPos] = currentVolume;
}


void VisualizerAnimation::_SendLeds(CHSV c, int shiftAmount)
{
    for (int i = 0; i < shiftAmount; i++) {
        _leds[i] = c;
    }
}

void VisualizerAnimation::_SendTrailingLeds(CHSV c, int shiftAmount)
{
    double shiftMult = 1.0 / static_cast<double>(shiftAmount);
    int i = shiftAmount;
   _leds[i] = c;
    for (int t = shiftAmount - 1; t >= 0; t--) {
        CHSV c2 = rgb2hsv_approximate(_leds[i]);
        c2.v *= (shiftAmount - t) * shiftMult;
        _ShiftLeds(1);
        _SendLeds(c2, 1);
    }
}

CHSV VisualizerAnimation::_getVisualizerBulletValue(double cd)
{
    CHSV toSend;
    if (cd < 1.05) {
        toSend = CHSV(0, 0, 0);
    }
    if (cd < 1.10) {
        toSend = CHSV(_gHue, 255, 5);
    }
    else if (cd < 1.15) {
        toSend = CHSV(_gHue, 255, 150);
    }
    else if (cd < 1.20) {
        toSend = CHSV(_gHue + 20, 255, 200);
    }
    else if (cd < 1.25) {
        toSend = CHSV(_gHue + 40, 255, 255);
    }
    else if (cd < 1.30) {
        toSend = CHSV(_gHue + 60, 255, 255);
    }
    else if (cd < 1.45) {
        toSend = CHSV(_gHue + 100, 255, 255);
    }
    else {
        toSend = CHSV(_gHue, 255, 255);
    }
    return toSend;
}

CHSV VisualizerAnimation::_getVisualizerBulletValue(int hue, double cd)
{
    CHSV toSend;
    if (cd < 1.05) {
        toSend = CHSV(0, 0, 0);
    }
    if (cd < 1.10) {
        toSend = CHSV(hue, 255, 5);
    }
    else if (cd < 1.15) {
        toSend = CHSV(hue, 255, 150);
    }
    else if (cd < 1.20) {
        toSend = CHSV(hue, 255, 200);
    }
    else if (cd < 1.25) {
        toSend = CHSV(hue, 255, 255);
    }
    else if (cd < 1.30) {
        toSend = CHSV(hue, 255, 255);
    }
    else if (cd < 1.45) {
        toSend = CHSV(hue, 255, 255);
    }
    else {
        toSend = CHSV(hue, 255, 255);
    }
    return toSend;
}

void VisualizerAnimation::_vuMeter(CHSV c, int mode)
{
    int vol = _getVolume(_incomingPacket, kBandStart, kBandEnd, 1.75);
    fill_solid(_leds, kNumPixels, CRGB::Black);
    int toPaint = map(vol, 0, 255, 0, kNumPixels);

    if (mode == 0) {
        for (int i = 0; i < toPaint; i++) {
            for (int i = 0; i < toPaint; i++) {
                _leds[i] = c;
            }
        }
    }
    else if (mode == 1) {
        for (int i = 0; i < toPaint; i++) {
            for (int i = 0; i < toPaint; i++) {
                _leds[i] = CHSV(map(i, 0, kNumPixels, 0, 255), 255, 255);
            }
        }
    }
    else {
        for (int i = 0; i < toPaint; i++) {
            _leds[i] = CHSV(((uint8_t)map(i, 0, _parent._display.size(), 0, 255)) + _gHue, 255, 255);
        }
    }
}

void VisualizerAnimation::_vuMeterTriColor()
{
    if (!*_incomingPacket) {
        fadeToBlackBy(_leds, _parent._display.size(), 5);
        return;
    }
    int vol = _getVolume(_incomingPacket, kBandStart, kBandEnd, 1.75);
    fill_solid(_leds, _parent._display.size(), CRGB::Black);
    int toPaint = map(vol, 0, 255, 0, _parent._display.size());
    if (vol < 153) {
        fill_solid(_leds, toPaint, CRGB::Green);
    }
    else if (vol < 204) {
        fill_solid(_leds, _parent._display.size() * 0.6, CRGB::Green);
        fill_solid(_leds + (int(_parent._display.size() * 0.6)), toPaint - _parent._display.size() * 0.6, CRGB::Orange);
    }
    else if (vol >= 204) {
        fill_solid(_leds, _parent._display.size() * 0.6, CRGB::Green);
        fill_solid(_leds + (int(_parent._display.size() * 0.6)), _parent._display.size() * 0.2, CRGB::Orange);
        fill_solid(_leds + (int(_parent._display.size() * 0.8)), toPaint - _parent._display.size() * 0.8, CRGB::Red);
    }
}

#endif

#endif
