/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

// NeoPixelBus output backend for the clock/LED matrix plugin (HAVE_NEOPIXELBUS).
//
// The color math and the animations use pixel_color.h, this backend only transmits the pixel
// buffer to the LEDs. Both ESP32 methods are supported and can be selected at runtime:
//   - RMT (default), up to 4 output pins on one channel with all 8 memory blocks (serial)
//   - I2S DMA, one output pin per I2S port (the ESP32 has two), transmitted in parallel
// Both can transmit in the same frame (mixed mode): the port of the visualizer microphone is
// reserved for it (HAVE_NEOPIXELBUS_SUPPORT_MIC) and the segments that no I2S port is left for are
// transmitted by the RMT transport.
//
// This header requires the pixel type (pixel_color.h) and is included by pixel_display.h
// after the color/clock headers.

#include <Arduino_compat.h>
#include "matrix_validation.h"

#if HAVE_NEOPIXELBUS

#if !ESP32
#    error HAVE_NEOPIXELBUS is only supported on ESP32
#endif

#include <NeoPixelBus.h>

// The I2S LED transport and the I2S microphone of the visualizer would have to share one of the two
// I2S ports: NeoPixelBus installs its own I2S driver with the DMA interrupt of the port
// (Esp32_i2s.c), the microphone the ESP-IDF driver on IOT_LED_MATRIX_I2S_PORT. The IDF driver then
// fails to register the interrupt, its cleanup resets the peripheral and the LED driver is left
// without a DMA interrupt - the transport waits forever for it (NeoEsp32I2sMethodBase::Update()
// spins in a while loop, i2sInit() cannot report a failure) and the task watchdog resets the
// device. HAVE_NEOPIXELBUS_SUPPORT_MIC=1 (clock_def.h) reserves the port of the microphone for it,
// the LED transport uses the remaining I2S ports and transmits the other segments with RMT.
#if IOT_LED_MATRIX_ENABLE_VISUALIZER_I2S_MICROPHONE && !HAVE_NEOPIXELBUS_SUPPORT_MIC
#    error "the I2S microphone of the visualizer and the NeoPixelBus I2S transport share the I2S ports - set HAVE_NEOPIXELBUS_SUPPORT_MIC=1 (the LED transport then uses the free port only) or disable IOT_LED_MATRIX_ENABLE_VISUALIZER_I2S_MICROPHONE"
#endif
#if HAVE_NEOPIXELBUS_SUPPORT_MIC
#    if !defined(IOT_LED_MATRIX_I2S_PORT)
#        error "clock_def.h has to be included before pixel_output_neobus.h"
#    endif
#    include <driver/i2s.h> // I2S_NUM_0/I2S_NUM_1, the value of IOT_LED_MATRIX_I2S_PORT
#endif

// The RMT refill interrupt of NeoPixelBus is not IRAM safe - it is installed with
// ESP_INTR_FLAG_LOWMED (NeoEsp32RmtMethod.h) - therefore a flash operation must not disable the
// flash cache while a frame is transmitted, otherwise the RMT buffer runs empty and the WS2812
// stream gets a gap (visible flicker). Holding the flash operation lock keeps the interrupt
// running, the same as FASTLED_ESP32_FLASH_LOCK does for the FastLED transports.
extern "C" {
    extern void spi_flash_op_lock(void);
    extern void spi_flash_op_unlock(void);
}

#if DEBUG_IOT_CLOCK
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

namespace Clock {

    // mirrors Clock::ShowMethodType::NEOBUS_RMT / NEOBUS_I2S
    enum class NeoBusMethodType : uint8_t {
        RMT = 0,
        I2S,
    };

    class NeoBusStrip {
    public:
        virtual ~NeoBusStrip() {}

        virtual void show(const PixelRGB *pixels, uint8_t brightness) = 0;

        // false if the output could not be initialized, the segment is then transmitted by RMT
        virtual bool isActive() const = 0;
    };

    // a single output pin with its own channel (I2S DMA)
    template<typename _Method>
    class NeoBusStripType : public NeoBusStrip {
    public:
        using BusType = NeoPixelBus<NeoGrbFeature, _Method>;

    public:
        NeoBusStripType(uint8_t pin, uint16_t numPixels, uint8_t channel, uint16_t offset) :
            _bus(new BusType(numPixels, pin, static_cast<NeoBusChannel>(channel))),
            _offset(offset)
        {
            if (!_bus->Begin()) {
                __DBG_printf("NeoPixelBus initialize failed pin=%u pixels=%u channel=%u", pin, numPixels, channel);
                delete _bus;
                _bus = nullptr;
            }
        }

