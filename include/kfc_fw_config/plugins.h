/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "kfc_fw_config/base.h"
#include "WebUIComponent.h"

// --------------------------------------------------------------------
// Plugins

#include "plugins/clock.h"
#include "plugins/remote.h"
#include "plugins/iot_switch.h"
#include "plugins/alarm.h"
#include "plugins/serial2tcp.h"
#include "plugins/mqtt_client.h"
#include "plugins/syslog.h"
#include "plugins/ntp_client.h"
#include "plugins/sensor.h"
#include "plugins/blinds_ctrl.h"
#include "plugins/dimmer.h"
#include "plugins/ping.h"
#include "plugins/mdns.h"
#include "plugins/weather_station.h"
#include "plugins/display.h"

namespace KFCConfigurationClasses {

    struct PluginsType {

        using RemoteControl = Plugins::RemoteControlConfigNS::RemoteControl;
        using IotSwitch = Plugins::SwitchConfigNS::IotSwitch;
        using WeatherStation = Plugins::WeatherStationConfigNS::WeatherStation;
        using Serial2TCP = Plugins::Serial2TCPConfigNS::Serial2TCP;
        using SyslogClient = Plugins::SyslogConfigNS::SyslogClient;
        using MqttClient = Plugins::MQTTConfigNS::MqttClient;
        using Blinds  = Plugins::BlindsConfigNS::Blinds;
        using NTPClient = Plugins::NTPClientConfigNs::NTPClient;
        using Dimmer = Plugins::DimmerConfigNS::Dimmer;
        using Ping = Plugins::PingConfigNS::Ping;
        using MDNS = Plugins:: MDNSConfigNS::MDNS;
        using Alarm = Plugins::AlarmConfigNS::Alarm;
        using Sensor = Plugins::SensorConfigNS::Sensor;
        using Clock = Plugins::ClockConfigNS::Clock;
        using Display = Plugins::DisplayConfigNS::Display;

        // --------------------------------------------------------------------
        // Plugin Structure

        RemoteControl remotecontrol;
        IotSwitch iotswitch;
        WeatherStation weatherstation;
        Alarm alarm;
        Serial2TCP serial2tcp;
        MqttClient mqtt;
        SyslogClient syslog;
        NTPClient ntpclient;
        Sensor sensor;
        Blinds blinds;
        Dimmer dimmer;
        Clock clock;
        Ping ping;
        MDNS mdns;
        Display display;
    };

}
