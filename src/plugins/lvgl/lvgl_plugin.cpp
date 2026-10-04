/**
 * Author: sascha_lammers@gmx.de
 */

#include "lvgl_plugin.h"

#include <LoopFunctions.h>
#include <PrintHtmlEntitiesString.h>
#include "lvgl_overview.h"
#include "lvgl_test_screen.h"
#include "devices/wt32_sc01/wt32_sc01.h"
#include "logger.h"
#include "plugins_menu.h"
#if DEBUG_LVGL_SCREENSHOT
#    include "lvgl_debug.h"
#endif

using Plugins = KFCConfigurationClasses::PluginsType;
using Display = KFCConfigurationClasses::Plugins::DisplayConfigNS::Display;
using DisplayConfig = Display::Config_t;

PROGMEM_DEFINE_PLUGIN_OPTIONS(
    LVGLPlugin,
    "lvgl",             // name
    "LVGL Display",     // friendly name
    "",                 // web_templates
    "lvgl",             // config_forms
    "",                 // reconfigure_dependencies
    PluginComponent::PriorityType::DISPLAY_PLUGIN,
    PluginComponent::RTCMemoryId::NONE,
    static_cast<uint8_t>(PluginComponent::MenuType::CUSTOM),
    false,              // allow_safe_mode
    false,              // setup_after_deep_sleep
    true,               // has_get_status
    true,               // has_config_forms
    false,              // has_web_ui
    false,              // has_web_templates
    false,              // has_at_mode
    0                   // __reserved
);

static LVGLPlugin plugin;

// screen manager of the LVGL UI, application plugins register their screens here
static LVGLScreenManager screenManager;

// The screen overview (one tile per registered screen) is shown by the default action of the tap
// and double tap gestures. It uses the manager, so it is created after it
static LVGLScreenOverview screenOverview(screenManager);

// The display test screen is only used if no application screen has been registered, it is
// created after the plugin setup phase (so that all plugins had a chance to register)
static bool _testScreenChecked = false;

// ------------------------------------------------------------------------------------------
// Brightness, brightness fade and idle power saving
//
// Everything runs from the main loop (displayLoop()), notifyActivity() may be called from another
// task as well and only writes scalars - the backlight itself is always set from the main loop.
//
// A brightness change is faded with a constant rate (the full 0-100% range takes
// kFadeRangeMillis), the idle timers are checked every kPowerTickMillis.

// fade time for the full 0-100% range
static constexpr uint32_t kFadeRangeMillis = 3000;
// resolution of the fade and of the idle timers
static constexpr uint32_t kPowerTickMillis = 20;
// The input is enabled again this long after the last touch was released. The touch controller
// may report the release late and the panel is polled at its own rate, a touch that is still
// held down when the display is woken up must not reach the UI
static constexpr uint32_t kInputReleaseGraceMillis = 120;

enum class PowerState : uint8_t {
    NORMAL, // backlight_level, the configured brightness
    POWER_SAVING, // power_saving_level, dimmed after power_saving_timeout
    STANDBY, // off, the display is turned off after standby_timeout
};

// current state of the backlight
static uint8_t _brightness = 100; // percent
static uint8_t _backlight = 255; // 0-255
static PowerState _powerState = PowerState::NORMAL;
// Display::getConfig() reads the storage, so the values used by the main loop are cached
static uint8_t _configuredLevel = 255; // backlight_level, 0-255
static uint8_t _powerSavingLevel = 77; // power_saving_level, 0-255
static uint32_t _powerSavingTimeout = 300000; // power_saving_timeout, milliseconds
static uint32_t _standbyTimeout = 3600000; // standby_timeout, milliseconds
// user activity
static uint32_t _lastActivity = 0; // millis() of the last touch/notifyActivity()
static uint32_t _touchCount = 0; // WT32_SC01::getTouchCount() of the last tick
static uint32_t _lastTick = 0;
// fade from _fadeFrom to _fadeTo, _fadeActive is true until the target has been reached
static bool _fadeActive = false;
static uint8_t _fadeFrom = 255;
static uint8_t _fadeTo = 255;
static uint32_t _fadeDuration = 0;
static uint32_t _fadeStart = 0;
// true while the display is off and the touch screen is suppressed (not passed to the UI)
static bool _inputSuppressed = false;
// true when a touch was seen while the input was suppressed. The input is only enabled again after
// that touch has ended, so the touch that wakes the display up is never delivered (entering the
// standby with nothing touched must not start the release timer)
static bool _wakeTouch = false;
// millis() when the touch was released, used to delay enabling the input again (`0` = not released)
static uint32_t _releaseTime = 0;

