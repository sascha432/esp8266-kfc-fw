#!/usr/bin/env python3
#
# Author: sascha_lammers@gmx.de
#
#
# strings from
#
# https://www.home-assistant.io/docs/mqtt/discovery/

from os import path
import os
import sys

project_dir = '../..'
mqtt_plugin_dir = path.abspath(path.join(path.dirname(__file__), project_dir, 'src/plugins/mqtt'))
mqtt_plugin_client_h = path.abspath(path.join(mqtt_plugin_dir, 'mqtt_client.h'))

if not path.exists(mqtt_plugin_dir):
    print('No such file or directory: %s' % mqtt_plugin_dir)
    sys.exit(-1)

if not path.exists(mqtt_plugin_client_h):
    print('No such file or directory: %s' % mqtt_plugin_client_h)
    sys.exit(-1)

strings = {
    'act_t':               'action_topic',
    'act_tpl':             'action_template',
    'atype':               'automation_type',
    'aux_cmd_t':           'aux_command_topic',
    'aux_stat_tpl':        'aux_state_template',
    'aux_stat_t':          'aux_state_topic',
    'avty':                'availability',
    'avty_mode':           'availability_mode',
    'avty_t':              'availability_topic',
    'away_mode_cmd_t':     'away_mode_command_topic',
    'away_mode_stat_tpl':  'away_mode_state_template',
    'away_mode_stat_t':    'away_mode_state_topic',
    'b_tpl':               'blue_template',
    'bri_cmd_t':           'brightness_command_topic',
    'bri_scl':             'brightness_scale',
    'bri_stat_t':          'brightness_state_topic',
    'bri_tpl':             'brightness_template',
    'bri_val_tpl':         'brightness_value_template',
    'clr_temp_cmd_tpl':    'color_temp_command_template',
    'bat_lev_t':           'battery_level_topic',
    'bat_lev_tpl':         'battery_level_template',
    'chrg_t':              'charging_topic',
    'chrg_tpl':            'charging_template',
    'clr_temp_cmd_t':      'color_temp_command_topic',
    'clr_temp_stat_t':     'color_temp_state_topic',
    'clr_temp_tpl':        'color_temp_template',
    'clr_temp_val_tpl':    'color_temp_value_template',
    'cln_t':               'cleaning_topic',
    'cln_tpl':             'cleaning_template',
    'cmd_off_tpl':         'command_off_template',
    'cmd_on_tpl':          'command_on_template',
    'cmd_t':               'command_topic',
    'cmd_tpl':             'command_template',
    'cod_arm_req':         'code_arm_required',
    'cod_dis_req':         'code_disarm_required',
    'curr_temp_t':         'current_temperature_topic',
    'curr_temp_tpl':       'current_temperature_template',
    'dev':                 'device',
    'dev_cla':             'device_class',
    'dock_t':              'docked_topic',
    'dock_tpl':            'docked_template',
    'err_t':               'error_topic',
    'err_tpl':             'error_template',
    'fanspd_t':            'fan_speed_topic',
    'fanspd_tpl':          'fan_speed_template',
    'fanspd_lst':          'fan_speed_list',
    'flsh_tlng':           'flash_time_long',
    'flsh_tsht':           'flash_time_short',
    'fx_cmd_t':            'effect_command_topic',
    'fx_list':             'effect_list',
    'fx_stat_t':           'effect_state_topic',
    'fx_tpl':              'effect_template',
    'fx_val_tpl':          'effect_value_template',
    'exp_aft':             'expire_after',
    'fan_mode_cmd_tpl':    'fan_mode_command_template',
    'fan_mode_cmd_t':      'fan_mode_command_topic',
    'fan_mode_stat_tpl':   'fan_mode_state_template',
    'fan_mode_stat_t':     'fan_mode_state_topic',
    'frc_upd':             'force_update',
    'g_tpl':               'green_template',
    'hold_cmd_tpl':        'hold_command_template',
    'hold_cmd_t':          'hold_command_topic',
    'hold_stat_tpl':       'hold_state_template',
    'hold_stat_t':         'hold_state_topic',
    'hs_cmd_t':            'hs_command_topic',
    'hs_stat_t':           'hs_state_topic',
    'hs_val_tpl':          'hs_value_template',
    'ic':                  'icon',
    'init':                'initial',
    'json_attr':           'json_attributes',
    'json_attr_t':         'json_attributes_topic',
    'json_attr_tpl':       'json_attributes_template',
    'max_mirs':            'max_mireds',
    'min_mirs':            'min_mireds',
    'max_temp':            'max_temp',
    'min_temp':            'min_temp',
    'mode_cmd_tpl':        'mode_command_template',
    'mode_cmd_t':          'mode_command_topic',
    'mode_stat_tpl':       'mode_state_template',
    'mode_stat_t':         'mode_state_topic',
    'name':                'name',
    'off_dly':             'off_delay',
    'on_cmd_type':         'on_command_type',
    'opt':                 'optimistic',
    'osc_cmd_t':           'oscillation_command_topic',
    'osc_cmd_tpl':         'oscillation_command_template',
    'osc_stat_t':          'oscillation_state_topic',
    'osc_val_tpl':         'oscillation_value_template',
    'pct_cmd_t':           'percentage_command_topic',
    'pct_cmd_tpl':         'percentage_command_template',
    'pct_stat_t':          'percentage_state_topic',
    'pct_val_tpl':         'percentage_value_template',
    'pl':                  'payload',
    'pl_arm_away':         'payload_arm_away',
    'pl_arm_home':         'payload_arm_home',
    'pl_arm_custom_b':     'payload_arm_custom_bypass',
    'pl_arm_nite':         'payload_arm_night',
    'pl_avail':            'payload_available',
    'pl_cln_sp':           'payload_clean_spot',
    'pl_cls':              'payload_close',
    'pl_disarm':           'payload_disarm',
    'pl_home':             'payload_home',
    'pl_lock':             'payload_lock',
    'pl_loc':              'payload_locate',
    'pl_not_avail':        'payload_not_available',
    'pl_not_home':         'payload_not_home',
    'pl_off':              'payload_off',
    'pl_on':               'payload_on',
    'pl_open':             'payload_open',
    'pl_osc_off':          'payload_oscillation_off',
    'pl_osc_on':           'payload_oscillation_on',
    'pl_paus':             'payload_pause',
    'pl_stop':             'payload_stop',
    'pl_strt':             'payload_start',
    'pl_stpa':             'payload_start_pause',
    'pl_ret':              'payload_return_to_base',
    'pl_toff':             'payload_turn_off',
    'pl_ton':              'payload_turn_on',
    'pl_unlk':             'payload_unlock',
    'pos_clsd':            'position_closed',
    'pos_open':            'position_open',
    'pow_cmd_t':           'power_command_topic',
    'pow_stat_t':          'power_state_topic',
    'pow_stat_tpl':        'power_state_template',
    'pr_mode_cmd_t':       'preset_mode_command_topic',
    'pr_mode_cmd_tpl':     'preset_mode_command_template',
    'pr_mode_stat_t':      'preset_mode_state_topic',
    'pr_mode_val_tpl':     'preset_mode_value_template',
    'pr_modes':            'preset_modes',
    'r_tpl':               'red_template',
    'ret':                 'retain',
    'rgb_cmd_tpl':         'rgb_command_template',
    'rgb_cmd_t':           'rgb_command_topic',
    'rgb_stat_t':          'rgb_state_topic',
    'rgb_val_tpl':         'rgb_value_template',
    'send_cmd_t':          'send_command_topic',
    'send_if_off':         'send_if_off',
    'set_fan_spd_t':       'set_fan_speed_topic',
    'set_pos_tpl':         'set_position_template',
    'set_pos_t':           'set_position_topic',
    'pos_t':               'position_topic',
    'pos_tpl':             'position_template',
    'spd_rng_min':         'speed_range_min',
    'spd_rng_max':         'speed_range_max',
    'src_type':            'source_type',
    'stat_clsd':           'state_closed',
    'stat_closing':        'state_closing',
    'stat_off':            'state_off',
    'stat_on':             'state_on',
    'stat_open':           'state_open',
    'stat_opening':        'state_opening',
    'stat_stopped':        'state_stopped',
    'stat_locked':         'state_locked',
    'stat_unlocked':       'state_unlocked',
    'stat_t':              'state_topic',
    'stat_tpl':            'state_template',
    'stat_val_tpl':        'state_value_template',
    'stype':               'subtype',
    'sup_feat':            'supported_features',
    'swing_mode_cmd_tpl':  'swing_mode_command_template',
    'swing_mode_cmd_t':    'swing_mode_command_topic',
    'swing_mode_stat_tpl': 'swing_mode_state_template',
    'swing_mode_stat_t':   'swing_mode_state_topic',
    'temp_cmd_tpl':        'temperature_command_template',
    'temp_cmd_t':          'temperature_command_topic',
    'temp_hi_cmd_tpl':     'temperature_high_command_template',
    'temp_hi_cmd_t':       'temperature_high_command_topic',
    'temp_hi_stat_tpl':    'temperature_high_state_template',
    'temp_hi_stat_t':      'temperature_high_state_topic',
    'temp_lo_cmd_tpl':     'temperature_low_command_template',
    'temp_lo_cmd_t':       'temperature_low_command_topic',
    'temp_lo_stat_tpl':    'temperature_low_state_template',
    'temp_lo_stat_t':      'temperature_low_state_topic',
    'temp_stat_tpl':       'temperature_state_template',
    'temp_stat_t':         'temperature_state_topic',
    'temp_unit':           'temperature_unit',
    'tilt_clsd_val':       'tilt_closed_value',
    'tilt_cmd_t':          'tilt_command_topic',
    'tilt_cmd_tpl':        'tilt_command_template',
    'tilt_inv_stat':       'tilt_invert_state',
    'tilt_max':            'tilt_max',
    'tilt_min':            'tilt_min',
    'tilt_opnd_val':       'tilt_opened_value',
    'tilt_opt':            'tilt_optimistic',
    'tilt_status_t':       'tilt_status_topic',
    'tilt_status_tpl':     'tilt_status_template',
    't':                   'topic',
    'uniq_id':             'unique_id',
    'unit_of_meas':        'unit_of_measurement',
    'val_tpl':             'value_template',
    'whit_val_cmd_t':      'white_value_command_topic',
    'whit_val_scl':        'white_value_scale',
    'whit_val_stat_t':     'white_value_state_topic',
    'whit_val_tpl':        'white_value_template',
    'xy_cmd_t':            'xy_command_topic',
    'xy_stat_t':           'xy_state_topic',
    'xy_val_tpl':          'xy_value_template',
}

