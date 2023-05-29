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

// read the ADC directly
// ------------------------------------------------------------------------
// the minimum time between each call has to be longer than kMinDelayMicros or the
// last value will be returned. the function might block for kMaxDelayMicros (25 milliseconds
// by default). use readValueWait() with maxWaitMicros 0 if the function needs to return
// immediately
//
// uint16_t readValue();
//
// read ADC and wait up to maxWaitMicros for a new result. maxAgeMicros tell if the last
// result can be used. if lastUpdate is not null, the time of the update will be stored
// and can be used to detect if and when the value was updated
//
// uint16_t readValueWait(uint16_t maxWaitMicros, uint16_t maxAgeMicros = 0, uint32_t *lastUpdate = nullptr);
//
//
// Using callbacks
// ------------------------------------------------------------------------
// request an average value for "numSamples" sent to a callback function
// the readInterval is microseconds but varies depending if it is possible to read
// the ADC or not. it might take significantly longer to get the result than (numSamples * readInterval)
// returns false if numSamples is 0 or readIntervalMicros less than kMinDelayMicros
//
// bool requestAverage(uint8_t numSamples, uint32_t readIntervalMicros, Callback callback);
//
//
// Continuous readings
// ------------------------------------------------------------------------
// for slower continous readings with an averaging period of "sampleInterval" milliseconds, the auto read
// timer can be used. by default it requests an average of 10 samples with an interval of 50 milliseconds
// every "interval" milliseconds. interval must exceed (numSamples + 1) * sampleInterval + (kMaxDelayMicros / 500)
//
// start continuous readings
// if the timer is already installed, it will return an error. use removeAutoReadTimer() before to cancel it
// AutoReadTimerResultType addAutoReadTimer(Event::milliseconds interval, Event::milliseconds sampleIntervalMillis = 50, uint8_t count = 10);
//
// stop continuous readings
// void removeAutoReadTimer();
//
// returns the result of the last average reading
// if there is no result, ADCResult.isValid() returns false
// the result also stores the number of samples, min./max./avg values and the time of the last update
// ADCResult getAutoReadValue();


class ADCManager {
public:
        static constexpr size_t kADCValueMaxBits = 12;
        static constexpr size_t kNumSampleMaxBits = 8;
#if defined(ESP8266)
        static constexpr size_t kMaxADCValue = 1024;        // 0-1024, 11bit
#else
        static constexpr size_t kMaxADCValue = 1023;        // 0-1023, 10bit
#endif

private:
    ADCManager();
    ~ADCManager();

    class ResultQueue;
public:
    enum class AutoReadTimerResultType {
        SUCCESS = 0,
        ALREADY_RUNNING = 1,
        INTERVAL_TOO_SHORT = 2,
        NUM_SAMPLES_INVALID = 3,
        SAMPLE_INTERVAL_TOO_SHORT = 4,
    };

    class ADCResult {
    public:
    public:
        ADCResult();
        ADCResult(uint16_t value, uint32_t lastUpdate);

        void update(uint16_t value, uint32_t timestampMicros);
        uint16_t getValue() const; // returns rounded mean value as int
        float getMeanValue() const;

        float getMedianValue() const;
        uint16_t getMinValue() const;
        uint16_t getMaxValue() const;
        uint32_t getValuesSum() const;
        uint32_t getLastUpdateMicros() const;
        uint16_t numSamples() const;
        bool isValid() const;
        bool isInvalid() const;

        void dump(Print &print);

    private:
        friend ResultQueue;
        friend ADCManager;

        uint32_t _lastUpdate;
        uint32_t _valueSum: (kNumSampleMaxBits + kADCValueMaxBits);
        uint32_t _samples: kNumSampleMaxBits;
        uint32_t _min: kADCValueMaxBits;
        uint32_t _max: kADCValueMaxBits;
        uint32_t _invalid: 1;

        static_assert(kMaxADCValue < (1 << kADCValueMaxBits), "Values don't fit");
    };

    static constexpr size_t ADCResultSize = sizeof(ADCResult);
    static_assert(ADCResultSize == sizeof(uint32_t) * 3, "Size does not match");