static uint8_t _levelFromPercent(uint8_t percent)
{
    if (percent > 100) {
        percent = 100;
    }
    return static_cast<uint8_t>((percent * 255U + 50U) / 100U);
}

static uint8_t _percentFromLevel(uint8_t level)
{
    return static_cast<uint8_t>((level * 100U + 127U) / 255U);
}

// elapsed milliseconds since a timestamp, never negative. `now` may have been read before the
// timestamp was updated (activity in the same tick, millis() wraparound), a plain unsigned
// subtraction would underflow to ~4294967295 and fire every timer at once
static uint32_t _elapsed(uint32_t now, uint32_t since)
{
    const auto elapsed = static_cast<int32_t>(now - since);
    return (elapsed > 0) ? static_cast<uint32_t>(elapsed) : 0;
}

// caches the brightness and the power saving settings, Display::getConfig() reads the storage.
// The power saving settings were added later, the stored blob of an existing device was zero
// filled when it grew to the new length - a value that is not in the valid range is "not
// initialized" and the default is used
static void _readDisplayConfig()
{
    const auto cfg = Display::getConfig();
    auto percent = static_cast<uint8_t>(cfg.backlight_level);
    if (percent < DisplayConfig::kMinValueFor_backlight_level) {
        percent = DisplayConfig::kMinValueFor_backlight_level;
    }
    else if (percent > DisplayConfig::kMaxValueFor_backlight_level) {
        percent = DisplayConfig::kMaxValueFor_backlight_level;
    }
    _configuredLevel = _levelFromPercent(percent);

    auto powerSavingLevel = static_cast<uint8_t>(cfg.power_saving_level);
    if (powerSavingLevel < DisplayConfig::kMinValueFor_power_saving_level || powerSavingLevel > DisplayConfig::kMaxValueFor_power_saving_level) {
        powerSavingLevel = DisplayConfig::kDefaultValueFor_power_saving_level;
    }
    _powerSavingLevel = _levelFromPercent(powerSavingLevel);

    auto powerSavingTimeout = static_cast<uint32_t>(cfg.power_saving_timeout);
    if (powerSavingTimeout < DisplayConfig::kMinValueFor_power_saving_timeout || powerSavingTimeout > DisplayConfig::kMaxValueFor_power_saving_timeout) {
        powerSavingTimeout = DisplayConfig::kDefaultValueFor_power_saving_timeout;
    }
    _powerSavingTimeout = powerSavingTimeout * 1000UL;

    auto standbyTimeout = static_cast<uint32_t>(cfg.standby_timeout);
    if (standbyTimeout < DisplayConfig::kMinValueFor_standby_timeout || standbyTimeout > DisplayConfig::kMaxValueFor_standby_timeout) {
        standbyTimeout = DisplayConfig::kDefaultValueFor_standby_timeout;
    }
    _standbyTimeout = standbyTimeout * 1000UL;
}

// sets the backlight, only called from the main loop
static void _setBacklight(uint8_t level)
{
    if (level == _backlight) {
        return;
    }
    _backlight = level;
    _brightness = _percentFromLevel(level);
    WT32_SC01::setBacklight(level);
}

// applies a brightness immediately (no fade)
static void _setBrightnessNow(uint8_t percent)
{
    _fadeActive = false;
    _brightness = (percent > 100) ? 100 : percent;
    _backlight = _levelFromPercent(_brightness);
    WT32_SC01::setBacklight(_backlight);
}

// starts fading to a backlight level, the delta of the 0-255 range decides how long it takes
static void _fadeToLevel(uint8_t level)
{
    if (_fadeActive && _fadeTo == level) {
        return;
    }
    _fadeFrom = _backlight;
    _fadeTo = level;
    _fadeStart = millis();
    const auto delta = (_fadeTo > _fadeFrom) ? (_fadeTo - _fadeFrom) : (_fadeFrom - _fadeTo);
    _fadeDuration = (kFadeRangeMillis * delta) / 255;
    _fadeActive = true;
}

// suspends the input of the panel (the touch that wakes the display up is not passed to the UI)
static void _setInputSuppressed(bool suppressed)
{
    if (_inputSuppressed == suppressed) {
        return;
    }
    _inputSuppressed = suppressed;
    _wakeTouch = false;
    _releaseTime = 0;
    WT32_SC01::setInputEnabled(!suppressed);
    __LDBG_printf("touch input %s", suppressed ? "suppressed (standby)" : "enabled");
}