        bool isActive() const override
        {
            return _bus != nullptr;
        }

        ~NeoBusStripType()
        {
            delete _bus;
        }

        void show(const PixelRGB *pixels, uint8_t brightness) override
        {
            if (!_bus) {
                return;
            }
            pixels += _offset;
            const uint16_t count = _bus->PixelCount();
            const uint16_t scale = brightness + 1; // 255 -> 256 -> no scaling
            for (uint16_t i = 0; i < count; i++) {
                const PixelRGB &color = pixels[i];
                _bus->SetPixelColor(i, RgbColor(
                    static_cast<uint8_t>((color.r * scale) >> 8),
                    static_cast<uint8_t>((color.g * scale) >> 8),
                    static_cast<uint8_t>((color.b * scale) >> 8)
                ));
            }
            _bus->Show();
        }

    private:
        BusType *_bus;
        uint16_t _offset;
    };

    // all output pins on one RMT channel using all RMT memory blocks of the ESP32
    //
    // The RMT channels share 8 memory blocks (64 items each, ~80us of WS2812 data per block) and the
    // driver refills the memory of a channel from its threshold interrupt, which is set to half of
    // that memory. With the blocks split over two channels (4 blocks each) an interrupt that is late
    // by more than ~160us leaves the data line idle in the middle of a frame: the strip latches, the
    // rest of the frame is shifted and the pattern flickers (the BLE/WiFi issue of NeoPixelBus).
    // A single strip with all 8 blocks (~320us) was measured to be stable, therefore every output
    // shares ONE channel and the RMT output signal is routed to the pin of the segment that is
    // transmitted (rmt_set_gpio). The segments are transmitted one after another, the time of a frame
    // is the sum of all segments (128 pixels are ~3.84ms).
    class NeoBusRmtMux {
    public:
        static constexpr uint8_t kMaxPins = Clock::MatrixValidation::kMaxStrips; // all output pins on one channel
        static constexpr uint8_t kRmtMemBlocks = 8;         // all RMT memory blocks
        static constexpr rmt_channel_t kChannel = RMT_CHANNEL_0;
        static constexpr uint32_t kTxTimeoutMs = 50;        // a stuck channel must not block the loop
        static constexpr uint32_t kUsPerPixel = 30;         // 24 bits * 1.25us (WS2812x)
        static constexpr uint32_t kOverrunToleranceUs = 50; // call/scheduling overhead, ~1.5 bit times

    public:
        using BusType = NeoPixelBus<NeoGrbFeature, NeoEsp32RmtNWs2812xMethod>;

    public:
        NeoBusRmtMux(const uint8_t *pins, const uint16_t *offsets, const uint16_t *counts) :
            _bus(nullptr),
            _numPins(0),
            _maxPixels(0),
            _lastPin(0xff),
            _timeouts(0),
            _maxOverrun(0),
            _maxWriteMicros(0),
            _transmissions(0),
            _transmissionsOver(0)
        {
            for (uint8_t i = 0; i < kMaxPins; i++) {
                _pins[i] = 0xff;
                _offsets[i] = 0;
                _counts[i] = 0;
                if (pins[i] == 0xff || !counts[i]) {
                    continue;
                }
                _pins[_numPins] = pins[i];
                _offsets[_numPins] = offsets[i];
                _counts[_numPins] = counts[i];
                if (counts[i] > _maxPixels) {
                    _maxPixels = counts[i];
                }
                _numPins++;
            }
            if (!_numPins) {
                return;
            }
            // the buffer holds the largest segment, the output pin is routed per segment
            gNeoPixelBusRmtMemBlocks = kRmtMemBlocks;
            _bus = new BusType(_maxPixels, _pins[0], static_cast<NeoBusChannel>(kChannel));
            if (!_bus->Begin()) {
                __DBG_printf("NeoPixelBus initialize failed channel=%u pixels=%u", static_cast<uint8_t>(kChannel), _maxPixels);
                delete _bus;
                _bus = nullptr;
                return;
            }
            // Begin() configured the channel, it connected the pin of the first segment
            _lastPin = _pins[0];
        }

