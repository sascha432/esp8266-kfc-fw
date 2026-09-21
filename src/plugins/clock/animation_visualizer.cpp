/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_LED_MATRIX_ENABLE_VISUALIZER

#include "clock.h"
#include "animation.h"
#include "stl_ext/algorithm.h"
#include "InterpolationLib.h"
#if IOT_LED_MATRIX_ENABLE_VISUALIZER_I2S_MICROPHONE
#    include "i2s_microphone.h"
#endif

#if 0
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

// most code is from
// https://github.com/NimmLor/esp8266-fastled-iot-webserver/blob/master/esp8266-fastled-iot-webserver.ino
//
// python software for windows (untested) and other OSs
// scripts/audio_analyzer/
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
#if IOT_LED_MATRIX_ENABLE_VISUALIZER_I2S_MICROPHONE
    VisualizerMicrophone *Clock::VisualizerAnimation::_microphone = nullptr;
#endif
int Clock::VisualizerAnimation::_usageCounter = 0;


VisualizerAnimation::~VisualizerAnimation()
{
    WiFiCallbacks::remove(WiFiCallbacks::EventType::ANY, this);
    if (--_usageCounter == 0) {
        // since _udp is shared among all instances of this class, the last one turns it off
        _udp.stop();
        #if IOT_LED_MATRIX_ENABLE_VISUALIZER_I2S_MICROPHONE
            if (_microphone) {
                delete _microphone;
                _microphone = nullptr;
            }
        #endif
    }
}

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
    #if IOT_LED_MATRIX_ENABLE_VISUALIZER_I2S_MICROPHONE
        if (_microphone) {
            delete _microphone;
            _microphone = nullptr;
        }
    #endif

    _clear();

    // the plasma phase starts at zero, the audio reactions are reset by _clear()
    _plasmaTime = 0;
    _plasmaHue = 0;
    _plasmaLastUpdate = millis();

    WiFiCallbacks::remove(WiFiCallbacks::EventType::ANY, this);

    if (_cfg.get_enum_input(_cfg) == AudioInputType::UDP) {

        __LDBG_printf("enable UDP input");

        // make sure the UDP socket is listening after a reconnect
        WiFiCallbacks::add(WiFiCallbacks::EventType::CONNECTED, [this](WiFiCallbacks::EventType event, void *payload) {
            this->_listen();
        }, this);

        _listen();
    }
    #if IOT_LED_MATRIX_ENABLE_VISUALIZER_I2S_MICROPHONE
        else if (_cfg.get_enum_input(_cfg) == AudioInputType::MICROPHONE) {

            if (!_microphone) {
                _microphone = new VisualizerMicrophone(
                    IOT_LED_MATRIX_I2S_PORT,
                    IOT_LED_MATRIX_I2S_MICROPHONE_SD,
                    IOT_LED_MATRIX_I2S_MICROPHONE_WS,
                    IOT_LED_MATRIX_I2S_MICROPHONE_SCK,
                    _storedData.data(), _storedData.size(),
                    _storedLoudness._leftLevel, _storedLoudness._rightLevel,
                    _cfg.mic_loudness_gain, _cfg.mic_band_gain / 10.0f
                );
                if (!_microphone->isRunning()) {
                    delete _microphone;
                    _microphone = nullptr;
                }
            }
            __LDBG_printf("enable microphone=%p input", _microphone);

        }
    #endif
    else {
        __LDBG_printf("no input source");
    }
}

