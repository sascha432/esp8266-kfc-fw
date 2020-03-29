/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <Buffer.h>
#include <KFCSyslog.h>
#include <PrintHtmlEntitiesString.h>
#include <LoopFunctions.h>
#include "kfc_fw_config.h"
#include "progmem_data.h"
#include "../include/templates.h"
#include "plugins.h"

#if defined(ESP32)
#define SYSLOG_PLUGIN_QUEUE_SIZE        4096
#elif defined(ESP8266)
#define SYSLOG_PLUGIN_QUEUE_SIZE        512
#endif

void syslog_process_queue();

#if DEBUG_USE_SYSLOG

SyslogStream *debugSyslog = nullptr;

void syslog_setup_debug_logger() {

    SyslogParameter parameter;
    parameter.setHostname(config.getDeviceName());
    parameter.setAppName(FSPGM(kfcfw));
    parameter.setProcessId(F("DEBUG"));
	parameter.setSeverity(SYSLOG_DEBUG);

    SyslogFilter *filter = new SyslogFilter(parameter);
    filter->addFilter(F("*.*"), F(DEBUG_USE_SYSLOG_TARGET));

	SyslogQueue *queue = new SyslogMemoryQueue(SYSLOG_PLUGIN_QUEUE_SIZE * 4);
    debugSyslog = new SyslogStream(filter, queue);

    DEBUG_OUTPUT = *debugSyslog;

    debug_printf_P(PSTR("Debug Syslog enabled, target " DEBUG_USE_SYSLOG_TARGET "\n"));

    LoopFunctions::add(syslog_process_queue);
}

#endif

SyslogStream *syslog = nullptr;

static void syslog_end()
{
    if (syslog) {
        _logger.setSyslog(nullptr);
#if DEBUG_USE_SYSLOG
        if (syslog->getQueue() == debugSyslog->getQueue()) { // remove shared object
            syslog->setQueue(nullptr);
        }
#endif
        delete syslog;
        syslog = nullptr;
#if DEBUG_USE_SYSLOG == 0
        LoopFunctions::remove(syslog_process_queue);
#endif
    }
}

static void syslog_deliver(uint16_t timeout) {
    if (syslog) {
        auto endTime = millis() + timeout;
        while(syslog->hasQueuedMessages() && millis() < endTime) {
            syslog->deliverQueue();
            delay(1);
        }
    }
}

void syslog_setup_logger()
{
    syslog_end();

    if (config._H_GET(Config().flags).syslogProtocol != SYSLOG_PROTOCOL_NONE) {

        // SyslogParameter parameter;
        // parameter.setHostname(config.getDeviceName());
        // parameter.setAppName(FSPGM(kfcfw));
        // parameter.setFacility(SYSLOG_FACILITY_KERN);
        // parameter.setSeverity(SYSLOG_NOTICE);

        SyslogFilter *filter = new SyslogFilter(config.getDeviceName(), FSPGM(kfcfw));
        auto &parameter = filter->getParameter();
        parameter.setFacility(SYSLOG_FACILITY_KERN);
        parameter.setSeverity(SYSLOG_NOTICE);

        filter->addFilter(F("*.*"), SyslogFactory::create(parameter, (SyslogProtocol)config._H_GET(Config().flags).syslogProtocol, config._H_STR(Config().syslog_host), config._H_GET(Config().syslog_port)));

#if DEBUG_USE_SYSLOG
        SyslogQueue *queue = debugSyslog ? debugSyslog->getQueue() : new SyslogMemoryQueue(SYSLOG_PLUGIN_QUEUE_SIZE);
#else
        SyslogQueue *queue = new SyslogMemoryQueue(SYSLOG_PLUGIN_QUEUE_SIZE);
#endif
        syslog = new SyslogStream(filter, queue);

        _logger.setSyslog(syslog);
        LoopFunctions::add(syslog_process_queue);
    }
}

void syslog_setup()
{
#if DEBUG_USE_SYSLOG
    syslog_setup_debug_logger();
#endif
    syslog_setup_logger();
}

void syslog_process_queue()
{
    static MillisTimer timer(1000);
    if (timer.reached()) {
#if DEBUG_USE_SYSLOG
        if (debugSyslog) {
            if (debugSyslog->hasQueuedMessages()) {
                debugSyslog->deliverQueue();
            }
        }
#endif
        if (syslog) {
            if (syslog->hasQueuedMessages()) {
                syslog->deliverQueue();
            }
        }
        timer.restart();
    }
}

class SyslogPlugin : public PluginComponent {
public:
    SyslogPlugin() {
        REGISTER_PLUGIN(this);
    }
    virtual PGM_P getName() const {
        return PSTR("syslog");
    }
    virtual const __FlashStringHelper *getFriendlyName() const {
        return F("Syslog Client");
    }
    PluginPriorityEnum_t getSetupPriority() const override;

    bool autoSetupAfterDeepSleep() const override;
    void setup(PluginSetupMode_t mode) override;
    void reconfigure(PGM_P source) override;
    void restart() override;

    virtual bool hasStatus() const override;
    virtual void getStatus(Print &output) override;

    virtual PGM_P getConfigureForm() const override {
        return getName();
    }
    void createConfigureForm(AsyncWebServerRequest *request, Form &form) override;

