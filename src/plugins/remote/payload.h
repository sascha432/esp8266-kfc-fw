/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "remote_def.h"
#include <Buffer.h>
#include <ArduinoJson.h>

#if DEBUG_IOT_REMOTE_CONTROL
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using KFCConfigurationClasses::Plugins;

namespace RemoteControl {

    // storage containers for the payload
    //
    // constructors are all PROGMEM safe
    //
    //   Binary
    //   String (stored as binary without NUL termination)
    //   Json (basically String with constructors for ArudinoJson, the ArudinoJson object isn't required after creating)

    // --------------------------------------------------------------------
    namespace Payload {
        class Binary {
        public:
            Binary(const Binary &) = delete;
            Binary &operator=(const Data &) = delete;

            Binary() : _data(nullptr), _size(0) {}

            // if data is null, the memory is allocated and filled with 0
            Binary(const uint8_t *data, size_t size) : _data(new uint8_t[size]), _size(size) {
                if (_data == nullptr) {
                    _size = 0;
                }
                else if (_size) {
                    if (data) {
                        memcpy_P(_data, data, _size);
                    }
                    else {
                        std::fill_n(_data, _size, 0);
                    }
                }
            }

            Binary(Binary &&move) noexcept :
                _data(std::exchange(move._data, nullptr)),
                _size(std::exchange(move._size, 0))
            {}

            ~Binary() {
                if (_data) {
                    delete[] _data;
                }
            }

            Binary &operator=(Binary &&move) noexcept {
                this->~Binary();
                ::new(static_cast<void *>(this)) Binary(std::move(move));
                return *this;
            }

            void clear() {
                if (_data) {
                    delete[] _data;
                    _data = nullptr;
                }
                _size = 0;
            }

            const uint8_t *data() const {
                return _data;
            }

            size_t size() const {
                return _size;
            }

            uint8_t *begin() {
                return _data;
            }

            uint8_t *end() {
                return _data + _size;
            }

        public:
            Binary(::String &&move) : _data(nullptr), _size(move.length()) {
                _data = reinterpret_cast<uint8_t *>(MoveStringHelper::move(std::move(move), nullptr));
                if (!_data) {
                    _size = 0;
                }
            }

        protected:
            uint8_t *_data;
            uint8_t _size;
        };

        class String : public Binary
        {
        public:
            String() {}
            String(const char *data, size_t size) : Binary(reinterpret_cast<const uint8_t *>(data), size) {}
            String(const __FlashStringHelper *data) : Binary(reinterpret_cast<const uint8_t *>(data), strlen_P(reinterpret_cast<PGM_P>(data))) {}
            String(const ::String &str) : String(str.c_str(), str.length()) {}
            String(::String &&str) : Binary(std::move(str)) {}

            size_t length() const {
                return _size;
            }

            ::String toString() const {
                ::String str;
                str.concat(reinterpret_cast<char *>(_data), _size);
                return str;
            }
        };

        class Json : public String {
        public:
            using String::String;

            Json() {}
            // Json(const JsonDocument &doc) {
            //     _size = measureJson(doc);
            //     _data = new uint8_t[_size];
            //     if (_data) {
            //         serializeJson(doc, _data, _size);
            //     }
            //     else {
            //         _data = nullptr;
            //         _size = 0;
            //     }
            // }
            // Json(const JsonDocument &doc, bool pretty) {
            //     _size = measureJsonPretty(doc);
            //     _data = new uint8_t[_size];
            //     if (_data) {
            //         serializeJsonPretty(doc, _data, _size);
            //     }
            //     else {
            //         _data = nullptr;
            //         _size = 0;
            //     }
            // }

            static Binary serializeFromJson(const JsonDocument &doc)
            {
                Binary payload(nullptr, measureJson(doc));
                if (payload.size()) {
                    serializeJson(doc, payload.begin(), payload.size());
                }
                return payload;
            }
        };

    }

}

#include <debug_helper_disable.h>
