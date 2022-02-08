// AUTOMATICALLY GENERATED FILE. DO NOT MODIFY
// GENERATOR: ./scripts/tools/create_mqtt_strings.py
//
#include "mqtt_strings.h"
PROGMEM_STRING_DEF(mqtt_component_switch, "switch");
PROGMEM_STRING_DEF(mqtt_component_light, "light");
PROGMEM_STRING_DEF(mqtt_component_sensor, "sensor");
PROGMEM_STRING_DEF(mqtt_component_binary_sensor, "binary_sensor");
PROGMEM_STRING_DEF(mqtt_component_fan, "fan");
PROGMEM_STRING_DEF(mqtt_component_storage, "storage");
PROGMEM_STRING_DEF(mqtt_status_topic, "/status");
PROGMEM_STRING_DEF(mqtt_status_topic_online, "online");
PROGMEM_STRING_DEF(mqtt_status_topic_offline, "offline");
PROGMEM_STRING_DEF(mqtt_bool_on, "ON");
PROGMEM_STRING_DEF(mqtt_bool_off, "OFF");
PROGMEM_STRING_DEF(mqtt_bool_true, "true");
PROGMEM_STRING_DEF(mqtt_bool_false, "false");
PROGMEM_STRING_DEF(mqtt_schema, "schema");
PROGMEM_STRING_DEF(mqtt_trigger, "trigger");
PROGMEM_STRING_DEF(mqtt_schema_json, "json");
PROGMEM_STRING_DEF(mqtt_type, "type");
PROGMEM_STRING_DEF(mqtt_component_device_automation, "device_automation");
PROGMEM_STRING_DEF(mqtt_friendly_name, "friendly_name");
#if MQTT_AUTO_DISCOVERY_USE_ABBREVIATIONS
PROGMEM_STRING_DEF(mqtt_action_topic, "act_t");
PROGMEM_STRING_DEF(mqtt_action_template, "act_tpl");
PROGMEM_STRING_DEF(mqtt_automation_type, "atype");
PROGMEM_STRING_DEF(mqtt_aux_command_topic, "aux_cmd_t");
PROGMEM_STRING_DEF(mqtt_aux_state_template, "aux_stat_tpl");
PROGMEM_STRING_DEF(mqtt_aux_state_topic, "aux_stat_t");
PROGMEM_STRING_DEF(mqtt_availability, "avty");
PROGMEM_STRING_DEF(mqtt_availability_mode, "avty_mode");
PROGMEM_STRING_DEF(mqtt_availability_topic, "avty_t");
PROGMEM_STRING_DEF(mqtt_availability_template, "avty_tpl");
PROGMEM_STRING_DEF(mqtt_away_mode_command_topic, "away_mode_cmd_t");
PROGMEM_STRING_DEF(mqtt_away_mode_state_template, "away_mode_stat_tpl");
PROGMEM_STRING_DEF(mqtt_away_mode_state_topic, "away_mode_stat_t");
PROGMEM_STRING_DEF(mqtt_blue_template, "b_tpl");
PROGMEM_STRING_DEF(mqtt_brightness_command_topic, "bri_cmd_t");
PROGMEM_STRING_DEF(mqtt_brightness_scale, "bri_scl");
PROGMEM_STRING_DEF(mqtt_brightness_state_topic, "bri_stat_t");
PROGMEM_STRING_DEF(mqtt_brightness_template, "bri_tpl");
PROGMEM_STRING_DEF(mqtt_brightness_value_template, "bri_val_tpl");
PROGMEM_STRING_DEF(mqtt_color_temp_command_template, "clr_temp_cmd_tpl");
PROGMEM_STRING_DEF(mqtt_battery_level_topic, "bat_lev_t");
PROGMEM_STRING_DEF(mqtt_battery_level_template, "bat_lev_tpl");
PROGMEM_STRING_DEF(mqtt_charging_topic, "chrg_t");
PROGMEM_STRING_DEF(mqtt_charging_template, "chrg_tpl");
PROGMEM_STRING_DEF(mqtt_color_temp_command_topic, "clr_temp_cmd_t");
PROGMEM_STRING_DEF(mqtt_color_temp_state_topic, "clr_temp_stat_t");
PROGMEM_STRING_DEF(mqtt_color_temp_template, "clr_temp_tpl");
PROGMEM_STRING_DEF(mqtt_color_temp_value_template, "clr_temp_val_tpl");
PROGMEM_STRING_DEF(mqtt_cleaning_topic, "cln_t");
PROGMEM_STRING_DEF(mqtt_cleaning_template, "cln_tpl");
PROGMEM_STRING_DEF(mqtt_command_off_template, "cmd_off_tpl");
PROGMEM_STRING_DEF(mqtt_command_on_template, "cmd_on_tpl");
PROGMEM_STRING_DEF(mqtt_command_topic, "cmd_t");
PROGMEM_STRING_DEF(mqtt_command_template, "cmd_tpl");
PROGMEM_STRING_DEF(mqtt_code_arm_required, "cod_arm_req");
PROGMEM_STRING_DEF(mqtt_code_disarm_required, "cod_dis_req");
PROGMEM_STRING_DEF(mqtt_code_trigger_required, "cod_trig_req");
PROGMEM_STRING_DEF(mqtt_current_temperature_topic, "curr_temp_t");
PROGMEM_STRING_DEF(mqtt_current_temperature_template, "curr_temp_tpl");
PROGMEM_STRING_DEF(mqtt_device, "dev");
PROGMEM_STRING_DEF(mqtt_device_class, "dev_cla");
PROGMEM_STRING_DEF(mqtt_docked_topic, "dock_t");
PROGMEM_STRING_DEF(mqtt_docked_template, "dock_tpl");
PROGMEM_STRING_DEF(mqtt_encoding, "e");
PROGMEM_STRING_DEF(mqtt_error_topic, "err_t");
PROGMEM_STRING_DEF(mqtt_error_template, "err_tpl");
PROGMEM_STRING_DEF(mqtt_fan_speed_topic, "fanspd_t");
PROGMEM_STRING_DEF(mqtt_fan_speed_template, "fanspd_tpl");
PROGMEM_STRING_DEF(mqtt_fan_speed_list, "fanspd_lst");
PROGMEM_STRING_DEF(mqtt_flash_time_long, "flsh_tlng");
PROGMEM_STRING_DEF(mqtt_flash_time_short, "flsh_tsht");
PROGMEM_STRING_DEF(mqtt_effect_command_topic, "fx_cmd_t");
PROGMEM_STRING_DEF(mqtt_effect_list, "fx_list");
PROGMEM_STRING_DEF(mqtt_effect_state_topic, "fx_stat_t");
PROGMEM_STRING_DEF(mqtt_effect_template, "fx_tpl");
PROGMEM_STRING_DEF(mqtt_effect_value_template, "fx_val_tpl");
PROGMEM_STRING_DEF(mqtt_expire_after, "exp_aft");
PROGMEM_STRING_DEF(mqtt_fan_mode_command_template, "fan_mode_cmd_tpl");
PROGMEM_STRING_DEF(mqtt_fan_mode_command_topic, "fan_mode_cmd_t");
PROGMEM_STRING_DEF(mqtt_fan_mode_state_template, "fan_mode_stat_tpl");
PROGMEM_STRING_DEF(mqtt_fan_mode_state_topic, "fan_mode_stat_t");
PROGMEM_STRING_DEF(mqtt_force_update, "frc_upd");
PROGMEM_STRING_DEF(mqtt_green_template, "g_tpl");
PROGMEM_STRING_DEF(mqtt_hold_command_template, "hold_cmd_tpl");
PROGMEM_STRING_DEF(mqtt_hold_command_topic, "hold_cmd_t");
PROGMEM_STRING_DEF(mqtt_hold_state_template, "hold_stat_tpl");
PROGMEM_STRING_DEF(mqtt_hold_state_topic, "hold_stat_t");
PROGMEM_STRING_DEF(mqtt_hs_command_topic, "hs_cmd_t");
PROGMEM_STRING_DEF(mqtt_hs_state_topic, "hs_stat_t");
PROGMEM_STRING_DEF(mqtt_hs_value_template, "hs_val_tpl");
PROGMEM_STRING_DEF(mqtt_icon, "ic");
PROGMEM_STRING_DEF(mqtt_initial, "init");
PROGMEM_STRING_DEF(mqtt_target_humidity_command_topic, "hum_cmd_t");
PROGMEM_STRING_DEF(mqtt_target_humidity_command_template, "hum_cmd_tpl");
PROGMEM_STRING_DEF(mqtt_target_humidity_state_topic, "hum_stat_t");
PROGMEM_STRING_DEF(mqtt_target_humidity_state_template, "hum_stat_tpl");
PROGMEM_STRING_DEF(mqtt_json_attributes, "json_attr");
PROGMEM_STRING_DEF(mqtt_json_attributes_topic, "json_attr_t");
PROGMEM_STRING_DEF(mqtt_json_attributes_template, "json_attr_tpl");
PROGMEM_STRING_DEF(mqtt_max_mireds, "max_mirs");
PROGMEM_STRING_DEF(mqtt_min_mireds, "min_mirs");
PROGMEM_STRING_DEF(mqtt_max_temp, "max_temp");
PROGMEM_STRING_DEF(mqtt_min_temp, "min_temp");
PROGMEM_STRING_DEF(mqtt_max_humidity, "max_hum");
PROGMEM_STRING_DEF(mqtt_min_humidity, "min_hum");
PROGMEM_STRING_DEF(mqtt_mode_command_template, "mode_cmd_tpl");
PROGMEM_STRING_DEF(mqtt_mode_command_topic, "mode_cmd_t");
PROGMEM_STRING_DEF(mqtt_mode_state_template, "mode_stat_tpl");
PROGMEM_STRING_DEF(mqtt_mode_state_topic, "mode_stat_t");
PROGMEM_STRING_DEF(mqtt_modes, "modes");
PROGMEM_STRING_DEF(mqtt_name, "name");
PROGMEM_STRING_DEF(mqtt_object_id, "obj_id");
PROGMEM_STRING_DEF(mqtt_off_delay, "off_dly");
PROGMEM_STRING_DEF(mqtt_on_command_type, "on_cmd_type");
PROGMEM_STRING_DEF(mqtt_optimistic, "opt");
PROGMEM_STRING_DEF(mqtt_oscillation_command_topic, "osc_cmd_t");
PROGMEM_STRING_DEF(mqtt_oscillation_command_template, "osc_cmd_tpl");
PROGMEM_STRING_DEF(mqtt_oscillation_state_topic, "osc_stat_t");
PROGMEM_STRING_DEF(mqtt_oscillation_value_template, "osc_val_tpl");
PROGMEM_STRING_DEF(mqtt_percentage_command_topic, "pct_cmd_t");
PROGMEM_STRING_DEF(mqtt_percentage_command_template, "pct_cmd_tpl");
PROGMEM_STRING_DEF(mqtt_percentage_state_topic, "pct_stat_t");
PROGMEM_STRING_DEF(mqtt_percentage_value_template, "pct_val_tpl");
PROGMEM_STRING_DEF(mqtt_payload, "pl");
PROGMEM_STRING_DEF(mqtt_payload_arm_away, "pl_arm_away");
PROGMEM_STRING_DEF(mqtt_payload_arm_home, "pl_arm_home");
PROGMEM_STRING_DEF(mqtt_payload_arm_custom_bypass, "pl_arm_custom_b");
PROGMEM_STRING_DEF(mqtt_payload_arm_night, "pl_arm_nite");
PROGMEM_STRING_DEF(mqtt_payload_available, "pl_avail");
PROGMEM_STRING_DEF(mqtt_payload_clean_spot, "pl_cln_sp");
PROGMEM_STRING_DEF(mqtt_payload_close, "pl_cls");
PROGMEM_STRING_DEF(mqtt_payload_disarm, "pl_disarm");
PROGMEM_STRING_DEF(mqtt_payload_home, "pl_home");
PROGMEM_STRING_DEF(mqtt_payload_lock, "pl_lock");
PROGMEM_STRING_DEF(mqtt_payload_locate, "pl_loc");
PROGMEM_STRING_DEF(mqtt_payload_not_available, "pl_not_avail");
PROGMEM_STRING_DEF(mqtt_payload_not_home, "pl_not_home");
PROGMEM_STRING_DEF(mqtt_payload_off, "pl_off");
PROGMEM_STRING_DEF(mqtt_payload_on, "pl_on");
PROGMEM_STRING_DEF(mqtt_payload_open, "pl_open");
PROGMEM_STRING_DEF(mqtt_payload_oscillation_off, "pl_osc_off");
PROGMEM_STRING_DEF(mqtt_payload_oscillation_on, "pl_osc_on");
PROGMEM_STRING_DEF(mqtt_payload_pause, "pl_paus");
PROGMEM_STRING_DEF(mqtt_payload_stop, "pl_stop");
PROGMEM_STRING_DEF(mqtt_payload_start, "pl_strt");
PROGMEM_STRING_DEF(mqtt_payload_start_pause, "pl_stpa");
PROGMEM_STRING_DEF(mqtt_payload_return_to_base, "pl_ret");
PROGMEM_STRING_DEF(mqtt_payload_reset_humidity, "pl_rst_hum");
PROGMEM_STRING_DEF(mqtt_payload_reset_mode, "pl_rst_mode");
PROGMEM_STRING_DEF(mqtt_payload_reset_percentage, "pl_rst_pct");
PROGMEM_STRING_DEF(mqtt_payload_reset_preset_mode, "pl_rst_pr_mode");
PROGMEM_STRING_DEF(mqtt_payload_turn_off, "pl_toff");
PROGMEM_STRING_DEF(mqtt_payload_turn_on, "pl_ton");
PROGMEM_STRING_DEF(mqtt_payload_trigger, "pl_trig");
PROGMEM_STRING_DEF(mqtt_payload_unlock, "pl_unlk");
PROGMEM_STRING_DEF(mqtt_position_closed, "pos_clsd");
PROGMEM_STRING_DEF(mqtt_position_open, "pos_open");
PROGMEM_STRING_DEF(mqtt_power_command_topic, "pow_cmd_t");
PROGMEM_STRING_DEF(mqtt_power_state_topic, "pow_stat_t");
PROGMEM_STRING_DEF(mqtt_power_state_template, "pow_stat_tpl");
PROGMEM_STRING_DEF(mqtt_preset_mode_command_topic, "pr_mode_cmd_t");
PROGMEM_STRING_DEF(mqtt_preset_mode_command_template, "pr_mode_cmd_tpl");
PROGMEM_STRING_DEF(mqtt_preset_mode_state_topic, "pr_mode_stat_t");
PROGMEM_STRING_DEF(mqtt_preset_mode_value_template, "pr_mode_val_tpl");
PROGMEM_STRING_DEF(mqtt_preset_modes, "pr_modes");
PROGMEM_STRING_DEF(mqtt_red_template, "r_tpl");
PROGMEM_STRING_DEF(mqtt_retain, "ret");
PROGMEM_STRING_DEF(mqtt_rgb_command_template, "rgb_cmd_tpl");
PROGMEM_STRING_DEF(mqtt_rgb_command_topic, "rgb_cmd_t");
PROGMEM_STRING_DEF(mqtt_rgb_state_topic, "rgb_stat_t");
PROGMEM_STRING_DEF(mqtt_rgb_value_template, "rgb_val_tpl");
PROGMEM_STRING_DEF(mqtt_send_command_topic, "send_cmd_t");
PROGMEM_STRING_DEF(mqtt_send_if_off, "send_if_off");
PROGMEM_STRING_DEF(mqtt_set_fan_speed_topic, "set_fan_spd_t");
PROGMEM_STRING_DEF(mqtt_set_position_template, "set_pos_tpl");
PROGMEM_STRING_DEF(mqtt_set_position_topic, "set_pos_t");
PROGMEM_STRING_DEF(mqtt_position_topic, "pos_t");
PROGMEM_STRING_DEF(mqtt_position_template, "pos_tpl");
PROGMEM_STRING_DEF(mqtt_speed_range_min, "spd_rng_min");
PROGMEM_STRING_DEF(mqtt_speed_range_max, "spd_rng_max");
PROGMEM_STRING_DEF(mqtt_source_type, "src_type");
PROGMEM_STRING_DEF(mqtt_state_class, "stat_cla");
PROGMEM_STRING_DEF(mqtt_state_closed, "stat_clsd");
PROGMEM_STRING_DEF(mqtt_state_closing, "stat_closing");
PROGMEM_STRING_DEF(mqtt_state_off, "stat_off");
PROGMEM_STRING_DEF(mqtt_state_on, "stat_on");
PROGMEM_STRING_DEF(mqtt_state_open, "stat_open");
PROGMEM_STRING_DEF(mqtt_state_opening, "stat_opening");
PROGMEM_STRING_DEF(mqtt_state_stopped, "stat_stopped");
PROGMEM_STRING_DEF(mqtt_state_locked, "stat_locked");
PROGMEM_STRING_DEF(mqtt_state_unlocked, "stat_unlocked");
PROGMEM_STRING_DEF(mqtt_state_topic, "stat_t");
PROGMEM_STRING_DEF(mqtt_state_template, "stat_tpl");
PROGMEM_STRING_DEF(mqtt_state_value_template, "stat_val_tpl");
PROGMEM_STRING_DEF(mqtt_subtype, "stype");
PROGMEM_STRING_DEF(mqtt_supported_features, "sup_feat");
PROGMEM_STRING_DEF(mqtt_swing_mode_command_template, "swing_mode_cmd_tpl");
PROGMEM_STRING_DEF(mqtt_swing_mode_command_topic, "swing_mode_cmd_t");
PROGMEM_STRING_DEF(mqtt_swing_mode_state_template, "swing_mode_stat_tpl");
PROGMEM_STRING_DEF(mqtt_swing_mode_state_topic, "swing_mode_stat_t");
PROGMEM_STRING_DEF(mqtt_temperature_command_template, "temp_cmd_tpl");
PROGMEM_STRING_DEF(mqtt_temperature_command_topic, "temp_cmd_t");
PROGMEM_STRING_DEF(mqtt_temperature_high_command_template, "temp_hi_cmd_tpl");
PROGMEM_STRING_DEF(mqtt_temperature_high_command_topic, "temp_hi_cmd_t");
PROGMEM_STRING_DEF(mqtt_temperature_high_state_template, "temp_hi_stat_tpl");
PROGMEM_STRING_DEF(mqtt_temperature_high_state_topic, "temp_hi_stat_t");
PROGMEM_STRING_DEF(mqtt_temperature_low_command_template, "temp_lo_cmd_tpl");
PROGMEM_STRING_DEF(mqtt_temperature_low_command_topic, "temp_lo_cmd_t");
PROGMEM_STRING_DEF(mqtt_temperature_low_state_template, "temp_lo_stat_tpl");
PROGMEM_STRING_DEF(mqtt_temperature_low_state_topic, "temp_lo_stat_t");
PROGMEM_STRING_DEF(mqtt_temperature_state_template, "temp_stat_tpl");
PROGMEM_STRING_DEF(mqtt_temperature_state_topic, "temp_stat_t");
PROGMEM_STRING_DEF(mqtt_temperature_unit, "temp_unit");
PROGMEM_STRING_DEF(mqtt_tilt_closed_value, "tilt_clsd_val");
PROGMEM_STRING_DEF(mqtt_tilt_command_topic, "tilt_cmd_t");
PROGMEM_STRING_DEF(mqtt_tilt_command_template, "tilt_cmd_tpl");
PROGMEM_STRING_DEF(mqtt_tilt_invert_state, "tilt_inv_stat");
PROGMEM_STRING_DEF(mqtt_tilt_max, "tilt_max");
PROGMEM_STRING_DEF(mqtt_tilt_min, "tilt_min");
PROGMEM_STRING_DEF(mqtt_tilt_opened_value, "tilt_opnd_val");
PROGMEM_STRING_DEF(mqtt_tilt_optimistic, "tilt_opt");
PROGMEM_STRING_DEF(mqtt_tilt_status_topic, "tilt_status_t");
PROGMEM_STRING_DEF(mqtt_tilt_status_template, "tilt_status_tpl");
PROGMEM_STRING_DEF(mqtt_topic, "t");
PROGMEM_STRING_DEF(mqtt_unique_id, "uniq_id");
PROGMEM_STRING_DEF(mqtt_unit_of_measurement, "unit_of_meas");
PROGMEM_STRING_DEF(mqtt_value_template, "val_tpl");
PROGMEM_STRING_DEF(mqtt_white_value_command_topic, "whit_val_cmd_t");
PROGMEM_STRING_DEF(mqtt_white_value_scale, "whit_val_scl");
PROGMEM_STRING_DEF(mqtt_white_value_state_topic, "whit_val_stat_t");
PROGMEM_STRING_DEF(mqtt_white_value_template, "whit_val_tpl");
PROGMEM_STRING_DEF(mqtt_xy_command_topic, "xy_cmd_t");
PROGMEM_STRING_DEF(mqtt_xy_state_topic, "xy_stat_t");
PROGMEM_STRING_DEF(mqtt_xy_value_template, "xy_val_tpl");
#else
PROGMEM_STRING_DEF(mqtt_action_topic, "action_topic");
PROGMEM_STRING_DEF(mqtt_action_template, "action_template");
PROGMEM_STRING_DEF(mqtt_automation_type, "automation_type");
PROGMEM_STRING_DEF(mqtt_aux_command_topic, "aux_command_topic");
PROGMEM_STRING_DEF(mqtt_aux_state_template, "aux_state_template");
PROGMEM_STRING_DEF(mqtt_aux_state_topic, "aux_state_topic");
PROGMEM_STRING_DEF(mqtt_availability, "availability");
PROGMEM_STRING_DEF(mqtt_availability_mode, "availability_mode");
PROGMEM_STRING_DEF(mqtt_availability_topic, "availability_topic");
PROGMEM_STRING_DEF(mqtt_availability_template, "availability_template");
PROGMEM_STRING_DEF(mqtt_away_mode_command_topic, "away_mode_command_topic");
PROGMEM_STRING_DEF(mqtt_away_mode_state_template, "away_mode_state_template");
PROGMEM_STRING_DEF(mqtt_away_mode_state_topic, "away_mode_state_topic");
PROGMEM_STRING_DEF(mqtt_blue_template, "blue_template");
PROGMEM_STRING_DEF(mqtt_brightness_command_topic, "brightness_command_topic");
PROGMEM_STRING_DEF(mqtt_brightness_scale, "brightness_scale");
PROGMEM_STRING_DEF(mqtt_brightness_state_topic, "brightness_state_topic");
PROGMEM_STRING_DEF(mqtt_brightness_template, "brightness_template");
PROGMEM_STRING_DEF(mqtt_brightness_value_template, "brightness_value_template");
PROGMEM_STRING_DEF(mqtt_color_temp_command_template, "color_temp_command_template");
PROGMEM_STRING_DEF(mqtt_battery_level_topic, "battery_level_topic");
PROGMEM_STRING_DEF(mqtt_battery_level_template, "battery_level_template");
PROGMEM_STRING_DEF(mqtt_charging_topic, "charging_topic");
PROGMEM_STRING_DEF(mqtt_charging_template, "charging_template");
PROGMEM_STRING_DEF(mqtt_color_temp_command_topic, "color_temp_command_topic");
PROGMEM_STRING_DEF(mqtt_color_temp_state_topic, "color_temp_state_topic");
PROGMEM_STRING_DEF(mqtt_color_temp_template, "color_temp_template");
PROGMEM_STRING_DEF(mqtt_color_temp_value_template, "color_temp_value_template");
PROGMEM_STRING_DEF(mqtt_cleaning_topic, "cleaning_topic");
PROGMEM_STRING_DEF(mqtt_cleaning_template, "cleaning_template");
PROGMEM_STRING_DEF(mqtt_command_off_template, "command_off_template");
PROGMEM_STRING_DEF(mqtt_command_on_template, "command_on_template");
PROGMEM_STRING_DEF(mqtt_command_topic, "command_topic");
PROGMEM_STRING_DEF(mqtt_command_template, "command_template");
PROGMEM_STRING_DEF(mqtt_code_arm_required, "code_arm_required");
PROGMEM_STRING_DEF(mqtt_code_disarm_required, "code_disarm_required");
PROGMEM_STRING_DEF(mqtt_code_trigger_required, "code_trigger_required");
PROGMEM_STRING_DEF(mqtt_current_temperature_topic, "current_temperature_topic");
PROGMEM_STRING_DEF(mqtt_current_temperature_template, "current_temperature_template");
PROGMEM_STRING_DEF(mqtt_device, "device");
PROGMEM_STRING_DEF(mqtt_device_class, "device_class");
PROGMEM_STRING_DEF(mqtt_docked_topic, "docked_topic");
PROGMEM_STRING_DEF(mqtt_docked_template, "docked_template");
PROGMEM_STRING_DEF(mqtt_encoding, "encoding");
PROGMEM_STRING_DEF(mqtt_error_topic, "error_topic");
PROGMEM_STRING_DEF(mqtt_error_template, "error_template");
PROGMEM_STRING_DEF(mqtt_fan_speed_topic, "fan_speed_topic");
PROGMEM_STRING_DEF(mqtt_fan_speed_template, "fan_speed_template");
PROGMEM_STRING_DEF(mqtt_fan_speed_list, "fan_speed_list");
PROGMEM_STRING_DEF(mqtt_flash_time_long, "flash_time_long");
PROGMEM_STRING_DEF(mqtt_flash_time_short, "flash_time_short");
PROGMEM_STRING_DEF(mqtt_effect_command_topic, "effect_command_topic");
PROGMEM_STRING_DEF(mqtt_effect_list, "effect_list");
PROGMEM_STRING_DEF(mqtt_effect_state_topic, "effect_state_topic");
PROGMEM_STRING_DEF(mqtt_effect_template, "effect_template");
PROGMEM_STRING_DEF(mqtt_effect_value_template, "effect_value_template");
PROGMEM_STRING_DEF(mqtt_expire_after, "expire_after");
PROGMEM_STRING_DEF(mqtt_fan_mode_command_template, "fan_mode_command_template");
PROGMEM_STRING_DEF(mqtt_fan_mode_command_topic, "fan_mode_command_topic");
PROGMEM_STRING_DEF(mqtt_fan_mode_state_template, "fan_mode_state_template");
PROGMEM_STRING_DEF(mqtt_fan_mode_state_topic, "fan_mode_state_topic");
PROGMEM_STRING_DEF(mqtt_force_update, "force_update");
PROGMEM_STRING_DEF(mqtt_green_template, "green_template");
PROGMEM_STRING_DEF(mqtt_hold_command_template, "hold_command_template");
PROGMEM_STRING_DEF(mqtt_hold_command_topic, "hold_command_topic");
PROGMEM_STRING_DEF(mqtt_hold_state_template, "hold_state_template");
PROGMEM_STRING_DEF(mqtt_hold_state_topic, "hold_state_topic");
PROGMEM_STRING_DEF(mqtt_hs_command_topic, "hs_command_topic");
PROGMEM_STRING_DEF(mqtt_hs_state_topic, "hs_state_topic");
PROGMEM_STRING_DEF(mqtt_hs_value_template, "hs_value_template");
PROGMEM_STRING_DEF(mqtt_icon, "icon");
PROGMEM_STRING_DEF(mqtt_initial, "initial");
PROGMEM_STRING_DEF(mqtt_target_humidity_command_topic, "target_humidity_command_topic");
PROGMEM_STRING_DEF(mqtt_target_humidity_command_template, "target_humidity_command_template");
PROGMEM_STRING_DEF(mqtt_target_humidity_state_topic, "target_humidity_state_topic");
PROGMEM_STRING_DEF(mqtt_target_humidity_state_template, "target_humidity_state_template");
PROGMEM_STRING_DEF(mqtt_json_attributes, "json_attributes");
PROGMEM_STRING_DEF(mqtt_json_attributes_topic, "json_attributes_topic");
PROGMEM_STRING_DEF(mqtt_json_attributes_template, "json_attributes_template");
PROGMEM_STRING_DEF(mqtt_max_mireds, "max_mireds");
PROGMEM_STRING_DEF(mqtt_min_mireds, "min_mireds");
PROGMEM_STRING_DEF(mqtt_max_temp, "max_temp");
PROGMEM_STRING_DEF(mqtt_min_temp, "min_temp");
PROGMEM_STRING_DEF(mqtt_max_humidity, "max_humidity");
PROGMEM_STRING_DEF(mqtt_min_humidity, "min_humidity");
PROGMEM_STRING_DEF(mqtt_mode_command_template, "mode_command_template");
PROGMEM_STRING_DEF(mqtt_mode_command_topic, "mode_command_topic");
PROGMEM_STRING_DEF(mqtt_mode_state_template, "mode_state_template");
PROGMEM_STRING_DEF(mqtt_mode_state_topic, "mode_state_topic");
PROGMEM_STRING_DEF(mqtt_modes, "modes");
PROGMEM_STRING_DEF(mqtt_name, "name");
PROGMEM_STRING_DEF(mqtt_object_id, "object_id");
PROGMEM_STRING_DEF(mqtt_off_delay, "off_delay");
PROGMEM_STRING_DEF(mqtt_on_command_type, "on_command_type");
PROGMEM_STRING_DEF(mqtt_optimistic, "optimistic");
PROGMEM_STRING_DEF(mqtt_oscillation_command_topic, "oscillation_command_topic");
PROGMEM_STRING_DEF(mqtt_oscillation_command_template, "oscillation_command_template");
PROGMEM_STRING_DEF(mqtt_oscillation_state_topic, "oscillation_state_topic");
PROGMEM_STRING_DEF(mqtt_oscillation_value_template, "oscillation_value_template");
PROGMEM_STRING_DEF(mqtt_percentage_command_topic, "percentage_command_topic");
PROGMEM_STRING_DEF(mqtt_percentage_command_template, "percentage_command_template");
PROGMEM_STRING_DEF(mqtt_percentage_state_topic, "percentage_state_topic");
PROGMEM_STRING_DEF(mqtt_percentage_value_template, "percentage_value_template");
PROGMEM_STRING_DEF(mqtt_payload, "payload");
PROGMEM_STRING_DEF(mqtt_payload_arm_away, "payload_arm_away");
PROGMEM_STRING_DEF(mqtt_payload_arm_home, "payload_arm_home");
PROGMEM_STRING_DEF(mqtt_payload_arm_custom_bypass, "payload_arm_custom_bypass");
PROGMEM_STRING_DEF(mqtt_payload_arm_night, "payload_arm_night");
PROGMEM_STRING_DEF(mqtt_payload_available, "payload_available");
PROGMEM_STRING_DEF(mqtt_payload_clean_spot, "payload_clean_spot");
PROGMEM_STRING_DEF(mqtt_payload_close, "payload_close");
PROGMEM_STRING_DEF(mqtt_payload_disarm, "payload_disarm");
PROGMEM_STRING_DEF(mqtt_payload_home, "payload_home");
PROGMEM_STRING_DEF(mqtt_payload_lock, "payload_lock");
PROGMEM_STRING_DEF(mqtt_payload_locate, "payload_locate");
PROGMEM_STRING_DEF(mqtt_payload_not_available, "payload_not_available");
PROGMEM_STRING_DEF(mqtt_payload_not_home, "payload_not_home");
PROGMEM_STRING_DEF(mqtt_payload_off, "payload_off");
PROGMEM_STRING_DEF(mqtt_payload_on, "payload_on");
PROGMEM_STRING_DEF(mqtt_payload_open, "payload_open");
PROGMEM_STRING_DEF(mqtt_payload_oscillation_off, "payload_oscillation_off");
PROGMEM_STRING_DEF(mqtt_payload_oscillation_on, "payload_oscillation_on");
PROGMEM_STRING_DEF(mqtt_payload_pause, "payload_pause");
PROGMEM_STRING_DEF(mqtt_payload_stop, "payload_stop");
PROGMEM_STRING_DEF(mqtt_payload_start, "payload_start");
PROGMEM_STRING_DEF(mqtt_payload_start_pause, "payload_start_pause");
PROGMEM_STRING_DEF(mqtt_payload_return_to_base, "payload_return_to_base");
PROGMEM_STRING_DEF(mqtt_payload_reset_humidity, "payload_reset_humidity");
PROGMEM_STRING_DEF(mqtt_payload_reset_mode, "payload_reset_mode");
PROGMEM_STRING_DEF(mqtt_payload_reset_percentage, "payload_reset_percentage");
PROGMEM_STRING_DEF(mqtt_payload_reset_preset_mode, "payload_reset_preset_mode");
PROGMEM_STRING_DEF(mqtt_payload_turn_off, "payload_turn_off");
PROGMEM_STRING_DEF(mqtt_payload_turn_on, "payload_turn_on");
PROGMEM_STRING_DEF(mqtt_payload_trigger, "payload_trigger");
PROGMEM_STRING_DEF(mqtt_payload_unlock, "payload_unlock");
PROGMEM_STRING_DEF(mqtt_position_closed, "position_closed");
PROGMEM_STRING_DEF(mqtt_position_open, "position_open");
PROGMEM_STRING_DEF(mqtt_power_command_topic, "power_command_topic");
PROGMEM_STRING_DEF(mqtt_power_state_topic, "power_state_topic");
PROGMEM_STRING_DEF(mqtt_power_state_template, "power_state_template");
PROGMEM_STRING_DEF(mqtt_preset_mode_command_topic, "preset_mode_command_topic");
PROGMEM_STRING_DEF(mqtt_preset_mode_command_template, "preset_mode_command_template");
PROGMEM_STRING_DEF(mqtt_preset_mode_state_topic, "preset_mode_state_topic");
PROGMEM_STRING_DEF(mqtt_preset_mode_value_template, "preset_mode_value_template");
PROGMEM_STRING_DEF(mqtt_preset_modes, "preset_modes");
PROGMEM_STRING_DEF(mqtt_red_template, "red_template");
PROGMEM_STRING_DEF(mqtt_retain, "retain");
PROGMEM_STRING_DEF(mqtt_rgb_command_template, "rgb_command_template");
PROGMEM_STRING_DEF(mqtt_rgb_command_topic, "rgb_command_topic");
PROGMEM_STRING_DEF(mqtt_rgb_state_topic, "rgb_state_topic");
PROGMEM_STRING_DEF(mqtt_rgb_value_template, "rgb_value_template");
PROGMEM_STRING_DEF(mqtt_send_command_topic, "send_command_topic");
PROGMEM_STRING_DEF(mqtt_send_if_off, "send_if_off");
PROGMEM_STRING_DEF(mqtt_set_fan_speed_topic, "set_fan_speed_topic");
PROGMEM_STRING_DEF(mqtt_set_position_template, "set_position_template");
PROGMEM_STRING_DEF(mqtt_set_position_topic, "set_position_topic");
PROGMEM_STRING_DEF(mqtt_position_topic, "position_topic");
PROGMEM_STRING_DEF(mqtt_position_template, "position_template");
PROGMEM_STRING_DEF(mqtt_speed_range_min, "speed_range_min");
PROGMEM_STRING_DEF(mqtt_speed_range_max, "speed_range_max");
PROGMEM_STRING_DEF(mqtt_source_type, "source_type");
PROGMEM_STRING_DEF(mqtt_state_class, "state_class");
PROGMEM_STRING_DEF(mqtt_state_closed, "state_closed");
PROGMEM_STRING_DEF(mqtt_state_closing, "state_closing");
PROGMEM_STRING_DEF(mqtt_state_off, "state_off");
PROGMEM_STRING_DEF(mqtt_state_on, "state_on");
PROGMEM_STRING_DEF(mqtt_state_open, "state_open");
PROGMEM_STRING_DEF(mqtt_state_opening, "state_opening");
PROGMEM_STRING_DEF(mqtt_state_stopped, "state_stopped");
PROGMEM_STRING_DEF(mqtt_state_locked, "state_locked");
PROGMEM_STRING_DEF(mqtt_state_unlocked, "state_unlocked");
PROGMEM_STRING_DEF(mqtt_state_topic, "state_topic");
PROGMEM_STRING_DEF(mqtt_state_template, "state_template");
PROGMEM_STRING_DEF(mqtt_state_value_template, "state_value_template");
PROGMEM_STRING_DEF(mqtt_subtype, "subtype");
PROGMEM_STRING_DEF(mqtt_supported_features, "supported_features");
PROGMEM_STRING_DEF(mqtt_swing_mode_command_template, "swing_mode_command_template");
PROGMEM_STRING_DEF(mqtt_swing_mode_command_topic, "swing_mode_command_topic");
PROGMEM_STRING_DEF(mqtt_swing_mode_state_template, "swing_mode_state_template");
PROGMEM_STRING_DEF(mqtt_swing_mode_state_topic, "swing_mode_state_topic");
PROGMEM_STRING_DEF(mqtt_temperature_command_template, "temperature_command_template");
PROGMEM_STRING_DEF(mqtt_temperature_command_topic, "temperature_command_topic");
PROGMEM_STRING_DEF(mqtt_temperature_high_command_template, "temperature_high_command_template");
PROGMEM_STRING_DEF(mqtt_temperature_high_command_topic, "temperature_high_command_topic");
PROGMEM_STRING_DEF(mqtt_temperature_high_state_template, "temperature_high_state_template");
PROGMEM_STRING_DEF(mqtt_temperature_high_state_topic, "temperature_high_state_topic");
PROGMEM_STRING_DEF(mqtt_temperature_low_command_template, "temperature_low_command_template");
PROGMEM_STRING_DEF(mqtt_temperature_low_command_topic, "temperature_low_command_topic");
PROGMEM_STRING_DEF(mqtt_temperature_low_state_template, "temperature_low_state_template");
PROGMEM_STRING_DEF(mqtt_temperature_low_state_topic, "temperature_low_state_topic");
PROGMEM_STRING_DEF(mqtt_temperature_state_template, "temperature_state_template");
PROGMEM_STRING_DEF(mqtt_temperature_state_topic, "temperature_state_topic");
PROGMEM_STRING_DEF(mqtt_temperature_unit, "temperature_unit");
PROGMEM_STRING_DEF(mqtt_tilt_closed_value, "tilt_closed_value");
PROGMEM_STRING_DEF(mqtt_tilt_command_topic, "tilt_command_topic");
PROGMEM_STRING_DEF(mqtt_tilt_command_template, "tilt_command_template");
PROGMEM_STRING_DEF(mqtt_tilt_invert_state, "tilt_invert_state");
PROGMEM_STRING_DEF(mqtt_tilt_max, "tilt_max");
PROGMEM_STRING_DEF(mqtt_tilt_min, "tilt_min");
PROGMEM_STRING_DEF(mqtt_tilt_opened_value, "tilt_opened_value");
PROGMEM_STRING_DEF(mqtt_tilt_optimistic, "tilt_optimistic");
PROGMEM_STRING_DEF(mqtt_tilt_status_topic, "tilt_status_topic");
PROGMEM_STRING_DEF(mqtt_tilt_status_template, "tilt_status_template");
PROGMEM_STRING_DEF(mqtt_topic, "topic");
PROGMEM_STRING_DEF(mqtt_unique_id, "unique_id");
PROGMEM_STRING_DEF(mqtt_unit_of_measurement, "unit_of_measurement");
PROGMEM_STRING_DEF(mqtt_value_template, "value_template");
PROGMEM_STRING_DEF(mqtt_white_value_command_topic, "white_value_command_topic");
PROGMEM_STRING_DEF(mqtt_white_value_scale, "white_value_scale");
PROGMEM_STRING_DEF(mqtt_white_value_state_topic, "white_value_state_topic");
PROGMEM_STRING_DEF(mqtt_white_value_template, "white_value_template");
PROGMEM_STRING_DEF(mqtt_xy_command_topic, "xy_command_topic");
PROGMEM_STRING_DEF(mqtt_xy_state_topic, "xy_state_topic");
PROGMEM_STRING_DEF(mqtt_xy_value_template, "xy_value_template");
#endif