    using Callback = std::function<void(const ADCResult &result)>;

    // WiFi needs certain time per second to keep connected
    // the documentation specifies that system_adc_read() can be called 1000 times per secnd with WiFi
    // enabled, but depending on the system load this does not work and adding a longer delay a couple
    // times per second solves the issue that WiFi disconnects
    static constexpr int kMinDelayMicros = 500;
    static constexpr int kMaxDelayMicros = 25000;
    static constexpr int kRepeatMaxDelayPerSecond = 3;
    static constexpr int kMaxDelayYieldTimeMicros = 5000;


private:
    class ResultQueue {
    public:
        ResultQueue(uint8_t numSamples, uint32_t intervalMicros, Callback callback, uint32_t id = 0);
        bool needsUpdate(uint32_t time) const {
            return (get_time_diff(_result._lastUpdate, time) >= _interval);
        }
        bool finished() const {
            return (_result._samples >= _samples);
        }
        uint32_t getId() const {
            return _id;
        }
        ADCResult &getResult() {
            return _result;
        }
        ADCResult getResult() const {
            return _result;
        }
        void invokeCallback();

        bool operator==(uint32_t id) const {
            return _id == id;
        }

    private:
        uint32_t _id;
        uint8_t _samples;
        uint32_t _interval;
        ADCResult _result;
        Callback _callback;
    };

    using ResultQueueList = std::list<ResultQueue>;

    class ReadTimer {
    public:
        ReadTimer(Event::milliseconds interval, Event::milliseconds sampleInterval, uint8_t numSamples) : _sampleInterval(sampleInterval.count()), _numSamples(numSamples) {
            _Timer(_timer).add(interval, true, [](Event::CallbackTimerPtr timer) {
                ReadTimer::readTimerCallback(timer);
            });
        }
        ~ReadTimer() {
            _Timer(_timer).remove();
        }

        void timerCallback(ADCManager &adc, Event::CallbackTimerPtr timer);
        static ADCManager *getADCInstance();
        static void readTimerCallback(Event::CallbackTimerPtr timer);
        const ADCResult &getResult() const {
            return _result;
        }

    private:
        Event::Timer _timer;
        ADCResult _result;
        uint16_t _sampleInterval;
        uint8_t _numSamples;
    };

public:
    // get instance... can be stored since the returned reference won't change
    static ADCManager *hasInstance();
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
    inline uint16_t readValueWait(uint16_t maxWaitMicros, uint16_t maxAgeMicros, uint32_t &lastUpdate) {
        return readValueWait(maxWaitMicros, maxAgeMicros, &lastUpdate);
    }
    // read value and set lastUpdate
    uint16_t readValue(uint32_t &lastUpdate);
    uint16_t readValue(uint64_t &lastUpdate);

    static constexpr uint32_t kQueueIdNone = 0;
    static constexpr uint32_t kQueueIdAutoReadValue = 0xfffffff7;

    // request average
    // details are at the top of the file
    bool requestAverage(uint8_t numSamples, uint32_t readIntervalMicros, Callback callback, uint32_t queueId = kQueueIdNone);

    void cancelAverageRequest(uint32_t queueId);

    // return lower 32bit part
    uint32_t getLastUpdate() const;

    // set offset that is added to the raw value of the adc
    // the value is clamped to 0 - kMaxADCValue
    void setOffset(int16_t offset);

    void setMinDelayMicros(uint16_t minDelayMicros);
    void setMaxDelayMicros(uint16_t maxDelayMicros);
    void setRepeatMaxDelayPerSecond(uint8_t repeatMaxDelayPerSecond);
    void setMaxDelayYieldTimeMicros(uint16_t maxDelayYieldTimeMicros);

    // install auto read timer
    // details are at the top of the file
    AutoReadTimerResultType addAutoReadTimer(Event::milliseconds interval, Event::milliseconds sampleInterval = Event::milliseconds(50), uint8_t count = 10);
    void removeAutoReadTimer();

