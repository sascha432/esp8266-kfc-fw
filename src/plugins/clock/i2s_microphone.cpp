/**
 * Author: sascha_lammers@gmx.de
 */

#if ESP32

// this is for the INMP441 MEMS microphone, other I2S microphones might work too

#include <Arduino_compat.h>
#include <arduinoFFT.h>
#include "i2s_microphone.h"
#include "clock.h"

#if defined(FASTLED_ESP32_I2S) && (IOT_LED_MATRIX_I2S_PORT == I2S_NUM_0)
#    error "FASTLED_ESP32_I2S (LED data over I2S) and the visualizer I2S microphone both use I2S port 0 - set IOT_LED_MATRIX_I2S_PORT to I2S_NUM_1 or disable IOT_LED_MATRIX_ENABLE_VISUALIZER_I2S_MICROPHONE"
#endif

#if 0
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif


// the DMA ring buffer has to hold more than one update period (48000 / 1000 * 20 = 960 samples)
static_assert(I2SMicrophone::kSampleRate / 1000 * Clock::kUpdateRate < I2SMicrophone::kMaxReadSamples, "read buffer too small");
static_assert((I2SMicrophone::kFftSize & (I2SMicrophone::kFftSize - 1)) == 0, "FFT size must be a power of two");

static constexpr float kTwoPi = 6.28318530718f;
static constexpr float kBandScale = 3.0f * 255.0f;          // sqrt(peak) * 3 * 255 - 4
static constexpr float kBandOffset = 4.0f;
static constexpr float kLoudnessScale = 128.0f * 10.0f;     // mic_loudness_gain 275 = 0.215 * peak

void IRAM_ATTR I2SMicrophoneHandler(void *clsPtr)
{
    reinterpret_cast<I2SMicrophone *>(clsPtr)->task();
}

bool I2SMicrophone::_allocate()
{
    _bins.reset(new _binsType());
    _vReal.reset(new _fftBufferType());
    _vImag.reset(new _fftBufferType());
    _window.reset(new _windowBufferType());
    _readBuffer.reset(new _readBufferType());
    _windowFactors.reset(new _windowFactorsType());
    return _bins && _vReal && _vImag && _window && _readBuffer && _windowFactors;
}