// changes the power state and starts the fade to the level of the new state
static void _setPowerState(PowerState state)
{
    if (_powerState == state) {
        return;
    }
    __LDBG_printf("power state %u -> %u", static_cast<unsigned>(_powerState), static_cast<unsigned>(state));
    _powerState = state;
    switch (state) {
    case PowerState::NORMAL:
        _fadeToLevel(_configuredLevel);
        break;
    case PowerState::POWER_SAVING:
        _fadeToLevel(_powerSavingLevel);
        break;
    default: // STANDBY
        _fadeToLevel(0);
        _setInputSuppressed(true);
        break;
    }
}

// user activity (touch screen, wakeup from another plugin)
static void _notifyActivity(bool fromTouch)
{
    _lastActivity = millis();
    if (!fromTouch && _inputSuppressed) {
        // woken up by something else than a touch (motion/presence sensor), nothing is held back
        _setInputSuppressed(false);
    }
    _setPowerState(PowerState::NORMAL);
}

// brightness fade and idle power saving, called from the main loop
static void _powerLoop()
{
    auto now = millis();
    if (_elapsed(now, _lastTick) < kPowerTickMillis) {
        return;
    }
    _lastTick = now;

    // any touch is activity, the touch controller works while the backlight is off
    auto touchCount = WT32_SC01::getTouchCount();
    if (touchCount != _touchCount) {
        _touchCount = touchCount;
        _notifyActivity(true);
    }

    // The touch that wakes the display up is not passed to the UI. The input is only enabled again
    // after a touch that was held back has ended (plus a short grace period for a touch the
    // controller still reports as released), and only when such a touch happened at all - while the
    // display is off and nothing touched it the input stays suppressed
    if (_inputSuppressed) {
        if (WT32_SC01::isTouched()) {
            _wakeTouch = true;
            _releaseTime = 0;
        }
        else if (_wakeTouch) {
            if (_releaseTime == 0) {
                _releaseTime = now;
            }
            else if (_elapsed(now, _releaseTime) >= kInputReleaseGraceMillis) {
                _wakeTouch = false;
                _setInputSuppressed(false);
            }
        }
    }

    // the activity above updated _lastActivity/_fadeStart (both are millis() of this tick), so the
    // time base is read again before anything is compared against them
    now = millis();

    if (_fadeActive) {
        const auto elapsed = _elapsed(now, _fadeStart);
        if (elapsed >= _fadeDuration) {
            _fadeActive = false;
            _setBacklight(_fadeTo);
        }
        else {
            const auto from = static_cast<int32_t>(_fadeFrom);
            const auto delta = static_cast<int32_t>(_fadeTo) - from;
            _setBacklight(static_cast<uint8_t>(from + (delta * static_cast<int32_t>(elapsed)) / static_cast<int32_t>(_fadeDuration)));
        }
    }

    // the idle timers, the display that is turned off is only left by user activity
    if (_powerState != PowerState::STANDBY) {
        const auto idle = _elapsed(now, _lastActivity);
        if (_standbyTimeout && idle >= _standbyTimeout) {
            _setPowerState(PowerState::STANDBY);
        }
        else if (_powerState == PowerState::NORMAL && _powerSavingTimeout && idle >= _powerSavingTimeout) {
            _setPowerState(PowerState::POWER_SAVING);
        }
    }
}

// LVGL is not re-entrant, everything runs from the main loop: the screen manager first, then
// the LVGL timer handler. The touch state is updated by the timer handler, so the brightness
// fade and the idle timers are the last step of the iteration
static void displayLoop()
{
    if (!_testScreenChecked) {
        _testScreenChecked = true;
        if (!screenManager.isReady()) {
            LVGLTestScreen::create();
            __LDBG_printf("no application screens registered, showing the display test screen");
        }
        else {
            screenManager.setOverview(&screenOverview);
        }
    }
#if DEBUG_LVGL_SCREENSHOT
    // performs a pending screenshot (it may switch the screen), LVGL stays in this task
    LVGLDebug::loop();
#endif
    screenManager.tick();
    WT32_SC01::loop();
    _powerLoop();
}

LVGLPlugin::LVGLPlugin() : PluginComponent(PROGMEM_GET_PLUGIN_OPTIONS(LVGLPlugin))
{
    REGISTER_PLUGIN(this, "LVGLPlugin");
}

uint8_t LVGLPlugin::getBrightness()
{
    return _brightness;
}

bool LVGLPlugin::isDimmed()
{
    return _powerState == PowerState::POWER_SAVING;
}

bool LVGLPlugin::isStandby()
{
    return _powerState == PowerState::STANDBY;
}

void LVGLPlugin::notifyActivity()
{
    _notifyActivity(false);
}

