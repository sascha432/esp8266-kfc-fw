/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "kfc_fw_config/base.h"
#include "WebUIComponent.h"

namespace KFCConfigurationClasses {

    // --------------------------------------------------------------------
    // Plugins

    struct Plugins {

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

        #include "plugins/clock.h"

        #include "plugins/ping.h"

        #include "plugins/mdns.h"

        // --------------------------------------------------------------------
        // Plugin Structure

        RemoteControl remotecontrol;
        IOTSwitch iotswitch;
        WeatherStation weatherstation;
        Alarm alarm;
        Serial2TCP serial2tcp;
        MQTTClient mqtt;
        SyslogClient syslog;
        NTPClient ntpclient;
        Sensor sensor;
        Blinds blinds;
        Dimmer dimmer;
        Clock clock;
        Ping ping;
        MDNS mdns;

    };

}