    void prepareDeepSleep(uint32_t sleepTimeMillis) override;

#if AT_MODE_SUPPORTED
    bool hasAtMode() const override;
    void atModeHelpGenerator() override;
    bool atModeHandler(AtModeArgs &args) override;
#endif
};

static SyslogPlugin plugin;

SyslogPlugin::PluginPriorityEnum_t SyslogPlugin::getSetupPriority() const
{
    return (PluginPriorityEnum_t)PRIO_SYSLOG;
}

bool SyslogPlugin::autoSetupAfterDeepSleep() const
{
    return true;
}

void SyslogPlugin::setup(PluginSetupMode_t mode)
{
    syslog_setup();
}

void SyslogPlugin::reconfigure(PGM_P source)
{
    syslog_setup();
}

void SyslogPlugin::restart()
{
    syslog_deliver(250);
    syslog_end();
}

bool SyslogPlugin::hasStatus() const
{
    return true;
}

void SyslogPlugin::getStatus(Print &output)
{
#if SYSLOG
    switch(config._H_GET(Config().flags).syslogProtocol) {
        case SYSLOG_PROTOCOL_UDP:
            output.printf_P(PSTR("UDP @ %s:%u"), config._H_STR(Config().syslog_host), config._H_GET(Config().syslog_port));
            break;
        case SYSLOG_PROTOCOL_TCP:
            output.printf_P(PSTR("TCP @ %s:%u"), config._H_STR(Config().syslog_host), config._H_GET(Config().syslog_port));
            break;
        case SYSLOG_PROTOCOL_TCP_TLS:
            output.printf_P(PSTR("TCP TLS @ %s:%u"), config._H_STR(Config().syslog_host), config._H_GET(Config().syslog_port));
            break;
        default:
            output.print(FSPGM(Disabled));
            break;
    }
#if DEBUG_USE_SYSLOG
    output.print(F(HTML_S(br) "Debugging enabled, target " DEBUG_USE_SYSLOG_TARGET));
#endif
#else
    output.print(FSPGM(Not_supported));
#endif
}

void SyslogPlugin::createConfigureForm(AsyncWebServerRequest *request, Form &form)
{
    form.add<uint8_t>(F("syslog_enabled"), _H_FLAGS_VALUE(Config().flags, syslogProtocol));
    form.addValidator(new FormRangeValidator(SYSLOG_PROTOCOL_NONE, SYSLOG_PROTOCOL_FILE));

    form.add<sizeof Config().syslog_host>(F("syslog_host"), config._H_W_STR(Config().syslog_host));
    form.addValidator(new FormValidHostOrIpValidator(true));

    form.add<uint16_t>(F("syslog_port"), &config._H_W_GET(Config().syslog_port));
    form.addValidator(new FormTCallbackValidator<uint16_t>([](uint16_t value, FormField &field) {
        if (value == 0) {
            value = 514;
            field.setValue(String(value));
        }
        return true;
    }));
    form.addValidator(new FormRangeValidator(F("Invalid port"), 1, 65535));

    form.finalize();
}

void SyslogPlugin::prepareDeepSleep(uint32_t sleepTimeMillis)
{
    uint16_t defaultWaitTime = 250;
    if (sleepTimeMillis > 60e3) {  // longer than 1min, increase wait time
        defaultWaitTime *= 4;
    }

#if DEBUG_USE_SYSLOG
    if (debugSyslog) {
        ulong timeout = millis() + defaultWaitTime * 8;    // long timeout in debug mode
        while(debugSyslog->hasQueuedMessages() && millis() < timeout) {
            debugSyslog->deliverQueue();
            delay(1);
        }
    }
#endif
    syslog_deliver(defaultWaitTime);
}

#if AT_MODE_SUPPORTED

#include "at_mode.h"

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(SQC, "SQC", "Clear syslog queue");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(SQI, "SQI", "Display syslog queue info");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(SQD, "SQD", "Display syslog queue");

static bool isEnabled(AtModeArgs &args)
{
    if (syslog) {
        return true;
    }
    args.print(F("Syslog is disabled"));
    return false;
}

bool SyslogPlugin::hasAtMode() const
{
    return true;
}

void SyslogPlugin::atModeHelpGenerator()
{
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(SQC), getName());
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(SQI), getName());
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(SQD), getName());
}

bool SyslogPlugin::atModeHandler(AtModeArgs &args)
{
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(SQC))) {
        if (isEnabled(args)) {
            syslog->getQueue()->clear();
            args.print(F("Queue cleared"));
        }
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(SQI))) {
        if (isEnabled(args)) {
            args.printf_P(PSTR("%d"), syslog->getQueue()->size());
        }
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(SQD))) {
        if (isEnabled(args)) {
            auto queue = syslog->getQueue();
            size_t index = 0;
            args.printf_P(PSTR("Messages in queue %d"), queue->size());
            while(index < queue->size()) {
                auto &item = queue->at(index++);
                if (item) {
                    args.printf_P(PSTR("Id %d (failures %d): %s"), item->getId(), item->getFailureCount(), item->getMessage().c_str());
                }
            }
        }
        return true;
    }
    return false;
}

#endif
