/**
 * Author: sascha_lammers@gmx.de
 */

#if MQTT_SUPPORT

#include "mqtt_component.h"

PROGMEM_STRING_DEF(mqtt_component_switch, "switch");
PROGMEM_STRING_DEF(mqtt_component_light, "light");
PROGMEM_STRING_DEF(mqtt_component_sensor, "sensor");
PROGMEM_STRING_DEF(mqtt_component_binary_sensor, "binary_sensor");

PROGMEM_STRING_DEF(mqtt_unique_id, "unique_id");
PROGMEM_STRING_DEF(mqtt_availability_topic, "availability_topic");
PROGMEM_STRING_DEF(mqtt_status_topic, "/status");
PROGMEM_STRING_DEF(mqtt_payload_available, "payload_available");
PROGMEM_STRING_DEF(mqtt_payload_not_available, "payload_not_available");
PROGMEM_STRING_DEF(mqtt_state_topic, "state_topic");
PROGMEM_STRING_DEF(mqtt_command_topic, "command_topic");
PROGMEM_STRING_DEF(mqtt_payload_on, "payload_on");
PROGMEM_STRING_DEF(mqtt_payload_off, "payload_off");
PROGMEM_STRING_DEF(mqtt_brightness_state_topic, "brightness_state_topic");
PROGMEM_STRING_DEF(mqtt_brightness_command_topic, "brightness_command_topic");
PROGMEM_STRING_DEF(mqtt_brightness_scale, "brightness_scale");
PROGMEM_STRING_DEF(mqtt_color_temp_state_topic, "color_state_topic");
PROGMEM_STRING_DEF(mqtt_color_temp_command_topic, "color_command_topic");

#endif
