/**
  Author: sascha_lammers@gmx.de
*/


#pragma once

#include <Arduino_compat.h>
#include <EventScheduler.h>
#include <MicrosTimer.h>
#include <list>

// ESP8266
// Singleton to read ADC values safely without WiFi to drop the connection

class ADCManager {
private:
    ADCManager();
    ~ADCManager();

    class ResultQueue;
public:

    class ADCResult {
    public:
        static constexpr size_t kADCValueMaxBits = 12;
        static constexpr size_t kNumSampleMaxBits = 8;
#if defined(ESP8266)
        static constexpr size_t kMaxADCValue = 1024;        // 0-1024, 11bit
#else
        static constexpr size_t kMaxADCValue = 1023;        // 0-1023, 10bit
#endif
    public:
        ADCResult();
        ADCResult(uint16_t value);

        uint16_t getValue() const;
        float getFloatValue() const;
        uint16_t getMinValue() const;
        uint16_t getMaxValue() const;
        bool isValid() const;
        bool isInvalid() const;

    private:
        friend ResultQueue;
        friend ADCManager;

        uint32_t _valueSum: (kNumSampleMaxBits + kADCValueMaxBits);
        uint32_t _samples: kNumSampleMaxBits;
        uint32_t _min: kADCValueMaxBits;
        uint32_t _max: kADCValueMaxBits;
        uint32_t _invalid: 1;

        static_assert(kMaxADCValue < (1 << kADCValueMaxBits), "Values don't fit");
    };

    static constexpr size_t ADCResultSize = sizeof(ADCResult);
    static_assert(ADCResultSize == sizeof(uint32_t) * 2, "Size does not match");

    using Callback = std::function<void(const ADCResult &result)>;

    // WiFi needs certain time per second to keep connected
    // the documentation specifies that system_adc_read() can be called 1000 times per secnd with WiFi
    // enabled, but depending on the system load this does not work and adding a longer delay a couple
    // times per second solves the issue that WiFi disconnects
    static constexpr int kMinDelayMicros = 500;
    static constexpr int kMaxDelayMicros = 10000;
    static constexpr int kRepeatMaxDelayPerSecond = 5;
    static constexpr int kMaxDelayYieldTimeMicros = 5000;

private:
    class ResultQueue {
    public:
        ResultQueue(uint8_t numSamples, uint16_t delayMillis, Callback callback);
        bool needsUpdate(uint32_t millis) const;
        void setLastUpdate(uint32_t millis);
        bool finished() const;
        ADCResult &getResult();
        void invokeCallback();

    private:
        uint8_t _samples;
        uint16_t _delay;
        ADCResult _result;
        Callback _callback;
        uint32_t _lastUpdate;
    };

    using ResultQueueList = std::list<ResultQueue>;

public:
    // get instance... can be stored since the returned reference won't change
    static ADCManager &getInstance();
    // terminate queue
    // invokeCallbacks=true invokes the result callback for each queued entry sending 0 as result
    static void terminate(bool invokeCallbacks);

    // return true if the ADC can be read
    bool canRead() const;
    // get value
    uint16_t readValue();
    // get value and wait for micros if current value older than maxAge
    uint16_t readValueWait(uint16_t maxWaitMicros, uint16_t maxAgeMicros = 0, uint32_t *lastUpdate = nullptr);
    uint16_t readValueWait(uint16_t maxWaitMicros, uint16_t maxAgeMicros, uint32_t &lastUpdate) {
        return readValueWait(maxWaitMicros, maxAgeMicros, &lastUpdate);
    }
    // read value and set lastUpdate
    uint16_t readValue(uint32_t &lastUpdate);

    // request num samples every delay milliseconds and submit result to callback
    // returns false if values are not valid
    // if the delay is shorter than kMinDelayMicros / 1000, the same value might be used twice
    bool requestAverage(uint8_t numSamples, uint16_t delayMillis, Callback callback);

    // return lower 32bit part
    uint32_t getLastUpdate() const;

private:
    uint32_t _canRead() const;
    void _updateTimestamp();
    void _terminate(bool invokeCallbacks);
    static void loop();
    void _loop();

private:
    uint16_t _value;
    uint32_t _nextDelay;
    uint64_t _lastUpdated;
    uint64_t _lastMaxDelay;
    ResultQueueList _queue;
};
