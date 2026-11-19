/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <PrintString.h>
#include <templates.h>
#include "mqtt_base.h"
#include "mqtt_strings.h"

namespace MQTT {

    namespace AutoDiscovery {

        // builds the topic and payload of one Home Assistant auto discovery entity
        //
        // topic: <prefix>/<component>/<object id or device name>[/<name>]/config
        class Entity {
        public:
            Entity();

            // returns true to continue, false to stop (format FormatType::TOPIC)
            bool create(ComponentPtr component, const String &componentName, FormatType format);
            bool create(ComponentType componentType, const String &componentName, FormatType format);

            template<typename _Ta>
            bool createJsonSchema(_Ta component, const String &componentName, FormatType format) {
                if (!create(component, componentName, format)) {
                    return false;
                }
                addSchemaJson();
                return true;
            }

        public:
            inline void addParameter(NameType name, const char *value) { // PROGMEM safe
                __addParameter(name, value, true);
            }

            inline void addParameter(NameType name, NameType value) {
                __addParameter(name, value, true);
            }

            inline void addParameter(NameType name, const String &value) {
                __addParameter(name, value, true);
            }

            inline void addParameter(NameType name, char value) {
                __addParameter(name, String(value), true);
            }

            inline void addParameter(NameType name, int32_t value) {
                __addParameter(name, String(value), false);
            }

            inline void addParameter(NameType name, uint32_t value) {
                __addParameter(name, String(value), false);
            }

            inline void addParameter(NameType name, float value) {
                __addParameter(name, String(value), false);
            }

            inline void addParameter(NameType name, double value) {
                __addParameter(name, String(value), false);
            }

            inline void addParameter(NameType name, int64_t value) {
                __addParameter(name, String(value), false);
            }

            inline void addParameter(NameType name, uint64_t value) {
                __addParameter(name, String(value), false);
            }

            template<typename _Ta, typename std::enable_if<std::is_same<_Ta, bool>::value, int>::type = 0>
            void addParameter(NameType name, _Ta value) {
                __addParameter(name, value ? FSPGM(mqtt_bool_true) : FSPGM(mqtt_bool_false), false);
            }

        public:
            template<typename _T>
            void addStateTopic(_T value) {
                addParameter(FSPGM(mqtt_state_topic), value);
            }

            // an empty name keeps the default name
            void addName(const String &value) {
                if (value.length() == 0) {
                    return;
                }
                _hasName = true;
                addParameter(FSPGM(mqtt_name), value);
            }

            template<typename _T>
            void addName(_T value) {
                _hasName = true;
                addParameter(FSPGM(mqtt_name), value);
            }

            void addFriendlyName(const String &value) {
                if (value.length() == 0) {
                    return;
                }
                addParameter(FSPGM(mqtt_friendly_name), value);
            }

            template<typename _T>
            void addFriendlyName(_T value) {
                addParameter(FSPGM(mqtt_friendly_name), value);
            }

            // the entity id that Home Assistant uses when it creates the entity: <component>.<object id>. an
            // existing entity keeps its id
            void addObjectId(const String &value);

            void addObjectId(const __FlashStringHelper *value) {
                addObjectId(String(value));
            }

            template<typename _T>
            void addIcon(_T value) {
                addParameter(FSPGM(mqtt_icon), value);
            }

            template<typename _T>
            void addStateTopicAndPayloadOnOff(_T value) {
                addParameter(FSPGM(mqtt_state_topic), value);
                addPayloadOnOff();
            }

            template<typename _T>
            void addCommandTopic(_T value) {
                addParameter(FSPGM(mqtt_command_topic), value);
            }

            template<typename _T>
            void addBrightnessStateTopic(_T value) {
                addParameter(FSPGM(mqtt_brightness_state_topic), value);
            }

            template<typename _T>
            void addBrightnessCommandTopic(_T value) {
                addParameter(FSPGM(mqtt_brightness_command_topic), value);
            }

            template<typename _T>
            void addBrightnessScale(_T value) {
                addParameter(FSPGM(mqtt_brightness_scale), value);
            }

            template<typename _T>
            void addColorTempStateTopic(_T value) {
                addParameter(FSPGM(mqtt_color_temp_state_topic), value);
            }

            template<typename _T>
            void addColorTempCommandTopic(_T value) {
                addParameter(FSPGM(mqtt_color_temp_command_topic), value);
            }

            template<typename _T>
            void addRGBStateTopic(_T value) {
                addParameter(FSPGM(mqtt_rgb_state_topic), value);
            }

            template<typename _T>
            void addRGBCommandTopic(_T value) {
                addParameter(FSPGM(mqtt_rgb_command_topic), value);
            }

            template<typename _T>
            void addEffectCommandTopic(_T value) {
                addParameter(FSPGM(mqtt_effect_command_topic), value);
            }

            template<typename _T>
            void addEffectStateTopic(_T value) {
                addParameter(FSPGM(mqtt_effect_state_topic), value);
            }

            // JSON array
            // ["Effect1","Effect2",...]
            void addEffectList(const String &json) {
                __addParameter(FSPGM(mqtt_effect_list), json.c_str(), false);
            }

            // JSON array
            // ["rgbw","color_temp",...]
            void addSupportedColorModes(const String &json) {
                __addParameter(F("supported_color_modes"), json.c_str(), false);
            }

