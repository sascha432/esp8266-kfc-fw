/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include "../src/plugins/mqtt/mqtt_json.h"
#include <Arduino_compat.h>
#include <array>
#include <vector>

#ifdef DEFAULT
// framework-arduinoespressif8266\cores\esp8266\Arduino.h
#    undef DEFAULT
#endif

class AsyncWebServerRequest;
class WebTemplate;
class AtModeArgs;
class KFCFWConfiguration;

struct ATModeCommandHelp_t;

using ATModeCommandHelpArray = const ATModeCommandHelp_t *[];
using ATModeCommandHelpArrayPtr = const ATModeCommandHelp_t **;

namespace FormUI {
    namespace Form {
        class BaseForm;
    }
}

namespace WebUINS {
    class Root;
    class Events;
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
        QUICK_CONNECT,
        DEEP_SLEEP,
        RTC,
        CONFIG,
        SERIAL2TCP,
        SAFE_MODE,
        SWITCH,
        MAX
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
        DISPLAY_PLUGIN,
        // lowest priority for plugins
        MIN = std::numeric_limits<int8_t>::max()
    };

    using RTCMemoryId = PluginComponents::RTCMemoryId;

    enum class FormCallbackType {
        CREATE_GET, // gets called to create the form and load data for a GET request
        CREATE_POST, // gets called to create the form and save data for a POST request before validation of the data
        SAVE, // gets called after a form has been validated successfully and before config.write() is called
        DISCARD // gets called after a form validation has failed and before config.discard() is called
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
        DEFAULT, // normal boot
        // called once per reboot
        SAFE_MODE, // safe mode active

        // these are called in sequence, DEFAULT is not called
        AUTO_WAKE_UP, // wake up from deep sleep
        DELAYED_AUTO_WAKE_UP, // called after a delay to initialize services that have been skipped during wake up
    };

    using PluginsVector = std::vector<PluginComponent *>;

    class Dependency;
    class Dependencies;

    using DependenciesPtr = std::shared_ptr<Dependencies>;

    using NameType = const __FlashStringHelper *;
    using NamedArray = MQTT::Json::NamedArray;

    enum class DependencyResponseType {
        SUCCESS = 0,
        NOT_LOADED,
        NOT_FOUND,
    };

    using DependencyCallback = std::function<void(const PluginComponent *plugin, DependencyResponseType response)>;
    using DependenciesVector = std::vector<Dependency>;

    class Dependency {
    public:
        Dependency(NameType name, DependencyCallback callback, const PluginComponent *source) :
            _name(name), _callback(callback), _source(source) { }

        // name of the plugin
        NameType _name;
        // callback to call after it has been initialized
        DependencyCallback _callback;
        // source of the dependency
        const PluginComponent *_source;

        void invoke(const PluginComponent *plugin) const;
        void invoke(DependencyResponseType type) const;

        bool operator==(NameType name) const
        {
            return strcmp_P_P(reinterpret_cast<PGM_P>(_name), reinterpret_cast<PGM_P>(name)) == 0;
        }
    };

    class Dependencies {
    public:
        Dependencies();
        ~Dependencies();

        bool dependsOn(NameType name, DependencyCallback callback, const PluginComponent *source);

        // template<typename _Ta = PluginComponent>
        // bool dependsOn(NameType name, std::function<void(const _Ta &, bool)> callback, const PluginComponent *source) {
        //     return dependsOn(name, [callback](const PluginComponent *plugin, bool failure) {
        //         callback(*reinterpret_cast<_Ta *>(plugin), failure);
        //     }, source);
        // }

        bool empty() const;
        size_t size() const;

        void check();
        void cleanup();
        void destroy();

        DependenciesVector &getVector();

    private:
        DependenciesVector _dependencies;
    };

    inline Dependencies::Dependencies()
    {
    }

    inline Dependencies::~Dependencies()
    {
        destroy();
    }

    inline DependenciesVector &Dependencies::getVector()
    {
        return _dependencies;
    }

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

class PluginComponentAtModeHelpInterface {
public:
#if AT_MODE_HELP_SUPPORTED
    virtual ATModeCommandHelpArrayPtr atModeCommandHelp(size_t &size) const = 0;
#endif
};

// set to one to copy PROGMEM to the stack before reading the data
#define PLUGINS_USE_MEMCPY_P_TO_READ_CONFIG ESP8266

#if ESP8266
using prog_uint32_t = uint32_t;
#endif

class PluginComponent : public PluginComponents::Component, public PluginComponentAtModeHelpInterface {
public:
    using Dependencies = PluginComponents::Dependencies;
    using DependencyCallback = PluginComponents::DependencyCallback;
    using DependenciesPtr = PluginComponents::DependenciesPtr;
    using DependencyResponseType = PluginComponents::DependencyResponseType;
    using NameType = PluginComponents::NameType;
    using PriorityType = PluginComponents::PriorityType;
    using RTCMemoryId = PluginComponents::RTCMemoryId;
    using MenuType = PluginComponents::MenuType;
    using SetupModeType = PluginComponents::SetupModeType;
    using FormCallbackType = PluginComponents::FormCallbackType;

