/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if IOT_SENSOR

#include <Arduino_compat.h>
#include <vector>
#include <EventScheduler.h>
#include "WebUIComponent.h"
#include "plugins.h"
#include "MQTTSensor.h"

#ifndef DEBUG_IOT_SENSOR
#define DEBUG_IOT_SENSOR 0
#endif

class SensorPlugin : public PluginComponent, public WebUIInterface {
public:
    typedef std::vector<MQTTSensor *> SensorVector;

// WebUIInterface
public:
    virtual void getValues(JsonArray &array) override;
    virtual void setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState) override;

// PluginComponent
public:
    SensorPlugin() {
        register_plugin(this);
    }

    virtual PGM_P getName() const;
    virtual PluginPriorityEnum_t getSetupPriority() const override {
        return (PluginPriorityEnum_t)110;
    }

    virtual void setup(PluginSetupMode_t mode) override;
    virtual void reconfigure(PGM_P source) override;

    virtual bool hasStatus() const override;
    virtual const String getStatus() override;

    virtual bool hasWebUI() const override;
    virtual void createWebUI(WebUI &webUI) override;
    virtual WebUIInterface *getWebUIInterface() override;

    static void timerEvent(EventScheduler::TimerPtr timer);

private:
    void _timerEvent();

    SensorVector _sensors;
    EventScheduler::TimerPtr _timer;
};

#endif

