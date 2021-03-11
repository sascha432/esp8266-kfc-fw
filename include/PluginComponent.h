/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <vector>
#include <array>
#include <EnumHelper.h>
#include "WebUIComponent.h"
#include "../src/plugins/mqtt/mqtt_json.h"

#ifdef DEFAULT
// framework-arduinoespressif8266\cores\esp8266\Arduino.h
#undef DEFAULT
#endif

class AsyncWebServerRequest;
class WebTemplate;
class AtModeArgs;
class KFCFWConfiguration;
using ATModeCommandHelpArray = const struct ATModeCommandHelp_t *[];
using ATModeCommandHelpArrayPtr = const struct ATModeCommandHelp_t **;

namespace FormUI {
    namespace Form {
        class BaseForm;
    }
}

/*
PROGMEM_DEFINE_PLUGIN_OPTIONS(
    xxxPluginClass,
    "",   // name
    "",   // friendly name
    "",   // web_templates
    "",   // config_forms
    "",   // reconfigure_dependencies
    PluginComponent::PriorityType::XXX,
    PluginComponent::RTCMemoryId::NONE,
    static_cast<uint8_t>(PluginComponent::MenuType::NONE),
    false,              // allow_safe_mode
    false,              // setup_after_deep_sleep
    false,              // has_get_status
    false,              // has_config_forms
    false,              // has_web_ui
    false,              // has_web_templates
    false,              // has_at_mode
    0                   // __reserved
);


virtual void createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request) override;
__LDBG_printf("source=%s", source.c_str());
*/

#include <push_pack.h>

class PluginComponent;

namespace PluginComponents {

    enum class RTCMemoryId : uint8_t {
        NONE,
        RESET_DETECTOR,
        CONFIG,
        DEEP_SLEEP,
        SERIAL2TCP,
        // REMOTE_INIT_PIN_STATE
    };

    enum class PriorityType : int8_t {
        NONE = std::numeric_limits<int8_t>::min(),
        // System
        RESET_DETECTOR,
        CONFIG,
        REMOTE,
        SAVECRASH,
        SERIAL2TCP,
        BUTTONS,
        MDNS,
        SSDP,
        SYSLOG,
        NTP,
        HTTP,
        ALARM,
        // Plugins
        // highest priority for plugins
        MAX = 0,
        // HASS,
        HTTP2SERIAL,
        MQTT,
        // default
        DEFAULT = std::numeric_limits<int8_t>::max() / 2,
        DIMMER_MODULE,
        SWITCH,
        BLINDS,
        SENSOR,
        WEATHER_STATION,
        PING_MONITOR,
        // lowest priority for plugins
        MIN = std::numeric_limits<int8_t>::max()
    };

    using RTCMemoryId = PluginComponents::RTCMemoryId;

    enum class FormCallbackType {
        CREATE_GET,     // gets called to create the form and load data for a GET request
        CREATE_POST,    // gets called to create the form and save data for a POST request before validation of the data
        SAVE,           // gets called after a form has been validated successfully and before config.write() is called
        DISCARD         // gets called after a form validation has failed and before config.discard() is called
    };

    enum class MenuType : uint8_t {
        AUTO,
        NONE,
        CUSTOM,
        MAX
    };
    static_assert((uint8_t)MenuType::MAX <= 0b100, "stored in 2 bits");

    enum class SetupModeType : uint8_t {
        // called once per reboot
        DEFAULT,                                        // normal boot
        // called once per reboot
        SAFE_MODE,                                      // safe mode active

        // these are called in sequence, DEFAULT is not called
        AUTO_WAKE_UP,                                   // wake up from deep sleep
        DELAYED_AUTO_WAKE_UP,                           // called after a delay to initialize services that have been skipped during wake up
    };

    using PluginsVector = std::vector<PluginComponent *>;

    class Dependency;
    class Dependencies;

    using DependenciesPtr = std::shared_ptr<Dependencies>;

    using NameType = const __FlashStringHelper *;
    using NamedJsonArray = MQTT::Json::NamedArray;

    using DependencyCallback = std::function<void(const PluginComponent *plugin)>;
    using DependenciesVector = std::vector<Dependency>;

    class Dependency {
    public:
        Dependency(NameType name, DependencyCallback callback, const PluginComponent *source) : _name(name), _callback(callback), _source(source) {}

        // name of the plugin
        NameType _name;
        // callback to call after it has been initialized
        DependencyCallback _callback;
        // source of the dependency
        const PluginComponent *_source;

        void invoke(const PluginComponent *plugin) const;