    typedef struct __attribute__packed__ {
        union {
            struct __attribute__packed__ {
                PriorityType priority;
                RTCMemoryId memory_id;
                uint8_t menu_type : 2;                      // 0-1
                uint8_t allow_safe_mode : 1;                // 2
                uint8_t setup_after_deep_sleep : 1;         // 3
                uint8_t has_get_status : 1;                 // 4
                uint8_t has_config_forms : 1;               // 5
                uint8_t has_web_ui : 1;                     // 6
                uint8_t has_web_templates : 1;              // 7
                uint8_t has_at_mode : 1;                    // 0
                uint8_t __reserved : 7;                     // 1-7
            };
            prog_uint32_t __dword;
        };
    } Options_t;

    #if !PLUGINS_USE_MEMCPY_P_TO_READ_CONFIG
        static_assert(sizeof(Options_t) == sizeof(prog_uint32_t), "change getOptions() to memcpy_P");
    #endif

    typedef struct {
        PGM_P name;
        PGM_P friendly_name;
        PGM_P web_templates;
        PGM_P config_forms;
        PGM_P reconfigure_dependencies;
        Options_t options;
    } Config_t;
    using PGM_Config_Ptr = const Config_t *;

public:
    PluginComponent(PGM_Config_Ptr config) :
        _setupTime(0),
        _pluginConfigPtr(config)
    {
    }

    // static functions
public:
    template <class T>
    static T *getPlugin(NameType name, bool isSetup = true)
    {
        return reinterpret_cast<T *>(findPlugin(name, isSetup));
    }
    static PluginComponent *findPlugin(NameType name, bool isSetup = true);

    static PluginComponent *getForm(const String &formName);
    static PluginComponent *getTemplate(const String &templateName);
    static PluginComponent *getByName(NameType name);
    static PluginComponent *getByMemoryId(RTCMemoryId memoryId);

    static PluginComponent *getByMemoryId(uint8_t memoryId)
    {
        return getByMemoryId(static_cast<RTCMemoryId>(memoryId));
    }

    inline static NameType getMemoryIdName(uint8_t id)
    {
        return getMemoryIdName(static_cast<RTCMemoryId>(id));
    }

    inline static NameType getMemoryIdName(RTCMemoryId id)
    {
        switch (id) {
        case RTCMemoryId::DEEP_SLEEP:
            return F("Deep Sleep");
        case RTCMemoryId::CONFIG:
            return F("Configuration");
        case RTCMemoryId::RESET_DETECTOR:
            return F("Reset Detector");
        case RTCMemoryId::QUICK_CONNECT:
            return F("Quick Connect");
        case RTCMemoryId::SERIAL2TCP:
            return F("Serial2TCP");
        case RTCMemoryId::RTC:
            return F("RTC");
        case RTCMemoryId::SAFE_MODE:
            return F("SAFE_MODE");
        case RTCMemoryId::SWITCH:
            return F("SWITCH");
        case RTCMemoryId::NONE:
        case RTCMemoryId::MAX:
            break;
        }
        return F("<UNKNOWN>");
    }

    // Configuration and options
public:
    const Config_t getConfig() const;
    const Options_t getOptions() const;

    PGM_P getName_P() const;
    NameType getName() const;
    NameType getFriendlyName() const;

    bool nameEquals(NameType name) const;
    bool nameEquals(const char *name) const;
    bool nameEquals(const String &name) const;

    bool allowSafeMode() const;
    bool hasReconfigureDependency(const String &name) const;
    bool hasStatus() const;
    MenuType getMenuType() const;
    bool hasWebTemplate(const String &name) const;
    bool canHandleForm(const String &name) const;
    PGM_P getConfigForms() const;
    bool hasWebUI() const;

    #if ENABLE_DEEP_SLEEP
        bool autoSetupAfterDeepSleep() const;
    #endif
    #if AT_MODE_SUPPORTED
        bool hasAtMode() const;
    #endif

    // calls reconfigure for all dependencies
    void invokeReconfigureNow(const String &source);

    // calls reconfigure at the end of the main loop() function
    void invokeReconfigure(const String &source);

    static bool isCreateFormCallbackType(FormCallbackType type);

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
    virtual void createWebUI(WebUINS::Root &webUI);
    virtual void getValues(WebUINS::Events &array);
    virtual void setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState);
    virtual bool getValue(const String &id, String &value, bool &state);

    #if AT_MODE_SUPPORTED
    #    if AT_MODE_HELP_SUPPORTED
        // returns array ATModeCommandHelp_t[size] or nullptr for no help
        virtual ATModeCommandHelpArrayPtr atModeCommandHelp(size_t &size) const;
        virtual void atModeHelpGenerator();
    #    endif
        virtual bool atModeHandler(AtModeArgs &args);
    #endif

