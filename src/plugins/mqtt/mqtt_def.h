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

// support for MQTT group topic
#ifndef MQTT_GROUP_TOPIC
#define MQTT_GROUP_TOPIC                                        0
#endif

// 0 to disable last will. status is set to offline when disconnect(false) is called
// useful for devices that go to deep sleep and stay "online"
#ifndef MQTT_SET_LAST_WILL
#define MQTT_SET_LAST_WILL                                      1
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
