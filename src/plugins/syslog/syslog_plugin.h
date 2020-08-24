/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <EventScheduler.h>
#include <kfc_fw_config.h>
#include "Syslog.h"
#include "plugins.h"

#if !defined(SYSLOG_SUPPORT) || !SYSLOG_SUPPORT
#error requires SYSLOG_SUPPORT=1
#endif

#if defined(ESP32)
#define SYSLOG_PLUGIN_QUEUE_SIZE            4096
#elif defined(ESP8266)
#define SYSLOG_PLUGIN_QUEUE_SIZE            1024
#endif

class SyslogPlugin : public PluginComponent {
public:
    static constexpr size_t kMaxQueueSize = SYSLOG_PLUGIN_QUEUE_SIZE;

public:
    SyslogPlugin();

    virtual void setup(SetupModeType mode) override;
    virtual void reconfigure(const String &source) override;
    virtual void shutdown() override;

    virtual void getStatus(Print &output) override;
    virtual void createConfigureForm(FormCallbackType type, const String &formName, Form &form, AsyncWebServerRequest *request) override;

#if ENABLE_DEEP_SLEEP
    void prepareDeepSleep(uint32_t sleepTimeMillis) override;
#endif

#if AT_MODE_SUPPORTED
    void atModeHelpGenerator() override;
    bool atModeHandler(AtModeArgs &args) override;
#endif

public:
    static void timerCallback(Event::CallbackTimerPtr timer);

private:
    void _zeroConfCallback(const String &hostname, const IPAddress &address, uint16_t port, MDNSResolver::ResponseType type);

private:
    void _begin();
    void _end();
    void _kill(uint32_t timeout);
    void _timerCallback(Event::CallbackTimerPtr timer);

private:
    SyslogStream *_stream;
    Event::Timer _timer;
};
