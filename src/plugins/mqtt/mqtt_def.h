/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#ifndef DEBUG_MQTT_CLIENT
#define DEBUG_MQTT_CLIENT                                       0
#endif

// home assistant auto discovery
#ifndef MQTT_AUTO_DISCOVERY
#define MQTT_AUTO_DISCOVERY                                     1
#endif

// Mode 0:
// -------
// set MQTT_AVAILABILITY_TOPIC to MQTT_AVAILABILITY_TOPIC_ONLINE after connecting
// and MQTT_AVAILABILITY_TOPIC_OFFLINE when disconnect() is called. last will
// is disabled
//
// connect: MQTT_AVAILABILITY_TOPIC -> MQTT_AVAILABILITY_TOPIC_ONLINE
// disconnect: MQTT_AVAILABILITY_TOPIC -> MQTT_AVAILABILITY_TOPIC_OFFLINE
// connection lost: do nothing
//
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
// connecting and MQTT_LAST_WILL_TOPIC_OFFLINE when the connctiong is lost.
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
#define MQTT_SET_LAST_WILL_MODE                                      1
#endif

// auto discovery availability topic
#ifndef MQTT_AVAILABILITY_TOPIC
#define MQTT_AVAILABILITY_TOPIC                                     FSPGM(mqtt_status_topic)
#endif

// set this to 1 if MQTT_AVAILABILITY_TOPIC_ONLINE or MQTT_AVAILABILITY_TOPIC_OFFLINE is different from default values
#ifndef MQTT_AVAILABILITY_TOPIC_ADD_PAYLOAD_ON_OFF
#define MQTT_AVAILABILITY_TOPIC_ADD_PAYLOAD_ON_OFF                  0
#endif

#ifndef MQTT_AVAILABILITY_TOPIC_ONLINE
#define MQTT_AVAILABILITY_TOPIC_ONLINE                              FSPGM(mqtt_status_topic_online)
#endif

#ifndef MQTT_AVAILABILITY_TOPIC_OFFLINE
#define MQTT_AVAILABILITY_TOPIC_OFFLINE                             FSPGM(mqtt_status_topic_offline)
#endif

// add name to each MQTT device
// Device Name + ' ' + Component Name
#ifndef MQTT_AUTO_DISCOVERY_USE_NAME
#define MQTT_AUTO_DISCOVERY_USE_NAME                                0
#endif

#if MQTT_SET_LAST_WILL_MODE == 1

#if (defined(MQTT_LAST_WILL_TOPIC) || defined(MQTT_LAST_WILL_TOPIC_ONLINE) || defined(MQTT_LAST_WILL_TOPIC_OFFLINE))
#error MQTT_LAST_WILL_TOPIC, MQTT_LAST_WILL_TOPIC_ONLINE and MQTT_LAST_WILL_TOPIC_OFFLINE not supported for MQTT_SET_LAST_WILL_MODE == 1
#endif

// for mode 1 set last will to availability topic

#define MQTT_LAST_WILL_TOPIC                                    MQTT_AVAILABILITY_TOPIC
#define MQTT_LAST_WILL_TOPIC_ONLINE                             MQTT_AVAILABILITY_TOPIC_ONLINE
#define MQTT_LAST_WILL_TOPIC_OFFLINE                            MQTT_AVAILABILITY_TOPIC_OFFLINE

#elif MQTT_SET_LAST_WILL_MODE == 2

// topic used to publish online status under a different topic than MQTT_AVAILABILITY_TOPIC

// default values are for a binary sensor
#ifndef MQTT_LAST_WILL_TOPIC_ONLINE
#define MQTT_LAST_WILL_TOPIC_ONLINE                             FSPGM(mqtt_bool_on)
#endif

#ifndef MQTT_LAST_WILL_TOPIC_OFFLINE
#define MQTT_LAST_WILL_TOPIC_OFFLINE                            FSPGM(mqtt_bool_off)
#endif

#elif MQTT_SET_LAST_WILL_MODE != 0

#error MQTT_SET_LAST_WILL_MODE invalid value

#endif

// enable callbacks for QoS. onSubscribe(), onPublish() and onUnsubscribe()
#ifndef MQTT_USE_PACKET_CALLBACKS
#define MQTT_USE_PACKET_CALLBACKS                               0
#endif

// timeout if subscribe/unsubcribe/publish cannot be sent
#ifndef MQTT_QUEUE_TIMEOUT
#define MQTT_QUEUE_TIMEOUT                                      7500
#endif

// default timeout waiting for the acknowledgement packet
#ifndef MQTT_DEFAULT_TIMEOUT
#define MQTT_DEFAULT_TIMEOUT                                    10000
#endif

// delay before retrying subscribe/unsubcribe/publish
#ifndef MQTT_QUEUE_RETRY_DELAY
#define MQTT_QUEUE_RETRY_DELAY                                  250
#endif

// maximum size of topc + payload + header (~7 byte) + safety limit for sending messages
#ifndef MQTT_MAX_MESSAGE_SIZE
#define MQTT_MAX_MESSAGE_SIZE                                   (TCP_SND_BUF - 64)
#endif

// incoming message that exceed the limit are discarded
#ifndef MQTT_RECV_MAX_MESSAGE_SIZE
#define MQTT_RECV_MAX_MESSAGE_SIZE                              1024
#endif

#if MQTT_AUTO_DISCOVERY_USE_ABBREVIATIONS

#define MQTT_DEVICE_REG_CONNECTIONS                             "cns"
#define MQTT_DEVICE_REG_IDENTIFIERS                             "ids"
#define MQTT_DEVICE_REG_NAME                                    "name"
#define MQTT_DEVICE_REG_MANUFACTURER                            "mf"
#define MQTT_DEVICE_REG_MODEL                                   "mdl"
#define MQTT_DEVICE_REG_SW_VERSION                              "sw"

#else

#define MQTT_DEVICE_REG_CONNECTIONS                             "connections"
#define MQTT_DEVICE_REG_IDENTIFIERS                             "identifiers"
#define MQTT_DEVICE_REG_NAME                                    "name"
#define MQTT_DEVICE_REG_MANUFACTURER                            "manufacturer"
#define MQTT_DEVICE_REG_MODEL                                   "model"
#define MQTT_DEVICE_REG_SW_VERSION                              "sw_version"

#endif

#ifndef DEBUG_MQTT_AUTO_DISCOVERY_QUEUE
#define DEBUG_MQTT_AUTO_DISCOVERY_QUEUE                         1
#endif

#ifndef DEBUG_MQTT_CLIENT_PAYLOAD_LEN
#define DEBUG_MQTT_CLIENT_PAYLOAD_LEN                           128
#endif

// initial delay after connect +-10%
#ifndef MQTT_AUTO_DISCOVERY_QUEUE_INITIAL_DELAY
#define MQTT_AUTO_DISCOVERY_QUEUE_INITIAL_DELAY                 30000
#endif

// delay before retrying to send
#ifndef MQTT_AUTO_DISCOVERY_ERROR_DELAY
#define MQTT_AUTO_DISCOVERY_ERROR_DELAY                         5000
#endif
