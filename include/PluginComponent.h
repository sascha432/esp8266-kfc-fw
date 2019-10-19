/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <vector>
#include "WebUIComponent.h"

class AsyncWebServerRequest;
class WebTemplate;
class Form;

class PluginComponent {
public:
    typedef enum {
        PLUGIN_SETUP_DEFAULT = 0,                   // normal boot
        PLUGIN_SETUP_SAFE_MODE,                     // safe mode active
        PLUGIN_SETUP_AUTO_WAKE_UP,                  // wake up from deep sleep
        PLUGIN_SETUP_DELAYED_AUTO_WAKE_UP,          // called after a delay to initialize services that have been skipped during wake up
    } PluginSetupMode_t;

    typedef enum : int8_t {
        PRIO_RESET_DETECTOR = -127,
        PRIO_CONFIG = -126,
        PRIO_MDNS = -90,
        PRIO_SYSLOG = -80,
        PRIO_NTP = -70,
        PRIO_HTTP = -60,
        MAX_PRIORITY = 0,           // highest prio, -127 to -1 is reserved for the system
        DEFAULT_PRIORITY = 64,
        MIN_PRIORITY = 127
    } PluginPriorityEnum_t;

    const static int ATModeQueryCommand = -1;

    virtual PGM_P getName() const = 0;
    bool nameEquals(const __FlashStringHelper *name) const;
    bool nameEquals(const char *name) const;
    bool nameEquals(const String &name) const;

    virtual PluginPriorityEnum_t getSetupPriority() const;
    virtual uint8_t getRtcMemoryId() const;
    virtual bool allowSafeMode() const;
    virtual bool autoSetupAfterDeepSleep() const;

    // executed during boot
    virtual void setup(PluginSetupMode_t mode);

    // executed after the configuration has been changed
protected:
    virtual void reconfigure(PGM_P source);
public:
    virtual bool hasReconfigureDependecy(PluginComponent *plugin) const;
    void invokeReconfigure(PGM_P source);

    // executed to get status information
    virtual bool hasStatus() const;
    virtual const String getStatus();

    // executed to get the configure form
    virtual bool canHandleForm(const String &formName) const;
    virtual void createConfigureForm(AsyncWebServerRequest *request, Form &form);

    // executed to get the template for the web ui
    virtual bool hasWebTemplate(const String &formName) const;
    virtual WebTemplate *getWebTemplate(const String &formName);

    virtual bool hasWebUI() const;
    virtual void createWebUI(WebUI &webUI);
    virtual WebUIInterface *getWebUIInterface();

    // executed before entering deep sleep
    virtual void prepareDeepSleep(uint32_t sleepTimeMillis);

    // at mode command handler
    virtual bool hasAtMode() const;
    virtual void atModeHelpGenerator();
    virtual bool atModeHandler(Stream &serial, const String &command, int8_t argc, char **argv);

    static PluginComponent *getForm(const String &formName);
    static PluginComponent *getTemplate(const String &formName);
    static PluginComponent *getByName(PGM_P name);
    static PluginComponent *getByMemoryId(uint8_t memoryId);
};


/*
class MYPLUGINNAME : public PluginComponent {
public:
    MYPLUGINNAME();
    virtual PGM_P getName() const;
    virtual PluginPriorityEnum_t getSetupPriority() const override;
    virtual uint8_t getRtcMemoryId() const override;
    virtual bool allowSafeMode() const override;
    virtual bool autoSetupAfterDeepSleep() const override;
    virtual void setup(PluginSetupMode_t mode) override;
    virtual void reconfigure(PGM_P source) override;
    virtual bool hasReconfigureDependecy(PluginComponent *plugin) const override;
    virtual bool hasStatus() const override;
    virtual const String getStatus() override;
    virtual bool canHandleForm(const String &formName) const override;
    virtual bool hasWebTemplate(const String &formName) const;
    virtual WebTemplate *getWebTemplate(const String &formName);
    virtual bool hasWebUI() const override;
    virtual void createWebUI(WebUI &webUI) override;
    virtual void createConfigureForm(AsyncWebServerRequest *request, Form &form) override;
    virtual void prepareDeepSleep(uint32_t sleepTimeMillis) override;
    virtual bool hasAtMode() const override;
    virtual void atModeHelpGenerator() override;
    virtual bool atModeHandler(Stream &serial, const String &command, int8_t argc, char **argv) override;
};

static MYPLUGINNAME plugin;

MYPLUGINNAME::MYPLUGINNAME() {
    register_plugin(this);
}

PGM_P MYPLUGINNAME::getName() const {
    return PSTR("MYPLUGINNAME");
}

MYPLUGINNAME::PluginPriorityEnum_t MYPLUGINNAME::getSetupPriority() const {
    return PluginPriorityEnum_t::DEFAULT_PRIORITY;
}

uint8_t MYPLUGINNAME::getRtcMemoryId() const {
    return true;
}

bool MYPLUGINNAME::allowSafeMode() const {
    return true;
}

bool MYPLUGINNAME::autoSetupAfterDeepSleep() const {
    return true;
}

void MYPLUGINNAME::setup(PluginSetupMode_t mode) {

}

void MYPLUGINNAME::reconfigure(PGM_P source) {

}

bool MYPLUGINNAME::hasReconfigureDependecy(PluginComponent *plugin) const {
    return false;
}

bool MYPLUGINNAME::hasStatus() const {
    return true;
}

const String MYPLUGINNAME::getStatus() {

}

bool MYPLUGINNAME::canHandleForm(const String &formName) const {
    return nameEquals(formName);
}

void MYPLUGINNAME::createConfigureForm(AsyncWebServerRequest *request, Form &form) {
}

bool MYPLUGINNAME::hasWebTemplate(const String &formName) const {

}

WebTemplate *MYPLUGINNAME::getWebTemplate(const String &formName) {

}

bool MYPLUGINNAME::hasWebUI() const {

}

void MYPLUGINNAME::createWebUI(WebUI &webUI) {

}

void MYPLUGINNAME::prepareDeepSleep(uint32_t sleepTimeMillis) {
}

bool MYPLUGINNAME::hasAtMode() const {
    return true;
}

void MYPLUGINNAME::atModeHelpGenerator() {

}

bool MYPLUGINNAME::atModeHandler(Stream &serial, const String &command, int8_t argc, char **argv) {

}
*/
