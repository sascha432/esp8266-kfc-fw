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
        _colsInterpolation = _parent._display.getCols() / static_cast<float>(kVisualizerPacketSize);
        _rowsInterpolation = (_parent._display.getRows() - _cfg.loudness_show) / kVisualizerMaxPacketValue;
    }
    else {
        _colsInterpolation = _parent._display.getRows() / static_cast<float>(kVisualizerPacketSize);
        _rowsInterpolation = (_parent._display.getCols() - _cfg.loudness_show) / kVisualizerMaxPacketValue;

    }

    _clear();
    _timeout = 5;
    _lastPacketTime = millis();

    // make sure the UDP socket is listening after a reconnect
    WiFiCallbacks::add(WiFiCallbacks::EventType::CONNECTED, [this](WiFiCallbacks::EventType event, void *payload) {
        this->_listen();
    }, this);

    _listen();
}

void VisualizerAnimation::_clear()
{
    _storedLoudness.clear();
    _peakLoudness.clear();
    _peakLoudnessTime = 0;
    _storedData.fill(0);
    _storedPeaks.fill(PeakType());
    _video.clear();
    _lastPacketTime = 0;
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


enum class UdpProtocolType : uint8_t {
    /*
        byte zero is the protocol id
        byte one is idle timeout (255 = no timeout)

        realistically, we can update about 150-250 LEDs, before UDP packets are getting to big



        WARLS (max 256 LEDs, up to 1026 byte, any of the 256 LEDs can be addressed)

        2 + n*4	LED Index
        3 + n*4	Red Value
        4 + n*4	Green Value
        5 + n*4	Blue Value
   */
    WARLS = 1,
    /*

        DRGB - NOT SUPPORTED (max 490 LEDs, 1470 byte, updating LED 490)

        2 + n*3	Red Value
        3 + n*3	Green Value
        4 + n*3	Blue Value
    */
    DRGB = 2,
    /*

        DRGBW - NOT SUPPORTED (max 367 LEDs, 1470 byte)

        2 + n*4	Red Value
        3 + n*4	Green Value
        4 + n*4	Blue Value
        5 + n*4	White Value
     * */
    DRGBW = 3,
    /*

        DNRGB (max. 65535 LEDs, up to 488 LEDs per 1470 byte packet, any of the 65535 LEDs can be addressed)

        2	Start index high byte
        3	Start index low byte
        4 + n*3	Red Value
        5 + n*3	Green Value
        6 + n*3	Blue Value

     */
    DNRGB = 4,
    /*
        WLEN_NOTIFIER - NOT SUPPORTED
    */
    WLEN_NOTIFIER = 5,
    /*
        if the first protocol byte is unknown and the packet size can be divided by 3, assume it is raw data

        for example https://github.com/scottlawsonbc/audio-reactive-led-strip/tree/master
        It can be modifed to use the WARLS protocol easily:
        Insert the following code in led.py after line 66: m.append(1); m.append(2); These are the first two bytes of the protocol. (line numbers probably outdated)

    */
    RAW = 255,
};

void VisualizerAnimation::_parseUdp()
{
    if (_timeout != 255 && _timeout != 0) {
        if (millis() - _lastPacketTime >= _timeout) {
            // clear display and set timeout to 0
            _parent._display.fill(CRGB(0));
            _timeout = 0;
            return;
        }
    }
    size_t size;
    while((size = _udp.parsePacket()) != 0) {
        if (!_udp.available()) {
            break;
        }
        int firstByte = _udp.peek();
        if (firstByte - 1) {
            break;
        }
        switch(static_cast<UdpProtocolType>(firstByte)) {
            case UdpProtocolType::WARLS: {
                struct ExitScope {
                    ExitScope(VisualizerAnimation *parent) : _parent(parent) {}
                    ~ExitScope() {
                        if (_parent) {
                            _parent->_clear();
                            __DBG_printf("UDP receive error");
                        }
                    }
                    void end() {
                        _parent = nullptr;
                    }
                    VisualizerAnimation *_parent;
                } exitScope(this);

                if (size < 6 && ((size - 2) & 3) != 0) {
                    // invalid packet size
                    return;
                }
                _udp.read();
                _timeout = _udp.read();
                if (_timeout == 0) {
                    // timeout 0 ignores the packet
                    return;
                }
                // verify magic
                if (_udp.read() != 255) {
                    return;
                }
                if (_udp.read() != 99) {
                    return;
                }
                if (_udp.read() != 88) {
                    return;
                }
                if (_udp.read() != 77) {
                    return;
                }
                if (_udp.read() != 66) {
                    return;
                }
                if (_udp.read() != 55) {
                    return;
                }
                _storedLoudness.setLeftChannel(_udp.read());
                _storedLoudness.setRightChannel(_udp.read());
                if (_udp.read() != 44) {
                    return;
                }
                uint8_t lines = _udp.read();
                if (lines != kVisualizerPacketSize) {
                    // invalid size
                    // we have no reshaping yet
                    return;
                }
                int read = _udp.readBytes(_storedData.begin(), lines);
                if (read != lines) {
                    // read failed
                    return;
                }
                // we do not read the rest...
                _lastPacketTime = millis();
                exitScope.end();
                _processNewData();
                return;
            }
            default:
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
                _lastPacketTime = millis();
                continue;
            }

            if (_video.isReady()) {
                return;
            }
        }

/*
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
        */
    }
}

