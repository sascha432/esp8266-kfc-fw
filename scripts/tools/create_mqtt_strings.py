#!/usr/bin/env python3
#
# Author: sascha_lammers@gmx.de
#
#
# strings from "Supported abbreviations"
#
# https://www.home-assistant.io/integrations/mqtt/#supported-abbreviations-in-mqtt-discovery-messages

from os import path
import argparse
import os
import re
import sys
import urllib.request

project_dir = '../..'
mqtt_plugin_dir = path.abspath(path.join(path.dirname(__file__), project_dir, 'src/plugins/mqtt'))
mqtt_plugin_client_h = path.abspath(path.join(mqtt_plugin_dir, 'mqtt_client.h'))

# https://www.home-assistant.io/integrations/mqtt/#supported-abbreviations-in-mqtt-discovery-messages
abbreviations_url = 'https://raw.githubusercontent.com/home-assistant/home-assistant.io/current/source/_integrations/mqtt.markdown'
abbreviations_section = '{% details "Supported abbreviations" %}'

# options that are used by the firmware but are no longer listed as abbreviations
# upstream. an identity mapping emits the full option name in both modes, which is
# what Home Assistant expects for options without an abbreviation
additional_strings = {
    'color_mode': 'color_mode',
    'object_id': 'object_id',
}


def fetch_abbreviations(url):
    print('Downloading %s' % url)
    request = urllib.request.Request(url, headers={'User-Agent': 'create_mqtt_strings.py'})
    with urllib.request.urlopen(request, timeout=60) as response:
        return response.read().decode('utf-8')


def parse_abbreviations(markdown):
    start = markdown.find(abbreviations_section)
    if start < 0:
        raise RuntimeError('Section not found: %s' % abbreviations_section)

    # the first 'txt' code block after the section marker is the table of abbreviations
    start = markdown.find('```txt', start)
    end = markdown.find('```', start + 6) if start >= 0 else -1
    if start < 0 or end < 0:
        raise RuntimeError('Code block not found after: %s' % abbreviations_section)

    strings = dict(re.findall(r"'(?P<key>[^']+)'\s*:\s*'(?P<value>[^']+)'", markdown[start + 6:end]))
    if len(strings) < 100:
        raise RuntimeError('Only %u abbreviations found, expected at least 100' % len(strings))

    # the generated identifier is based on the expanded string, duplicates would not compile
    values = list(strings.values())
    duplicates = sorted(set(val for val in values if values.count(val) > 1))
    if duplicates:
        raise RuntimeError('Duplicate strings: %s' % ', '.join(duplicates))

    for key, val in additional_strings.items():
        if val in strings.values():
            raise RuntimeError('Additional string already exists: %s' % key)
        strings[key] = val

    print('Parsed %u abbreviations' % len(strings))
    return dict(sorted(strings.items()))


if not path.exists(mqtt_plugin_dir):
    print('No such file or directory: %s' % mqtt_plugin_dir)
    sys.exit(-1)

if not path.exists(mqtt_plugin_client_h):
    print('No such file or directory: %s' % mqtt_plugin_client_h)
    sys.exit(-1)

parser = argparse.ArgumentParser(description='Generate mqtt_strings.h/cpp from the Home Assistant MQTT discovery abbreviations')
parser.add_argument('--url', default=abbreviations_url, help='URL of the mqtt.markdown file')
parser.add_argument('--file', help='read mqtt.markdown from a local file instead of downloading it')
args = parser.parse_args()

if args.file:
    with open(args.file, 'r', encoding='utf-8') as f:
        markdown = f.read()
else:
    markdown = fetch_abbreviations(args.url)

strings = parse_abbreviations(markdown)

header = '// AUTOMATICALLY GENERATED FILE. DO NOT MODIFY\n// GENERATOR: ./scripts/tools/create_mqtt_strings.py\n//\n'

with open(path.abspath(path.join(mqtt_plugin_dir, 'mqtt_strings.h')), 'w', newline = '\n') as f:

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
    f.write('PROGMEM_STRING_DECL(mqtt_component_fan);\n')
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
    f.write('PROGMEM_STRING_DECL(mqtt_friendly_name);\n')
    for key, val in strings.items():
        f.write('PROGMEM_STRING_DECL(mqtt_%s);\n' % val)

with open(path.abspath(path.join(mqtt_plugin_dir, 'mqtt_strings.cpp')), 'w') as f:

    f.write(header)
    f.write('#include "mqtt_strings.h"\n')
    f.write('PROGMEM_STRING_DEF(mqtt_component_switch, "switch");\n')
    f.write('PROGMEM_STRING_DEF(mqtt_component_light, "light");\n')
    f.write('PROGMEM_STRING_DEF(mqtt_component_sensor, "sensor");\n')
    f.write('PROGMEM_STRING_DEF(mqtt_component_binary_sensor, "binary_sensor");\n')
    f.write('PROGMEM_STRING_DEF(mqtt_component_fan, "fan");\n')
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
    f.write('PROGMEM_STRING_DEF(mqtt_friendly_name, "friendly_name");\n')
    f.write('#if MQTT_AUTO_DISCOVERY_USE_ABBREVIATIONS\n')

    for key, val in strings.items():
        f.write('PROGMEM_STRING_DEF(mqtt_%s, \"%s\");\n' % (val, key))

    f.write('#else\n')

    for key, val in strings.items():
        f.write('PROGMEM_STRING_DEF(mqtt_%s, \"%s\");\n' % (val, val))

    f.write('#endif\n')