void VisualizerAnimation::_clear()
{
    _storedLoudness.clear();
    _peakLoudness.clear();
    _peakLoudnessTime = 0;
    _storedData.fill(0);
    _storedPeaks.fill(PeakType());
    _video.clear();
    _lastPacketTime = millis();
    _timeout = kUDPInfiniteTimeout;
    // the plasma phase keeps running, only the audio reactions are reset
    _plasmaLevel = 0;
    _plasmaBands.fill(0);
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
    if (_timeout != kUDPInfiniteTimeout) {
        if (millis() - _lastPacketTime >= _timeout) {
            // clear data set
            _clear();
            _timeout = kUDPInfiniteTimeout;
            __LDBG_printf("UDP timeout");
            return;
        }
    }
    size_t size;
    while((size = _udp.parsePacket()) != 0 && _udp.available()) {
        // read data
        {
            // clear data on read error
            struct ExitScope {
                ExitScope(VisualizerAnimation *parent) :
                    _parent(parent)
                {
                }
                ~ExitScope()
                {
                    #if ESP32
                        _udp.flush();
                    #endif
                    if (_parent) {
                        _parent->_clear();
                        __LDBG_printf("UDP receive error");
                    }
                }
                void release()
                {
                    _parent = nullptr;
                }
                VisualizerAnimation *_parent;
            } exitScope(this);

            if (size >= 128) {
                // video data is sent in big chunks
                VideoHeaderType header;

                if (_udp.readBytes(header, header.size()) != header.size()) {
                    _video.clear();
                    __LDBG_printf("read error=%u", header.size());
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
                    __LDBG_printf("skipping frame");
                    return;
                }

                size -= header.size();
                auto ptr = _video.allocateItemBuffer(size);
                if (!ptr) {
                    // out of ram
                    _video.clear();
                    __LDBG_printf("malloc failed=%u", size);
                    continue;
                }

                if (_udp.readBytes(ptr, size) != size) {
                    _video.clear();
                    __LDBG_printf("read error=%u", size);
                    continue;
                }

                if (_video.isReady()) {
                    __LDBG_printf("frame ready size=%u", _video.getTotalDataSize());
                    _lastPacketTime = millis();
                    exitScope.release();
                    return;
                }

                // try next one
                __LDBG_printf("unknown state");
                exitScope.release();
                continue;
            }

            int firstByte = _udp.peek();
            switch(static_cast<UdpProtocolType>(firstByte)) {
                case UdpProtocolType::WARLS: {
                    struct WARLSProtocol_t {
                        uint8_t protocolId;
                        uint8_t timeout;
                        uint16_t magic1;
                        uint8_t leftChannel;
                        uint8_t rightChannel;
                        uint8_t magic2;
                        uint8_t lines;
                    };

                    if (size < (sizeof(WARLSProtocol_t) + 2)) {
                        __LDBG_printf("size=%u required=%u", size, (sizeof(WARLSProtocol_t) + 2));
                        return; // invalid packet size
                    }
                    if (((size - 2) & 3) != 0) {
                        __LDBG_printf("size=%u aligned=%u", size, ((size - 2) & 3));
                        return; // invalid packet size
                    }
                    WARLSProtocol_t protocol;
                    if (_udp.readBytes(reinterpret_cast<uint8_t *>(&protocol), sizeof(protocol)) != sizeof(protocol)) {
                        __LDBG_printf("read error=%u", sizeof(protocol));
                        return; // read error
                    }
                    if (protocol.timeout == 0) {
                        __LDBG_printf("timeout is zero");
                        return; // timeout 0 ignores the packet
                    }
                    if (protocol.magic1 != ((77 << 8) | 222)) { // 0x4dde
                        __LDBG_printf("magic1!=%04x", protocol.magic1);
                        return; // invalid magic
                    }
                    if (protocol.magic2 != 66) { // 0x42
                        __LDBG_printf("magic2!=%02x", protocol.magic2);
                        return; // invalid magic
                    }
                    if (protocol.lines < 2 || protocol.lines > kVisualizerPacketSize) {
                        // invalid size
                        __LDBG_printf("invalid lines=%u required=2-%u", protocol.lines, kVisualizerPacketSize);
                        return;
                    }

                    // copy values
                    _timeout = protocol.timeout * kUDPTimeoutMultiplier;
                    __LDBG_printf("timeout=%d left=%u right=%u lines=%u", _timeout, protocol.leftChannel, protocol.rightChannel, protocol.lines);
                    _storedLoudness.setLeftChannel(protocol.leftChannel);
                    _storedLoudness.setRightChannel(protocol.rightChannel);

                    if (kVisualizerPacketSize == protocol.lines) {
                        int read = _udp.readBytes(_storedData.begin(), protocol.lines);
                        if (read != protocol.lines) {
                            __LDBG_printf("read error=%u", protocol.lines);
                            return; // read failed
                        }
                    }
                    else if (kVisualizerPacketSize / 2 == protocol.lines) {
                        auto outPtr = _storedData.begin();
                        for(uint8_t i = 0; i < protocol.lines; i++) {
                            auto data = _udp.read();
                            *outPtr++ = data;
                            *outPtr++ = data;
                        }
                    }
                    else if (kVisualizerPacketSize / 4 == protocol.lines) {
                        auto outPtr = _storedData.begin();
                        for(uint8_t i = 0; i < protocol.lines; i++) {
                            auto data = _udp.read();
                            *outPtr++ = data;
                            *outPtr++ = data;
                            *outPtr++ = data;
                            *outPtr++ = data;
                        }
                    }
                    else {
                        double xValues[kVisualizerPacketSize];
                        double yValues[kVisualizerPacketSize];
                        for(uint8_t i = 0; i < protocol.lines; i++) {
                            xValues[i] = i;
                            yValues[i] = _udp.read();
                        }
                        auto outPtr = _storedData.begin();
                        for(uint8_t i = 0; i < kVisualizerPacketSize; i++) {
                            *outPtr++ = Interpolation::CatmullSpline(xValues, yValues, protocol.lines, i * protocol.lines / float(kVisualizerPacketSize));
                        }
                    }

                    _lastPacketTime = millis();

                    exitScope.release();
                    return;
                }
                default:
                    __LDBG_printf("protocol %d not supported", firstByte);
                    break;
            }
        }
    }
}

