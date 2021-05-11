/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <kfc_fw_config.h>
#include "animation.h"

#if DEBUG_IOT_CLOCK
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using KFCConfigurationClasses::Plugins;

namespace Clock {

    class StoredState {
    public:
        typedef struct __attribute__packed__ {
            uint16_t _crc16;
            ConfigType _config;
        } FileFormat_t;

    public:

        StoredState() : _storage()
        {}

        StoredState(const ConfigType &config, bool enabled, uint8_t brightness) :
            _storage({0, config})
        {
            _storage._config.enabled = enabled;
            _storage._config.setBrightness(brightness);
            _updateCrc();
        }

        bool hasValidData() const {
            return _storage._crc16 != 0;
        }
        void clear() {
            _storage = {};
        }

        bool operator==(const StoredState &state) const {
            return _getCrc() == state._getCrc();
        }
        bool operator!=(const StoredState &state) const {
            return !operator==(state);
        }

        const ConfigType &getConfig() const {
            return _storage._config;
        }

        void setEnabled(bool enabled) {
            _storage._config.enabled = enabled;
        }

        bool store(Stream &stream) {
            if (hasValidData()) {
                _updateCrc();
                return (stream.write(reinterpret_cast<const uint8_t *>(&_storage), sizeof(_storage)) == sizeof(_storage));
            }
            return false;
        }

        static StoredState load(Stream &stream) {
            StoredState state;
            if (
                (stream.readBytes(reinterpret_cast<uint8_t *>(&state._storage), sizeof(state._storage)) != sizeof(state._storage)) ||
                (state._storage._crc16 != _getCrc(state._storage))
            ) {
                state.clear();
            }
            return state;
        }

    private:
        static uint16_t _getCrc(const FileFormat_t &storage) {
            return crc16_update(&storage._config, sizeof(storage._config));
        }

        uint16_t _getCrc() const {
            return _getCrc(_storage);
        }

        void _updateCrc() {
            _storage._crc16 = _getCrc();
        }

    private:
        FileFormat_t _storage;
    };

}

#if DEBUG_IOT_CLOCK
#include <debug_helper_disable.h>
#endif
