/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <JsonBaseReader.h>
#include <PrintString.h>

#include "StreamString.h"

namespace MQTT {

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

    class JsonReader : public ::JsonBaseReader {
    public:
        JsonReader();
        JsonReader(Stream* stream);

        virtual bool processElement();

        virtual bool recoverableError(JsonErrorEnum_t type) {
            // String err;
            // error(err, type);
            // Serial.printf("ERROR %u: %s", type, err.c_str());
            return true;
        }

        void clear() {
            _brightness = -1;
            _color_temp = -1;
            _color = RGB();
            _state = -1;
            _transition = NAN;
            _white_value = -1;
            _effect = String();
        }

    public:
        int32_t _brightness;
        int32_t _color_temp;
        RGB _color;
        int8_t _state;
        float _transition;
        int32_t _white_value;
        String _effect;
    };

}
