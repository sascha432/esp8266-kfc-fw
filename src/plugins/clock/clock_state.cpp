/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <WebUIAlerts.h>
#include "clock.h"

#if DEBUG_IOT_CLOCK
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#if IOT_CLOCK_SAVE_STATE

void ClockPlugin::_saveStateDelayed()
{
    if (get_time_diff(_saveTimestamp, millis()) > IOT_CLOCK_SAVE_STATE_DELAY * 1000U) {
        __LDBG_printf("save state delay skipped");
        _saveState();
    }
    else {
        _Timer(_saveTimer).add(Event::seconds(IOT_CLOCK_SAVE_STATE_DELAY), false, [this](Event::CallbackTimerPtr) {
            __LDBG_printf("executing save state delay");
            _saveState();
        });
    }
}

void ClockPlugin::_saveState()
{
    if (_debug) {
        __DBG_printf("DEBUG MODE - saving the state has been disabled");
        return;
    }
    auto state = _getState();
    auto newState = StoredState(_config, (_isEnabled && _targetBrightness), (_targetBrightness == 0 && _savedBrightness) ? _savedBrightness : _targetBrightness);
    __LDBG_printf("save brightness=%u enabled=%u", newState.getConfig().getBrightness(), newState.getConfig().enabled);
    if (state != newState) {
        auto file = KFCFS.open(FSPGM(iot_clock_save_state_file, "/.pvt/device.state"), fs::FileOpenMode::write);
        if (file) {
            _saveTimer.remove();
            _saveTimestamp = millis();

            __LDBG_printf("saving state enabled=%u brightness=%u animation=%u solid_color=%s blink_colon=%u",
                newState.getConfig().enabled,
                newState.getConfig().getBrightness(),
                newState.getConfig().animation,
                Color(newState.getConfig().solid_color).toString().c_str(),
                IF_IOT_CLOCK(newState.getConfig().blink_colon_speed) IF_IOT_LED_MATRIX(0)
            );

            if (!newState.store(file)) {
                __LDBG_printf("failed to store state %s", SPGM(iot_clock_save_state_file));
                WebAlerts::Alert::error(F("Failed to store state"), WebAlerts::ExpiresType::REBOOT);
            }
        }
    }
    else {
        __LDBG_printf("state did not change");
        _saveTimer.remove();
    }
}

ClockPlugin::StoredState ClockPlugin::_getState() const
{
    auto file = KFCFS.open(FSPGM(iot_clock_save_state_file), fs::FileOpenMode::read);
    if (file) {
        auto state = StoredState::load(file);
        if (state.hasValidData()) {
#if DEBUG_IOT_CLOCK
            auto &cfg = state.getConfig();
            __DBG_printf("loaded state enabled=%u brightness=%u animation=%u solid_color=%s blink_colon=%u", cfg.enabled, cfg.getBrightness(), cfg.getAnimation(), Color(cfg.solid_color).toString().c_str(), IF_IOT_CLOCK(cfg.blink_colon_speed) IF_IOT_LED_MATRIX(0));
#endif
            return state;
        }
    }
    __LDBG_printf("failed to load state %s", SPGM(iot_clock_save_state_file));
    return StoredState();
}

#endif

void ClockPlugin::_setState(bool state)
{
    if (state) {
        if (_targetBrightness == 0) {
            if (_savedBrightness) {
                setBrightness(_savedBrightness);
#if IOT_CLOCK_SAVE_STATE
                _saveState();
#endif
            }
            else {
                setBrightness(_config.getBrightness());
#if IOT_CLOCK_SAVE_STATE
                _saveState();
#endif
            }
        }
    }
    else {
        if (_targetBrightness != 0) {
            _savedBrightness = _targetBrightness;
        }
        setBrightness(0);
#if IOT_CLOCK_SAVE_STATE
        _saveState();
#endif
    }
}
