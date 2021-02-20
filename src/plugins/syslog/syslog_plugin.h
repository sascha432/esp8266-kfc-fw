/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <EventScheduler.h>
#include <kfc_fw_config.h>
#include <Syslog.h>
#include "plugins.h"

#if !defined(SYSLOG_SUPPORT) || !SYSLOG_SUPPORT
#error requires SYSLOG_SUPPORT=1
#endif

#if defined(ESP32)
#define SYSLOG_PLUGIN_QUEUE_SIZE            4096
#elif defined(ESP8266)
#define SYSLOG_PLUGIN_QUEUE_SIZE            1024
#endif

class SyslogPlugin : public PluginComponent, public SyslogQueueManager {
public:
    static constexpr size_t kMaxQueueSize = SYSLOG_PLUGIN_QUEUE_SIZE;

public:
    SyslogPlugin();

    virtual void setup(SetupModeType mode) override;
    virtual void reconfigure(const String &source) override;
    virtual void shutdown() override;

    virtual void getStatus(Print &output) override;
    virtual void createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request) override;

#if ENABLE_DEEP_SLEEP
    void prepareDeepSleep(uint32_t sleepTimeMillis) override;
#endif

#if AT_MODE_SUPPORTED
    virtual ATModeCommandHelpArrayPtr atModeCommandHelp(size_t &size) const override;
    virtual bool atModeHandler(AtModeArgs &args) override;
    bool atModeHasStream(AtModeArgs &args) const;
#endif

public:
    static void timerCallback(Event::CallbackTimerPtr timer);
    virtual void queueSize(uint32_t size, bool isAvailable);
    static NameType protocolToString(SyslogProtocol proto);

    static void waitForQueue(uint32_t maxMillis);

private:
    void _zeroConfCallback(const SyslogStream *stream, const String &hostname, const IPAddress &address, uint16_t port, MDNSResolver::ResponseType type);

private:
    void _begin();
    void _end();
    // try to deliver any existing queue and terminate stream
    // does not free all memory
    void _kill(uint32_t timeout);
    void _timerCallback(Event::CallbackTimerPtr timer);
    void _waitForQueue(SyslogStream *stream, uint32_t maxMillis);

private:
    SyslogStream *_stream;
    Event::Timer _timer;
};