void LVGLPlugin::setBrightness(uint8_t percent)
{
    if (percent < DisplayConfig::kMinValueFor_backlight_level) {
        percent = DisplayConfig::kMinValueFor_backlight_level;
    }
    else if (percent > DisplayConfig::kMaxValueFor_backlight_level) {
        percent = DisplayConfig::kMaxValueFor_backlight_level;
    }
    _setBrightnessNow(percent);
    _lastActivity = millis();
}

uint8_t LVGLPlugin::getConfiguredBrightness()
{
    return _percentFromLevel(_configuredLevel);
}

void LVGLPlugin::setConfiguredBrightness(uint8_t percent)
{
    if (percent < DisplayConfig::kMinValueFor_backlight_level) {
        percent = DisplayConfig::kMinValueFor_backlight_level;
    }
    else if (percent > DisplayConfig::kMaxValueFor_backlight_level) {
        percent = DisplayConfig::kMaxValueFor_backlight_level;
    }
    auto &cfg = Display::getWriteableConfig();
    cfg.backlight_level = percent;
    Display::setConfig(cfg);
    __LDBG_printf("screen brightness stored: %u%%", static_cast<unsigned>(percent));
    // reads the value back, leaves the power saving mode and fades to the new level
    _applyBrightness();
}

uint8_t LVGLPlugin::getPowerSavingLevel()
{
    return _percentFromLevel(_powerSavingLevel);
}

void LVGLPlugin::setPowerSavingLevel(uint8_t percent)
{
    if (percent < DisplayConfig::kMinValueFor_power_saving_level) {
        percent = DisplayConfig::kMinValueFor_power_saving_level;
    }
    else if (percent > DisplayConfig::kMaxValueFor_power_saving_level) {
        percent = DisplayConfig::kMaxValueFor_power_saving_level;
    }
    auto &cfg = Display::getWriteableConfig();
    cfg.power_saving_level = percent;
    Display::setConfig(cfg);
    _readDisplayConfig();
    _lastActivity = millis();
    __LDBG_printf("idle brightness stored: %u%%", static_cast<unsigned>(percent));
}

uint32_t LVGLPlugin::getPowerSavingTimeout()
{
    return _powerSavingTimeout / 1000UL;
}

void LVGLPlugin::setPowerSavingTimeout(uint32_t seconds)
{
    if (seconds < DisplayConfig::kMinValueFor_power_saving_timeout) {
        seconds = DisplayConfig::kMinValueFor_power_saving_timeout;
    }
    else if (seconds > DisplayConfig::kMaxValueFor_power_saving_timeout) {
        seconds = DisplayConfig::kMaxValueFor_power_saving_timeout;
    }
    auto &cfg = Display::getWriteableConfig();
    cfg.power_saving_timeout = seconds;
    Display::setConfig(cfg);
    _readDisplayConfig();
    _lastActivity = millis();
    __LDBG_printf("idle timeout stored: %us", static_cast<unsigned>(seconds));
}

uint32_t LVGLPlugin::getStandbyTimeout()
{
    return _standbyTimeout / 1000UL;
}

void LVGLPlugin::setStandbyTimeout(uint32_t seconds)
{
    if (seconds < DisplayConfig::kMinValueFor_standby_timeout) {
        seconds = DisplayConfig::kMinValueFor_standby_timeout;
    }
    else if (seconds > DisplayConfig::kMaxValueFor_standby_timeout) {
        seconds = DisplayConfig::kMaxValueFor_standby_timeout;
    }
    auto &cfg = Display::getWriteableConfig();
    cfg.standby_timeout = seconds;
    Display::setConfig(cfg);
    _readDisplayConfig();
    _lastActivity = millis();
    __LDBG_printf("standby timeout stored: %us", static_cast<unsigned>(seconds));
}

void LVGLPlugin::sleep()
{
    __LDBG_printf("display off requested");
    _lastActivity = millis();
    _setPowerState(PowerState::STANDBY);
}

LVGLScreenManager &LVGLPlugin::screens()
{
    return screenManager;
}

void LVGLPlugin::_applyBrightness(bool fade)
{
    _readDisplayConfig();

    // a configuration change restarts the idle timers and leaves the power saving mode
    _powerState = PowerState::NORMAL;
    _lastActivity = millis();
    if (fade) {
        _fadeToLevel(_configuredLevel);
    }
    else {
        _setBrightnessNow(_percentFromLevel(_configuredLevel));
    }
    __LDBG_printf("brightness=%u%% backlight=%u/255", static_cast<unsigned>(_brightness), static_cast<unsigned>(_backlight));
}

