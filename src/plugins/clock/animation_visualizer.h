/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "animation.h"
#include <FastLED.h>

// how to create video streaming:
// ffmpeg -i test.mp4 -vf "fps=10,scale=32:16:flags=lanczos,eq=gamma=0.7" -pix_fmt rgb24 -c:v rawvideo test.rgb -y
// then use python udp_send_packet.py to broadcast this to one or more devices. 512LEDs 32x16 was not impressive.
// a lot issues with gamma correction and that LEDs do not seem to work well with RGB conversion from videos.

namespace Clock {

    class VisualizerAnimation : public Animation {
    public:
        using VisualizerAnimationConfig = KFCConfigurationClasses::Plugins::ClockConfigNS::VisualizerType;
        using VisualizerAnimationType = VisualizerAnimationConfig::VisualizerAnimationType;
        using OrientationType = VisualizerAnimationConfig::OrientationType;
        using DisplayType = Clock::DisplayType;

    public:
        VisualizerAnimation(ClockPlugin &clock, VisualizerAnimationConfig &cfg) :
            Animation(clock),
            _lastUpdate(0),
            _storedLoudness(),
            _storedData(),
            _storedPeaks(),
            _cfg(cfg),
            _lastBrightness(0),
            _lastFinished(true)
        {
            _lastCol = cfg.color;
            _disableBlinkColon = true;
        }

        ~VisualizerAnimation()
        {
            _udp.stop();
        }

        virtual void begin() override;

        virtual void loop(uint32_t millisValue) override;

        virtual void copyTo(DisplayType &display, uint32_t millisValue) override
        {
            _copyTo(display, millisValue);
        }

        virtual void copyTo(DisplayBufferType &buffer, uint32_t millisValue) override
        {
            _copyTo(buffer, millisValue);
        }

        template<typename _Ta>
        uint8_t _getDataIndex(_Ta &display, int col)
        {
            return std::clamp<uint8_t>((col * kVisualizerPacketSize) / static_cast<float>(display.getCols()), 0, kVisualizerPacketSize);
        }

