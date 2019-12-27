/**
 * Author: sascha_lammers@gmx.de
 */

#if SYSLOG

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
    parameter.setHostname(config._H_STR(Config().device_name));
    parameter.setAppName(FSPGM(kfcfw));
    parameter.setProcessId(F("DEBUG"));
	parameter.setSeverity(SYSLOG_DEBUG);

    SyslogFilter *filter = _debug_new SyslogFilter(parameter);
    filter->addFilter(F("*.*"), F(DEBUG_USE_SYSLOG_TARGET));

	SyslogQueue *queue = _debug_new SyslogMemoryQueue(SYSLOG_PLUGIN_QUEUE_SIZE * 4);
    debugSyslog = _debug_new SyslogStream(filter, queue);

    DEBUG_OUTPUT = *debugSyslog;

    debug_printf_P(PSTR("Debug Syslog enabled, target " DEBUG_USE_SYSLOG_TARGET "\n"));

    LoopFunctions::add(syslog_process_queue);
}

#endif

SyslogStream *syslog = nullptr;

void syslog_setup_logger() {

    if (syslog) {
        _logger.setSyslog(nullptr);
        delete syslog;
        syslog = nullptr;
#if DEBUG_USE_SYSLOG == 0
        LoopFunctions::remove(syslog_process_queue);
#endif
    }

    if (config._H_GET(Config().flags).syslogProtocol != SYSLOG_PROTOCOL_NONE) {

        SyslogParameter parameter;
        parameter.setHostname(config.getString(_H(Config().device_name)));
        parameter.setAppName(FSPGM(kfcfw));
        parameter.setFacility(SYSLOG_FACILITY_KERN);
        parameter.setSeverity(SYSLOG_NOTICE);

        SyslogFilter *filter = _debug_new SyslogFilter(parameter);
        filter->addFilter(F("*.*"), SyslogFactory::create(filter->getParameter(), (SyslogProtocol)config._H_GET(Config().flags).syslogProtocol, config._H_STR(Config().syslog_host), config._H_GET(Config().syslog_port)));

#if DEBUG_USE_SYSLOG
        SyslogQueue *queue = debugSyslog ? debugSyslog->getQueue() : _debug_new SyslogMemoryQueue(SYSLOG_PLUGIN_QUEUE_SIZE);
#else
        SyslogQueue *queue = _debug_new SyslogMemoryQueue(SYSLOG_PLUGIN_QUEUE_SIZE);
#endif
        syslog = _debug_new SyslogStream(filter, queue);

        _logger.setSyslog(syslog);
        LoopFunctions::add(syslog_process_queue);
    }
}

void syslog_setup() {
#if DEBUG_USE_SYSLOG
    syslog_setup_debug_logger();
#endif
    syslog_setup_logger();
}

void syslog_process_queue() {
    static MillisTimer timer(1000UL);
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
        REGISTER_PLUGIN(this, "SyslogPlugin");
    }
    PGM_P getName() const;
    PluginPriorityEnum_t getSetupPriority() const override;

    bool autoSetupAfterDeepSleep() const override;
    void setup(PluginSetupMode_t mode) override;
    void reconfigure(PGM_P source) override;

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
    bool atModeHandler(Stream &serial, const String &command, int8_t argc, char **argv) override;
#endif
};

static SyslogPlugin plugin;

PGM_P SyslogPlugin::getName() const
{
    return PSTR("syslog");
}

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
    form.add<uint8_t>(F("syslog_enabled"), _H_STRUCT_FORMVALUE(Config().flags, uint8_t, syslogProtocol));
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
    if (syslog) {
        ulong timeout = millis() + defaultWaitTime;
        while(syslog->hasQueuedMessages() && millis() < timeout) {
            syslog->deliverQueue();
            delay(1);
        }
    }
}

#if AT_MODE_SUPPORTED

#include "at_mode.h"

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(SQC, "SQC", "Clear syslog queue");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(SQI, "SQI", "Display syslog queue info");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(SQD, "SQD", "Display syslog queue");

void print_syslog_disabled(Stream &output)
{
    output.println(F("+SQx: Syslog is disabled"));
}

bool SyslogPlugin::hasAtMode() const
{
    return true;
}

void SyslogPlugin::atModeHelpGenerator()
{
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(SQC));
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(SQI));
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(SQD));
}

bool SyslogPlugin::atModeHandler(Stream &serial, const String &command, int8_t argc, char **argv)
{
    if (constexpr_String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(SQC))) {
        if (syslog) {
            syslog->getQueue()->clear();
            serial.println(F("+SQC: Queue cleared"));
        }
        else {
            print_syslog_disabled(serial);
        }
        return true;
    } else if (constexpr_String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(SQI))) {
        if (syslog) {
            serial.printf_P(PSTR("+SQI: %d\n"), syslog->getQueue()->size());
        }
        else {
            print_syslog_disabled(serial);
        }
        return true;
    } else if (constexpr_String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(SQD))) {
        if (syslog) {
            auto queue = syslog->getQueue();
            size_t index = 0;
            serial.printf_P(PSTR("+SQD: messages in queue %d\n"), queue->size());
            while(index < queue->size()) {
                auto &item = queue->at(index++);
                if (item) {
                    serial.printf_P(PSTR("+SQD: id %d (failures %d): %s\n"), item->getId(), item->getFailureCount(), item->getMessage().c_str());
                }
            }
        }
        else {
            print_syslog_disabled(serial);
        }
        return true;
    }
    return false;
}

#endif

#endif