header = '// AUTOMATICALLY GENERATED FILE. DO NOT MODIFY\n// GENERATOR: ./scripts/tools/create_mqtt_strings.py\n//\n'

with open(path.abspath(path.join(mqtt_plugin_dir, 'mqtt_strings.h')), 'w') as f:

    f.write(header)
    f.write('#pragma once\n')
    f.write('#include <Arduino_compat.h>\n')
    f.write('// use abbreviations to reduce the size of the auto discovery\n')
    f.write('#ifndef MQTT_AUTO_DISCOVERY_USE_ABBREVIATIONS\n')
    f.write('#define MQTT_AUTO_DISCOVERY_USE_ABBREVIATIONS 1\n')
    f.write('#endif\n')
    f.write('PROGMEM_STRING_DECL(mqtt_component_switch);\n')
    f.write('PROGMEM_STRING_DECL(mqtt_component_light);\n')
    f.write('PROGMEM_STRING_DECL(mqtt_component_sensor);\n')
    f.write('PROGMEM_STRING_DECL(mqtt_component_binary_sensor);\n')
    f.write('PROGMEM_STRING_DECL(mqtt_component_storage);\n')
    f.write('PROGMEM_STRING_DECL(mqtt_status_topic);\n');
    f.write('PROGMEM_STRING_DECL(mqtt_status_topic_online);\n')
    f.write('PROGMEM_STRING_DECL(mqtt_status_topic_offline);\n')
    f.write('PROGMEM_STRING_DECL(mqtt_bool_on);\n')
    f.write('PROGMEM_STRING_DECL(mqtt_bool_off);\n')
    f.write('PROGMEM_STRING_DECL(mqtt_bool_true);\n')
    f.write('PROGMEM_STRING_DECL(mqtt_bool_false);\n')
    f.write('PROGMEM_STRING_DECL(mqtt_schema);\n')
    f.write('PROGMEM_STRING_DECL(mqtt_trigger);\n')
    f.write('PROGMEM_STRING_DECL(mqtt_schema_json);\n')
    f.write('PROGMEM_STRING_DECL(mqtt_type);\n')
    f.write('PROGMEM_STRING_DECL(mqtt_component_device_automation);\n')
    for key, val in strings.items():
        f.write('PROGMEM_STRING_DECL(mqtt_%s);\n' % val)

