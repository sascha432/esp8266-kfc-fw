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
        using VisualizerPeakType = VisualizerAnimationConfig::VisualizerPeakType;
        using OrientationType = VisualizerAnimationConfig::OrientationType;
        using DisplayType = Clock::DisplayType;

    public:
        VisualizerAnimation(ClockPlugin &clock, Clock::Color color, VisualizerAnimationConfig &cfg) :
            Animation(clock, color),
            _storedLoudness(0),
            _peakLoudness(0),
            _peakLoudnessTime(0),
            _storedData(),
            _storedPeaks(),
            _video(getNumPixels()),
            _cfg(cfg)
        {
            _udpUsageCounter++;
            _disableBlinkColon = true;
        }

        ~VisualizerAnimation()
        {
            WiFiCallbacks::remove(WiFiCallbacks::EventType::ANY, this);
            if (--_udpUsageCounter == 0) {
                // since _udp is shared among all instances of this class, the last one turns it off
                _udp.stop();
            }
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
            return std::clamp<uint8_t>((col * kVisualizerPacketSize) / static_cast<float>(display.getCols()), 0, kVisualizerPacketSize - 1);
        }

        template<typename _Ta>
        void _copyTo(_Ta &display, uint32_t millisValue);

        void _listen();
        void _parseUdp();

        void printDebugInfo(Print &output)
        {
            uint32_t ms = millis();
            output.printf_P(PSTR("Max. spectrum bars %u, video data %d, loudness=%.2f%%, peak sink rate=%.4fs\n"), kVisualizerPacketSize, _video._data.size(), (_storedLoudness * 100) / kVisualizerMaxPacketValue, _cfg.peak_falling_speed / 1000.0);
            for(uint8_t i = 0; i < kVisualizerPacketSize; i++) {
                output.printf_P(PSTR("%02u: val=%d peak=%d/%d age=%dms\n"), i, _storedData[i], _storedPeaks[i].getPeakValue(), _storedPeaks[i].getPeakPosition(ms, getRows()), _storedPeaks[i].getLastPeakUpdate(ms));

            }
        }

    protected:
        static constexpr int kVisualizerPacketSize = 32;
        static constexpr float kVisualizerMaxPacketValue = 254.0;

        class PeakType
        {
        public:
            PeakType(uint8_t value = 0, uint32_t millis = 0, uint16_t peakSinkRate = 0, VisualizerPeakType peakType = VisualizerPeakType::DISABLED) :
                _millis(millis),
                _value(value),
                _peakType(peakType),
                _peakSinkRate(peakSinkRate)
            {
            }

            void add(uint8_t value, uint32_t millis, uint16_t peakSinkRate, VisualizerPeakType peakType)
            {
                _peakSinkRate = peakSinkRate;
                _peakType = peakType;
                if (value > _value || (_peakType == VisualizerPeakType::FADING && getFading(millis) == 255)) {
                    _value = value;
                    _millis = millis;
                }
            }

            uint16_t getLastPeakUpdate(uint32_t millis) const
            {
                return std::min(_peakSinkRate + 1U, _diff(millis));
            }

            uint8_t getFading(uint32_t millis) const
            {
                return std::min<uint32_t>(_diff(millis) * 256 / _peakSinkRate, 255);
            }

            // this function return -1 if no peak is to be displayed
            int16_t getPeakPosition(uint32_t millis, uint16_t rows) const
            {
                if (_peakType == VisualizerPeakType::FADING) {
                    return _value;
                }
                if (_millis == 0 || _value == 0) {
                    return -1;
                }
                int diff = (_diff(millis) * rows) / _peakSinkRate;
                if (diff > _value) {
                    return -1;
                }
                return _value - diff;
            }

            uint8_t getPeakValue() const
            {
                return _value;
            }

            CRGB getPeakColor(uint32_t color, uint32_t millis) const
            {
                CRGB value;
                switch(_peakType) {
                    case VisualizerPeakType::FADING:
                        value = CRGB(color);
                        fadeToBlackBy(&value, 1, getFading(millis));
                        break;
                    case VisualizerPeakType::DISABLED:
                        return CRGB(0);
                    case VisualizerPeakType::ENABLED:
                    case VisualizerPeakType::FALLING_DOWN:
                    default:
                        value = CRGB(color);
                        break;
                }
                return value;
            }

        private:

            inline __attribute__((__always_inline__))
            uint32_t _diff(uint32_t millis) const
            {
                return millis - _millis;
            }

        private:
            uint32_t _millis;
            uint8_t _value;
            VisualizerPeakType _peakType;
            uint16_t _peakSinkRate;
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

            VideoType(uint32_t pixels) : _header(), _numPixels(pixels)
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
                return _numPixels * _header.getBytesPerPixel();
            }

            void begin()
            {
                _position = _data.begin();
            }

            CRGB getRGB();
            uint32_t _numPixels;
        };

        uint8_t _storedLoudness;
        uint8_t _peakLoudness;
        uint32_t _peakLoudnessTime;
        std::array<uint8_t, kVisualizerPacketSize> _storedData;
        std::array<PeakType, kVisualizerPacketSize> _storedPeaks;
        VideoType _video;
        VisualizerAnimationConfig &_cfg;
        float _colsInterpolation; // horizontal
        float _rowsInterpolation; // vertical
        uint8_t _showLoudness;

        static WiFiUDP _udp;
        static int _udpUsageCounter;
    };

}
