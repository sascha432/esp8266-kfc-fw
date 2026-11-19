/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#ifndef DEBUG_MQTT_CLIENT
#    define DEBUG_MQTT_CLIENT 0
#endif

// home assistant auto discovery
#ifndef MQTT_AUTO_DISCOVERY
#    define MQTT_AUTO_DISCOVERY 1
#endif

// publish the auto discovery after each connect. a device that sleeps most of the time (remote control)
// publishes it only when requested (configuration change, AT command, WebUI)
#ifndef MQTT_AUTO_DISCOVERY_ON_CONNECT
#    if IOT_REMOTE_CONTROL
#        define MQTT_AUTO_DISCOVERY_ON_CONNECT 0
#    else
#        define MQTT_AUTO_DISCOVERY_ON_CONNECT 1
#    endif
#endif

// Mode 1: (default)
// -------
// set MQTT_AVAILABILITY_TOPIC to MQTT_AVAILABILITY_TOPIC_ONLINE after connecting and
// set last will to MQTT_AVAILABILITY_TOPIC_OFFLINE. this will automatically set the
// device status to offline when the connection is lost
//
// connect: MQTT_AVAILABILITY_TOPIC -> MQTT_AVAILABILITY_TOPIC_ONLINE
// disconnect: MQTT_AVAILABILITY_TOPIC -> MQTT_AVAILABILITY_TOPIC_OFFLINE
// connection lost: MQTT_AVAILABILITY_TOPIC -> MQTT_AVAILABILITY_TOPIC_OFFLINE
//
// Mode 2:
// -------
// useful for devices that go to deep sleep and stay "online"
//
// set MQTT_AVAILABILITY_TOPIC to MQTT_AVAILABILITY_TOPIC_ONLINE after connecting
// and MQTT_AVAILABILITY_TOPIC_OFFLINE when disconnect() is called. set last
// will to MQTT_LAST_WILL_TOPIC and publish MQTT_LAST_WILL_TOPIC_ONLINE after
// connecting and MQTT_LAST_WILL_TOPIC_OFFLINE when the connection is lost.
// the online status will not be affected by losing the connection (while the
// device is in deep sleep) and publish the real status under a different topic
//
// connect: MQTT_AVAILABILITY_TOPIC -> MQTT_AVAILABILITY_TOPIC_ONLINE and MQTT_LAST_WILL_TOPIC -> MQTT_LAST_WILL_TOPIC_ONLINE
// disconnect: MQTT_AVAILABILITY_TOPIC -> MQTT_AVAILABILITY_TOPIC_OFFLINE and MQTT_LAST_WILL_TOPIC -> MQTT_LAST_WILL_TOPIC_OFFLINE
// connection lost: MQTT_LAST_WILL_TOPIC -> MQTT_LAST_WILL_TOPIC_OFFLINE
//
// NOTE: this requires to add a binary sensor for MQTT_LAST_WILL_TOPIC to show up
//
#ifndef MQTT_SET_LAST_WILL_MODE
#    define MQTT_SET_LAST_WILL_MODE 1
#endif

// auto discovery availability topic
#ifndef MQTT_AVAILABILITY_TOPIC
#    define MQTT_AVAILABILITY_TOPIC FSPGM(mqtt_status_topic)
#endif

// set this to 1 if MQTT_AVAILABILITY_TOPIC_ONLINE or MQTT_AVAILABILITY_TOPIC_OFFLINE is different from default values
#ifndef MQTT_AVAILABILITY_TOPIC_ADD_PAYLOAD_ON_OFF
#    define MQTT_AVAILABILITY_TOPIC_ADD_PAYLOAD_ON_OFF 0
#endif

#ifndef MQTT_AVAILABILITY_TOPIC_ONLINE
#    define MQTT_AVAILABILITY_TOPIC_ONLINE SPGM(mqtt_status_topic_online)
#endif

#ifndef MQTT_AVAILABILITY_TOPIC_OFFLINE
#    define MQTT_AVAILABILITY_TOPIC_OFFLINE SPGM(mqtt_status_topic_offline)
#endif

#if MQTT_SET_LAST_WILL_MODE == 1

#    if (defined(MQTT_LAST_WILL_TOPIC) || defined(MQTT_LAST_WILL_TOPIC_ONLINE) || defined(MQTT_LAST_WILL_TOPIC_OFFLINE))
#        error MQTT_LAST_WILL_TOPIC, MQTT_LAST_WILL_TOPIC_ONLINE and MQTT_LAST_WILL_TOPIC_OFFLINE not supported for MQTT_SET_LAST_WILL_MODE == 1
#    endif

// for mode 1 set last will to availability topic

#    define MQTT_LAST_WILL_TOPIC         MQTT_AVAILABILITY_TOPIC
#    define MQTT_LAST_WILL_TOPIC_ONLINE  MQTT_AVAILABILITY_TOPIC_ONLINE
#    define MQTT_LAST_WILL_TOPIC_OFFLINE MQTT_AVAILABILITY_TOPIC_OFFLINE

#elif MQTT_SET_LAST_WILL_MODE == 2

// topic used to publish online status under a different topic than MQTT_AVAILABILITY_TOPIC

// default values are for a binary sensor
#    ifndef MQTT_LAST_WILL_TOPIC_ONLINE
#        define MQTT_LAST_WILL_TOPIC_ONLINE SPGM(mqtt_bool_on)
#    endif

#    ifndef MQTT_LAST_WILL_TOPIC_OFFLINE
#        define MQTT_LAST_WILL_TOPIC_OFFLINE SPGM(mqtt_bool_off)
#    endif

#else

#    error MQTT_SET_LAST_WILL_MODE invalid value

#endif

// maximum size of topic + payload + header (~7 byte) + safety limit for sending messages. a message is
// only handed to the MQTT library when it fits into the TCP buffer
#ifndef MQTT_MAX_MESSAGE_SIZE
#    define MQTT_MAX_MESSAGE_SIZE (TCP_SND_BUF - 64)
#endif

// incoming messages that exceed the limit are discarded
#ifndef MQTT_RECV_MAX_MESSAGE_SIZE
#    define MQTT_RECV_MAX_MESSAGE_SIZE 1024
#endif

// maximum size of all messages waiting to be sent. publish() returns false if the limit is reached
#ifndef MQTT_OUTBOX_MAX_SIZE
#    if ESP8266
#        define MQTT_OUTBOX_MAX_SIZE 4096
#    else
#        define MQTT_OUTBOX_MAX_SIZE 16384
#    endif
#endif

// pause after each auto discovery message
#ifndef MQTT_DISCOVERY_GAP_MS
#    define MQTT_DISCOVERY_GAP_MS 250
#endif

// delay before the auto discovery starts if it was requested (configuration change, AT command, WebUI). after
// connecting, a random delay between 0 and the configured start window is used
#ifndef MQTT_DISCOVERY_REQUEST_DELAY_MS
#    define MQTT_DISCOVERY_REQUEST_DELAY_MS 2000
#endif

// the retained auto discovery of the device is collected until no message has been received for this time
#ifndef MQTT_DISCOVERY_COLLECT_QUIET_MS
#    define MQTT_DISCOVERY_COLLECT_QUIET_MS 3000
#endif

// maximum time to collect the retained auto discovery
#ifndef MQTT_DISCOVERY_COLLECT_MAX_MS
#    define MQTT_DISCOVERY_COLLECT_MAX_MS 20000
#endif
