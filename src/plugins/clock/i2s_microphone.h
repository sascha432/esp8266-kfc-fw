/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if ESP8266
#    error the ESP8266 does not have enough RAM for audio processing
#endif

#include <driver/i2s.h>
#include <freertos/task.h>
#include <array>
#include <memory>

template <typename T> class ArduinoFFT;

class I2SMicrophone {
public:
    // the bands use the same frequency plan as the UDP path (scripts/audio_analyzer/spectrum.py)
    // and the Windows application, so both inputs look the same
    static constexpr uint32_t kSampleRate = 48000;
    static constexpr uint16_t kFftSize = 2048;              // 42.7 ms window, bin width 23.44 Hz
    static constexpr uint16_t kMaxReadSamples = kFftSize;   // i2s_read() returns whatever is available
    static constexpr uint8_t kNumBands = 32;
    static constexpr float kFreqMax = 16800.0f;             // highest band edge
    static constexpr float kLogScale = 1.092f;              // px, 1.0 = linear band spacing
    // amplitude relative to full scale, everything below is noise (the noise level of the
    // previous implementation was 2350 with a 256 point FFT)
    static constexpr float kNoiseLevel = 0.00112f;
    static constexpr uint8_t kPeakDecay = 6;                // peak falloff per update, 0 = disabled

public:
    I2SMicrophone(i2s_port_t i2sPort, uint8_t i2sSd, uint8_t i2sWs, uint8_t i2sSck, uint8_t *data, size_t dataSize, uint8_t &loudnessLeft, uint8_t &loudnessRight, float loudnessGain, float bandGain);
    ~I2SMicrophone();

    void task();
    void readI2S();

    bool isRunning() const
    {
        return _taskHandle != nullptr;
    }

private:
    using _binsType = std::array<uint16_t, kNumBands>;
    using _fftBufferType = std::array<float, kFftSize>;
    using _windowBufferType = std::array<int16_t, kFftSize>;
    using _readBufferType = std::array<int16_t, kMaxReadSamples>;
    // the Hann window is symmetric, only half of the factors are stored
    using _windowFactorsType = std::array<float, kFftSize / 2>;

    bool _allocate();
    void _fadeOut();

private:
    std::unique_ptr<_binsType> _bins;
    std::unique_ptr<_fftBufferType> _vReal;
    std::unique_ptr<_fftBufferType> _vImag;
    std::unique_ptr<_windowBufferType> _window;
    std::unique_ptr<_readBufferType> _readBuffer;
    std::unique_ptr<_windowFactorsType> _windowFactors;
    std::unique_ptr<ArduinoFFT<float>> _fft;

    TaskHandle_t _taskHandle;
    i2s_port_t _i2sPort;
    uint8_t *_data;
    size_t _dataSize;
    uint8_t &_loudnessLeft;
    uint8_t &_loudnessRight;
    float _loudnessGain;
    float _bandGain;
    float _magnitudeScale;
};
