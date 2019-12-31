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

    typedef enum : uint8_t {
        NONE = 0,
        AUTO,
        CUSTOM,
    } MenuTypeEnum_t;

    static const int ATModeQueryCommand = -1;

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

    // calls reconfigure for all dependencies
    void invokeReconfigureNow(PGM_P source);

    // adds a timer to call invokeReconfigureNow() in the main loop() to avoid issues from being called inside interrupts
    void invokeReconfigure(PGM_P source);

    // executed to get status information
    virtual bool hasStatus() const;
    virtual void getStatus(Print &output);

    // name of the form
    virtual PGM_P getConfigureForm() const;
    // returns if the form can be handled. only needed for custom forms. the default is using getConfigureForm()
    virtual bool canHandleForm(const String &formName) const;
    // executed to get the configure form
    virtual void createConfigureForm(AsyncWebServerRequest *request, Form &form);

    // get type of menu entry
    virtual MenuTypeEnum_t getMenuType() const;
    // create custom menu entries
    virtual void createMenu();

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

    // executed before a restart
    virtual void restart();

    static PluginComponent *getForm(const String &formName);
    static PluginComponent *getTemplate(const String &formName);
    static PluginComponent *getByName(PGM_P name);
    static PluginComponent *getByMemoryId(uint8_t memoryId);
};