            template<typename _T>
            void addTopic(_T value) {
                addParameter(FSPGM(mqtt_topic), value);
            }

            template<typename _T>
            void addType(_T value) {
                addParameter(FSPGM(mqtt_type), value);
            }

            template<typename _T>
            void addSubType(_T value) {
                addParameter(FSPGM(mqtt_subtype), value);
            }

            template<typename _T>
            void addExpireAfter(_T value) {
                addParameter(FSPGM(mqtt_expire_after), value);
            }

            void addValueTemplate(PGM_P value) { // PROGMEM safe
                PrintString value_json(F("{{ value_json.%s }}"), value);
                addParameter(FSPGM(mqtt_value_template), value_json.c_str());
            }

            void addValueTemplate(const __FlashStringHelper *value) {
                addValueTemplate(reinterpret_cast<PGM_P>(value));
            }

            void addValueTemplate(const String &value) {
                addValueTemplate(value.c_str());
            }

            template<typename _T>
            void addPayloadOn(_T value) {
                addParameter(FSPGM(mqtt_payload_on), value);
            }

            template<typename _T>
            void addPayloadOff(_T value) {
                addParameter(FSPGM(mqtt_payload_off), value);
            }

            void addPayloadOnOff() {
                addPayloadOn(FSPGM(mqtt_bool_on));
                addPayloadOff(FSPGM(mqtt_bool_off));
            }

            template<typename _T>
            void addUnitOfMeasurement(_T value) {
                addParameter(FSPGM(mqtt_unit_of_measurement), value);
            }

            void setVisibleByDefault(bool visible) {
                addParameter(FSPGM(mqtt_visible_by_default), visible);
            }

            void setEnabledByDefault(bool enabled) {
                addParameter(FSPGM(mqtt_enabled_by_default), enabled);
            }

            void addAutomationType() {
                addParameter(FSPGM(mqtt_automation_type), FSPGM(mqtt_trigger));
            }

            void addSchemaJson() {
                addParameter(FSPGM(mqtt_schema), FSPGM(mqtt_schema_json));
            }

            void addDeviceClass(NameType deviceClass) {
                addParameter(FSPGM(mqtt_device_class), deviceClass);
            }

            template<typename _Ta>
            void addDeviceClass(NameType deviceClass, _Ta unitOfMeasurement) {
                addParameter(FSPGM(mqtt_unit_of_measurement), unitOfMeasurement);
                addParameter(FSPGM(mqtt_device_class), deviceClass);
            }

            template<typename _Ta>
            void addStateClass(_Ta stateClass) {
                addParameter(FSPGM(mqtt_state_class), stateClass);
            }

            void addSubType(const String &subType) {
                addParameter(FSPGM(mqtt_subtype), subType);
            }

            void addPayloadAndSubType(const String &subType, NameType type, NameType typePrefix) {
                addParameter(FSPGM(mqtt_subtype), subType);
                addParameter(FSPGM(mqtt_type), PrintString(F("%s_%s"), typePrefix, type));
                addParameter(FSPGM(mqtt_payload), PrintString(F("%s_%s"), subType.c_str(), type));
            }

            template<typename _T>
            void addSpeedRangeMin(_T value) {
                addParameter(FSPGM(mqtt_speed_range_min), value);
            }

            template<typename _T>
            void addSpeedRangeMax(_T value) {
                addParameter(FSPGM(mqtt_speed_range_max), value);
            }

            template<typename _T>
            void addPercentageCommandTopic(_T value) {
                addParameter(FSPGM(mqtt_percentage_command_topic), value);
            }

            template<typename _T>
            void addPercentageStateTopic(_T value) {
                addParameter(FSPGM(mqtt_percentage_state_topic), value);
            }

            template<typename _T>
            void addPresetModeStateTopic(_T value) {
                addParameter(FSPGM(mqtt_preset_mode_state_topic), value);
            }

            template<typename _T>
            void addPresetModeCommandTopic(_T value) {
                addParameter(FSPGM(mqtt_preset_mode_command_topic), value);
            }

            // JSON array
            // ["Mode1","Mode2",...]
            void addPresetModes(const String &json) {
                __addParameter(FSPGM(mqtt_preset_modes), json.c_str(), false);
            }

        public:
            // closes the JSON object, adds the default name if the component did not add one
            void finalize();

            String &getPayload() {
                return static_cast<String &>(_discovery);
            }

            String &getTopic() {
                return _topic;
            }

            // the auto discovery topics of this device, <prefix>/+/<node>/config and <prefix>/+/<node>/+/config
            static String getConfigWildcardTopic();
            static String getConfig2ndLevelWildcardTopic();
            // topic for the device triggers
            static String getTriggersTopic();

        private:
            void __addParameter(NameType name, const char *value, bool quotes = true); // PROGMEM safe

            inline void __addParameter(NameType name, NameType value, bool quotes = true) {
                __addParameter(name, reinterpret_cast<const char *>(value), quotes);
            }

            inline void __addParameter(NameType name, const String &value, bool quotes = true) {
                __addParameter(name, value.c_str(), quotes);
            }

            bool _create(ComponentType componentType, const String &name, FormatType format);

            PrintString _discovery;
            String _topic;
            String _name;
            #if MQTT_AUTO_DISCOVERY_USE_ABBREVIATIONS
                String _baseTopic;
            #endif
            FormatType _format;
            ComponentType _type;
            bool _hasName;
        };

    }

}