    // allowIncomplete
    // false: if the required number of samples has not been reached and no previous result is available, the result will be invalid
    // true: returns at least one sample reading before the first read cycle is complete
    ADCResult getAutoReadValue(bool allowIncomplete = true);

private:
    uint16_t __readAndNormalizeADCValue();
    uint32_t _canRead() const;
    void _updateTimestamp();
    void _terminate(bool invokeCallbacks);
    static void loop();
    void _loop();

private:
    uint16_t _value;
    int16_t _offset;
    uint32_t _nextDelay;
    uint64_t _lastUpdated;
    uint64_t _lastMaxDelay;
    ResultQueueList _queue;

    uint16_t _minDelayMicros;
    uint16_t _maxDelayMicros;
    uint16_t _maxDelayYieldTimeMicros;
    uint8_t _repeatMaxDelayPerSecond;

    ReadTimer *_readTimer;
};


// ------------------------------------------------------------------------

inline void ADCManager::setOffset(int16_t offset)
{
    _offset = offset;
}

inline void ADCManager::setMinDelayMicros(uint16_t minDelayMicros)
{
    _minDelayMicros = std::min(20000, std::max(500, (int)minDelayMicros));
}

inline void ADCManager::setMaxDelayMicros(uint16_t maxDelayMicros)
{
    _maxDelayMicros = std::min(50000, std::max(2000, (int)maxDelayMicros));
}

inline void ADCManager::setRepeatMaxDelayPerSecond(uint8_t repeatMaxDelayPerSecond)
{
    _repeatMaxDelayPerSecond = std::min(20, std::max(1, (int)repeatMaxDelayPerSecond));
}

inline void ADCManager::setMaxDelayYieldTimeMicros(uint16_t maxDelayYieldTimeMicros)
{
    _maxDelayYieldTimeMicros = std::min(ADCManager::kMaxDelayYieldTimeMicros * 2, std::max(ADCManager::kMaxDelayYieldTimeMicros, (int)maxDelayYieldTimeMicros));
}

inline ADCManager::AutoReadTimerResultType ADCManager::addAutoReadTimer(Event::milliseconds interval, Event::milliseconds sampleInterval, uint8_t numSamples)
{
    if (numSamples == 0) {
        return AutoReadTimerResultType::NUM_SAMPLES_INVALID;
    }
    if (sampleInterval.count() == 0) {
        return AutoReadTimerResultType::SAMPLE_INTERVAL_TOO_SHORT;
    }
    if (interval.count() < (numSamples + 1) * sampleInterval.count() + (kMaxDelayMicros / 500)) {
        return AutoReadTimerResultType::INTERVAL_TOO_SHORT;
    }
    if (_readTimer) {
        return AutoReadTimerResultType::ALREADY_RUNNING;
    }
    _readTimer = new ReadTimer(interval, sampleInterval, numSamples);
    return AutoReadTimerResultType::SUCCESS;
}

inline void ADCManager::removeAutoReadTimer() {
    if (_readTimer) {
        delete _readTimer;
        _readTimer = nullptr;
    }
}

inline ADCManager::ADCResult ADCManager::getAutoReadValue(bool allowIncomplete) {
    if (_readTimer) {
        auto result = _readTimer->getResult(); // stores the last complete result
        if (result.isValid()) {
            return result;
        }
    }
    if (allowIncomplete) {
        auto iterator = std::find(_queue.begin(), _queue.end(), kQueueIdAutoReadValue); // search queue for result
        if (iterator != _queue.end()) {
            auto result = iterator->getResult();
            if (result.isValid()) {
                return result;
            }
            else {
                // no valid result in queue, read single value
                uint32_t lastUpdate = 0;
                auto value = readValueWait(50, ~0, lastUpdate);
                return ADCResult(value, lastUpdate);
            }
        }
    }
    return ADCResult();
}

// ------------------------------------------------------------------------

inline void ADCManager::ADCResult::update(uint16_t value, uint32_t timestampMicros)
{
    _min = std::min<uint16_t>(_min, value);
    _max = std::max<uint16_t>(_max, value);
    _valueSum += value;
    _samples++;
    _lastUpdate = timestampMicros;
}