void VisualizerAnimation::_processNewData()
{
    auto timeMillis = millis();

    uint16_t i = 0;
    auto showPeaks = _cfg.get_enum_peak_show(_cfg);
    for(uint8_t &value: _storedData) {
        _storedPeaks[i++].add(value, timeMillis, _cfg.peak_falling_speed, showPeaks);
    }
    if (timeMillis - _peakLoudnessTime > 750) {
        _peakLoudness.clear();
        _peakLoudnessTime = timeMillis;
    }
    _peakLoudness.updatePeaks(_storedLoudness);
}

template<typename _Ta>
void VisualizerAnimation::_copyTo(_Ta &display, uint32_t millisValue)
{
    switch(_cfg.get_enum_type(_cfg)) {
        case VisualizerAnimationType::VUMETER_COLOR_1D:
        case VisualizerAnimationType::VUMETER_1D: {
            display.fill(CRGB(0));
            auto color = _getColor();
            auto cols = display.getCols();
            auto visType = _cfg.get_enum_type(_cfg);
            auto peakLevel = (_peakLoudness.getLeftLevel() + _peakLoudness.getRightLevel()) >> 1;
            CoordinateType end = std::min(((_storedLoudness.getLeftLevel() + _storedLoudness.getRightLevel()) * cols) / 512, cols - 1);
            CoordinateType peak = std::min((peakLevel * cols) / 256, cols - 1);
            for (int row = 0; row < display.getRows(); row++) {
                for (int col = 0; col < cols; col++) {
                    auto colOfs = _cfg.loudness_offset ? ((col + _cfg.loudness_offset) % cols) : cols;
                    if (_cfg.loudness_peaks && col == peak) {
                        display.setPixel(row, colOfs, CRGB(255, 0, 0));
                    }
                    else if (col < end) {
                        if (visType == VisualizerAnimationType::VUMETER_COLOR_1D) {
                            display.setPixel(row, colOfs, color);
                        }
                        else {
                            display.setPixel(row, colOfs, CRGB(255, std::max(255 - peakLevel, 0), 0));
                        }
                    }
                }
            }
        }
        break;
        case VisualizerAnimationType::VUMETER_COLOR_STEREO_1D:
        case VisualizerAnimationType::VUMETER_STEREO_1D: {
            display.fill(CRGB(0));
            auto color = _getColor();
            auto cols = display.getCols();
            auto visType = _cfg.get_enum_type(_cfg);
            auto peakLevel = (_peakLoudness.getLeftLevel() + _peakLoudness.getRightLevel()) >> 1;
            CoordinateType centerCol = std::max(display.getCols() >> 1, 1);
            CoordinateType loudnessLeft = std::max(centerCol - ((_storedLoudness.getLeftLevel() * centerCol) >> 8), 0);
            CoordinateType loudnessRight = std::min(centerCol + ((_storedLoudness.getRightLevel() * centerCol) >> 8), cols - 1);
            CoordinateType peakLoudnessColLeft = std::max(centerCol - ((_peakLoudness.getLeftLevel() * centerCol) >> 8) - 1, 0);
            CoordinateType peakLoudnessColRight = std::min(centerCol + ((_peakLoudness.getRightLevel() * centerCol) >> 8), cols - 1);
            for (int row = 0; row < display.getRows(); row++) {
                for (int col = 0; col < cols; col++) {
                    auto colOfs = _cfg.loudness_offset ? ((col + _cfg.loudness_offset) % cols) : cols;
                    if (_cfg.loudness_peaks && ((col == peakLoudnessColLeft) || (col == peakLoudnessColRight))) {
                        display.setPixel(row, colOfs, CRGB(255, 0, 0));
                    }
                    else if (col >= loudnessLeft && col < loudnessRight) {
                        if (visType == VisualizerAnimationType::VUMETER_COLOR_STEREO_1D) {
                            display.setPixel(row, colOfs, color);
                        }
                        else {
                            int pos = (col < centerCol ? _peakLoudness.getLeftLevel() : _peakLoudness.getRightLevel()) >> 1;
                            display.setPixel(row, colOfs, CRGB(255, std::max(255 - pos, 0), 0));
                        }
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
            auto cols = display.getCols();
            float hue = 0;
            float hueIncr = 232 / static_cast<float>(cols); // starts with red and ends with pink
            for (int col = 0; col < cols; col++) {
                auto colOfs = _cfg.loudness_offset ? ((col + _cfg.loudness_offset) % cols) : cols;
                hsv.hue = hue;
                hue += hueIncr;
                auto rgb = CRGB(hsv);
                auto index = _getDataIndex(display, col);
                rgb.fadeToBlackBy(kVisualizerMaxPacketValue - _storedData[index]);
                for (int row = 0; row < display.getRows(); row++) {
                    display.setPixel(row, colOfs, rgb);
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
            CoordinateType lastRow = display.getRows() - _cfg.loudness_show;
            CoordinateType centerCol = std::max(display.getCols() >> 1, 1);
            CoordinateType loudnessLeft = std::max(centerCol - ((_storedLoudness.getLeftLevel() * centerCol) >> 8), 0);
            CoordinateType loudnessRight = std::min(centerCol + ((_storedLoudness.getRightLevel() * centerCol) >> 8), display.getCols() - 1);
            CoordinateType peakLoudnessColLeft = std::max(centerCol - ((_peakLoudness.getLeftLevel() * centerCol) >> 8) - 1, 0);
            CoordinateType peakLoudnessColRight = std::min(centerCol + ((_peakLoudness.getRightLevel() * centerCol) >> 8), display.getCols() - 1);
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
                // bar pixels
                CoordinateType endRow = std::min<int>(((_storedData[index] * (lastRow + 1)) >> 8), lastRow);
                for (CoordinateType row = 0; row < endRow; row++) {
                    if (_cfg.get_enum_type(_cfg) == VisualizerAnimationType::SPECTRUM_GRADIENT_BARS_2D) {
                        int pos = ((row * 256) / lastRow);
                        rgb = CRGB(pos, std::max(255 - pos, 0), 0);
                    }
                    display.setPixel(row, col, rgb);
                }
                // peak pixels
                if (_cfg.get_enum_peak_show(_cfg) != VisualizerPeakType::DISABLED) {
                    millisValue = millis();
                    auto peakLoudness = _storedPeaks[index].getPeakPosition(millisValue);
                    if (peakLoudness) {
                        // update time, the udp handler updates the pixels in real time in an interrupt
                        CoordinateType peakLoudnessRow = std::min<int>(((peakLoudness * (lastRow + 1)) >> 8), lastRow - 1);
                        if (peakLoudnessRow >= endRow - 1) {
                            display.setPixel(peakLoudnessRow, col, _storedPeaks[index].getPeakColor(_cfg.peak_extra_color ? _cfg.peak_color : rgb, millisValue));
                        }
                        else {
                            // the pixel has fallen below the current level
                            // get the stored peak value
                            peakLoudnessRow = std::min<int>(((_storedPeaks[index].getPeakValue() * (lastRow + 1)) >> 8), lastRow - 1);
                            if (peakLoudnessRow >= endRow - 1) {
                                display.setPixel(peakLoudnessRow, col, _storedPeaks[index].getPeakColor(_cfg.peak_extra_color ? _cfg.peak_color : rgb, millisValue));
                            }
                        }
                    }
                }
                // VU Meter
                if (_cfg.loudness_show) {
                    if (_cfg.loudness_peaks && ((col == peakLoudnessColLeft) || (col == peakLoudnessColRight))) {
                        display.setPixel(lastRow, col, CRGB(255, 0, 0));
                    }
                    else if (col >= loudnessLeft && col < loudnessRight) {
                        int position = ((centerCol - col) * 256) / centerCol;
                        if (position < 0) {
                            position = -position;
                        }
                        display.setPixel(lastRow, col, CRGB(position, std::max(200 - position, 0), 0));
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
            //TODO blink red green to show an error has occurred
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