        bool operator==(NameType name) const {
            return strcmp_P_P(reinterpret_cast<PGM_P>(_name), reinterpret_cast<PGM_P>(name)) == 0;
        }
    };

    class Dependencies {
    public:
        Dependencies()
        {}

        ~Dependencies() {
            destroy();
        }

        bool dependsOn(NameType name, DependencyCallback callback, const PluginComponent *source);

        bool empty() const;
        size_t size() const;

        void check();
        void cleanup();
        void destroy();

    private:
        DependenciesVector _dependencies;
    };


    inline bool Dependencies::empty() const
    {
        return _dependencies.empty();
    }

    inline size_t Dependencies::size() const
    {
        return _dependencies.size();
    }

    class Component {
    public:
    };

}

class PluginComponent : PluginComponents::Component {
public:

    using Dependencies = PluginComponents::Dependencies;
    using DependencyCallback = PluginComponents::DependencyCallback;
    using NameType = PluginComponents::NameType;
    using NamedJsonArray = PluginComponents::NamedJsonArray;
    using PriorityType = PluginComponents::PriorityType;
    using RTCMemoryId = PluginComponents::RTCMemoryId;
    using MenuType = PluginComponents::MenuType;
    using SetupModeType = PluginComponents::SetupModeType;
    using FormCallbackType = PluginComponents::FormCallbackType;
    using DependenciesPtr = PluginComponents::DependenciesPtr;

    typedef struct __attribute__packed__  {
        union {
            struct __attribute__packed__ {
                PriorityType priority;
                RTCMemoryId memory_id;
                uint8_t menu_type: 2;                       // 0-1
                uint8_t allow_safe_mode: 1;                 // 2
                uint8_t setup_after_deep_sleep: 1;          // 3
                uint8_t has_get_status: 1;                  // 4
                uint8_t has_config_forms: 1;                // 5
                uint8_t has_web_ui: 1;                      // 6
                uint8_t has_web_templates: 1;               // 7
                uint8_t has_at_mode: 1;                     // 0
                uint8_t __reserved: 7;                      // 1-7
            };
            uint32_t __dword;
        };
    } Options_t;
    static_assert(sizeof(Options_t) == sizeof(uint32_t), "change getOptions() to memcpy_P");

    typedef struct {
        PGM_P name;
        PGM_P friendly_name;
        PGM_P web_templates;
        PGM_P config_forms;
        PGM_P reconfigure_dependencies;
        Options_t options;
    } Config_t;
    using PGM_Config_t = const Config_t *;


public:
    PluginComponent(PGM_Config_t config) : _setupTime(0), _pluginConfig(config) {
    }

// static functions
public:
    template<class T>
    static T *getPlugin(NameType name, bool isSetup = true) {
        return reinterpret_cast<T *>(findPlugin(name, isSetup));
    }
    static PluginComponent *findPlugin(NameType name, bool isSetup = true);

    static PluginComponent *getForm(const String &formName);
    static PluginComponent *getTemplate(const String &templateName);
    static PluginComponent *getByName(NameType name);
    static PluginComponent *getByMemoryId(RTCMemoryId memoryId);

    static PluginComponent *getByMemoryId(uint8_t memoryId) {
        return getByMemoryId(static_cast<RTCMemoryId>(memoryId));
    }

// Configuration and options
public:
    PGM_P getName_P() const {
        return reinterpret_cast<PGM_P>(_pluginConfig->name);
    }
    NameType getName() const {
        return reinterpret_cast<NameType>(_pluginConfig->name);
    }
    NameType getFriendlyName() const {
        return reinterpret_cast<NameType>(_pluginConfig->friendly_name);
    }

    bool nameEquals(NameType name) const;
    bool nameEquals(const char *name) const;
    bool nameEquals(const String &name) const;


    Options_t getOptions() const {
        Options_t options;
        // memcpy_P(&options, &_pluginConfig->options, sizeof(options));
        options.__dword = pgm_read_dword(&_pluginConfig->options);
        return options;
    }

    bool allowSafeMode() const {
        return getOptions().allow_safe_mode;
    }
#if ENABLE_DEEP_SLEEP
    bool autoSetupAfterDeepSleep() const {
        return getOptions().setup_after_deep_sleep;
    }
#endif
    bool hasReconfigureDependecy(const String &name) const {
        return stringlist_find_P_P(reinterpret_cast<PGM_P>(_pluginConfig->reconfigure_dependencies), name.c_str(), ',') != -1;
    }

    bool hasStatus() const {
        return getOptions().has_get_status;
    }