bool LVGLPlugin::_initialize()
{
    if (WT32_SC01::display()) { // already initialized
        return true;
    }
    if (!WT32_SC01::begin()) {
        auto error = WT32_SC01::lastError();
        Logger_error(error ? PrintString(F("LVGL: initializing the display failed: %s"), error) : PrintString(F("LVGL: initializing the display failed")));
        return false;
    }
    Logger_notice(F("LVGL display initialized, %ux%u"), IOT_WT32_SC01_TFT_WIDTH, IOT_WT32_SC01_TFT_HEIGHT);
    return true;
}

void LVGLPlugin::setup(SetupModeType mode, const PluginComponents::DependenciesPtr &dependencies)
{
    // the backlight level is applied by WT32_SC01::begin() as well, so that a failed
    // display init does not leave it at the LVGL default (127/255). The panel is not ready yet
    // and the main loop does the fading, so the level is set without a fade
    _applyBrightness(false);
    if (!_initialize()) {
        return;
    }
#if DEBUG_LVGL_SCREENSHOT
    LVGLDebug::setup();
#endif
    LOOP_FUNCTION_ADD(displayLoop);
}

void LVGLPlugin::reconfigure(const String &source)
{
    __LDBG_printf("source=%s", source.c_str());
    _applyBrightness();
}

void LVGLPlugin::shutdown()
{
    LoopFunctions::remove(displayLoop);
#if DEBUG_LVGL_SCREENSHOT
    LVGLDebug::release();
#endif
    // the input is enabled again, the plugin may be set up a second time
    _fadeActive = false;
    _setInputSuppressed(false);
    screenManager.setOverview(nullptr);
    screenManager.release();
    LVGLTestScreen::clear();
}

void LVGLPlugin::getStatus(Print &output)
{
    if (!WT32_SC01::display()) {
        output.print(F("Display not initialized"));
        if (auto error = WT32_SC01::lastError()) {
            output.print(F(": "));
            output.print(error);
        }
        output.print(HTML_S(br));
        output.printf_P(PSTR("Brightness: %u%% (%u/255)" HTML_S(br)), static_cast<unsigned>(_brightness), static_cast<unsigned>(WT32_SC01::getBacklight()));
        return;
    }

    output.printf_P(PSTR("Display: LGFX_WT32_SC01, ST7796S, %ux%u, initialized" HTML_S(br)), static_cast<unsigned>(IOT_WT32_SC01_TFT_WIDTH), static_cast<unsigned>(IOT_WT32_SC01_TFT_HEIGHT));

    output.printf_P(PSTR("Touch: FT6336U, I2C address 0x%02x, %u points, init %s, %u press(es)"),
        static_cast<unsigned>(IOT_WT32_SC01_TOUCH_I2C_ADDRESS), static_cast<unsigned>(IOT_WT32_SC01_TOUCH_POINTS),
        WT32_SC01::touchReady() ? "ok" : "FAILED", static_cast<unsigned>(WT32_SC01::getTouchCount()));
    int32_t x;
    int32_t y;
    if (WT32_SC01::getLastTouch(x, y)) {
        output.printf_P(PSTR(", last %d,%d"), static_cast<int>(x), static_cast<int>(y));
    }
    output.print(HTML_S(br));

    output.printf_P(PSTR("Brightness: %u%% (%u/255)" HTML_S(br)), static_cast<unsigned>(_brightness), static_cast<unsigned>(WT32_SC01::getBacklight()));

    output.printf_P(PSTR("Power saving: %s, dim %u%% after %us, standby after %us" HTML_S(br)),
        (_powerState == PowerState::NORMAL) ? "active" : ((_powerState == PowerState::POWER_SAVING) ? "dimmed" : "display off"),
        static_cast<unsigned>(_percentFromLevel(_powerSavingLevel)), static_cast<unsigned>(_powerSavingTimeout / 1000), static_cast<unsigned>(_standbyTimeout / 1000));

    output.printf_P(PSTR("UI: %u screen(s)"), static_cast<unsigned>(screenManager.count()));
    if (auto screen = screenManager.getActiveScreen()) {
        output.printf_P(PSTR(", active '%s' (%us)"), screen->getName(), static_cast<unsigned>(screenManager.getActiveTime()));
    }
    else if (screenManager.count()) {
        output.print(F(", none active"));
    }
    output.print(HTML_S(br));
}

void LVGLPlugin::createMenu()
{
    bootstrapMenu.addMenuItem(getFriendlyName(), F("lvgl.html"), navMenu.config);
    #if DEBUG_LVGL_SCREENSHOT
        bootstrapMenu.addMenuItem(F("LVGL Screen"), F("lvgl-screen"), navMenu.util);
    #endif
}
