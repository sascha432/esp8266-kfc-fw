/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
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

void ClockPlugin::_saveState(int32_t brightness)
{
    if (brightness == -1) {
        brightness = _targetBrightness;
    }
    auto state = _getState();
    auto newState = StoredState(brightness, _config.getAnimation(), _color IF_IOT_CLOCK(_config.blink_colon_speed));
    if (state != newState) {
        auto file = KFCFS.open(FSPGM(iot_clock_save_state_file, "/.pvt/device.state"), fs::FileOpenMode::write);
        if (file) {
            _saveTimer.remove();
            _saveTimestamp = millis();

            __LDBG_printf("saving state brightness=%u animation=%u color=%s blink_colon=%u", brightness, _config.animation, _color.toString().c_str(), IF_IOT_CLOCK(_config.blink_colon_speed) IF_IOT_LED_MATRIX(0));

            _copyToState(state, brightness);
            if (!state.store(file)) {
                __LDBG_printf("failed to store state %s", SPGM(iot_clock_save_state_file));
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
            __LDBG_printf("loaded state brightness=%u animation=%u color=%s blink_colon=%u", state.getBrightness(), state.getAnimation(), state.getColor().toString().c_str(), IF_IOT_CLOCK(state.getBlinkColonSpeed()) IF_IOT_LED_MATRIX(0));
            return state;
        }
    }
    __LDBG_printf("failed to load state %s", SPGM(iot_clock_save_state_file));
    return StoredState();
}

void ClockPlugin::_copyToState(StoredState &state, BrightnessType brightness)
{
    state.setBrightness(brightness);
    state.setAnimation(_config.animation);
    state.setColor(_color);
#if !IOT_LED_MATRIX
    state.setBlinkColonSpeed(_config.blink_colon_speed);
#endif
}

void ClockPlugin::_copyFromState(const StoredState &state)
{
    _targetBrightness = state.getBrightness();
    _config.animation = state.getAnimationInt();
    _color = state.getColor();
#if !IOT_LED_MATRIX
    setBlinkColon(state.getBlinkColonSpeed());
#endif
}

#endif

void ClockPlugin::_setState(bool state)
{
    if (state) {
        if (_targetBrightness == 0) {
            if (_savedBrightness) {
                setBrightness(_savedBrightness);
#if IOT_CLOCK_SAVE_STATE
                _saveState(_savedBrightness);
#endif
            }
            else {
                setBrightness(_config.getBrightness());
#if IOT_CLOCK_SAVE_STATE
                _saveState(_config.getBrightness());
#endif
            }
        }
    }
    else {
        setBrightness(0);
#if IOT_CLOCK_SAVE_STATE
        _saveState(0);
#endif
    }
}
