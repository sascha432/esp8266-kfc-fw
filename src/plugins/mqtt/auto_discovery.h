/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <PrintString.h>
#include "mqtt_base.h"
#include "mqtt_strings.h"

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

            inline void addParameter(NameType name, int value) {
                __addParameter(name, String(value), false);
            }

            inline void addParameter(NameType name, double value) {
                __addParameter(name, String(value), false);
            }

            template<typename _Ta, typename std::enable_if<!std::is_same<_Ta, bool>::value && std::is_arithmetic<_Ta>::value, int>::type = 0>
            void addParameter(NameType name, _Ta value) {
                PrintString str; // support for uint64_t
                str.print(value);
                __addParameter(name, static_cast<String &>(str), false);
            }

            template<typename _Ta, typename std::enable_if<std::is_same<_Ta, bool>::value, int>::type = 0>
            void addParameter(NameType name, _Ta value) {
                __addParameter(name, value ? PSTR("true") : PSTR("false"), false);
            }

        public:
            template<typename _T>
            void addStateTopic(_T value) {
                addParameter(FSPGM(mqtt_state_topic), value);
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
            // ["Efect1","Effect2",...]
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

            void addValueTemplate(const char *value) { // PROGMEM safe
                PrintString value_json(F("{{ value_json.%s }}"), value);
                addParameter(FSPGM(mqtt_value_template), value_json.c_str());
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
                addPayloadOn(1);
                addPayloadOff(0);
            }

            template<typename _T>
            void addUnitOfMeasurement(_T value) {
                addParameter(FSPGM(mqtt_unit_of_measurement), value);
            }

            void addAutomationType() {
                addParameter(FSPGM(mqtt_automation_type), F("trigger"));
            }

            void addSchemaJson() {
                addParameter(F("schema"), F("json"));
            }

            void addDeviceClass(NameType deviceClass) {
                addParameter(FSPGM(mqtt_device_class), deviceClass);
            }

            void addSubType(const String &buttonName) {
                addParameter(FSPGM(mqtt_subtype), buttonName);
            }

            void addPayloadAndType(const String &buttonName, NameType type) {
                addParameter(FSPGM(mqtt_type), String(F("button_")) + String(type));
                addParameter(FSPGM(mqtt_payload), buttonName + '_' + String(type));
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