        ~NeoBusRmtMux()
        {
            // drive the outputs low before the driver releases the channel
            for (uint8_t i = 0; i < _numPins; i++) {
                _disconnect(_pins[i]);
            }
            delete _bus;
        }

        // transmit the frame of every segment, the output pin is switched in between
        void show(const PixelRGB *pixels, uint8_t brightness)
        {
            if (!_bus) {
                return;
            }
            const uint16_t total = _bus->PixelCount();
            const uint16_t scale = brightness + 1; // 255 -> 256 -> no scaling
            const uint8_t *buffer = _bus->Pixels();
            if (!buffer) {
                return;
            }
            for (uint8_t i = 0; i < _numPins; i++) {
                // the memory is shared, the frame of the previous pin must be transmitted before the
                // output signal can be routed to the pin of this segment
                if (!_waitForTxDone()) {
                    return;
                }
                _route(_pins[i]);

                const uint16_t count = (_counts[i] < total) ? _counts[i] : total;
                for (uint16_t index = 0; index < count; index++) {
                    const PixelRGB &color = pixels[_offsets[i] + index];
                    _bus->SetPixelColor(index, RgbColor(
                        static_cast<uint8_t>((color.r * scale) >> 8),
                        static_cast<uint8_t>((color.g * scale) >> 8),
                        static_cast<uint8_t>((color.b * scale) >> 8)
                    ));
                }

                // only the pixels of this segment are transmitted (the bus buffer is sized for the
                // largest segment) - rmt_write_sample() translates the data into the RMT memory
                const uint32_t start = micros();
                if (rmt_write_sample(kChannel, buffer, count * NeoGrbFeature::PixelSize, false) != ESP_OK) {
                    __DBG_printf("channel %u: writing %u pixels failed", static_cast<uint8_t>(kChannel), count);
                    continue;
                }
                // the driver translates and copies the data into the RMT memory before the
                // transmission starts, that time is not part of the frame
                const uint32_t writeMicros = micros() - start;
                if (!_waitForTxDone()) {
                    return;
                }
                _registerDuration(count, writeMicros, micros() - start - writeMicros);
            }
        }

        uint8_t getNumPins() const
        {
            return _numPins;
        }

        // longest measured frame that took longer than the pixels need (24 bits * 1.25us), a stuck
        // refill interrupt makes the RMT transmit data of the memory twice, see NeoBusRmtMux
        uint32_t getMaxOverrunMicros() const
        {
            return _maxOverrun;
        }

        // frames that never finished, the refill interrupt stopped (see kTxTimeoutMs)
        uint32_t getTxTimeouts() const
        {
            return _timeouts;
        }

        // number of transmitted segments and how many of them took longer than the pixels need
        uint32_t getTransmissions() const
        {
            return _transmissions;
        }

        uint32_t getTransmissionsOver() const
        {
            return _transmissionsOver;
        }

        // longest time the driver needed to translate and copy a frame into the RMT memory
        uint32_t getMaxWriteMicros() const
        {
            return _maxWriteMicros;
        }

        // time the pixels of all segments need on the wire (30us per pixel), the shortest frame possible
        uint32_t getWireMicros() const
        {
            uint32_t total = 0;
            for (uint8_t i = 0; i < _numPins; i++) {
                total += static_cast<uint32_t>(_counts[i]) * kUsPerPixel;
            }
            return total;
        }

    private:
        // duration of the transmission without the time the driver needs to translate and copy the
        // data into the RMT memory. Anything above the wire time (30us per pixel) means the refill
        // interrupt was late and the RMT transmitted data of the memory a second time
        void _registerDuration(uint16_t pixels, uint32_t writeMicros, uint32_t transmitMicros)
        {
            _transmissions++;
            if (writeMicros > _maxWriteMicros) {
                _maxWriteMicros = writeMicros;
            }
            const uint32_t expected = static_cast<uint32_t>(pixels) * kUsPerPixel;
            if (transmitMicros > expected + kOverrunToleranceUs) {
                _transmissionsOver++;
                const uint32_t overrun = transmitMicros - expected;
                if (overrun > _maxOverrun) {
                    _maxOverrun = overrun;
                }
            }
        }

        bool _waitForTxDone()
        {
            if (rmt_wait_tx_done(kChannel, kTxTimeoutMs / portTICK_PERIOD_MS) == ESP_OK) {
                return true;
            }
            _timeouts++;
            return false;
        }

