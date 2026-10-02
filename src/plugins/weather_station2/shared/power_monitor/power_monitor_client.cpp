/**
 * Author: sascha_lammers@gmx.de
 */

#include "power_monitor_client.h"

#include <WiFi.h>
#include <WiFiClient.h>

#ifndef DEBUG_WEATHER_STATION2
#    define DEBUG_WEATHER_STATION2 0
#endif

#if DEBUG_WEATHER_STATION2
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

namespace WeatherStation2 {

namespace PowerMonitor {

Client::Client()
{
}

Client::~Client()
{
    stop();
}

void Client::begin()
{
    if (_task) {
        return;
    }
    _stop = false;
    TaskHandle_t handle = nullptr;
    if (xTaskCreate(_taskEntry, "power-mon", kTaskStack, this, 1, &handle) == pdPASS) {
        _task = handle;
        __LDBG_printf("reader task started (stack=%u)", static_cast<unsigned>(kTaskStack));
    }
    else {
        __LDBG_printf("cannot start the reader task");
    }
}

void Client::setTarget(const String &host, uint16_t port)
{
    MUTEX_LOCK_BLOCK(_lock) {
        _host = host;
        _port = port ? port : kDefaultPort;
    }
    __LDBG_printf("target %s:%u", host.c_str(), static_cast<unsigned>(port));
}

void Client::stop()
{
    _stop = true;
    // the reader waits in 250ms steps or in a connect with kConnectTimeout, so give it enough
    // time to leave the loop
    for (uint8_t i = 0; i < 120 && _task; i++) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    MUTEX_LOCK_BLOCK(_lock) {
        _connected = false;
    }
}

bool Client::isConfigured() const
{
    MUTEX_LOCK_BLOCK(_lock) {
        return _host.length() != 0;
    }
    return false;
}

bool Client::isConnected() const
{
    MUTEX_LOCK_BLOCK(_lock) {
        return _connected;
    }
    return false;
}

uint32_t Client::getSampleCount() const
{
    MUTEX_LOCK_BLOCK(_lock) {
        return _sampleCount;
    }
    return 0;
}

uint32_t Client::getLastSampleAge() const
{
    MUTEX_LOCK_BLOCK(_lock) {
        return _lastSampleMillis ? static_cast<uint32_t>(millis() - _lastSampleMillis) : 0;
    }
    return 0;
}

String Client::getError() const
{
    MUTEX_LOCK_BLOCK(_lock) {
        return _error;
    }
    return String();
}

bool Client::getSample(uint32_t channelId, Sample &sample) const
{
    MUTEX_LOCK_BLOCK(_lock) {
        for (uint8_t i = 0; i < _entryCount; i++) {
            if (_entries[i].channelId == channelId && _entries[i].sample.valid) {
                sample = _entries[i].sample;
                return true;
            }
        }
        return false;
    }
    return false;
}

void Client::_setConnection(bool connected, const char *error)
{
    MUTEX_LOCK_BLOCK(_lock) {
        _connected = connected;
        if (connected) {
            // a new connection starts with a fresh sample counter, the values of the previous one
            // stay visible while the source is offline
            _sampleCount = 0;
            _lastSampleMillis = 0;
            _error = String();
        }
        else if (error) {
            _error = error;
        }
    }
}

bool Client::_getTarget(String &host, uint16_t &port) const
{
    MUTEX_LOCK_BLOCK(_lock) {
        host = _host;
        port = _port;
        return host.length() != 0;
    }
    return false;
}

void Client::_store(const Frame &frame)
{
    const auto now = millis();
    MUTEX_LOCK_BLOCK(_lock) {
        Entry *entry = nullptr;
        for (uint8_t i = 0; i < _entryCount; i++) {
            if (_entries[i].channelId == frame.channelId) {
                entry = &_entries[i];
                break;
            }
        }
        if (!entry) {
            if (_entryCount >= kMaxSamples) {
                // the server sends more channels than we keep, ignore the rest
                return;
            }
            entry = &_entries[_entryCount++];
            entry->channelId = frame.channelId;
        }
        auto &sample = entry->sample;
        sample.valid = true;
        sample.voltage = frame.voltageMillivolt / 1000.0f;
        sample.current = frame.currentMilliamps / 1000.0f;
        sample.power = frame.powerMilliwatt / 1000.0f;
        // only the total counter is used, "energy since this run" of the server is ignored
        sample.energy = frame.energyMilliwatthoursTotal / 1000000.0;
        sample.timestamp = frame.timestamp;
        _sampleCount++;
        _lastSampleMillis = now;
    }
}

void Client::_taskEntry(void *arg)
{
    static_cast<Client *>(arg)->_readLoop();
    vTaskDelete(nullptr);
}

void Client::_readLoop()
{
    uint8_t buffer[128];
    uint8_t frameBuffer[sizeof(Frame)];
    size_t frameLength = 0;

    for (;;) {
        if (_stop) {
            break;
        }
        String host;
        uint16_t port = kDefaultPort;
        if (!_getTarget(host, port)) {
            // not configured, wait for the form
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        if (!WiFi.isConnected()) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        WiFiClient client;
        client.setTimeout(kConnectTimeout);
        if (!client.connect(host.c_str(), port, kConnectTimeout)) {
            __LDBG_printf("cannot connect to %s:%u", host.c_str(), static_cast<unsigned>(port));
            _setConnection(false, "connect failed");
        }
        else {
            __LDBG_printf("connected to %s:%u", host.c_str(), static_cast<unsigned>(port));
            _setConnection(true, nullptr);
            frameLength = 0;

            while (!_stop && client.connected()) {
                const auto available = client.available();
                if (available <= 0) {
                    vTaskDelay(pdMS_TO_TICKS(10));
                    continue;
                }
                const auto size = (static_cast<size_t>(available) > sizeof(buffer)) ? sizeof(buffer) : static_cast<size_t>(available);
                const auto length = client.read(buffer, size);
                if (length <= 0) {
                    break;
                }
                for (int i = 0; i < length; i++) {
                    frameBuffer[frameLength++] = buffer[i];
                    if (frameLength == sizeof(Frame)) {
                        frameLength = 0;
                        Frame frame;
                        memcpy(&frame, frameBuffer, sizeof(Frame));
                        // skip the reserved control frames (the one-shot daily energy block)
                        if ((frame.channelId & kControlChannelId) == 0) {
                            _store(frame);
                        }
                    }
                }
            }
            client.stop();
            MUTEX_LOCK_BLOCK(_lock) {
                __LDBG_printf("connection to %s:%u closed after %u sample(s)", _host.c_str(), static_cast<unsigned>(_port), static_cast<unsigned>(_sampleCount));
            }
            _setConnection(false, nullptr);
        }

        // wait before the next attempt, in steps so stop() does not have to wait for the full delay
        for (uint32_t i = 0; i < kReconnectDelay && !_stop; i += 250) {
            vTaskDelay(pdMS_TO_TICKS(250));
        }
    }
    _task = nullptr;
    __LDBG_printf("reader task stopped");
}

} // namespace PowerMonitor

} // namespace WeatherStation2
