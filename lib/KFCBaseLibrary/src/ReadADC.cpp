/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include "LoopFunctions.h"
#include "ReadADC.h"

#ifndef DEBUG_READ_ADC
#define DEBUG_READ_ADC 1
#endif

#if DEBUG_READ_ADC
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#undef _min
#undef _max

#if DEBUG_READ_ADC && 0
static uint32_t count = 0;
static inline uint16 __system_adc_read() {
    count++;
    if (count%10000==0) {
        __DBG_printf("read_adc: #%u", count);
    }
    return system_adc_read();
}
#else
static inline uint16 __system_adc_read() {
    return system_adc_read();
}
#endif

ADCManager::ADCResult::ADCResult() : _valueSum(0), _samples(1), _min(0), _max(0), _invalid(true)
{
}

ADCManager::ADCResult::ADCResult(uint16_t value) : _valueSum(value), _samples(1), _min(value), _max(value), _invalid(false)
{
}

uint16_t ADCManager::ADCResult::getValue() const
{
    return _valueSum / _samples;
}

float ADCManager::ADCResult::getFloatValue() const
{
    return _valueSum / (float)_samples;
}

uint16_t ADCManager::ADCResult::getMinValue() const
{
    return _min;
}

uint16_t ADCManager::ADCResult::getMaxValue() const
{
    return _max;
}

bool ADCManager::ADCResult::isValid() const
{
    return _invalid == 0;
}

bool ADCManager::ADCResult::isInvalid() const
{
    return _invalid != 0;
}


ADCManager::ResultQueue::ResultQueue(uint8_t numSamples, uint16_t delayMillis, Callback callback) : _samples(numSamples), _delay(delayMillis), _callback(callback), _lastUpdate(0)
{
    __LDBG_printf("samples=%u delay=%u callback=%p", _samples, _delay, &_callback);
}

bool ADCManager::ResultQueue::needsUpdate(uint32_t millis) const
{
    return get_time_diff(_lastUpdate, millis) >= _delay;
}

void ADCManager::ResultQueue::setLastUpdate(uint32_t millis)
{
    _lastUpdate = millis;
}

bool ADCManager::ResultQueue::finished() const
{
    return _result._samples == _samples;
}

ADCManager::ADCResult &ADCManager::ResultQueue::getResult()
{
    return _result;
}

void ADCManager::ResultQueue::invokeCallback()
{
    __LDBG_printf("callback=%p result=%.2f min=%u max=%u samples=%u invalid=%u", &_callback, _result.getFloatValue(), _result.getMinValue(), _result.getMaxValue(), _samples, _result._invalid);
    _callback(_result);
}


static ADCManager *adc = nullptr;

ADCManager::ADCManager() : _value(__system_adc_read()), _nextDelay(kMinDelayMicros), _lastUpdated(micros64()), _lastMaxDelay(_lastUpdated), _minDelayMicros(kMinDelayMicros), _maxDelayMicros(kMaxDelayMicros), _repeatMaxDelayPerSecond(kRepeatMaxDelayPerSecond), _maxDelayYieldTimeMicros(kMaxDelayYieldTimeMicros)
{
}

ADCManager::~ADCManager()
{
    LoopFunctions::remove(loop);
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
    uint32_t ms = millis();
    for(auto &queue: _queue) {
        if (queue.needsUpdate(ms)) {
            auto value = readValue();
            auto &result = queue.getResult();
            result._min = std::min((uint16_t)result._min, value);
            result._max = std::max((uint16_t)result._max, value);
            result._valueSum += value;
            result._samples++;
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

uint32_t ADCManager::_canRead() const
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

bool ADCManager::canRead() const
{
    return _canRead() >= _nextDelay;
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

uint16_t ADCManager::readValue()
{
    auto diff = _canRead();
    if (diff < _nextDelay) {
        return _value;
    }
    _updateTimestamp();
    return (_value = __system_adc_read());
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
    return (_value = __system_adc_read());
}

uint16_t ADCManager::readValue(uint32_t &lastUpdate)
{
    auto diff = _canRead();
    if (diff < _nextDelay) {
        lastUpdate = (uint32_t)_lastUpdated;
        return _value;
    }
    _updateTimestamp();
    lastUpdate = (uint32_t)_lastUpdated;
    return (_value = __system_adc_read());
}

uint32_t ADCManager::getLastUpdate() const
{
    return (uint32_t)_lastUpdated;
}

bool ADCManager::requestAverage(uint8_t numSamples, uint16_t delayMillis, Callback callback)
{
    if (numSamples == 0 || delayMillis == 0) {
        __LDBG_printf("invalid values: samples=%u delay=%u", numSamples, delayMillis);
        return false;
    }
    if (_queue.empty()) {
        LoopFunctions::add(loop);
    }
    _queue.emplace_back(numSamples, delayMillis, callback);
    return true;
}

void ADCManager::setMinDelayMicros(uint16_t minDelayMicros)
{
    _minDelayMicros = std::min(20000, std::max(500, (int)minDelayMicros));
}

void ADCManager::setMaxDelayMicros(uint16_t maxDelayMicros)
{
    _maxDelayMicros = std::min(50000, std::max(2000, (int)maxDelayMicros));
}

void ADCManager::setRepeatMaxDelayPerSecond(uint8_t repeatMaxDelayPerSecond)
{
    _repeatMaxDelayPerSecond = std::min(20, std::max(1, (int)repeatMaxDelayPerSecond));
}

void ADCManager::setMaxDelayYieldTimeMicros(uint16_t maxDelayYieldTimeMicros)
{
    _maxDelayYieldTimeMicros = std::min(ADCManager::kMaxDelayYieldTimeMicros * 2, std::max(ADCManager::kMaxDelayYieldTimeMicros, (int)maxDelayYieldTimeMicros));
}
