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
        VisualizerAnimation(ClockPlugin &clock, Clock::Color color, VisualizerAnimationConfig &cfg) :
            Animation(clock, color),
            _storedLoudness(),
            _storedData(),
            _storedPeaks(),
            _cfg(cfg)
        {
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
                    auto color = _getColor();
                    int end = (display.getCols() * _storedLoudness) / (kVisualizerMaxPacketValue);
                    for (int row = 0; row < display.getRows(); row++) {
                        for (int col = 0; col < display.getCols(); col++) {
                            if (col < end) {
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
                case VisualizerAnimationType::SPECTRUM_COLOR_BARS_2D:
                case VisualizerAnimationType::SPECTRUM_RAINBOW_BARS_2D: {
                    display.fill(CRGB(0));
                    CHSV hsv;
                    hsv.hue = 0;
                    hsv.val = 255;
                    hsv.sat = 240;
                    int oldIndex = -1;
                    CRGB color = _getColor();
                    for (int col = 0; col < display.getCols(); col++) {
                        auto rgb = _cfg.get_enum_type(_cfg) == VisualizerAnimationType::SPECTRUM_COLOR_BARS_2D ? color : CRGB(hsv);
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
                        if (_cfg.peak_color != 0) {
                            auto peakValue = _storedPeaks[index].getPeakPosition(millisValue);
                            if (peakValue >= 0) {
                                auto peak = std::max<uint16_t>(peakValue * _rowsInterpolation, display.getRows() - 1);
                                display.setPixel(peak, col, _storedPeaks[index].getPeakColor(_cfg.peak_color));
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

        void _listen();
        void _parseUdp();

        void printDebugInfo(Print &output)
        {
            uint32_t ms = millis();
            output.printf_P(PSTR("Max. spectrum bars %u, video data %d, loudness=%.2f%%, peak sink rate=%.4fs\n"), kVisualizerPacketSize, _video._data.size(), (_storedLoudness * 100) / kVisualizerMaxPacketValue, kPeakSinkRate / 1000.0);
            for(uint8_t i = 0; i < kVisualizerPacketSize; i++) {
                output.printf_P(PSTR("%02u: val=%d peak=%d/%d age=%dms"), i, _storedData[i], _storedPeaks[i].getPeakValue(), _storedPeaks[i].getPeakPosition(ms), _storedPeaks[i].getLastPeakUpdate(ms));

            }
            output.println();
        }

    protected:
        static constexpr int kVisualizerPacketSize = 32;
        static constexpr float kVisualizerMaxPacketValue = 254.0;
        static constexpr uint16_t kPeakSinkRate = 2048;

        class PeakType
        {
        public:
            PeakType(uint8_t value = 0, uint32_t millis = 0) :
                _value(value),
                _millis(millis)
            {
            }

            void add(uint8_t value, uint32_t millis)
            {
                if ((value > _value) || (millis - _millis > kPeakSinkRate)) {
                    _value = value;
                    _millis = millis;
                }
            }

            uint16_t getLastPeakUpdate(uint32_t millis) const
            {
                uint32_t diff = (millis - _millis);
                return std::min(kPeakSinkRate + 1U, diff);
            }

            // this function return -1 if no peak is to be displayed
            int16_t getPeakPosition(uint32_t millis) const
            {
                if (_millis == 0 || _value == 0) {
                    return -1;
                }
                int diff = ((millis - _millis) * kVisualizerMaxPacketValue) / kPeakSinkRate;
                if (diff > _value) {
                    return -1;
                }
                return _value - diff;
            }

            uint8_t getPeakValue() const
            {
                return _value;
            }

            CRGB getPeakColor(uint32_t color) const
            {
                return CRGB(color);
            }

        private:
            uint8_t _value;
            uint32_t _millis;
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

        uint8_t _storedLoudness;
        std::array<uint8_t, kVisualizerPacketSize> _storedData;
        std::array<PeakType, kVisualizerPacketSize> _storedPeaks;
        VideoType _video;
        VisualizerAnimationConfig &_cfg;
        WiFiUDP _udp;
        float _colsInterpolation; // horizontal
        float _rowsInterpolation; // vertical
    };

}