I2SMicrophone::I2SMicrophone(i2s_port_t i2sPort, uint8_t i2sSd, uint8_t i2sWs, uint8_t i2sSck, uint8_t *data, size_t dataSize, uint8_t &loudnessLeft, uint8_t &loudnessRight, float loudnessGain, float bandGain) :
    _taskHandle(nullptr),
    _i2sPort(i2sPort),
    _data(data),
    _dataSize(dataSize),
    _loudnessLeft(loudnessLeft),
    _loudnessRight(loudnessRight),
    _loudnessGain(loudnessGain),
    _bandGain(bandGain),
    _magnitudeScale(0)
{
    i2s_driver_uninstall(_i2sPort);

    if (!_allocate()) {
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
        // 4 x 512 frames = 42.7 ms, more than one update period including jitter
        .dma_buf_count = 4,
        .dma_buf_len = 512,
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
    _window->fill(0);

    // band edges of the UDP/desktop path: freq(k) = px**k * fIncr * (k + 1) / px**bands
    {
        const float fIncr = kFreqMax / kNumBands;
        const float pxMul = 1.0f / powf(kLogScale, kNumBands);
        const float binScale = (kFftSize / 2.0f) / (kSampleRate / 2.0f);
        float maxFrequency = fIncr;
        float mul = maxFrequency * pxMul;
        float pxPow = 1.0f;
        for (uint8_t i = 0; i < kNumBands; i++) {
            const float frequency = pxPow * mul;
            pxPow *= kLogScale;
            maxFrequency += fIncr;
            mul = maxFrequency * pxMul;
            (*_bins)[i] = std::min<uint16_t>(static_cast<uint16_t>(frequency * binScale), kFftSize / 2);
        }
    }

    // symmetric Hann window, the same as np.hanning() of the UDP path
    {
        float windowSum = 0;
        for (uint16_t i = 0; i < kFftSize / 2; i++) {
            const float factor = 0.5f - 0.5f * cosf(kTwoPi * i / (kFftSize - 1.0f));
            (*_windowFactors)[i] = factor;
            windowSum += factor;
        }
        // the window is symmetric, every factor is used twice
        windowSum *= 2.0f;
        // a full scale sine is 1.0, same scale as the spectrum sent over UDP
        _magnitudeScale = 2.0f / (windowSum * 32768.0f);
    }

    _fft.reset(new ArduinoFFT<float>(_vReal->data(), _vImag->data(), kFftSize, static_cast<float>(kSampleRate)));
    if (!_fft) {
        __LDBG_printf("out of memory");
        return;
    }

    // start audio processor task
    xTaskCreatePinnedToCore(I2SMicrophoneHandler, "I2SMicrophone", 1024 * 4, this, tskIDLE_PRIORITY + 2, &_taskHandle, 0);
    __LDBG_printf("microphone task=%p fft=%u bands=%u", _taskHandle, kFftSize, kNumBands);
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

void I2SMicrophone::_fadeOut()
{
    // no data from the microphone, fade out the display
    for (size_t i = 0; i < _dataSize; i++) {
        _data[i] = _data[i] > kPeakDecay ? _data[i] - kPeakDecay : 0;
    }
    _loudnessLeft = _loudnessRight = 0;
}

void I2SMicrophone::task()
{
    for(;;) {
        const uint32_t start = millis();

        readI2S();

        // i2s_read() already waits for the update period, only delay the remaining time
        const uint32_t elapsed = millis() - start;
        if (elapsed < Clock::kUpdateRate) {
            delay(Clock::kUpdateRate - elapsed);
        }
    }
}

void I2SMicrophone::readI2S()
{
    auto &vReal = *_vReal;
    auto &vImag = *_vImag;
    auto &window = *_window;
    auto &readBuffer = *_readBuffer;

    // read everything that is available, the request is larger than the amount of data
    // that arrives during one update period, so the call returns what has accumulated
    size_t bytes = 0;
    const auto err = i2s_read(_i2sPort, readBuffer.data(), readBuffer.size() * sizeof(int16_t), &bytes, pdMS_TO_TICKS(Clock::kUpdateRate));
    const auto count = std::min<size_t>(bytes / sizeof(int16_t), readBuffer.size());
    if (count == 0) {
        __LDBG_printf("i2s_read err=%x bytes=%u", err, bytes);
        _fadeOut();
        return;
    }

    // keep the newest kFftSize samples
    if (count >= kFftSize) {
        memcpy(window.data(), readBuffer.data() + (count - kFftSize), kFftSize * sizeof(int16_t));
    }
    else {
        memmove(window.data(), window.data() + count, (kFftSize - count) * sizeof(int16_t));
        memcpy(window.data() + (kFftSize - count), readBuffer.data(), count * sizeof(int16_t));
    }

    // loudness is the peak of the new samples, same scale as BASS_WASAPI_GetLevel() >> 7
    int32_t peak = 0;
    for (size_t i = 0; i < count; i++) {
        const auto value = static_cast<int32_t>(readBuffer[i]);
        peak = std::max(peak, value < 0 ? -value : value);
    }

    // remove the DC offset of the microphone and apply the window
    float mean = 0;
    for (uint16_t i = 0; i < kFftSize; i++) {
        mean += window[i];
    }
    mean /= kFftSize;
    const auto &factors = *_windowFactors;
    for (uint16_t i = 0; i < kFftSize; i++) {
        const float factor = factors[i < kFftSize / 2 ? i : kFftSize - 1 - i];
        vReal[i] = (window[i] - mean) * factor;
        vImag[i] = 0;
    }

    _fft->compute(FFTDirection::Forward);
    _fft->complexToMagnitude();

    // peak of every band, the bin of the upper edge belongs to the next band
    uint16_t b0 = 0;
    for (size_t i = 0; i < _dataSize; i++) {
        const uint16_t b1 = (*_bins)[i];
        float magnitude;
        if (b1 > b0) {
            magnitude = 0;
            const uint16_t end = std::min<uint16_t>(b1, kFftSize / 2);
            for (uint16_t j = b0; j < end; j++) {
                magnitude = std::max(magnitude, vReal[j]);
            }
        }
        else {
            magnitude = vReal[std::min<uint16_t>(b0, kFftSize / 2 - 1)];
        }
        b0 = b1;

        // amplitude relative to full scale without the noise floor
        const float value = magnitude * _magnitudeScale - kNoiseLevel;
        int32_t output = value > 0 ? static_cast<int32_t>(sqrtf(value * _bandGain) * kBandScale - kBandOffset) : 0;
        output = std::clamp<int32_t>(output, 0, 255);
        // slower falloff to reduce flickering
        if (kPeakDecay && output < _data[i]) {
            output = std::max<int32_t>(output, _data[i] - kPeakDecay);
        }
        _data[i] = static_cast<uint8_t>(output);
    }

    // we have mono only
    _loudnessLeft = _loudnessRight = static_cast<uint8_t>(std::min(255.0f, peak * _loudnessGain / kLoudnessScale));
}

#endif
