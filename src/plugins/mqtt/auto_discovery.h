/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <PrintString.h>
#include "mqtt_base.h"
#include "mqtt_strings.h"

#if 0
#include <debug_helper_disable_ptr_validation.h>
#endif

namespace MQTT {

    namespace AutoDiscovery {

        class Entity {
        public:

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
                __DBG_validatePointerCheck(name, VP_HPS);
                __DBG_validatePointerCheck(value, VP_HPS);
                __addParameter(name, value, true);
            }

            inline void addParameter(NameType name, NameType value) {
                __DBG_validatePointerCheck(name, VP_HPS);
                __DBG_validatePointerCheck(value, VP_HPS);
                __addParameter(name, value, true);
            }

            inline void addParameter(NameType name, const String &value) {
                __DBG_validatePointerCheck(name, VP_HPS);
                __addParameter(name, value, true);
            }

            inline void addParameter(NameType name, char value) {
                __DBG_validatePointerCheck(name, VP_HPS);
                __addParameter(name, String(value), true);
            }

            inline void addParameter(NameType name, int32_t value) {
                __DBG_validatePointerCheck(name, VP_HPS);
                __addParameter(name, String(value), false);
            }

            inline void addParameter(NameType name, uint32_t value) {
                __DBG_validatePointerCheck(name, VP_HPS);
                __addParameter(name, String(value), false);
            }

            inline void addParameter(NameType name, float value) {
                __DBG_validatePointerCheck(name, VP_HPS);
                __addParameter(name, String(value), false);
            }

            inline void addParameter(NameType name, double value) {
                __DBG_validatePointerCheck(name, VP_HPS);
                __addParameter(name, String(value), false);
            }

            inline void addParameter(NameType name, int64_t value) {
                __DBG_validatePointerCheck(name, VP_HPS);
                __addParameter(name, String(value), false);
            }

            inline void addParameter(NameType name, uint64_t value) {
                __DBG_validatePointerCheck(name, VP_HPS);
                __addParameter(name, String(value), false);
            }

            // template<typename _Ta, typename std::enable_if<!std::is_same<_Ta, bool>::value && std::is_arithmetic<_Ta>::value, int>::type = 0>
            // void addParameter(NameType name, _Ta value) {
            //     __DBG_validatePointerCheck(name, VP_HPS);
            //     PrintString str; // support for uint64_t
            //     str.print(value);
            //     __addParameter(name, static_cast<String &>(str), false);
            // }

            template<typename _Ta, typename std::enable_if<std::is_same<_Ta, bool>::value, int>::type = 0>
            void addParameter(NameType name, _Ta value) {
                __DBG_validatePointerCheck(name, VP_HPS);
                __addParameter(name, value ? FSPGM(mqtt_bool_true) : FSPGM(mqtt_bool_false), false);
            }

        public:
            template<typename _T>
            void addStateTopic(_T value) {
                addParameter(FSPGM(mqtt_state_topic), value);
            }

            void addName(const String &value) {
                if (value.length() == 0) {
                    return;
                }
                addParameter(FSPGM(mqtt_name), value);
            }

            template<typename _T>
            void addName(_T value) {
                addParameter(FSPGM(mqtt_name), value);
            }

            template<typename _T>
            void addObjectId(_T value) {
                addParameter(FSPGM(mqtt_object_id), value);
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
            void addEffectList(String json) {
                __addParameter(FSPGM(mqtt_effect_list), json.c_str(), false);
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
            void addubType(_T value) {
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
            void addPresetModes(String json) {
                __addParameter(FSPGM(mqtt_preset_modes), json.c_str(), false);
            }


        public:
            void finalize();
            String &getPayload();
            String &getTopic();

            // roughly the free space needed in the MQTT clients tcp buffer
            size_t getMessageSize() const;

            static String getWildcardTopic();
            static String getConfigWildcardTopic();
            static String getConfig2ndLevelWildcardTopic();
            static String getTriggersTopic();

        private:
            void __addParameter(NameType name, const char *value, bool quotes = true); // PROGMEM safe

            inline void __addParameter(NameType name, NameType value, bool quotes = true) {
                __addParameter(name, reinterpret_cast<const char *>(value), quotes);
            }

            inline void __addParameter(NameType name, const String &value, bool quotes = true) {
                __addParameter(name, value.c_str(), quotes);
            }

        private:
            bool _create(ComponentType componentType, const String &name, FormatType format, NameType platform = FSPGM(mqtt));
            const String _getUnqiueId(const String &name);

            FormatType _format;
            PrintString _discovery;
            String _topic;
            #if MQTT_AUTO_DISCOVERY_USE_ABBREVIATIONS
                String _baseTopic;
            #endif
        };

        inline String &Entity::getPayload()
        {
            return static_cast<String &>(_discovery);
        }

        inline String &Entity::getTopic()
        {
            return _topic;
        }

        inline size_t Entity::getMessageSize() const
        {
            return _discovery.length() + _topic.length() + 16;
        }

    }

}
