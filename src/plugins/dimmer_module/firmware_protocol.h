/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include  <Arduino_compat.h>
#include "DimmerTwoWireEx.h"

#ifndef DIMMER_FIRMWARE_VERSION
#define DIMMER_FIRMWARE_VERSION                     0x020200
#endif

#define DIMMER_CURRENT_LEVEL                        -1

#if DIMMER_FIRMWARE_VERSION < 0x020200

#define DIMMER_I2C_SLAVE                            1
#define DIMMER_TEMP_OFFSET_DIVIDER                  4.0
#define DIMMER_FIRWARE_VERSION_MAJOR                2
#define DIMMER_FIRWARE_VERSION_MINOR                1

#include "../firmware/2.1.x/dimmer_commands.h"

#elif DIMMER_FIRMWARE_VERSION >= 0x020200

#define DIMMER_I2C_SLAVE                            1
#define DIMMER_FIRWARE_VERSION_MAJOR                2
#define DIMMER_FIRWARE_VERSION_MINOR                2

#include "./firmware/2.2.x/dimmer_commands.h"

#elif DIMMER_FIRMWARE_VERSION >= 0x030000

#define DIMMER_FIRWARE_VERSION_MAJOR                3
#define DIMMER_FIRWARE_VERSION_MINOR                0

#include "./firmware/3.0.x/dimmer_commands.h"

#endif



// class DimmerMetrics {
// public:
//     DimmerMetrics() : _metrics({NAN, NAN, NAN, 0}) {}

//     DimmerMetrics &operator=(const dimmer_metrics_t &metrics)  {
//         _metrics.temperature = metrics.ntc_temp;
//         _metrics.temperature2 = metrics.internal_temp;
//         _metrics.frequency = metrics.frequency;
//         _metrics.vcc = metrics.vcc;
//         return *this;
//     }

//     // NTC
//     float getTemp() const {
//         return _metrics.temperature;
//     }
//     // ATmega
//     float getTemp2() const {
//         return _metrics.temperature2;
//     }
//     float getFrequency() const {
//         return _metrics.frequency;
//     }
//     uint16_t getVCCmV() const {
//         return _metrics.vcc;
//     }
//     float getVCC() const {
//         return _metrics.vcc / 1000.0;
//     }

//     bool hasTemp() const {
//         return !isnan(_metrics.temperature);
//     }
//     bool hasTemp2() const {
//         return !isnan(_metrics.temperature2);
//     }
//     bool hasFrequency() const {
//         return !isnan(_metrics.frequency);
//     }
//     bool hasVCC() const {
//         return _metrics.vcc;
//     }

// private:
//     typedef struct {
//         float temperature;
//         float temperature2;
//         float frequency;
//         uint16_t vcc;
//     } Metrics_t;

//     Metrics_t _metrics;
// };
// // typedef struct __attribute__packed__ {
// //     uint8_t temp_check_value;
// //     uint16_t vcc;
// //     float frequency;
// //     float ntc_temp;
// //     float internal_temp;
// // } dimmer_metrics_t;


// // supported by 2.x and 3.x
// class DimmerRetrieveVersionLegacy {
// public:
//     static constexpr uint16_t INVALID_VERSION = ~0;

//     typedef struct __attribute__packed__ {
//         uint8_t read_length_address;
//         uint8_t size;
//         uint8_t version_address;
//     } LegacyGetVersion_t;

//     inline static constexpr uint16_t getVersion(uint8_t major, uint8_t minor, uint8_t revision) {
//         return dimmer_version_to_uint16(major, minor, revision);
//     }

//     class Version {
//     private:
//         typedef union __attribute__packed__ {
//             uint16_t word;
//             struct {
//                 uint16_t revision: 5;
//                 uint16_t minor: 5;
//                 uint16_t major: 6;
//             };
//         } Version_t;

//         Version_t _version;

//     public:
//         static constexpr uint16_t INVALID_VERSION = DimmerRetrieveVersionLegacy::INVALID_VERSION;

//         Version() : _version({INVALID_VERSION}) {}
//         Version(uint16_t version) : _version({version}) {}

//         operator bool() const {
//             return _version.word && (_version.word != INVALID_VERSION);
//         }

//         bool operator==(int version) const {
//             return _version.word == version;
//         }

//         bool operator!=(int version) const {
//             return _version.word != version;
//         }

//         operator int() const {
//             return _version.word;
//         }

//         Version &operator=(int version) {
//             _version.word = version;
//             return *this;
//         }

//         uint8_t getMajor() const {
//             return _version.major;
//         }

//         uint8_t getMinor() const {
//             return _version.minor;
//         }

//         uint8_t getRevision() const {
//             return _version.revision;
//         }

//         bool read(Stream &wire) {
//             if (wire.readBytes(reinterpret_cast<uint8_t *>(&_version), size()) == size()) {
//                 return true;
//             }
//             _version.word = INVALID_VERSION;
//             return false;
//         }

//         static constexpr size_t size() {
//             static_assert(sizeof(Version_t) == 2, "invalid size");
//             return sizeof(Version_t);
//         }
//     };

//     DimmerRetrieveVersionLegacy() : _data({0x8a, Version::size(), 0xb9}) {}

//     LegacyGetVersion_t &data() {
//         return _data;
//     }

// private:
//     LegacyGetVersion_t _data;
// };
