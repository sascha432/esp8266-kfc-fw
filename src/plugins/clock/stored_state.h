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

namespace Clock {

    using Plugins = KFCConfigurationClasses::PluginsType;

    class StoredState {
    public:
        struct FileFormat {
            uint32_t _crc;
            ClockConfigType _config;

            FileFormat() : _crc(~0U) {}
            FileFormat(const ClockConfigType &config) : _crc(~0U), _config(config) {}

            operator uint8_t *() {
                return reinterpret_cast<uint8_t *>(this);
            }

            operator const uint8_t *() const {
                return reinterpret_cast<const uint8_t *>(this);
            }
        };

    public:

        StoredState() : _storage()
        {}

        StoredState(const ClockConfigType &config, bool enabled, uint8_t brightness) :
            _storage(config)
        {
            _storage._config.enabled = enabled;
            _storage._config.setBrightness(brightness);
            _updateCrc();
        }

        bool hasValidData() const {
            return (_storage._crc != 0) && (_storage._crc != ~0U);
        }

        void clear() {
            _storage = FileFormat();
        }

        bool operator==(const StoredState &state) const {
            return _getCrc() == state._getCrc();
        }

        bool operator!=(const StoredState &state) const {
            return !operator==(state);
        }

        const ClockConfigType &getConfig() const {
            return _storage._config;
        }

        void setEnabled(bool enabled) {
            _storage._config.enabled = enabled;
        }

        bool store(Stream &stream) {
            if (hasValidData()) {
                _updateCrc();
                return (stream.write(_storage, sizeof(_storage)) == sizeof(_storage));
            }
            return false;
        }

        static StoredState load(Stream &stream) {
            StoredState state;
            if (
                (stream.readBytes(state._storage, sizeof(state._storage)) != sizeof(state._storage)) ||
                (state._storage._crc != _getCrc(state._storage))
            ) {
                state.clear();
            }
            return state;
        }

    private:
        static uint32_t _getCrc(const FileFormat &storage) {
            return crc32(&storage._config, sizeof(storage._config));
        }

        uint32_t _getCrc() const {
            return _getCrc(_storage);
        }

        void _updateCrc() {
            _storage._crc = _getCrc();
        }

    private:
        FileFormat _storage;
    };

}

#if DEBUG_IOT_CLOCK
#include <debug_helper_disable.h>
#endif
