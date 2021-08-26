/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include "LoopFunctions.h"
#include "ReadADC.h"

#ifndef DEBUG_READ_ADC
#    define DEBUG_READ_ADC (0 || defined(DEBUG_ALL))
#endif

// #define DEBUG_READ_ADC_PRINT_INTERVAL           5000

// display interval of how often the ADC has been read per second
#ifndef DEBUG_READ_ADC_PRINT_INTERVAL
#    if DEBUG_READ_ADC
#        define DEBUG_READ_ADC_PRINT_INTERVAL 10000
#    else
#        define DEBUG_READ_ADC_PRINT_INTERVAL 0
#    endif
#endif

#if DEBUG_READ_ADC
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

#undef _min
#undef _max

#if DEBUG_READ_ADC_PRINT_INTERVAL && ESP8266

    static uint16_t ___system_adc_read() {
        static uint32_t last = 0;
        static uint32_t count = 0;
        auto diff = get_time_diff(last, millis());
        if (diff > DEBUG_READ_ADC_PRINT_INTERVAL) {
            last = millis();
            __DBG_printf("system_adc_read() called %.3f/second (#%u)", count / (diff / 1000.0), count);
            count = 0;
        }
        count++;
        return system_adc_read();
    }

#else

    #if ESP8266

        static inline uint16_t ___system_adc_read() {
            return system_adc_read();
        }

    #elif ESP32

        #include <esp_adc_cal.h>

        #ifndef READ_ADC_DEFAULT_PIN
            #define READ_ADC_DEFAULT_PIN adc1_channel_t::ADC1_CHANNEL_0
        #endif

        static inline uint16_t ___system_adc_read() {
            return adc1_get_raw(READ_ADC_DEFAULT_PIN);
        }

    #endif


#endif

ADCManager::ResultQueue::ResultQueue(uint8_t numSamples, uint32_t intervalMicros, Callback callback, uint32_t id) :
    _id(id),
    _samples(numSamples),
    _interval(intervalMicros),
    _callback(callback)
{
    __LDBG_printf("id=%u samples=%u delay=%u callback=%p", _id, _samples, _interval, &_callback);
}

void ADCManager::ResultQueue::invokeCallback()
{
    __LDBG_printf("callback=%p mean=%.2f median=%.2f min=%u max=%u samples=%u invalid=%u",
        &_callback,
        _result.getMeanValue(),
        _result.getMedianValue(),
        _result.getMinValue(),
        _result.getMaxValue(),
        _samples,
        _result._invalid
    );
    _callback(_result);
}

ADCManager::ADCResult::ADCResult() :
    _lastUpdate(0),
    _valueSum(0),
    _samples(0),
    _min(kMaxADCValue),
    _max(0),
    _invalid(true)
{
}

ADCManager::ADCResult::ADCResult(uint16_t value, uint32_t lastUpdate) :
    _lastUpdate(lastUpdate),
    _valueSum(value),
    _samples(1),
    _min(value),
    _max(value),
    _invalid(false)
{
}

static ADCManager *adc = nullptr;

ADCManager::ADCManager() :
    _value(__readAndNormalizeADCValue()),
    _offset(0),
    _nextDelay(kMinDelayMicros),
    _lastUpdated(micros64()),
    _lastMaxDelay(_lastUpdated),
    _minDelayMicros(kMinDelayMicros),
    _maxDelayMicros(kMaxDelayMicros),
    _maxDelayYieldTimeMicros(kMaxDelayYieldTimeMicros),
    _repeatMaxDelayPerSecond(kRepeatMaxDelayPerSecond),
    _readTimer(nullptr)

{
}

ADCManager *ADCManager::hasInstance()
{
    return adc;
}

ADCManager &ADCManager::getInstance()
{
    if (!adc) {
        adc = new ADCManager();
    }
    return *adc;
}

void ADCManager::terminate(bool invokeCallbacks)
{
    if (adc) {
        adc->_terminate(invokeCallbacks);
    }
}

void ADCManager::loop()
{
    if (adc) {
        adc->_loop();
    }
}

void ADCManager::_terminate(bool invokeCallbacks)
{
    if (!_queue.empty()) {
        if (invokeCallbacks) {
            for(auto &queue: _queue) {
                queue.getResult() = ADCResult(); // create invalid result
                queue.invokeCallback();
            }
        }
        _queue.clear();
    }
    LoopFunctions::remove(loop);
}

void ADCManager::_loop()
{
    uint32_t ms = micros();
    for(auto &queue: _queue) {
        if (queue.needsUpdate(ms)) {
            auto value = readValue();
            auto &result = queue.getResult();
            queue.getResult().update(value, ms);
            if (queue.finished()) {
                result._invalid = false;
                queue.invokeCallback();
            }
        }
    }
    _queue.erase(std::remove_if(_queue.begin(), _queue.end(), [](const ResultQueue &result) {
        return result.finished();
    }), _queue.end());

    if (_queue.empty()) {
        LoopFunctions::remove(loop);
    }
}

uint16_t ADCManager::__readAndNormalizeADCValue()
{
    int32_t value = ___system_adc_read() + _offset;
    if (value < 0) {
        return 0;
    }
    else if (value >= (int32_t)kMaxADCValue)  {
        return kMaxADCValue;
    }
    return value;
}

void ADCManager::_updateTimestamp()
{
    auto tmp = micros64();
    if (_nextDelay == _maxDelayMicros) {
        _nextDelay = _minDelayMicros;
        _lastMaxDelay = tmp;
        _lastUpdated = tmp;
    }
    else {
        if (tmp - _lastUpdated > (1000000 / _repeatMaxDelayPerSecond)) {
            _lastMaxDelay = tmp;
            _nextDelay = _minDelayMicros;
        }
        else {
            if (tmp - _lastMaxDelay > (1000000 / _repeatMaxDelayPerSecond)) {
                _nextDelay = _maxDelayMicros;
                if (_maxDelayYieldTimeMicros) {
                    if (can_yield()) {
                        delayMicroseconds(_maxDelayYieldTimeMicros);
                    }
                }
            }
        }
        _lastUpdated = tmp;
    }
}

uint16_t ADCManager::readValueWait(uint16_t maxWaitMicros, uint16_t maxAgeMicros, uint32_t *lastUpdate)
{
    uint64_t diff = (micros64() - _lastUpdated);
    uint32_t tmp;
    if ((diff < _nextDelay) || (diff <= maxAgeMicros) || ((tmp = _nextDelay - diff) > maxWaitMicros) || !can_yield()) {
        if (lastUpdate) {
            *lastUpdate = (uint32_t)_lastUpdated;
        }
        return _value;
    }

    delayMicroseconds(tmp);
    _updateTimestamp();
    if (lastUpdate) {
        *lastUpdate = (uint32_t)_lastUpdated;
    }
    return (_value = __readAndNormalizeADCValue());
}

bool ADCManager::requestAverage(uint8_t numSamples, uint32_t readIntervalMicros, Callback callback, uint32_t queueId)
{
    __LDBG_printf("numSamples=%u read_interval=%u callback=%p", numSamples, readIntervalMicros, lambda_target(callback));
    if (numSamples == 0 || readIntervalMicros == 0) {
        __LDBG_printf("invalid values: samples=%u delay=%u", numSamples, readIntervalMicros);
        return false;
    }
    if (_queue.empty()) {
        LoopFunctions::add(loop);
    }
    _queue.emplace_back(numSamples, readIntervalMicros, callback, queueId);
    return true;
}

