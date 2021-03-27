/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

// MQTT::Json can be used independently from the MQTT plugin

#include <Arduino_compat.h>
#include <JsonBaseReader.h>
#include <PrintString.h>
#include <JsonTools.h>

#define DEBUG_MQTT_JSON                     0

#if DEBUG_MQTT_JSON
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#undef RGB

namespace MQTT {

    namespace Json {

        struct RGB {
            float x;
            float y;
            float h;
            float s;
            uint8_t r;
            uint8_t g;
            uint8_t b;
            union {
                struct {
                    bool has_r: 1;
                    bool has_g: 1;
                    bool has_b: 1;
                    bool has_x: 1;
                    bool has_y: 1;
                    bool has_h: 1;
                    bool has_s: 1;
                };
                uint8_t _flags;
            };

            RGB() : _flags(0) {}

            bool hasRGB() const {
                return has_r && has_g && has_b;
            }
            bool hasXY() const {
                return has_x && has_y;
            }
            bool hasHS() const {
                return has_h && has_s;
            }

            String toString() const {
                if (hasRGB()) {
                    return PrintString(F("#%02x%02x%02x"), static_cast<unsigned>(r), static_cast<unsigned>(g), static_cast<unsigned>(b));
                }
                return String();
            }
        };

        class Reader : public ::JsonBaseReader {
        public:
            Reader();
            Reader(Stream* stream);

            virtual bool processElement();

            virtual bool recoverableError(JsonErrorEnum_t type) {
                // String err;
                // error(err, type);
                // Serial.printf("ERROR %u: %s", type, err.c_str());
                return true;
            }

            void clear() {
                brightness = -1;
                color_temp = -1;
                color = RGB();
                state = -1;
                transition = NAN;
                white_value = -1;
                effect = String();
            }

        public:
            int32_t brightness;
            int32_t color_temp;
            RGB color;
            int8_t state;
            float transition;
            int32_t white_value;
            String effect;
        };

    }

}

#include "mqtt_json_writer.hpp"

#include <debug_helper_disable.h>
