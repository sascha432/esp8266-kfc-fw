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


// TODO integrate all available animations and options

using namespace Clock;


void VisualizerAnimation::begin()
{
    _udp.stop();
    _clearPacketBuffer();

    _lastUpdate = millis();
    _updateRate = 1000 / 120;
    fill_solid(_leds, kNumPixels, CRGB(0));
    _orientation = _cfg.get_enum_orientation(_cfg);
    _barLength = _orientation == VisualizerAnimationConfig::OrientationType::VERTICAL ? kCols : kRows;

    if (_cfg.multicast) {
        auto ip = WiFi.localIP();
        auto multicastIp = WiFi.broadcastIP();
        auto result = _udp.beginMulticast(ip, multicastIp, _cfg.port);
        __DBG_printf("ip=%s multi=%s port=%u begin_multicast=%u", ip.toString().c_str(), multicastIp.toString().c_str(), _cfg.port, result);
    }
    else {
        auto result = _udp.begin(_cfg.port);
        __DBG_printf("ip=* port=%u begin=%u", _cfg.port, result);
    }
    __DBG_printf("begin update_rate=%u pixels=%u bar_len=%u", _updateRate, kNumPixels, _barLength);
}

void VisualizerAnimation::_logPackets()
{
    #if DEBUG
        // _lastUpdate is millis() when the loop function was called
        if (_lastUpdate - _packets.last_update > 5000)  {
            _packets.last_update = _lastUpdate;
            __DBG_printf("UDP incoming=%u dropped=%u valid=%u invalid=%u invalid_size=%u rate=%u", _packets.incoming, _packets.dropped, _packets.valid, _packets.invalid, _packets.invalid_size, _updateRate);
        }
    #endif
}

bool VisualizerAnimation::_parseUdp()
{
    int size;
    // get next packet and discard the last one
    if ((size = _udp.parsePacket()) == 0) {
        return false;
    }
    _packets.incoming++;
    for(;;) {
        if (_hasValidPacketInBuffer()) {
            // buffer is not empty, skip this packet and keep the last one
            _packets.dropped++;
            break;
        }
        else if (size <= 0) {
            // invalid packet size, destroy packet
        }
        else {
            // read data into buffer
            auto len = std::min<int>(_udp.readBytes(_incomingPacket, std::min(size, kVisualizerPacketSize)), kVisualizerPacketSize);
            if (len != kNumPixels) {
                // length does not match the number of pixels
                _packets.invalid_size++;
            }
            // check if we have valid data
            if (len && _hasValidPacketInBuffer()) {
                // valid packet
                // fill the buffer with zeros and add trailing zero byte
                std::fill(std::begin(_incomingPacket) + len, std::end(_incomingPacket), 0);
                _packets.valid++;
                break;
            }

        }
        // count as invalid and clean buffer
        _packets.invalid++;
        _clearPacketBuffer();
        _logPackets();
        return false;
    }
    _logPackets();
    return true;
}