with open(path.abspath(path.join(mqtt_plugin_dir, 'mqtt_strings.cpp')), 'w') as f:

    f.write(header)
    f.write('#include "mqtt_strings.h"\n')
    f.write('PROGMEM_STRING_DEF(mqtt_component_switch, "switch");\n')
    f.write('PROGMEM_STRING_DEF(mqtt_component_light, "light");\n')
    f.write('PROGMEM_STRING_DEF(mqtt_component_sensor, "sensor");\n')
    f.write('PROGMEM_STRING_DEF(mqtt_component_binary_sensor, "binary_sensor");\n')
    f.write('PROGMEM_STRING_DEF(mqtt_component_storage, "storage");\n')
    f.write('PROGMEM_STRING_DEF(mqtt_status_topic, "/status");\n')
    f.write('PROGMEM_STRING_DEF(mqtt_status_topic_online, "online");\n')
    f.write('PROGMEM_STRING_DEF(mqtt_status_topic_offline, "offline");\n')
    f.write('PROGMEM_STRING_DEF(mqtt_bool_on, "ON");\n')
    f.write('PROGMEM_STRING_DEF(mqtt_bool_off, "OFF");\n')
    f.write('PROGMEM_STRING_DEF(mqtt_bool_true, "true");\n')
    f.write('PROGMEM_STRING_DEF(mqtt_bool_false, "false");\n')
    f.write('PROGMEM_STRING_DEF(mqtt_schema, "schema");\n')
    f.write('PROGMEM_STRING_DEF(mqtt_trigger, "trigger");\n')
    f.write('PROGMEM_STRING_DEF(mqtt_schema_json, "json");\n')
    f.write('PROGMEM_STRING_DEF(mqtt_type, "type");\n')
    f.write('PROGMEM_STRING_DEF(mqtt_component_device_automation, "device_automation");\n')
    f.write('#if MQTT_AUTO_DISCOVERY_USE_ABBREVIATIONS\n')

    for key, val in strings.items():
        f.write('PROGMEM_STRING_DEF(mqtt_%s, \"%s\");\n' % (val, key))

    f.write('#else\n')

    for key, val in strings.items():
        f.write('PROGMEM_STRING_DEF(mqtt_%s, \"%s\");\n' % (val, val))

    f.write('#endif\n')