        void _route(uint8_t pin)
        {
            if (pin == _lastPin) {
                return;
            }
            if (_lastPin != 0xff) {
                _disconnect(_lastPin);
            }
            if (rmt_set_gpio(kChannel, RMT_MODE_TX, static_cast<gpio_num_t>(pin), false) != ESP_OK) {
                __DBG_printf("RMT output pin %u could not be connected", pin);
                return;
            }
            _lastPin = pin;
        }

        // drive the pin low, the strip of that segment sees the idle level and latches its frame
        void _disconnect(uint8_t pin)
        {
            gpio_matrix_out(pin, SIG_GPIO_OUT_IDX, false, false);
            pinMode(pin, OUTPUT);
            digitalWrite(pin, LOW);
        }

    private:
        BusType *_bus;
        uint8_t _pins[kMaxPins];
        uint16_t _offsets[kMaxPins];
        uint16_t _counts[kMaxPins];
        uint8_t _numPins;
        uint16_t _maxPixels;
        uint8_t _lastPin;
        uint32_t _timeouts;
        uint32_t _maxOverrun;
        uint32_t _maxWriteMicros;
        uint32_t _transmissions;
        uint32_t _transmissionsOver;
    };

    // creates the transport for the configured segments
    //
    // The RMT transport transmits all of its segments on one channel with all memory blocks (serial,
    // see NeoBusRmtMux), the I2S transport uses one I2S port per segment (parallel, DMA). Both run in
    // the same frame: a segment that no I2S port is left for is transmitted by the RMT transport.
    class NeoBusStrips {
    public:
        static constexpr uint8_t kMaxStrips = Clock::MatrixValidation::kMaxStrips;    // at most 4 segments
        static constexpr uint8_t kMaxI2sStrips = Clock::MatrixValidation::kMaxI2sStrips; // ESP32 has I2S0 and I2S1
        static constexpr uint8_t kRmtMemBlocks = 8; // all RMT memory blocks, see NeoBusRmtMux
        // I2S port of the visualizer microphone (IOT_LED_MATRIX_I2S_PORT), 0xff if it does not use
        // one. The port is reserved for the microphone and never used by the LED transport, see the
        // file comment
        static constexpr uint8_t kMicI2sPort = Clock::MatrixValidation::kMicI2sPort;

    public:
        NeoBusStrips() :
            _rmt(nullptr),
            _numStrips(0),
            _method(NeoBusMethodType::RMT),
            _frameCount(0),
            _fpsTimer(0),
            _fps(0)
        {
            for (uint8_t i = 0; i < kMaxStrips; i++) {
                _strips[i] = nullptr;
            }
        }

        ~NeoBusStrips()
        {
            clear();
        }

        // rebuild the transport, called when the segments or the show method changed
        void update(NeoBusMethodType method, const uint8_t *pins, const uint16_t *offsets, const uint16_t *counts)
        {
            clear();
            _method = method;

            // the segments that no I2S port is available for are transmitted by the RMT transport
            uint8_t rmtPins[kMaxStrips];
            uint16_t rmtOffsets[kMaxStrips];
            uint16_t rmtCounts[kMaxStrips];
            uint8_t numRmt = 0;
            for (uint8_t i = 0; i < kMaxStrips; i++) {
                rmtPins[i] = 0xff;
                rmtOffsets[i] = 0;
                rmtCounts[i] = 0;
            }

            // I2S DMA uses one port per output pin, the ports that are left after reserving the port
            // of the visualizer microphone (kMicI2sPort)
            uint8_t stripIndex = 0;
            for (uint8_t i = 0; i < kMaxStrips; i++) {
                if (pins[i] == 0xff || !counts[i]) {
                    continue;
                }
                const uint8_t channel = (method == NeoBusMethodType::I2S) ? _getI2sChannel(stripIndex) : 0xff;
                auto strip = (channel == 0xff) ? nullptr : new NeoBusStripType<NeoEsp32I2sNWs2812xMethod>(pins[i], counts[i], channel, offsets[i]);
                if (strip && strip->isActive()) {
                    _strips[i] = strip;
                    _numStrips++;
                    stripIndex++;
                    continue;
                }
                delete strip;
                if (method == NeoBusMethodType::I2S) {
                    __DBG_printf("%s for pin %u, using the RMT transport", channel == 0xff ? "no free I2S port" : "I2S initialization failed", pins[i]);
                }
                rmtPins[numRmt] = pins[i];
                rmtOffsets[numRmt] = offsets[i];
                rmtCounts[numRmt] = counts[i];
                numRmt++;
            }

            if (numRmt) {
                // all RMT outputs share one channel with all memory blocks, see NeoBusRmtMux
                gNeoPixelBusRmtMemBlocks = kRmtMemBlocks;
                _rmt = new NeoBusRmtMux(rmtPins, rmtOffsets, rmtCounts);
                _numStrips += _rmt->getNumPins();
            }
        }

