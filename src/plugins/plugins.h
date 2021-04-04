/**
 * Author: sascha_lammers@gmx.de
 */

// conditional includes for
//
// - for all plugin headers
// - PinMonitor
// - WebAlerts

#pragma once

#if IOT_ALARM_PLUGIN_ENABLED
#include "../src/plugins/alarm/alarm.h"
#endif
#if HTTP2SERIAL_SUPPORT
#include "../src/plugins/http2serial/http2serial.h"
#endif
#if MQTT_SUPPORT
#include "../src/plugins/mqtt/component.h"
#include "../src/plugins/mqtt/mqtt_strings.h"
#include "../src/plugins/mqtt/mqtt_client.h"
#endif
#if IOT_SENSOR
#include "../src/plugins/sensor/sensor.h"
#endif
#if IOT_REMOTE_CONTROL
#include "../src/plugins/remote/remote.h"
#endif
#if IOT_CLOCK
#include "../src/plugins/clock/clock.h"
#endif
#if STK500V1
#include "../src/plugins/stk500v1/STK500v1Programmer.h"
#endif
#if NTP_CLIENT
#include "../src/plugins/ntp/ntp_plugin.h"
#endif
#if MDNS_PLUGIN
#include <ESP8266SSDP.h>
#include "../src/plugins/mdns/mdns_plugin.h"
#include "../src/plugins/mdns/mdns_resolver.h"
#include "../src/plugins/mdns/mdns_sd.h"
#endif
#if IOT_DIMMER_MODULE || IOT_ATOMIC_SUN_V2
#include "../src/plugins/dimmer_module/dimmer_base.h"
#endif
#if IOT_BLINDS_CTRL
#include "../src/plugins/blinds_ctrl/blinds_plugin.h"
#endif
#if SYSLOG_SUPPORT
#include <Syslog.h>
#include <SyslogStream.h>
#endif
#if NTP_CLIENT
#include "../src/plugins/ntp/ntp_plugin.h"
#endif
#if PIN_MONITOR
#include <PinMonitor.h>
#endif
#if IOT_SSDP_SUPPORT
#include "../src/plugins/ssdp/ssdp.h"
#endif
#if WEBUI_ALERTS_ENABLED && WEBUI_ALERTS_USE_MQTT
#include "../include/WebUIAlerts.h"
#endif
