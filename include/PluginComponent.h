/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <vector>
#include "WebUIComponent.h"

#ifdef DEFAULT
// framework-arduinoespressif8266\cores\esp8266\Arduino.h
#undef DEFAULT
#endif

class AsyncWebServerRequest;
class WebTemplate;
class Form;
class AtModeArgs;

class PluginComponent {
public:
    enum class SetupModeType : uint8_t {
        DEFAULT,                        // normal boot
        SAFE_MODE,                     // safe mode active
        AUTO_WAKE_UP,                  // wake up from deep sleep
        DELAYED_AUTO_WAKE_UP,          // called after a delay to initialize services that have been skipped during wake up
    };

    enum class PriorityType : int8_t {
        RESET_DETECTOR = -127,
        CONFIG = -120,
        SERIAL2TCP = -115,
        BUTTONS = -100,
        MDNS = -90,
        SYSLOG = -80,
        NTP = -70,
        HTTP = -60,
        ALARM = -55,
        HASS = -50,
        MAX = 0,           // highest prio, -127 to -1 is reserved for the system
        HTTP2SERIAL = 10,
        MQTT = 20,
        DEFAULT = 64,
        SENSOR = 110,
        PING_MONITOR = 126,
        MIN = 127
    } ;

    enum class MenuType {
        NONE,
        AUTO,
        CUSTOM,
    };

    PluginComponent() : _setupTime(0) {
    }

    template<class T>
    static T *getPlugin(const __FlashStringHelper *name) {
        return reinterpret_cast<T *>(findPlugin(name));
    }
    static PluginComponent *findPlugin(const __FlashStringHelper *name);

    virtual PGM_P getName() const = 0;
    virtual const __FlashStringHelper *getFriendlyName() const;
    bool nameEquals(const __FlashStringHelper *name) const;
    bool nameEquals(const char *name) const;
    bool nameEquals(const String &name) const;

    virtual PriorityType getSetupPriority() const;
    virtual uint8_t getRtcMemoryId() const;
    virtual bool allowSafeMode() const;
#if ENABLE_DEEP_SLEEP
    virtual bool autoSetupAfterDeepSleep() const;
#endif

    // executed during boot
    virtual void setup(SetupModeType mode);
    // executed before a restart
    virtual void shutdown();

    // executed after the configuration has been changed
protected:
    virtual void reconfigure(PGM_P source);
public:
    virtual bool hasReconfigureDependecy(PluginComponent *plugin) const;

    // calls reconfigure for all dependencies
    void invokeReconfigureNow(PGM_P source);

    // adds a scheduled function call invokeReconfigureNow() to avoid issues from being called inside interrupts
    void invokeReconfigure(PGM_P source);

    // executed to get status information
    virtual bool hasStatus() const;
    virtual void getStatus(Print &output);

    // name of the form
    virtual PGM_P getConfigureForm() const;
    // returns if the form can be handled. only needed for custom forms. the default is comparing the forName with getConfigureForm()
    virtual bool canHandleForm(const String &formName) const;
    // executed to get the configure form
    virtual void createConfigureForm(AsyncWebServerRequest *request, Form &form);
    // gets called after a form has been validated successfully and *before* config.write() is called
    virtual void configurationSaved(Form *form);
    // gets called if a form validation failed and *befor*e config.discard() is called
    virtual void configurationDiscarded(Form *form);

    // get type of menu entry
    virtual MenuType getMenuType() const;
    // create custom menu entries
    virtual void createMenu();

    // executed to get the template for the web ui
    virtual bool hasWebTemplate(const String &formName) const;
    virtual WebTemplate *getWebTemplate(const String &formName);

    virtual bool hasWebUI() const;
    virtual void createWebUI(WebUI &webUI);
    virtual WebUIInterface *getWebUIInterface();

#if ENABLE_DEEP_SLEEP
    // executed before entering deep sleep
    virtual void prepareDeepSleep(uint32_t sleepTimeMillis);
#endif

#if AT_MODE_SUPPORTED
    // at mode command handler
    virtual bool hasAtMode() const;
    virtual void atModeHelpGenerator();
    virtual bool atModeHandler(AtModeArgs &args);
#endif

    static PluginComponent *getForm(const String &formName);
    static PluginComponent *getTemplate(const String &formName);
    static PluginComponent *getByName(PGM_P name);
    static PluginComponent *getByMemoryId(uint8_t memoryId);

    uint32_t getSetupTime() const {
        return _setupTime;
    }
    void setSetupTime() {
        _setupTime = millis();
    }

private:
    uint32_t _setupTime;
};
