/**
 * Author: sascha_lammers@gmx.de
 */

#include "syslog_plugin.h"

using SyslogClient = KFCConfigurationClasses::Plugins::SyslogClient;

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

SyslogPlugin::SyslogPlugin() : PluginComponent(PROGMEM_GET_PLUGIN_OPTIONS(SyslogPlugin)), syslog(nullptr)
{
    REGISTER_PLUGIN(this, "SyslogPlugin");
}

void SyslogPlugin::setup(SetupModeType mode)
{
    begin();
}

void SyslogPlugin::reconfigure(const String &source)
{
    end();
    begin();
}

void SyslogPlugin::shutdown()
{
    kill(250);
}

void SyslogPlugin::timerCallback(EventScheduler::TimerPtr timer)
{
    plugin._timerCallback(timer);
}

void SyslogPlugin::_timerCallback(EventScheduler::TimerPtr timer)
{
#if DEBUG
    if (!syslog) {
        __debugbreak_and_panic_printf_P(PSTR("_timerCallback() syslog=nullptr\n"));
    }
#endif
    if (syslog->hasQueuedMessages()) {
        syslog->deliverQueue();
    }
}

void SyslogPlugin::begin()
{
#if DEBUG
    if (syslog) {
        __debugbreak_and_panic_printf_P(PSTR("begin() called twice\n"));
    }
#endif

    if (SyslogClient::isEnabled()) {

        auto cfg = SyslogClient::getConfig();
        _hostname = SyslogClient::getHostname();
        _port = cfg.port;

        if (config.hasZeroConf(_hostname)) {
            config.resolveZeroConf(_hostname, _port, [this](const String &hostname, const IPAddress &address, uint16_t port, MDNSResolver::ResponseType type) {
                this->_zeroConfCallback(hostname, address, port, type);
            });
        }
        else {
            _zeroConfCallback(_hostname, convertToIPAddress(_hostname), _port, MDNSResolver::ResponseType::NONE);
        }
    }
}

void SyslogPlugin::_zeroConfCallback(const String &hostname, const IPAddress &address, uint16_t port, MDNSResolver::ResponseType type)
{
    auto cfg = SyslogClient::getConfig();
    // SyslogParameter parameter;
    // parameter.setHostname(config.getDeviceName());
    // parameter.setAppName(FSPGM(kfcfw));
    // parameter.setFacility(SYSLOG_FACILITY_KERN);
    // parameter.setSeverity(SYSLOG_NOTICE);

    SyslogFilter *filter = new SyslogFilter(config.getDeviceName(), FSPGM(kfcfw));
    auto &parameter = filter->getParameter();
    parameter.setFacility(SYSLOG_FACILITY_KERN);
    parameter.setSeverity(SYSLOG_NOTICE);

    filter->addFilter(F("*.*"), SyslogFactory::create(parameter, cfg.protocol_enum, _hostname, _port));

    syslog = new SyslogStream(filter, new SyslogMemoryQueue(SYSLOG_PLUGIN_QUEUE_SIZE));

    _logger.setSyslog(syslog);
    syslogTimer.add(100, true, timerCallback);
}

void SyslogPlugin::end()
{
    if (syslog) {
        syslogTimer.remove();
        _logger.setSyslog(nullptr);
        delete syslog;
        syslog = nullptr;
    }
}

void SyslogPlugin::kill(uint16_t timeout)
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

void SyslogPlugin::getStatus(Print &output)
{
#if SYSLOG_SUPPORT
    switch(SyslogClient::getConfig().protocol_enum) {
        case SyslogClient::SyslogProtocolType::UDP:
            output.printf_P(PSTR("UDP @ %s:%u"), _hostname.c_str(), _port);
            break;
        case SyslogClient::SyslogProtocolType::TCP:
            output.printf_P(PSTR("TCP @ %s:%u"), _hostname.c_str(), _port);
            break;
        case SyslogClient::SyslogProtocolType::TCP_TLS:
            output.printf_P(PSTR("TCP TLS @ %s:%u"), _hostname.c_str(), _port);
            break;
        default:
            output.print(FSPGM(Disabled));
            break;
    }
#else
    output.print(FSPGM(Not_supported));
#endif
}

#if ENABLE_DEEP_SLEEP

void SyslogPlugin::prepareDeepSleep(uint32_t sleepTimeMillis)
{
    uint16_t defaultWaitTime = 250;
    if (sleepTimeMillis > 60000) {  // longer than 1min, increase wait time
        defaultWaitTime *= 4;
    }

    kill(defaultWaitTime);
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
