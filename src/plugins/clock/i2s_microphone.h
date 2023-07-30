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

class I2SMicrophone {
public:
    static constexpr uint32_t kSampleRate = 48000;
    static constexpr uint32_t kMaxSamples = 256;
    static constexpr uint8_t kNumBands = 32;
    static constexpr double kNoiseLevel = 2350;

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
    bool _i2s_read_double(float &loudness);

private:
    using _sampleBufferType = std::array<double, kMaxSamples>;
    using _fftBufferType = std::array<double, kMaxSamples>;
    using _outputBufferType = std::array<double, kNumBands>;

    std::unique_ptr<_sampleBufferType> _vReal;
    std::unique_ptr<_fftBufferType> _vImag;
    std::unique_ptr<_outputBufferType> _output;

    TaskHandle_t _taskHandle;
    i2s_port_t _i2sPort;
    uint8_t *_data;
    size_t _dataSize;
    uint8_t &_loudnessLeft;
    uint8_t &_loudnessRight;
    float _loudnessGain;
    float _bandGain;
};