inline void ADCManager::ADCResult::dump(Print &print)
{
    print.printf_P(PSTR("ADC result\nmin: %u\nmax: %u\nsamples: %u\nsum: %u\navg: %f\nmedian: %f\n"), _min, _max, _samples, _valueSum, getMeanValue(), getMedianValue());
}

inline uint16_t ADCManager::ADCResult::getValue() const
{
    return (_valueSum + (_samples - 1)) / _samples;
}

inline float ADCManager::ADCResult::getMeanValue() const
{
    return _valueSum / (float)_samples;
}

inline float ADCManager::ADCResult::getMedianValue() const
{
    return (_min + _max) / 2;
}

inline uint16_t ADCManager::ADCResult::getMinValue() const
{
    return _min;
}

inline uint16_t ADCManager::ADCResult::getMaxValue() const
{
    return _max;
}

inline uint32_t ADCManager::ADCResult::getValuesSum() const
{
    return _valueSum;
}

inline uint32_t ADCManager::ADCResult::getLastUpdateMicros() const
{
    return _lastUpdate;
}

inline uint16_t ADCManager::ADCResult::numSamples() const
{
    return _samples;
}

inline bool ADCManager::ADCResult::isValid() const
{
    return !_invalid;
}

inline bool ADCManager::ADCResult::isInvalid() const
{
    return _invalid;
}

// ------------------------------------------------------------------------

inline void ADCManager::ReadTimer::timerCallback(ADCManager &adc, Event::CallbackTimerPtr timer)
{
    adc.requestAverage(_numSamples, _sampleInterval * 1000, [this](const ADCResult &result) {
        // check if the adc manager and readTimer still exist
        auto adc = getADCInstance();
        if (adc && adc->_readTimer == this) {
            _result = result;
        }
    }, kQueueIdAutoReadValue);
}

inline ADCManager *ADCManager::ReadTimer::getADCInstance()
{
    auto adc = ADCManager::hasInstance();
    if (!adc || !adc->_readTimer) {
        return nullptr;
    }
    return adc;
}

inline void ADCManager::ReadTimer::readTimerCallback(Event::CallbackTimerPtr timer)
{
    auto adc = getADCInstance();
    if (!adc) {
        __LDBG_printf("adc manager or read_timer=null");
        timer->disarm();
        return;
    }
    adc->_readTimer->timerCallback(*adc, timer);
}

inline ADCManager::~ADCManager()
{
    LoopFunctions::remove(loop);
    if (_readTimer) {
        delete _readTimer;
    }
}

inline uint32_t ADCManager::_canRead() const
{
    // if (!can_yield()) {
    //     return 0;
    // }
    uint64_t diff = micros64() - _lastUpdated;
    if (diff > std::numeric_limits<uint32_t>::max()) {
        return std::numeric_limits<uint32_t>::max();
    }
    return diff;
}

inline bool ADCManager::canRead() const
{
    return _canRead() >= _nextDelay;
}

inline uint16_t ADCManager::readValue(uint32_t &lastUpdate)
{
    auto diff = _canRead();
    if (diff < _nextDelay) {
        lastUpdate = (uint32_t)_lastUpdated;
        return _value;
    }
    _updateTimestamp();
    lastUpdate = (uint32_t)_lastUpdated;
    return (_value = __readAndNormalizeADCValue());
}

inline uint16_t ADCManager::readValue(uint64_t &lastUpdate)
{
    auto diff = _canRead();
    if (diff < _nextDelay) {
        lastUpdate = _lastUpdated;
        return _value;
    }
    _updateTimestamp();
    lastUpdate = _lastUpdated;
    return (_value = __readAndNormalizeADCValue());
}

inline uint16_t ADCManager::readValue()
{
    auto diff = _canRead();
    if (diff < _nextDelay) {
        return _value;
    }
    _updateTimestamp();
    return (_value = __readAndNormalizeADCValue());
}

inline uint32_t ADCManager::getLastUpdate() const
{
    return (uint32_t)_lastUpdated;
}

inline void ADCManager::cancelAverageRequest(uint32_t queueId)
{
    _queue.erase(std::remove(_queue.begin(), _queue.end(), queueId), _queue.end());
}