public:
    uint32_t getSetupTime() const;
    void setSetupTime();
    void clearSetupTime();
    bool isEnabled() const;

    static bool addToBlacklist(const String &name);
    static bool removeFromBlacklist(const String &name);
    static const char *getBlacklist();

private:
    friend KFCFWConfiguration;
    friend PluginComponents::Dependencies;

    uint32_t _setupTime;
    PGM_Config_Ptr _pluginConfigPtr;
};

inline const PluginComponent::Config_t PluginComponent::getConfig() const
{
    #if PLUGINS_USE_MEMCPY_P_TO_READ_CONFIG
        Config_t tmp;
        memcpy_P(&tmp, _pluginConfigPtr, sizeof(tmp));
        return tmp;
    #else
        return *_pluginConfigPtr;
    #endif
}

inline const PluginComponent::Options_t PluginComponent::getOptions() const
{
    #if PLUGINS_USE_MEMCPY_P_TO_READ_CONFIG
        return getConfig().options;
    #else
        Options_t options;
        options.__dword = pgm_read_dword(&_pluginConfigPtr->options.__dword);
        return options;
    #endif
}

inline PGM_P PluginComponent::getName_P() const
{
    return getConfig().name;
}

inline PluginComponent::NameType PluginComponent::getName() const
{
    return reinterpret_cast<NameType>(getConfig().name);
}

inline PluginComponent::NameType PluginComponent::getFriendlyName() const
{
    return reinterpret_cast<NameType>(getConfig().friendly_name);
}

inline bool PluginComponent::allowSafeMode() const
{
    return getOptions().allow_safe_mode;
}

inline bool PluginComponent::hasReconfigureDependency(const String &name) const
{
    return stringlist_find_P_P(getConfig().reconfigure_dependencies, name.c_str(), ',') != -1;
}

inline bool PluginComponent::hasStatus() const
{
    return getOptions().has_get_status;
}

inline PluginComponent::MenuType PluginComponent::getMenuType() const
{
    return static_cast<MenuType>(getOptions().menu_type);
}

inline bool PluginComponent::hasWebTemplate(const String &name) const
{
    if (!getOptions().has_web_templates) {
        return false;
    }
    return stringlist_find_P_P(getConfig().web_templates, name.c_str(), ',') != -1;
}

inline bool PluginComponent::canHandleForm(const String &name) const
{
    if (!getOptions().has_config_forms) {
        return false;
    }
    return stringlist_find_P_P(getConfigForms(), name.c_str(), ',') != -1;
}

inline PGM_P PluginComponent::getConfigForms() const
{
    return getConfig().config_forms;
}

inline bool PluginComponent::hasWebUI() const
{
    return getOptions().has_web_ui;
}

#if ENABLE_DEEP_SLEEP
    inline bool PluginComponent::autoSetupAfterDeepSleep() const
    {
        return getOptions().setup_after_deep_sleep;
    }
#endif

#if AT_MODE_SUPPORTED
    inline bool PluginComponent::hasAtMode() const
    {
        return getOptions().has_at_mode;
    }
#endif

inline bool PluginComponent::isCreateFormCallbackType(FormCallbackType type)
{
    return type == FormCallbackType::CREATE_GET || type == FormCallbackType::CREATE_POST;
}

inline bool PluginComponent::isEnabled() const
{
    return _setupTime != 0;
}

#define PROGMEM_DEFINE_PLUGIN_OPTIONS(class_name, name, friendly_name, web_templates, config_forms, reconfigure_dependencies, ...) \
    static const char _plugins_config_progmem_name_##class_name[] PROGMEM = { name };                                              \
    static const char _plugins_config_progmem_friendly_name_##class_name[] PROGMEM = { friendly_name };                            \
    static const char _plugins_config_progmem_web_templates_##class_name[] PROGMEM = { web_templates };                            \
    static const char _plugins_config_progmem_config_forms_##class_name[] PROGMEM = { config_forms };                              \
    static const char _plugins_config_progmem_reconfigure_dependencies_##class_name[] PROGMEM = { reconfigure_dependencies };      \
    static const PluginComponent::Config_t _plugins_config_progmem_config_t_##class_name PROGMEM = {                               \
        _plugins_config_progmem_name_##class_name,                                                                                 \
        _plugins_config_progmem_friendly_name_##class_name,                                                                        \
        _plugins_config_progmem_web_templates_##class_name,                                                                        \
        _plugins_config_progmem_config_forms_##class_name,                                                                         \
        _plugins_config_progmem_reconfigure_dependencies_##class_name,                                                             \
        { __VA_ARGS__ }                                                                                                            \
    };

#define PROGMEM_GET_PLUGIN_OPTIONS(class_name) &_plugins_config_progmem_config_t_##class_name

#include <pop_pack.h>