void VisualizerAnimation::_updatePeakData(uint32_t millisValue)
{
    CoordinateType i = 0;
    auto showPeaks = _cfg.get_enum_peak_show(_cfg);
    for(uint8_t &value: _storedData) {
        _storedPeaks[i++].add(value, millisValue, _cfg.peak_falling_speed, showPeaks);
    }
    if (millisValue - _peakLoudnessTime > 750) {
        _peakLoudness.clear();
        _peakLoudnessTime = millisValue;
    }
    _peakLoudness.updatePeaks(_storedLoudness);
}

// audio reactive plasma
// the audio level is turned into an envelope with a fast attack and a slow release,
// the envelope and the smoothed spectrum are used by the plasma reactions
void VisualizerAnimation::_updatePlasmaAudio(uint32_t millisValue)
{
    const auto &cfg = _cfg.plasma_audio;

    const uint32_t elapsed = millisValue - _plasmaLastUpdate;
    if (elapsed == 0) {
        return;
    }
    _plasmaLastUpdate = millisValue;

    // level of the loudest channel, sensitivity is in percent
    const uint16_t rawLevel = std::max(_storedLoudness.getLeftLevel(), _storedLoudness.getRightLevel());
    const float target = std::min<uint32_t>(255, static_cast<uint32_t>(rawLevel * (cfg.sensitivity / 100.0f))) / 255.0f;

    // fast attack, slow release
    const float tau = (target > _plasmaLevel ? cfg.attack : cfg.release) * 0.001f; // seconds
    float reaction = (elapsed * 0.001f) / tau;
    if (reaction > 1.0f) {
        reaction = 1.0f;
    }
    _plasmaLevel += (target - _plasmaLevel) * reaction;

    // the phase time is accumulated instead of using the millis value, so the speed can be modulated
    float speed = 1.0f;
    if (cfg.enable_speed) {
        speed += _plasmaLevel * (cfg.speed_gain / 255.0f) * kPlasmaMaxSpeedBoost;
    }
    _plasmaTime += elapsed * (cfg.speed * (1.0f / (192.0f * 100000.0f))) * speed;

    if (cfg.enable_hue) {
        _plasmaHue += elapsed * (cfg.hue_gain / 255.0f) * kPlasmaMaxHueRate * _plasmaLevel;
        if (_plasmaHue >= 1024.0f) {
            _plasmaHue -= 1024.0f; // one full hue rotation
        }
    }
}

