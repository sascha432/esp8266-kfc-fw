/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if IOT_ATOMIC_SUN_V2

#include <Arduino_compat.h>
#include <HardwareSerial.h>
#include <PrintString.h>
#include <PrintHtmlEntitiesString.h>
#include <WebUIComponent.h>
#include "../mqtt/mqtt_client.h"
#include "serial_handler.h"
#include "../dimmer_module/dimmer_base.h"
#include "../dimmer_module/dimmer_module_form.h"

#ifndef DEBUG_4CH_DIMMER
#define DEBUG_4CH_DIMMER 0
#endif

#ifndef IOT_ATOMIC_SUN_BAUD_RATE
#define IOT_ATOMIC_SUN_BAUD_RATE            57600
#endif

#ifndef IOT_ATOMIC_SUN_MAX_BRIGHTNESS
#define IOT_ATOMIC_SUN_MAX_BRIGHTNESS       8333
#endif

typedef struct {
    struct {
        String set;
        String state;
        bool value;
    } state;
    struct {
        String set;
        String state;
        int16_t value;
    } brightness;
    struct {
        String set;
        String state;
        uint16_t value;
    } color;
} Driver_4ChDimmer_MQTTComponentData_t;

class Driver_4ChDimmer : public MQTTComponent, public Dimmer_Base
{
public:
    Driver_4ChDimmer();

    virtual MQTTAutoDiscovery *createAutoDiscovery(MQTTAutoDiscovery::Format_t format) override;
    virtual void onConnect(MQTTClient *client) override;
    virtual void onMessage(MQTTClient *client, char *topic, char *payload, size_t len) override;

    virtual bool on(uint8_t channel = -1) override;
    virtual bool off(uint8_t channel = -1) override;
    virtual int16_t getChannel(uint8_t channel) const override;
    virtual bool getChannelState(uint8_t channel) const override;
    virtual void setChannel(uint8_t channel, int16_t level, float time) override;
    virtual uint8_t getChannelCount() const override;

   void publishState(MQTTClient *client = nullptr);

protected:
    void _begin();
    void _end();
    void _printStatus(PrintHtmlEntitiesString &out);

private:
    void _createTopics();
    void _publishState(MQTTClient *client);

    void _setChannels(float fadetime);
    void _getChannels();

private:
#if DEBUG_4CH_DIMMER
    uint8_t endTransmission();
#else
    inline uint8_t endTransmission();
#endif

    Driver_4ChDimmer_MQTTComponentData_t _data;
    int16_t _storedBrightness;
    int16_t _channels[4];
    uint8_t _qos;
    String _metricsTopic;
};

class AtomicSunPlugin : public Driver_4ChDimmer, public DimmerModuleForm {
public:
    AtomicSunPlugin() {
        register_plugin(this);
    }
    virtual PGM_P getName() const;
    virtual PluginPriorityEnum_t getSetupPriority() const override;

    virtual void setup(PluginSetupMode_t mode) override;
    virtual void reconfigure(PGM_P source) override;

    virtual bool hasWebUI() const override;
    virtual void createWebUI(WebUI &webUI) override;
    virtual WebUIInterface *getWebUIInterface() override;

    virtual bool hasStatus() const override;
    virtual const String getStatus() override;
};

#endif
