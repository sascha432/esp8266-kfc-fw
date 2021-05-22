// AUTOMATICALLY GENERATED FILE. DO NOT MODIFY
// GENERATOR: ./scripts/tools/create_mqtt_strings.py
//
#pragma once
#include <Arduino_compat.h>
// use abbreviations to reduce the size of the auto discovery
#ifndef MQTT_AUTO_DISCOVERY_USE_ABBREVIATIONS
#define MQTT_AUTO_DISCOVERY_USE_ABBREVIATIONS 1
#endif
PROGMEM_STRING_DECL(mqtt_component_switch);
PROGMEM_STRING_DECL(mqtt_component_light);
PROGMEM_STRING_DECL(mqtt_component_sensor);
PROGMEM_STRING_DECL(mqtt_component_binary_sensor);
PROGMEM_STRING_DECL(mqtt_component_storage);
PROGMEM_STRING_DECL(mqtt_status_topic);
PROGMEM_STRING_DECL(mqtt_status_topic_online);
PROGMEM_STRING_DECL(mqtt_status_topic_offline);
PROGMEM_STRING_DECL(mqtt_bool_on);
PROGMEM_STRING_DECL(mqtt_bool_off);
PROGMEM_STRING_DECL(mqtt_bool_true);
PROGMEM_STRING_DECL(mqtt_bool_false);
PROGMEM_STRING_DECL(mqtt_schema);
PROGMEM_STRING_DECL(mqtt_trigger);
PROGMEM_STRING_DECL(mqtt_schema_json);
PROGMEM_STRING_DECL(mqtt_type);
PROGMEM_STRING_DECL(mqtt_component_device_automation);
PROGMEM_STRING_DECL(mqtt_action_topic);
PROGMEM_STRING_DECL(mqtt_action_template);
PROGMEM_STRING_DECL(mqtt_automation_type);
PROGMEM_STRING_DECL(mqtt_aux_command_topic);
PROGMEM_STRING_DECL(mqtt_aux_state_template);
PROGMEM_STRING_DECL(mqtt_aux_state_topic);
PROGMEM_STRING_DECL(mqtt_availability);
PROGMEM_STRING_DECL(mqtt_availability_mode);
PROGMEM_STRING_DECL(mqtt_availability_topic);
PROGMEM_STRING_DECL(mqtt_away_mode_command_topic);
PROGMEM_STRING_DECL(mqtt_away_mode_state_template);
PROGMEM_STRING_DECL(mqtt_away_mode_state_topic);
PROGMEM_STRING_DECL(mqtt_blue_template);
PROGMEM_STRING_DECL(mqtt_brightness_command_topic);
PROGMEM_STRING_DECL(mqtt_brightness_scale);
PROGMEM_STRING_DECL(mqtt_brightness_state_topic);
PROGMEM_STRING_DECL(mqtt_brightness_template);
PROGMEM_STRING_DECL(mqtt_brightness_value_template);
PROGMEM_STRING_DECL(mqtt_color_temp_command_template);
PROGMEM_STRING_DECL(mqtt_battery_level_topic);
PROGMEM_STRING_DECL(mqtt_battery_level_template);
PROGMEM_STRING_DECL(mqtt_charging_topic);
PROGMEM_STRING_DECL(mqtt_charging_template);
PROGMEM_STRING_DECL(mqtt_color_temp_command_topic);
PROGMEM_STRING_DECL(mqtt_color_temp_state_topic);
PROGMEM_STRING_DECL(mqtt_color_temp_template);
PROGMEM_STRING_DECL(mqtt_color_temp_value_template);
PROGMEM_STRING_DECL(mqtt_cleaning_topic);
PROGMEM_STRING_DECL(mqtt_cleaning_template);
PROGMEM_STRING_DECL(mqtt_command_off_template);
PROGMEM_STRING_DECL(mqtt_command_on_template);
PROGMEM_STRING_DECL(mqtt_command_topic);
PROGMEM_STRING_DECL(mqtt_command_template);
PROGMEM_STRING_DECL(mqtt_code_arm_required);
PROGMEM_STRING_DECL(mqtt_code_disarm_required);
PROGMEM_STRING_DECL(mqtt_current_temperature_topic);
PROGMEM_STRING_DECL(mqtt_current_temperature_template);
PROGMEM_STRING_DECL(mqtt_device);
PROGMEM_STRING_DECL(mqtt_device_class);
PROGMEM_STRING_DECL(mqtt_docked_topic);
PROGMEM_STRING_DECL(mqtt_docked_template);
PROGMEM_STRING_DECL(mqtt_error_topic);
PROGMEM_STRING_DECL(mqtt_error_template);
PROGMEM_STRING_DECL(mqtt_fan_speed_topic);
PROGMEM_STRING_DECL(mqtt_fan_speed_template);
PROGMEM_STRING_DECL(mqtt_fan_speed_list);
PROGMEM_STRING_DECL(mqtt_flash_time_long);
PROGMEM_STRING_DECL(mqtt_flash_time_short);
PROGMEM_STRING_DECL(mqtt_effect_command_topic);
PROGMEM_STRING_DECL(mqtt_effect_list);
PROGMEM_STRING_DECL(mqtt_effect_state_topic);
PROGMEM_STRING_DECL(mqtt_effect_template);
PROGMEM_STRING_DECL(mqtt_effect_value_template);
PROGMEM_STRING_DECL(mqtt_expire_after);
PROGMEM_STRING_DECL(mqtt_fan_mode_command_template);
PROGMEM_STRING_DECL(mqtt_fan_mode_command_topic);
PROGMEM_STRING_DECL(mqtt_fan_mode_state_template);
PROGMEM_STRING_DECL(mqtt_fan_mode_state_topic);
PROGMEM_STRING_DECL(mqtt_force_update);
PROGMEM_STRING_DECL(mqtt_green_template);
PROGMEM_STRING_DECL(mqtt_hold_command_template);
PROGMEM_STRING_DECL(mqtt_hold_command_topic);
PROGMEM_STRING_DECL(mqtt_hold_state_template);
PROGMEM_STRING_DECL(mqtt_hold_state_topic);
PROGMEM_STRING_DECL(mqtt_hs_command_topic);
PROGMEM_STRING_DECL(mqtt_hs_state_topic);
PROGMEM_STRING_DECL(mqtt_hs_value_template);
PROGMEM_STRING_DECL(mqtt_icon);
PROGMEM_STRING_DECL(mqtt_initial);
PROGMEM_STRING_DECL(mqtt_json_attributes);
PROGMEM_STRING_DECL(mqtt_json_attributes_topic);
PROGMEM_STRING_DECL(mqtt_json_attributes_template);
PROGMEM_STRING_DECL(mqtt_max_mireds);
PROGMEM_STRING_DECL(mqtt_min_mireds);
PROGMEM_STRING_DECL(mqtt_max_temp);
PROGMEM_STRING_DECL(mqtt_min_temp);
PROGMEM_STRING_DECL(mqtt_mode_command_template);
PROGMEM_STRING_DECL(mqtt_mode_command_topic);
PROGMEM_STRING_DECL(mqtt_mode_state_template);
PROGMEM_STRING_DECL(mqtt_mode_state_topic);
PROGMEM_STRING_DECL(mqtt_name);
PROGMEM_STRING_DECL(mqtt_off_delay);
PROGMEM_STRING_DECL(mqtt_on_command_type);
PROGMEM_STRING_DECL(mqtt_optimistic);
PROGMEM_STRING_DECL(mqtt_oscillation_command_topic);
PROGMEM_STRING_DECL(mqtt_oscillation_command_template);
PROGMEM_STRING_DECL(mqtt_oscillation_state_topic);
PROGMEM_STRING_DECL(mqtt_oscillation_value_template);
PROGMEM_STRING_DECL(mqtt_percentage_command_topic);
PROGMEM_STRING_DECL(mqtt_percentage_command_template);
PROGMEM_STRING_DECL(mqtt_percentage_state_topic);
PROGMEM_STRING_DECL(mqtt_percentage_value_template);
PROGMEM_STRING_DECL(mqtt_payload);
PROGMEM_STRING_DECL(mqtt_payload_arm_away);
PROGMEM_STRING_DECL(mqtt_payload_arm_home);
PROGMEM_STRING_DECL(mqtt_payload_arm_custom_bypass);
PROGMEM_STRING_DECL(mqtt_payload_arm_night);
PROGMEM_STRING_DECL(mqtt_payload_available);
PROGMEM_STRING_DECL(mqtt_payload_clean_spot);
PROGMEM_STRING_DECL(mqtt_payload_close);
PROGMEM_STRING_DECL(mqtt_payload_disarm);
PROGMEM_STRING_DECL(mqtt_payload_home);
PROGMEM_STRING_DECL(mqtt_payload_lock);
PROGMEM_STRING_DECL(mqtt_payload_locate);
PROGMEM_STRING_DECL(mqtt_payload_not_available);
PROGMEM_STRING_DECL(mqtt_payload_not_home);
PROGMEM_STRING_DECL(mqtt_payload_off);
PROGMEM_STRING_DECL(mqtt_payload_on);
PROGMEM_STRING_DECL(mqtt_payload_open);
PROGMEM_STRING_DECL(mqtt_payload_oscillation_off);
PROGMEM_STRING_DECL(mqtt_payload_oscillation_on);
PROGMEM_STRING_DECL(mqtt_payload_pause);
PROGMEM_STRING_DECL(mqtt_payload_stop);
PROGMEM_STRING_DECL(mqtt_payload_start);
PROGMEM_STRING_DECL(mqtt_payload_start_pause);
PROGMEM_STRING_DECL(mqtt_payload_return_to_base);
PROGMEM_STRING_DECL(mqtt_payload_turn_off);
PROGMEM_STRING_DECL(mqtt_payload_turn_on);
PROGMEM_STRING_DECL(mqtt_payload_unlock);
PROGMEM_STRING_DECL(mqtt_position_closed);
PROGMEM_STRING_DECL(mqtt_position_open);
PROGMEM_STRING_DECL(mqtt_power_command_topic);
PROGMEM_STRING_DECL(mqtt_power_state_topic);
PROGMEM_STRING_DECL(mqtt_power_state_template);
PROGMEM_STRING_DECL(mqtt_preset_mode_command_topic);
PROGMEM_STRING_DECL(mqtt_preset_mode_command_template);
PROGMEM_STRING_DECL(mqtt_preset_mode_state_topic);
PROGMEM_STRING_DECL(mqtt_preset_mode_value_template);
PROGMEM_STRING_DECL(mqtt_preset_modes);
PROGMEM_STRING_DECL(mqtt_red_template);
PROGMEM_STRING_DECL(mqtt_retain);
PROGMEM_STRING_DECL(mqtt_rgb_command_template);
PROGMEM_STRING_DECL(mqtt_rgb_command_topic);
PROGMEM_STRING_DECL(mqtt_rgb_state_topic);
PROGMEM_STRING_DECL(mqtt_rgb_value_template);
PROGMEM_STRING_DECL(mqtt_send_command_topic);
PROGMEM_STRING_DECL(mqtt_send_if_off);
PROGMEM_STRING_DECL(mqtt_set_fan_speed_topic);
PROGMEM_STRING_DECL(mqtt_set_position_template);
PROGMEM_STRING_DECL(mqtt_set_position_topic);
PROGMEM_STRING_DECL(mqtt_position_topic);
PROGMEM_STRING_DECL(mqtt_position_template);
PROGMEM_STRING_DECL(mqtt_speed_range_min);
PROGMEM_STRING_DECL(mqtt_speed_range_max);
PROGMEM_STRING_DECL(mqtt_source_type);
PROGMEM_STRING_DECL(mqtt_state_closed);
PROGMEM_STRING_DECL(mqtt_state_closing);
PROGMEM_STRING_DECL(mqtt_state_off);
PROGMEM_STRING_DECL(mqtt_state_on);
PROGMEM_STRING_DECL(mqtt_state_open);
PROGMEM_STRING_DECL(mqtt_state_opening);
PROGMEM_STRING_DECL(mqtt_state_stopped);
PROGMEM_STRING_DECL(mqtt_state_locked);
PROGMEM_STRING_DECL(mqtt_state_unlocked);
PROGMEM_STRING_DECL(mqtt_state_topic);
PROGMEM_STRING_DECL(mqtt_state_template);
PROGMEM_STRING_DECL(mqtt_state_value_template);
PROGMEM_STRING_DECL(mqtt_subtype);
PROGMEM_STRING_DECL(mqtt_supported_features);
PROGMEM_STRING_DECL(mqtt_swing_mode_command_template);
PROGMEM_STRING_DECL(mqtt_swing_mode_command_topic);
PROGMEM_STRING_DECL(mqtt_swing_mode_state_template);
PROGMEM_STRING_DECL(mqtt_swing_mode_state_topic);
PROGMEM_STRING_DECL(mqtt_temperature_command_template);
PROGMEM_STRING_DECL(mqtt_temperature_command_topic);
PROGMEM_STRING_DECL(mqtt_temperature_high_command_template);
PROGMEM_STRING_DECL(mqtt_temperature_high_command_topic);
PROGMEM_STRING_DECL(mqtt_temperature_high_state_template);
PROGMEM_STRING_DECL(mqtt_temperature_high_state_topic);
PROGMEM_STRING_DECL(mqtt_temperature_low_command_template);
PROGMEM_STRING_DECL(mqtt_temperature_low_command_topic);
PROGMEM_STRING_DECL(mqtt_temperature_low_state_template);
PROGMEM_STRING_DECL(mqtt_temperature_low_state_topic);
PROGMEM_STRING_DECL(mqtt_temperature_state_template);
PROGMEM_STRING_DECL(mqtt_temperature_state_topic);
PROGMEM_STRING_DECL(mqtt_temperature_unit);
PROGMEM_STRING_DECL(mqtt_tilt_closed_value);
PROGMEM_STRING_DECL(mqtt_tilt_command_topic);
PROGMEM_STRING_DECL(mqtt_tilt_command_template);
PROGMEM_STRING_DECL(mqtt_tilt_invert_state);
PROGMEM_STRING_DECL(mqtt_tilt_max);
PROGMEM_STRING_DECL(mqtt_tilt_min);
PROGMEM_STRING_DECL(mqtt_tilt_opened_value);
PROGMEM_STRING_DECL(mqtt_tilt_optimistic);
PROGMEM_STRING_DECL(mqtt_tilt_status_topic);
PROGMEM_STRING_DECL(mqtt_tilt_status_template);
PROGMEM_STRING_DECL(mqtt_topic);
PROGMEM_STRING_DECL(mqtt_unique_id);
PROGMEM_STRING_DECL(mqtt_unit_of_measurement);
PROGMEM_STRING_DECL(mqtt_value_template);
PROGMEM_STRING_DECL(mqtt_white_value_command_topic);
PROGMEM_STRING_DECL(mqtt_white_value_scale);
PROGMEM_STRING_DECL(mqtt_white_value_state_topic);
PROGMEM_STRING_DECL(mqtt_white_value_template);
PROGMEM_STRING_DECL(mqtt_xy_command_topic);
PROGMEM_STRING_DECL(mqtt_xy_state_topic);
PROGMEM_STRING_DECL(mqtt_xy_value_template);