template<typename _Ta>
void VisualizerAnimation::_copyToPlasmaAudio(_Ta &display)
{
    const auto &cfg = _cfg.plasma_audio;

    // smooth the spectrum, it is updated by the audio input source (a few frames time constant)
    for(size_t i = 0; i < _plasmaBands.size(); i++) {
        _plasmaBands[i] = (_plasmaBands[i] * 3 + _storedData[i]) >> 2;
    }

    float zoomX = 1.0f;
    float zoomY = 1.0f;
    if (cfg.enable_zoom) {
        // the plasma is getting larger with a rising level
        const float zoom = 1.0f + _plasmaLevel * (cfg.zoom_gain / 255.0f) * kPlasmaMaxZoom;
        zoomX = zoom;
        zoomY = zoom;
    }

    const uint8_t *bands = nullptr;
    uint8_t bandGain = 0;
    if (cfg.enable_bands && cfg.band_gain) {
        bands = _plasmaBands.data();
        bandGain = cfg.band_gain;
    }

    const uint32_t hueShift = cfg.hue_shift + static_cast<uint32_t>(_plasmaHue);
    PlasmaField::copyTo(display, cfg, PlasmaField::ParamsType(_plasmaTime, hueShift, zoomX, zoomY, bands, bandGain));
}

template<typename _Ta>
void VisualizerAnimation::_copyTo(_Ta &display, uint32_t millisValue)
{
    _updatePeakData(millisValue);

    // display data
    switch(_cfg.get_enum_type(_cfg)) {
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
            CoordinateType lastRow = display.getRows() - _cfg.vumeter_rows;
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
                        // int pos = ((row << 8) / lastRow);
                        // rgb = CRGB(pos, std::max(255 - pos, 0), 0);
                        int pos = ((row << (8 + 7)) / lastRow); // multiply by 128 shifting another 7 bits to the left
                        pos = std::min(pos / 115, 255); // now we can use 7 bits additional precision: pos * (128/115==1.113)
                        rgb = CRGB(pos, 255 - pos, 0);
                    }
                    // reduce brightness for spectrum pixels
                    // CRGB rgbTmp = rgb;
                    // fadeToBlackBy(&rgbTmp, 1, 200);
                    display.setPixel(row, col, rgb);
                }
                // peak pixels
                if (_cfg.get_enum_peak_show(_cfg) != VisualizerPeakType::DISABLED) {
                    millisValue = millis();
                    auto peakLoudness = _storedPeaks[index].getPeakPosition(millisValue);
                    if (peakLoudness) {
                        // the udp handler updates the pixels in real time in an interrupt
                        // make sure the peak is above the current level at all times
                        CoordinateType peakLoudnessRow = std::min<int>(((std::max<int16_t>(_storedData[index] + 1, peakLoudness) * (lastRow + 1)) >> 8), lastRow - 1);
                        if (peakLoudnessRow >= endRow - 1) {
                            display.setPixel(peakLoudnessRow, col, _storedPeaks[index].getPeakColor(_cfg.peak_extra_color ? uint32_t(_cfg.peak_color) : uint32_t(rgb), millisValue));
                        }
                    }
                }
                // VU Meter
                if (_cfg.vumeter_rows) {
                    constexpr auto colOfs = 0;
                    for (CoordinateType row = lastRow; row < display.getRows(); row++) {
                        if (_cfg.vumeter_peaks && ((col == peakLoudnessColLeft) || (col == peakLoudnessColRight))) {
                            display.setPixel(row, col + colOfs, CRGB(255, 0, 0));
                        }
                        else if (col >= loudnessLeft && col < loudnessRight) {
                            int position = ((centerCol - col) * 256) / centerCol;
                            if (position < 0) {
                                position = -position;
                            }
                            display.setPixel(row, col + colOfs, CRGB(position, std::max(200 - position, 0), 0));
                        }
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
        case VisualizerAnimationType::PLASMA_AUDIO: {
            _copyToPlasmaAudio(display);
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
    if (_cfg.get_enum_input(_cfg) == AudioInputType::UDP) {
        // read udp in every loop
        _parseUdp();
    }
    if (_cfg.get_enum_type(_cfg) == VisualizerAnimationType::PLASMA_AUDIO) {
        _updatePlasmaAudio(millisValue);
    }
}

#endif