    MenuType getMenuType() const {
        return static_cast<MenuType>(getOptions().menu_type);
    }

    bool hasWebTemplate(const String &name) const {
        if (!getOptions().has_web_templates) {
            return false;
        }
        return stringlist_find_P_P(reinterpret_cast<PGM_P>(_pluginConfig->web_templates), name.c_str(), ',') != -1;
    }

    bool canHandleForm(const String &name) const {
        if (!getOptions().has_config_forms) {
            return false;
        }
        return stringlist_find_P_P(reinterpret_cast<PGM_P>(_pluginConfig->config_forms), name.c_str(), ',') != -1;
    }

    PGM_P getConfigForms() const {
        return _pluginConfig->config_forms;
    }

    bool hasWebUI() const {
        return getOptions().has_web_ui;
    }

#if AT_MODE_SUPPORTED
    bool hasAtMode() const {
        return getOptions().has_at_mode;
    }
#endif

    // calls reconfigure for all dependencies
    void invokeReconfigureNow(const String &source);

    // calls reconfigure at the end of the main loop() function
    void invokeReconfigure(const String &source);

    static bool isCreateFormCallbackType(FormCallbackType type) {
        return type == FormCallbackType::CREATE_GET || type == FormCallbackType::CREATE_POST;
    }

// Virtual methods
public:
    // executed after all plugins have been registered
    virtual void preSetup(SetupModeType mode);

    // executed during boot
    virtual void setup(SetupModeType mode, const PluginComponents::DependenciesPtr &dependencies);

    // executed before a restart
    virtual void shutdown();

#if ENABLE_DEEP_SLEEP
    // executed before entering deep sleep
    virtual void prepareDeepSleep(uint32_t sleepTimeMillis);
#endif

    // executed after the configuration has been changed. source is the name of the plugin that initiated the reconfigure call
    // system sources: wifi, network, device, password
    virtual void reconfigure(const String &source);

    // status.html
    virtual void getStatus(Print &output);

    // add entries for web interface menu
    virtual void createMenu();

     // form processing for html files. formName will be wifi for /wifi.html
    virtual void createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request);

    // template processing for html files. templateName will be home for /home.html
    virtual WebTemplate *getWebTemplate(const String &templateName);

    // webui.html
    virtual void createWebUI(WebUIRoot &webUI);
    virtual void getValues(JsonArray &array);
    virtual void getValues(NamedJsonArray &array);
    virtual void setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState);

#if AT_MODE_SUPPORTED
    // returns array ATModeCommandHelp_t[size] or nullptr for no help
    virtual ATModeCommandHelpArrayPtr atModeCommandHelp(size_t &size) const;
    // do not override if atModeCommandHelp exists
    virtual void atModeHelpGenerator();
    virtual bool atModeHandler(AtModeArgs &args);
#endif

public:
    uint32_t getSetupTime() const;
    void setSetupTime();
    void clearSetupTime();
    bool isEnabled() const {
        return _setupTime != 0;
    }

    static bool addToBlacklist(const String &name);
    static bool removeFromBlacklist(const String &name);
    static const char *getBlacklist();

private:
    friend KFCFWConfiguration;
    friend PluginComponents::Dependencies;

    uint32_t _setupTime;
    PGM_Config_t _pluginConfig;
};

#define PROGMEM_DEFINE_PLUGIN_OPTIONS(class_name, name, friendly_name, web_templates, config_forms, reconfigure_dependencies, ...) \
    static const char _plugins_config_progmem_name_##class_name[] PROGMEM = { name }; \
    static const char _plugins_config_progmem_friendly_name_##class_name[] PROGMEM = { friendly_name }; \
    static const char _plugins_config_progmem_web_templates_##class_name[] PROGMEM = { web_templates }; \
    static const char _plugins_config_progmem_config_forms_##class_name[] PROGMEM = { config_forms }; \
    static const char _plugins_config_progmem_reconfigure_dependencies_##class_name[] PROGMEM = { reconfigure_dependencies }; \
    static const PluginComponent::Config_t _plugins_config_progmem_config_t_##class_name PROGMEM = { \
        _plugins_config_progmem_name_##class_name, \
        _plugins_config_progmem_friendly_name_##class_name, \
        _plugins_config_progmem_web_templates_##class_name, \
        _plugins_config_progmem_config_forms_##class_name, \
        _plugins_config_progmem_reconfigure_dependencies_##class_name, \
        { __VA_ARGS__ } \
    };

#define PROGMEM_GET_PLUGIN_OPTIONS(class_name)          &_plugins_config_progmem_config_t_##class_name

#include <pop_pack.h>
