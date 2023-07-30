/**
 * Author: sascha_lammers@gmx.de
 */

#if ESP32

// this is for the INMP441 MEMS microphone, other I2S microphones might work too

#include <Arduino_compat.h>
#include <arduinoFFT.h>
#include "i2s_microphone.h"
#include "clock.h"

#if 0
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif


static constexpr uint16_t BW(const uint8_t lo, const uint8_t hi)
{
    return lo | (hi << 8);
}

#include "i2s_microphone_bands.h"

inline static uint8_t getBandStart(uint8_t band)
{
    return bands[band] & 0xff;
}

inline static uint8_t getBandEnd(uint8_t band)
{
    return bands[band] >> 8;
}

inline static float getBandFactor1(uint8_t band)
{
    return factors1[band];
}

inline static float getBandFactor2(uint8_t band)
{
    return factors2[band];
}

void IRAM_ATTR I2SMicrophoneHandler(void *clsPtr)
{
    reinterpret_cast<I2SMicrophone *>(clsPtr)->task();
}

I2SMicrophone::I2SMicrophone(i2s_port_t i2sPort, uint8_t i2sSd, uint8_t i2sWs, uint8_t i2sSck, uint8_t *data, size_t dataSize, uint8_t &loudnessLeft, uint8_t &loudnessRight, float loudnessGain, float bandGain) :
    _taskHandle(nullptr),
    _i2sPort(i2sPort),
    _data(data),
    _dataSize(dataSize),
    _loudnessLeft(loudnessLeft),
    _loudnessRight(loudnessRight),
    _loudnessGain(loudnessGain),
    _bandGain(bandGain)
{
    i2s_driver_uninstall(_i2sPort);

    _vReal.reset(new _sampleBufferType());
    if (!_vReal) {
        __LDBG_printf("out of memory");
        return;
    }

    _vImag.reset(new _fftBufferType());
    if (!_vImag) {
        __LDBG_printf("out of memory");
        return;
    }

    _output.reset(new _outputBufferType());
    if (!_output) {
        __LDBG_printf("out of memory");
        return;
    }

    const i2s_config_t i2s_config =
    {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = kSampleRate,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 2,
        .dma_buf_len = kMaxSamples,
        .use_apll = false
    };

    // install driver
    esp_err_t err = i2s_driver_install(_i2sPort, &i2s_config, 0, NULL);
    if (err != ESP_OK) {
        __LDBG_printf("i2s_driver_install err=%x", err);
        return;
    }

    const i2s_pin_config_t rx_pin_config = {
        .bck_io_num = i2sSck,
        .ws_io_num = i2sWs,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = i2sSd
    };

    // set pins
    err = i2s_set_pin(_i2sPort, &rx_pin_config);
    if (err != ESP_OK) {
        __LDBG_printf("i2s_set_pin err=%x", err);
        return;
    }

    // start i2s
    err = i2s_start(_i2sPort);
    if (err != ESP_OK) {
        __LDBG_printf("i2s_start err=%x", err);
        return;
    }

    std::fill_n(_data, _dataSize, 0); // clear all bands
    _dataSize = std::min<size_t>(_dataSize, kNumBands); // only process available bands

    // start audio processor task
    xTaskCreatePinnedToCore(I2SMicrophoneHandler, "I2SMicrophone", 1024 * 2, this, tskIDLE_PRIORITY + 2, &_taskHandle, 0);
    __LDBG_printf("microphone task=%p", _taskHandle);
}

I2SMicrophone::~I2SMicrophone()
{
    __LDBG_printf("microphone removed task=%p", _taskHandle);
    if (_taskHandle) {
        // remove task
        vTaskDelete(_taskHandle);
        _taskHandle = nullptr;

        // stop i2s
        i2s_stop(_i2sPort);
    }

    // remove driver...
    i2s_driver_uninstall(_i2sPort);
}

bool I2SMicrophone::_i2s_read_double(float &loudness)
{
    esp_err_t err;
    size_t readBytes = 0;
    int16_t sampleBuffer[kMaxSamples];
    if ((err = i2s_read(_i2sPort, sampleBuffer, sizeof(sampleBuffer), &readBytes, 100 / portTICK_RATE_MS)) == ESP_OK) {
        if (readBytes == sizeof(sampleBuffer)) {

            // clear data
            _vReal->fill(0.0);

            // copy 16bit sample buffer to double array and calculate loudness
            auto src = sampleBuffer;
            auto dst = _vReal->data();
            auto count = kMaxSamples;
            while(count--) {
                const auto val = *src++;
                loudness += powf(val, 2.0);
                *dst++ = val;
            }

            // get mean value and adjust level to fit into 8bit
            const auto kLoudnessGain = kMaxSamples * _loudnessGain;
            loudness = std::min(loudness / kLoudnessGain, 255.0f);

            return true;
        }
        else {
            __LDBG_printf("not enough data %d!=%d", readBytes, sizeof(sampleBuffer));
        }
    }
    else {
        __LDBG_printf("i2s_read failed err=%x", err);
    }
   return false;
}

void I2SMicrophone::task()
{
    for(;;) {
        auto start = millis();

        readI2S();

        constexpr unsigned long kReadRateMillis = Clock::kUpdateRate;
        const uint32_t dly = std::clamp(kReadRateMillis - (millis() - start), 1UL, kReadRateMillis);
        delay(dly);
    }
}

void I2SMicrophone::readI2S()
{
    auto &vReal = *_vReal.get();
    auto &vImag = *_vImag.get();
    auto &output = *_output.get();
    float loudness;

    // read i2s data and convert it to double
    if (_i2s_read_double(loudness)) {

        // clear data
        vImag.fill(0.0);
        output.fill(0.0);

        // run FFT
        arduinoFFT _fft(vReal.data(), vImag.data(), kMaxSamples, kSampleRate);
        _fft.DCRemoval();
        // _fft.Windowing(FFTWindow::Blackman_Harris, FFTDirection::Forward); // this is slower than Hamming (3ms)
        _fft.Windowing(FFTWindow::Hamming, FFTDirection::Forward);
        _fft.Compute(FFTDirection::Forward);
        _fft.ComplexToMagnitude();

        // get peak values per band
        for(uint8_t i = 0; i < _dataSize; i++) {
            float peak = 0;
            auto start = getBandStart(i);
            auto end = getBandEnd(i);
            auto f1 = getBandFactor1(i);
            auto f2 = getBandFactor2(i);
            for(uint8_t j = start; j <= end; j++) {
                float val = vReal[j];
                if (val < kNoiseLevel) { // filter noise
                    if (j == start && f1 != 0.0) {
                        val *= f1; // use partial part of first sample
                    }
                    if (j == end) {
                        val *= f2; // use partial part of last sample
                    }
                    peak = std::max(peak, val);
                }
            }
            // store peak value
            output[i] = peak;
        }

        // copy processed output
        const auto peak = 255.0f * _bandGain / 32768.0f;
        for(uint8_t i = 0; i < _dataSize; i++) {
            _data[i] = output[i] * peak;
        }

        // we have mono only
        _loudnessLeft = loudness;
        _loudnessRight = _loudnessLeft;

    }
}

#endif
