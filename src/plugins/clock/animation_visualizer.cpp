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

    _showLoudness = _cfg.loudness_show ? 1 : 0;
    if (_cfg.get_enum_orientation(_cfg) == OrientationType::HORIZONTAL) {
        _colsInterpolation = _parent._display.getCols() / static_cast<float>(kVisualizerPacketSize);
        _rowsInterpolation = (_parent._display.getRows() - _showLoudness) / kVisualizerMaxPacketValue;
    }
    else {
        _colsInterpolation = _parent._display.getRows() / static_cast<float>(kVisualizerPacketSize);
        _rowsInterpolation = (_parent._display.getCols() - _showLoudness) / kVisualizerMaxPacketValue;

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

extern "C" float m_factor;


void VisualizerAnimation::_parseUdp()
{
    size_t size;
    while((size = _udp.parsePacket()) != 0) {
        if (!_udp.available()) {
            break;
        }
        if (size >= 128) {
            // video data is sent in big chunks
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
            // 1/4 of the size, just quadruple all bytes
            for(int i = 0; i < kVisualizerPacketSize; i++) {
                auto data = _udp.read();
                if (data == -1) {
                    std::fill(_storedData.begin() + i, _storedData.end(), 0);
                    break;
                }
                // quadruple data
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
                // double data
                _storedData[i++] = data;
                _storedData[i] = data;
            }
        }
        else if (size == kVisualizerPacketSize * 2) {
            // double the size, take the peak of 2 values
            uint8_t buffer[2];
            for(int i = 0; i < kVisualizerPacketSize; i++) {
                auto len = _udp.readBytes(buffer, 2);
                if (len != 2) {
                    std::fill(_storedData.begin() + i, _storedData.end(), 0);
                    break;
                }
                _storedData[i] = std::max<int>(buffer[0], buffer[1]); // peak value
            }
        }
        else if (size == 1) {
            // single value, fill the entire spectrum with it
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

        float loudness = 0;
        float totalLoudness = 0.001;
        uint16_t i = 0;
        auto showPeaks = _cfg.get_enum_peak_show(_cfg);
        for(uint8_t &value: _storedData) {
            // correct the logarithmic scales to something that represents the human ear
            #if 1
                // 3.2x faster... 72us
                // tested with GCC 8.4.0 and ESP32
                constexpr uint32_t kShift = 10; // 1024
                constexpr float kSquareNum = 7.97;
                constexpr float kMul = (1.0 / (1 << (kShift >> 1)/* for sqrt(i * 1024) * sqrt(1024) */))/* 1.0 / sqrt(1024) */;
                constexpr uint16_t kMax = (kSquareNum / kMul); // kSquareNum * 32.0
                constexpr float kReducedMul = kMul / (kSquareNum / 2.25); // limit multiplier to 2.25
                static_assert(kMax >= 100 && kMax <= 255, "kMax should above 100 for more precision and must be below 256");
                static_assert((kVisualizerPacketSize << kShift) < 0xffff, "sqrt16() out of range");
                static_assert(std::sqrt(float(1 << kShift)) == float(1 << (kShift >> 1)), "sqrt(1 << kShift) does not match 1 << (kShift >> 1)");
                // increase the small values (0-31) up to 31744 to fit into uint16_t, it uses full 8 bit precision
                float bandCorrection = ((sqrt16(i << kShift/* i * 1024 */))) * kReducedMul/* ((kMax - uint16_t(sqrt(i * 1024))) / 32.0) *  */;
            #else
                // correction factor for loudness value per band, 220-230us
                constexpr float kMul = 5.65685424949238f; // sqrt(32)
                constexpr float kReducedMul = 1 / (kMul / 2.25); // limit multiplier to 2.25
                float bandCorrection = ((kMul - std::sqrt(float(i)))) * kReducedMul;
            #endif
            // add a linear correction that is actually a curve cause bands are logarithmic
            constexpr float kMul2 = (1.0 / float(kVisualizerPacketSize - 1)); // to avoid division
            totalLoudness += bandCorrection;
            loudness += value / bandCorrection;
            _storedPeaks[i++].add(value, timeMillis, _cfg.peak_falling_speed, showPeaks);
        }
        loudness = std::min<float>(std::sqrt(loudness * m_factor) / totalLoudness, 255.0f);
        if (loudness != _storedLoudness) {
            _storedLoudness = loudness;
        }
        if (timeMillis - _peakLoudnessTime > 750) {
            _peakLoudness = 0;
            _peakLoudnessTime = timeMillis;
        }
        _peakLoudness = std::clamp<uint16_t>(_storedLoudness + 1, _peakLoudness, 255);
    }
    return;
}

template<typename _Ta>
void VisualizerAnimation::_copyTo(_Ta &display, uint32_t millisValue)
{
    switch(_cfg.get_enum_type(_cfg)) {
        case VisualizerAnimationType::LOUDNESS_1D: {
            display.fill(CRGB(0));
            auto color = _getColor();
            CoordinateType end = std::max((_storedLoudness * display.getCols()) / 256, display.getCols() - 1);
            CoordinateType peak = std::max((_peakLoudness * display.getCols()) / 256, display.getCols() - 1);
            for (int row = 0; row < display.getRows(); row++) {
                for (int col = 0; col < display.getCols(); col++) {
                    if (col == peak) {
                        display.setPixel(row, col, CRGB(255, 0, 0));
                    }
                    else if (col < end) {
                        display.setPixel(row, col, color);
                    }
                }
            }
        }
        break;
        case VisualizerAnimationType::SPECTRUM_RAINBOW_1D: {
            display.fill(CRGB(0));
            CHSV hsv;
            hsv.hue = 0;
            hsv.val = 255;
            hsv.sat = 240;
            float hue = 0;
            float hueIncr = 232 / static_cast<float>(display.getCols()); // starts with red and ends with pink
            for (int col = 0; col < display.getCols(); col++) {
                hsv.hue = hue;
                hue += hueIncr;
                auto rgb = CRGB(hsv);
                auto index = _getDataIndex(display, col);
                rgb.fadeToBlackBy(kVisualizerMaxPacketValue - _storedData[index]);
                for (int row = 0; row < display.getRows(); row++) {
                    display.setPixel(row, col, rgb);
                }
            }
        }
        break;
        case VisualizerAnimationType::SPECTRUM_COLOR_BARS_2D:
        case VisualizerAnimationType::SPECTRUM_GRADIENT_BARS_2D:
        case VisualizerAnimationType::SPECTRUM_RAINBOW_BARS_2D: {
            display.fill(CRGB(0));
            CHSV hsv;
            hsv.hue = 0;
            hsv.val = 255;
            hsv.sat = 240;
            int16_t oldIndex = -1;
            CRGB color = _getColor();
            CoordinateType lastRow = display.getRows() - _showLoudness;
            CoordinateType centerCol = std::max(display.getCols() >> 1, 1);
            CoordinateType loudnessLen = std::min<int>((_storedLoudness * centerCol) >> 8, centerCol);
            CoordinateType peakLoudnessCol = std::min<int>((_peakLoudness * centerCol) >> 8, centerCol);
            CoordinateType peakLoudnessColLeft = std::max(centerCol - peakLoudnessCol - 1, 0);
            CoordinateType peakLoudnessColRight = std::min(centerCol + peakLoudnessCol, display.getCols() - 1);
            for (CoordinateType col = 0; col < display.getCols(); col++) {
                CRGB rgb;
                switch(_cfg.get_enum_type(_cfg)) {
                    case VisualizerAnimationType::SPECTRUM_RAINBOW_BARS_2D:
                        rgb = CRGB(hsv);
                        break;
                    case VisualizerAnimationType::SPECTRUM_COLOR_BARS_2D:
                    default:
                        rgb = color;
                        break;

                }
                    int8_t index = _getDataIndex(display, col);
                if (index != oldIndex) {
                    oldIndex = index;
                    // change color for each bar
                    hsv.hue += (224 / kVisualizerPacketSize); // starts with red and ends with pink
                }
                int end = _storedData[index] * _rowsInterpolation;
                for (CoordinateType row = 0; row < lastRow; row++) {
                    if (row < end) {
                        if (_cfg.get_enum_type(_cfg) == VisualizerAnimationType::SPECTRUM_GRADIENT_BARS_2D) {
                            int pos = ((row * 255) / lastRow);
                            rgb = CRGB(pos, std::max(168 - pos, 0), 0);
                        }
                        display.setPixel(row, col, rgb);
                    }
                }
                if (_cfg.get_enum_peak_show(_cfg) != VisualizerPeakType::DISABLED) {
                    // update time, the udp handler updates the pixels in real time in an interrupt
                    millisValue = millis();
                    auto numRows = display.getRows();
                    auto peakValue = _storedPeaks[index].getPeakPosition(millisValue, numRows);
                    if (peakValue >= 0) {
                        auto peak = std::clamp<uint16_t>(peakValue * _rowsInterpolation, end - 1, numRows - 1);
                        display.setPixel(peak, col, _storedPeaks[index].getPeakColor(_cfg.peak_extra_color ? _cfg.peak_color : rgb, millisValue));
                    }
                }
                if (_showLoudness) {
                    auto row = display.getRows() - 1;
                    if (_cfg.loudness_peaks && ((col == peakLoudnessColLeft) || (col == peakLoudnessColRight))) {
                        display.setPixel(row, col, CRGB(255, 0, 0));
                    }
                    else if (col >= centerCol - loudnessLen && col < centerCol + loudnessLen) {
                        int position = ((centerCol - col) * 255) / centerCol;
                        if (position < 0) {
                            position = -position;
                        }
                        display.setPixel(row, col, CRGB(position, std::max(200 - position, 0), 0));
                    }
                }
            }
        }
        break;
        case VisualizerAnimationType::RGB24_VIDEO:
        case VisualizerAnimationType::RGB565_VIDEO: {
            if (_video.isReady()) {
                _video.begin();
                for (int row = display.getRows() - 1; row >= 0; row--) {
                    for (int col = 0; col < display.getCols(); col++) {
                        display.setPixel(row, col, _video.getRGB());
                    }
                }
                _video.clear();
            }
        }
        break;
        default:
            //TODO
            uint8_t r = ((millis() / 500) % 2) ? 32 : 0;
            uint8_t g = r ? 0 : 32;
            display.fill(CRGB(r, g, 0));
            break;
    }
}


void VisualizerAnimation::loop(uint32_t millisValue)
{
    // read udp in every loop
    _parseUdp();
}

#endif
