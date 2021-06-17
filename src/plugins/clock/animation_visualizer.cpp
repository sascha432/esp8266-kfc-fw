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

bool VisualizerAnimation::_parseUdp()
{
    size_t size;
    if (!(size = _udp.parsePacket()) || !_udp.available()) {
        return false;
    }
    if (size && size <= kVisualizerPacketSize) {
        if (*_incomingPacket) {
            __LDBG_printf("dropped UDP packet");
        }
        auto len = _udp.readBytes(_incomingPacket, size);
        _incomingPacket[len] = 0;
        __LDBG_printf("new data size=%u", len);
        return true;
    }
    else {
        __LDBG_printf("dumping size=%u", size);
        // invalid size
        while(size-- && _udp.read() != -1) {
            // read packet and dump it
        }
    }
    return false;
}

void VisualizerAnimation::loop(uint32_t millisValue)
{
    if (get_time_diff(_loopTimer, millisValue) >= _updateRate) {
        _loopTimer = millisValue;
        if (_parseUdp() && *_incomingPacket) {
            fadeToBlackBy(_leds, kNumPixels, 150);
            switch(static_cast<VisualizerAnimationConfig::VisualizerType>(_cfg.type)) {
                case VisualizerAnimationConfig::VisualizerType::SINGLE_COLOR:
                    _SingleColorVisualizer();
                    break;
                case VisualizerAnimationConfig::VisualizerType::SINGLE_COLOR_DOUBLE_SIDED:
                    _SingleColorVisualizerDoubleSided();
                    break;
                case VisualizerAnimationConfig::VisualizerType::RAINBOW:
                    _RainbowVisualizer();
                    break;
                case VisualizerAnimationConfig::VisualizerType::RAINBOW_DOUBLE_SIDED:
                    _RainbowVisualizerDoubleSided();
                    break;
                default:
                    break;
            }
            *_incomingPacket = 0;
        }
    }
}

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
    for (int i = DisplayType::kNumPixels - 1; i >= cnt; i--) {
        _leds[i] = _leds[i - cnt];
    }
    // memmove(&_leds[cnt], &_leds[0], (DisplayType::kNumPixels - cnt) * sizeof(_leds[0]));
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
            _leds[i] = CHSV(((uint8_t)map(i, 0, DisplayType::kNumPixels, 0, 255)) + _gHue, 255, 255);
        }
    }
}

void VisualizerAnimation::_vuMeterTriColor()
{
    if (!*_incomingPacket) {
        fadeToBlackBy(_leds, DisplayType::kNumPixels, 5);
        return;
    }
    int vol = _getVolume(_incomingPacket, kBandStart, kBandEnd, 1.75);
    fill_solid(_leds, DisplayType::kNumPixels, CRGB::Black);
    int toPaint = map(vol, 0, 255, 0, DisplayType::kNumPixels);
    if (vol < 153) {
        fill_solid(_leds, toPaint, CRGB::Green);
    }
    else if (vol < 204) {
        fill_solid(_leds, DisplayType::kNumPixels * 0.6, CRGB::Green);
        fill_solid(_leds + (int(DisplayType::kNumPixels * 0.6)), toPaint - DisplayType::kNumPixels * 0.6, CRGB::Orange);
    }
    else if (vol >= 204) {
        fill_solid(_leds, DisplayType::kNumPixels * 0.6, CRGB::Green);
        fill_solid(_leds + (int(DisplayType::kNumPixels * 0.6)), DisplayType::kNumPixels * 0.2, CRGB::Orange);
        fill_solid(_leds + (int(DisplayType::kNumPixels * 0.8)), toPaint - DisplayType::kNumPixels * 0.8, CRGB::Red);
    }
}

int VisualizerAnimation::_getPeakPosition()
{
    int pos = 0;
    uint8_t posval = 0;
    for (int i = 0; i < kVisualizerPacketSize && _incomingPacket[i]; i++) {
        if (_incomingPacket[i] > posval) {
            pos = i;
            posval = _incomingPacket[i];
        }
    }
    if (posval < 30) {
        pos = -1;
    }
    return pos;
}