void VisualizerAnimation::loop(uint32_t millisValue)
{
    // read udp in every loop
    _parseUdp();

    if (millisValue - _lastUpdate >= _updateRate) {
        _lastUpdate = millisValue;
        // do we have a valid packet in the buffer?
        if (_hasValidPacketInBuffer()) {
            fadeToBlackBy(_leds, kNumPixels, 150);
            switch(static_cast<VisualizerAnimationConfig::VisualizerType>(_cfg.type)) {
                case VisualizerAnimationConfig::VisualizerType::SINGLE_COLOR:
                    _singleColorVisualizer();
                    break;
                case VisualizerAnimationConfig::VisualizerType::SINGLE_COLOR_DOUBLE_SIDED:
                    _singleColorVisualizerDoubleSided();
                    break;
                case VisualizerAnimationConfig::VisualizerType::RAINBOW:
                    _rainbowVisualizer();
                    break;
                case VisualizerAnimationConfig::VisualizerType::RAINBOW_DOUBLE_SIDED:
                    _rainbowVisualizerDoubleSided();
                    break;
                default:
                    break;
            }
            // mark packet as processed
            _clearPacketBuffer();
        }
    }
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

int VisualizerAnimation::_getPeakPosition()
{
    auto ptr = _incomingPacket;
    int pos = 0;
    uint8_t posVal = 0;
    for (int i = 0; i < kVisualizerPacketSize && *ptr; ++i, ++ptr) {
        if (*ptr > posVal) {
            posVal = *ptr;
            pos = i;
        }
    }
    if (posVal < 30) {
        pos = -1;
    }
    return pos;
}

void VisualizerAnimation::_printPeak(CHSV c, int pos, int grpSize)
{
    fadeToBlackBy(_leds, _parent._display.size(), 12);
    _leds[pos] = c;
    for (int i = 0; i < ((grpSize - 1) / 2); i++) {
        _leds[pos + i] = c;
        _leds[pos - i] = c;
    }
}

#endif

void VisualizerAnimation::_setBar(int row, int num, CRGB color)
{
    _setBar(row, num, rgb2hsv_approximate(color));
}

void VisualizerAnimation::_setBar(int row, int num, CHSV color)
{
    if (_orientation == VisualizerAnimationConfig::OrientationType::VERTICAL) {
        num = std::min<int>(num, kCols);
        row = std::min<int>(row, kRows) + 1;
        for (int col = 1; col <= num; col++) {
            _leds[row * kRows - col] = color;
        }
    }
    else {
        // flip col and row
        num = std::min<int>(num, kRows);
        row = std::min<int>(row, kCols) + 1;
        for (int col = 1; col <= num; col++) {
            _leds[col * kRows - row] = color;
        }
    }
}

void VisualizerAnimation::_setBarDoubleSided(int row, int num, CHSV color)
{
    // split the rectangle in half and mirror the animation starting from the outer edge
    if (_orientation == VisualizerAnimationConfig::OrientationType::VERTICAL) {
        // ceil(num / 2)
        num = std::min<int>(num + 1, kCols) / 2;
        row = std::min<int>(row, kRows);
        for (int col = 0; col < num; col++) {
            _leds[row * kRows + col] = color;
            _leds[(row + 1) * kRows - col - 1] = color;
        }
    }
    else {
        num = std::min<int>(num + 1, kRows) / 2;
        row = std::min<int>(row, kCols);
        for (int col = 0; col < num; col++) {
            _leds[col * kRows + row] = color;
            _leds[(col + 1) * kRows - row - 1] = color;
        }
    }
}

void VisualizerAnimation::_setBarDoubleSided(int row, int num, CRGB color)
{
    _setBar(row, num, rgb2hsv_approximate(color));
}

void VisualizerAnimation::_rainbowVisualizer()
{
    auto ptr = _incomingPacket;
    int maxValue = (_barLength - 1);
    for (int i = 0, j = 0; i < _barLength && *ptr; j += 255) {
        // basically map(i, 0, _barLength - 1, 0, 255) with a single division and add operation
        _setBar(i++, *ptr++, CHSV((j / maxValue), 255, 255));
    }
}

void VisualizerAnimation::_rainbowVisualizerDoubleSided()
{
    auto ptr = _incomingPacket;
    int maxValue = (_barLength - 1);
    for (int i = 0, j = 0; i < _barLength && *ptr; j += 255) {
        _setBarDoubleSided(i++, *ptr++, CHSV((j / maxValue), 255, 255));
    }
}

void VisualizerAnimation::_singleColorVisualizer()
{
    auto ptr = _incomingPacket;
    for (int i = 0; i < _barLength && *ptr; ) {
        _setBar(i++, *ptr++, CRGB(_cfg.color));
    }
}

void VisualizerAnimation::_singleColorVisualizerDoubleSided()
{
    auto ptr = _incomingPacket;
    for (int i = 0; i < _barLength && *ptr; ) {
        _setBarDoubleSided(i++, *ptr++, CRGB(_cfg.color));
    }
}

#endif
