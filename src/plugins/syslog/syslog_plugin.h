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

class SyslogPlugin : public PluginComponent {
public:
    SyslogPlugin();

    virtual void setup(SetupModeType mode, const DependenciesPtr &dependencies) override;
    virtual void reconfigure(const String &source) override;
    virtual void shutdown() override;

    virtual void getStatus(Print &output) override;
    virtual void createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request) override;

#if ENABLE_DEEP_SLEEP
    void prepareDeepSleep(uint32_t sleepTimeMillis) override;
#endif

#if AT_MODE_SUPPORTED
    #if AT_MODE_HELP_SUPPORTED
        virtual ATModeCommandHelpArrayPtr atModeCommandHelp(size_t &size) const override;
    #endif
    virtual bool atModeHandler(AtModeArgs &args) override;
    bool atModeHasStream(AtModeArgs &args) const;
#endif

public:
    static void timerCallback(Event::CallbackTimerPtr timer);
    static NameType protocolToString(SyslogProtocol proto);

    static void waitForQueue(uint32_t maxMillis);
    static void rearmTimer();

    static SyslogPlugin &getInstance();

private:
    void _zeroConfCallback(const Syslog *stream, const String &hostname, const IPAddress &address, uint16_t port, MDNSResolver::ResponseType type);

private:
    void _begin();
    void _end();
    // try to deliver any existing queue and terminate stream
    // does not free all memory
    void _kill(uint32_t timeout);
    void _timerCallback(Event::CallbackTimerPtr timer);
    void _waitForQueue(uint32_t maxMillis);

private:
    Syslog *_syslog;
    Event::Timer _timer;
    SemaphoreMutex _lock;
};

extern "C" SyslogPlugin syslogPlugin;

inline void SyslogPlugin::waitForQueue(uint32_t maxMillis)
{
    getInstance()._waitForQueue(maxMillis);
}

inline void SyslogPlugin::rearmTimer()
{
    _Timer(getInstance()._timer).add(Event::milliseconds(125), true, timerCallback);
}

inline void SyslogPlugin::timerCallback(Event::CallbackTimerPtr timer)
{
    getInstance()._timerCallback(timer);
}

inline SyslogPlugin &SyslogPlugin::getInstance()
{
    return syslogPlugin;
}
