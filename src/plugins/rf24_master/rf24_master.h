/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if IOT_RF24_MASTER

#include <Arduino_compat.h>
#include <vector>
#include <EventScheduler.h>
#include "plugins.h"
#include "RF24Channel.h"

class RF24MasterPlugin : public PluginComponent {
// PluginComponent
public:
    RF24MasterPlugin();

    virtual PGM_P getName() const;
    virtual PluginPriorityEnum_t getSetupPriority() const override {
        return (PluginPriorityEnum_t)110;
    }

    virtual void setup(PluginSetupMode_t mode) override;
    virtual void reconfigure(PGM_P source) override;

    virtual bool hasStatus() const override;
    virtual void getStatus(Print &output) override;

#if AT_MODE_SUPPORTED
    virtual bool hasAtMode() const override;
    virtual void atModeHelpGenerator() override;
    virtual bool atModeHandler(Stream &serial, const String &command, int8_t argc, char **argv) override;
#endif

public:
    static void loop();

private:
    void _initChannels();
    void _loop();
    RF24Channel *_addChannel(DigitalPin *ce, DigitalPin *csn, uint8_t channel);
    void _removeChannel(RF24Channel *rf24);

private:
    typedef std::unique_ptr<RF24Channel> RF24ChannelPtr;
    typedef std::vector<RF24ChannelPtr> RF24ChannelsVector_t;

    RF24ChannelsVector_t _channels;
    String _sharedSecret;
#if IOT_RF24_MASTER_HAVE_PCF8574
    PCF8574 _pcf8574;
#endif
};

#endif