        void show(const PixelRGB *pixels, uint8_t brightness)
        {
            if (!_numStrips) {
                return;
            }
            spi_flash_op_lock();
            // mixed mode: the I2S segments are started first, the DMA transmits them in the
            // background while the RMT transport sends its segments one after another
            for (uint8_t i = 0; i < kMaxStrips; i++) {
                if (_strips[i]) {
                    _strips[i]->show(pixels, brightness);
                }
            }
            if (_rmt) {
                _rmt->show(pixels, brightness);
            }
            spi_flash_op_unlock();
            _countFrame();
        }

        void clear()
        {
            for (uint8_t i = 0; i < kMaxStrips; i++) {
                if (_strips[i]) {
                    delete _strips[i];
                    _strips[i] = nullptr;
                }
            }
            if (_rmt) {
                delete _rmt;
                _rmt = nullptr;
            }
            _numStrips = 0;
        }

        uint8_t getNumStrips() const
        {
            return _numStrips;
        }

        // segments transmitted by the I2S DMA and by the RMT transport (mixed mode)
        uint8_t getI2sStrips() const
        {
            return _numStrips - getRmtStrips();
        }

        uint8_t getRmtStrips() const
        {
            return _rmt ? _rmt->getNumPins() : 0;
        }

        bool empty() const
        {
            return _numStrips == 0;
        }

        NeoBusMethodType getMethod() const
        {
            return _method;
        }

        // frames per second, measured over one second
        float getFps() const
        {
            return _fps;
        }

        // longest frame of the RMT transport that took longer than the pixels need, in micro seconds
        uint32_t getMaxOverrunMicros() const
        {
            return _rmt ? _rmt->getMaxOverrunMicros() : 0;
        }

        // frames of the RMT transport that never finished, see NeoBusRmtMux::kTxTimeoutMs
        uint32_t getTxTimeouts() const
        {
            return _rmt ? _rmt->getTxTimeouts() : 0;
        }

        // transmitted segments of the RMT transport and how many of them took longer than expected
        uint32_t getTransmissions() const
        {
            return _rmt ? _rmt->getTransmissions() : 0;
        }

        uint32_t getTransmissionsOver() const
        {
            return _rmt ? _rmt->getTransmissionsOver() : 0;
        }

        uint32_t getMaxWriteMicros() const
        {
            return _rmt ? _rmt->getMaxWriteMicros() : 0;
        }

        uint32_t getWireMicros() const
        {
            return _rmt ? _rmt->getWireMicros() : 0;
        }

    private:
        // I2S port for the n-th I2S segment, 0xff if no port is left (the microphone's port is never
        // used, see kMicI2sPort)
        uint8_t _getI2sChannel(uint8_t index) const
        {
            uint8_t seen = 0;
            for (uint8_t channel = 0; channel < kMaxI2sStrips; channel++) {
                if (channel == kMicI2sPort) {
                    continue;
                }
                if (seen++ == index) {
                    return channel;
                }
            }
            return 0xff;
        }

        void _countFrame()
        {
            const uint32_t now = millis();
            if (_fpsTimer == 0) {
                // first frame, start measuring
                _fpsTimer = now ? now : 1;
                _frameCount = 0;
                return;
            }
            _frameCount++;
            const uint32_t elapsed = now - _fpsTimer;
            if (elapsed >= 1000) {
                _fps = _frameCount * 1000.0f / elapsed;
                _frameCount = 0;
                _fpsTimer = now;
            }
        }

    private:
        NeoBusRmtMux *_rmt;
        NeoBusStrip *_strips[kMaxStrips];
        uint8_t _numStrips;
        NeoBusMethodType _method;
        uint32_t _frameCount;
        uint32_t _fpsTimer;
        float _fps;
    };

}

#if DEBUG_IOT_CLOCK
#    include <debug_helper_disable.h>
#endif

#endif