void VisualizerAnimation::_printPeak(CHSV c, int pos, int grpSize)
{
    fadeToBlackBy(_leds, DisplayType::kNumPixels, 12);
    _leds[pos] = c;
    // CHSV c2 = c;
    // c2.v = 150;
    for (int i = 0; i < ((grpSize - 1) / 2); i++) {
        _leds[pos + i] = c;
        _leds[pos - i] = c;
    }
}

void VisualizerAnimation::_setBar(int row, double num, CRGB color, bool fill_ends)
{
    _setBar(row, num, rgb2hsv_approximate(color), fill_ends);
}

void VisualizerAnimation::_setBar(int row, int num, CRGB color)
{
    _setBar(row, num, rgb2hsv_approximate(color));
}

void VisualizerAnimation::_setBar(int row, double num, CHSV col, bool fill_ends)
{
    //fill_solid(leds + (row * kRows), kRows, CRGB::Black);
    //fadeToBlackBy(leds + (row * kRows), kRows, 100);
    double f = (num / 255.00) * kRows;
    for (int l = 0; l < f; l++)
    {
        // if (row % 2 == 1 && true) {
            _leds[row * kRows + l] = col;
        // }
        // else {
            // _leds[(row + 1) * kRows - (l + 1)] = col;
        // }
    }
    if (fill_ends && f != kRows) {
        // if (row % 2 == 1 && true) {
            _leds[row * kRows + (int)f + 1] = CHSV(col.hue, col.sat, col.val * (int(f) - f));
        // }
        // else {
            // _leds[(row + 1) * kRows - (int)f - 1] = CHSV(col.hue, col.sat, col.val * (int(f) - f));
        // }
    }
}

void VisualizerAnimation::_setBar(int row, int num, CHSV col)
{
        // fill_solid(_leds + (row * kRows), num, col);
//     // }
//     // else {
        for (int i = 0; i < num; i++) {
            _leds[(row + 1) * kRows - i] = col;
        }
//     // }
}

void VisualizerAnimation::_setBarDoubleSided(int row, double num, CRGB color, bool fill_ends)
{
    _setBarDoubleSided(row, num, rgb2hsv_approximate(color), fill_ends);
}

void VisualizerAnimation::_setBarDoubleSided(int row, double num, CHSV col, bool fill_ends)
{
    double f = (num / 255.00) * (kRows / 2);
    for (int l = 0; l < f; l++) {
        // if (row % 2 == 1) {
            _leds[row * kRows + l] = col;
            _leds[(row + 1) * kRows - (l + 1)] = col;
        // }
        // else {
            // _leds[(row + 1) * kRows - (l + 1)] = col;
            // _leds[(row)*kRows + l] = col;
        // }
    }
    if (fill_ends && f != kRows)
    {
        // if (row % 2 == 1) {
            _leds[row * kRows + (int)f + 1] = CHSV(col.hue, col.sat, col.val * (int(f) - f));
        // }
        // else {
            // _leds[(row + 1) * kRows - (int)f - 1] = CHSV(col.hue, col.sat, col.val * (int(f) - f));
        // }
    }
}

void VisualizerAnimation::_RainbowVisualizer()
{
    for (int i = 0; i < kCols && _incomingPacket[i]; i++) {
        _setBar(i, static_cast<double>(_incomingPacket[i]), CHSV(map(i, 0, kCols - 1, 0, 255), 255, 255), false);
    }
}

void VisualizerAnimation::_RainbowVisualizerDoubleSided()
{
    for (int i = 0; i < kCols && _incomingPacket[i]; i++) {
        _setBarDoubleSided(i, static_cast<double>(_incomingPacket[i]), CHSV(map(i, 0, kCols - 1, 0, 255), 255, 255), false);
    }
}

void VisualizerAnimation::_SingleColorVisualizer()
{
    for (int i = 0; i < kCols && _incomingPacket[i]; i++) {
        _setBar(i, static_cast<double>(_incomingPacket[i]), CRGB(_cfg.color));
    }
}

void VisualizerAnimation::_SingleColorVisualizerDoubleSided()
{
    for (int i = 0; i < kCols && _incomingPacket[i]; i++) {
        _setBarDoubleSided(i, static_cast<double>(_incomingPacket[i]), CRGB(_cfg.color), false);
    }
}

#endif
