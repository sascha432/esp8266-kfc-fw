/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <Buffer.h>
#include <KFCSyslog.h>
#include <PrintHtmlEntitiesString.h>
#include <LoopFunctions.h>
#include "kfc_fw_config.h"
#include "../include/templates.h"
#include "plugins.h"

#if defined(ESP32)
#define SYSLOG_PLUGIN_QUEUE_SIZE        4096
#elif defined(ESP8266)
#define SYSLOG_PLUGIN_QUEUE_SIZE        512
#endif

SyslogStream *syslog = nullptr;
static EventScheduler::Timer syslogTimer;

static void syslog_end()
{
    if (syslog) {
        syslogTimer.remove();
        _logger.setSyslog(nullptr);
        delete syslog;
        syslog = nullptr;
    }
}

static void syslog_kill(uint16_t timeout)
{
    if (syslog) {
        syslogTimer.remove();
        auto endTime = millis() + timeout;
        while(syslog->hasQueuedMessages() && millis() < endTime) {
            syslog->deliverQueue();
            delay(1);
        }
        syslog->getQueue()->kill();
    }
}

static void syslog_timer_callback(EventScheduler::TimerPtr)
{
#if DEBUG
    if (!syslog) {
        __debugbreak_and_panic_printf_P(PSTR("syslog_timer_callback() syslog=nullptr\n"));
    }
#endif
    if (syslog->hasQueuedMessages()) {
        syslog->deliverQueue();
    }
}

static void syslog_setup()
{
#if DEBUG
    if (syslog) {
        __debugbreak_and_panic_printf_P(PSTR("syslog_setup() called twice\n"));
    }
#endif
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

        syslog = new SyslogStream(filter, new SyslogMemoryQueue(SYSLOG_PLUGIN_QUEUE_SIZE));

        _logger.setSyslog(syslog);
        syslogTimer.add(100, true, syslog_timer_callback);
    }
}

class SyslogPlugin : public PluginComponent {
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
};

static SyslogPlugin plugin;


PROGMEM_DEFINE_PLUGIN_OPTIONS(
    SyslogPlugin,
    "syslog",           // name
    "Syslog Client",    // friendly name
    "",                 // web_templates
    "syslog",           // config_forms
    "",                 // reconfigure_dependencies
    PluginComponent::PriorityType::SYSLOG,
    PluginComponent::RTCMemoryId::NONE,
    static_cast<uint8_t>(PluginComponent::MenuType::AUTO),
    false,              // allow_safe_mode
    true,               // setup_after_deep_sleep
    true,               // has_get_status
    true,               // has_config_forms
    false,              // has_web_ui
    false,              // has_web_templates
    true,               // has_at_mode
    0                   // __reserved
);

SyslogPlugin::SyslogPlugin() : PluginComponent(PROGMEM_GET_PLUGIN_OPTIONS(SyslogPlugin))
{
    REGISTER_PLUGIN(this, "SyslogPlugin");
}

void SyslogPlugin::setup(SetupModeType mode)
{
    syslog_setup();
}

void SyslogPlugin::reconfigure(const String & source)
{
    syslog_end();
    syslog_setup();
}

void SyslogPlugin::shutdown()
{
    syslog_kill(250);
}

void SyslogPlugin::getStatus(Print &output)
{
#if SYSLOG_SUPPORT
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
#else
    output.print(FSPGM(Not_supported));
#endif
}

void SyslogPlugin::createConfigureForm(FormCallbackType type, const String &formName, Form &form, AsyncWebServerRequest *request)
{
    if (!isCreateFormCallbackType(type)) {
        return;
    }

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
    form.addValidator(new FormRangeValidator(FSPGM(Invalid_port), 1, 65535));

    form.finalize();
}

#if ENABLE_DEEP_SLEEP

void SyslogPlugin::prepareDeepSleep(uint32_t sleepTimeMillis)
{
    uint16_t defaultWaitTime = 250;
    if (sleepTimeMillis > 60000) {  // longer than 1min, increase wait time
        defaultWaitTime *= 4;
    }

    syslog_kill(defaultWaitTime);
}

#endif

#if AT_MODE_SUPPORTED

#include "at_mode.h"

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(SQ, "SQ", "<clear|info|queue>", "Syslog queue command");

void SyslogPlugin::atModeHelpGenerator()
{
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(SQ), getName_P());
}

bool SyslogPlugin::atModeHandler(AtModeArgs &args)
{
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(SQ))) {
        if (syslog) {
            auto ch = args.toLowerChar(0);
            if (ch == 'c') {
                syslog->getQueue()->clear();
                args.print(F("Queue cleared"));
            }
            else if (ch == 'q') {
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
            else {
                args.printf_P(PSTR("%d"), syslog->getQueue()->size());
            }
        }
        else {
            args.print(F("Syslog is disabled"));
        }
        return true;
    }
    return false;
}

#endif