        template<typename _Ta>
        void _copyTo(_Ta &display, uint32_t millisValue)
        {

            switch(_cfg.get_enum_type(_cfg)) {
                case VisualizerAnimationType::LOUDNESS_1D: {
                    display.fill(CRGB(0));
                    int end = (display.getCols() * _storedLoudness) / (kVisualizerMaxPacketValue);
                    for (int row = 0; row < display.getRows(); row++) {
                        for (int col = 0; col < display.getCols(); col++) {
                            if (col < end) {
                                display.setPixel(row, col, _lastCol);
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
                    float hueIncr = 256.0 / display.getCols();
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
                case VisualizerAnimationType::SPECTRUM_RAINBOW_BARS_2D: {
                    display.fill(CRGB(0));
                    CHSV hsv;
                    hsv.hue = 0;
                    hsv.val = 255;
                    hsv.sat = 240;
                    int oldIndex = -1;
                    for (int col = 0; col < display.getCols(); col++) {
                        auto rgb = CRGB(hsv);
                        auto index = _getDataIndex(display, col);
                        if (index != oldIndex) {
                            oldIndex = index;
                            // change color for each bar
                            hsv.hue += (256 / kVisualizerPacketSize); // show full spectrum over the entire width
                        }
                        auto end = _storedData[index] * _rowsInterpolation;
                        for (int row = 0; row < display.getRows(); row++) {
                            if (row < end) {
                                display.setPixel(row, col, rgb);
                            }
                        }
                    }
                }
                break;
                case VisualizerAnimationType::SPECTRUM_COLOR_BARS_2D: {
                    display.fill(CRGB(0));
                    for (int col = 0; col < display.getCols(); col++) {
                        auto index = _getDataIndex(display, col);
                        auto end = _storedData[index] * _rowsInterpolation;
                        for (int row = 0; row < display.getRows(); row++) {
                            if (row < end) {
                                display.setPixel(row, col, _lastCol);
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

        #if 0
        // unused and untested code
        int _getVolume(uint8_t vals[], int start, int end, double factor);
        void _BrightnessVisualizer();
        bool _FadeUp(CRGB c, int start, int end, int update_rate, int starting_brightness, bool isNew);
        void _TrailingBulletsVisualizer();
        void _ShiftLeds(int shiftAmount);
        void _SendLeds(CHSV c, int shiftAmount);
        void _SendTrailingLeds(CHSV c, int shiftAmount);
        CHSV _getVisualizerBulletValue(double cd);
        CHSV _getVisualizerBulletValue(int hue, double cd);
        void _vuMeter(CHSV c, int mode);
        void _vuMeterTriColor();
        int _getPeakPosition();
        void _printPeak(CHSV c, int pos, int grpSize);
        #endif

        bool _parseUdp();

    protected:
        static constexpr int kVisualizerPacketSize = 32;
        static constexpr float kVisualizerMaxPacketValue = 254.0;

        class PeakType
        {
        public:
            uint8_t value;
            uint32_t millis;

            PeakType(uint8_t _value = 0, uint32_t _millis = 0) : value(_value), millis(_millis)
            {}

            void add(uint8_t _value, uint32_t _millis)
            {
                // timeout, reset peak
                if (_millis - millis > 5000) {
                    value = _value;
                }
                else {
                    value = std::min(value, _value);
                }
                millis = _millis;
            }

            uint16_t getPeakPosition() const {
                //TODO
                return 0;
            }

            CRGB getPeakColor() const {
                // TODO
                return CRGB(0);
            }
        };

        enum ColorFormatType : uint8_t {
            RGB565,
            RGB24,
            BGR24,
        };

        using VideoStorageType = std::vector<uint8_t>;

        class VideoHeaderType
        {
        public:
            uint32_t _frameId;
            uint16_t _position;
            ColorFormatType _format;
            uint8_t _reserved;

            VideoHeaderType() :
                _frameId(0),
                _position(0),
                _format(ColorFormatType::RGB565)
            {
            }

            void invalidate()
            {
                _position = 0;
                _frameId++;
            }

            uint8_t getBytesPerPixel() const
            {
                switch(_format) {
                    case ColorFormatType::RGB565:
                        return 2;
                    case ColorFormatType::RGB24:
                    case ColorFormatType::BGR24:
                        return 3;
                }
                return 1;
            }

            size_t getPosition() const
            {
                return _position * getBytesPerPixel();
            }

            operator uint8_t *() {
                return reinterpret_cast<uint8_t *>(this);
            }

            constexpr size_t size() const {
                return sizeof(*this);
            }

        };

        class VideoType
        {
        public:
            VideoHeaderType _header;
            VideoStorageType _data;
            VideoStorageType::iterator _position;

            VideoType() : _header()
            {
            }

            void invalidate()
            {
                _header.invalidate();
                _data.clear();
            }

            void clear()
            {
                _header.invalidate();
                _data.clear();
            }

            bool isValid(const VideoHeaderType &header);

            uint8_t *allocateItemBuffer(size_t addSize)
            {
                auto size = _data.size();
                _data.resize(_data.size() + addSize);
                if (_data.size() == size + addSize) {
                    return _data.data() + size;
                }
                return nullptr;
            }

            bool isReady() const
            {
                return _data.size() >= getTotalDataSize();
            }

            size_t getTotalDataSize() const
            {
                return kNumPixels * _header.getBytesPerPixel();
            }

            void begin()
            {
                _position = _data.begin();
            }

            CRGB getRGB();
        };

        uint32_t _lastUpdate;
        uint16_t _updateRate;
        uint8_t _storedLoudness;
        std::array<uint8_t, kVisualizerPacketSize> _storedData;
        std::array<PeakType, kVisualizerPacketSize> _storedPeaks;
        VideoType _video;
        VisualizerAnimationConfig &_cfg;
        WiFiUDP _udp;
        float _colsInterpolation; // horizontal
        float _rowsInterpolation; // vertical
        int _lastBrightness;
        bool _lastFinished;
        CRGB _lastCol;
    };

}
