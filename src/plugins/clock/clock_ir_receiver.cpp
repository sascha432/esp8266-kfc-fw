/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <LoopFunctions.h>
#include "clock.h"
#include "EventScheduler.h"
#include "web_server.h"

#if defined(IOT_LED_MATRIX_IR_REMOTE_PIN) && IOT_LED_MATRIX_IR_REMOTE_PIN != -1

#if DEBUG_IOT_CLOCK
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

using namespace Clock;

// ------------------------------------------------------------------------
// Remote control buttons
// ------------------------------------------------------------------------
//
// Nothing is hardcoded, the buttons are assigned in the IR Remote form and an unassigned button
// (IRRemoteConfigType::kNoCode) does nothing.
//
// The repeat frames that the remote sends while a button is held contain no code, they only mean
// "the button is still pressed". The decoder passes the last frame again, the actions decide if they
// can be applied repeatedly (brightness, color channels) or only once (power, animation, color).

void ClockPlugin::_irRemoteCallback(uint32_t code, bool repeat)
{
    if (!IRRemoteConfigType::hasCode(code)) {
        return;
    }
    if (_irLearnMode) {
        // a button is being captured in the WebUI, all actions are disabled and the code is only
        // reported to the browser
        if (!repeat) {
            _irLearnCode = code;
            _irLearnId++;
        }
        return;
    }
    // the callbacks are invoked by the event scheduler while it iterates its callbacks, applying the
    // action from there could modify the timer list (e.g. _saveState()), same as buttonCallback()
    LoopFunctions::callOnce([this, code, repeat]() {
        _irRemoteAction(code, repeat);
    });
}

void ClockPlugin::_irSetLearnMode(bool enable)
{
    __LDBG_printf("learn mode=%u", enable);
    if (enable) {
        _irLearnCode = 0;
        _irLearnId++;
        _irLearnTimeout = millis() + kIRLearnTimeout;
    }
    _irLearnMode = enable;
}

// safety net: if the browser stops polling (e.g. the tab was closed while the dialog was open), the
// actions must not stay disabled
void ClockPlugin::_irLearnTimeoutCheck()
{
    if (_irLearnMode && static_cast<int32_t>(millis() - _irLearnTimeout) >= 0) {
        __LDBG_printf("learn mode timeout");
        _irSetLearnMode(false);
    }
}

// returns the state of the receiver as JSON:
//  learn   1 while the browser captures a button (all actions are disabled)
//  id      incremented for every frame received while learning (a new value means a new code)
//  code    the code of the last frame received while learning, 00000000 if none was received
//  frames  number of frames received
//  repeats number of repeat frames received (the button is still held)
void ClockPlugin::_irWebHandler(AsyncWebServerRequest *request)
{
    auto &plugin = getInstance();

    if (!WebServer::Plugin::isAuthenticated(request)) {
        request->send(403);
        return;
    }

    const auto action = request->arg(F("action"));
    if (action == F("learn")) {
        plugin._irSetLearnMode(true);
    }
    else if (action == F("stop")) {
        plugin._irSetLearnMode(false);
    }
    else if (plugin._irLearnMode) {
        // the browser is polling, keep learning enabled for as long as the dialog is open
        plugin._irLearnTimeout = millis() + kIRLearnTimeout;
    }

    PrintString json;
    json.printf_P(PSTR("{\"learn\":%u,\"id\":%lu,\"code\":\"%08x\",\"frames\":%u,\"repeats\":%u}"),
        plugin._irLearnMode ? 1U : 0U,
        static_cast<unsigned long>(plugin._irLearnId),
        static_cast<unsigned int>(plugin._irLearnCode),
        plugin._irFrameCount,
        plugin._irRepeatCount
    );

    HttpHeaders headers;
    headers.addNoCache();
    auto response = request->beginResponse(200, FSPGM(mime_application_json), json);
    headers.setResponseHeaders(response);
    request->send(response);
}

void ClockPlugin::_registerIRRemoteWebHandler()
{
    __LDBG_printf("registering handler");
    WebServer::Plugin::addHandler(F("/ir-remote.json"), [](AsyncWebServerRequest *request) {
        ClockPlugin::_irWebHandler(request);
    });
}

void ClockPlugin::_irRemoteAction(uint32_t code, bool repeat)
{
    using ActionType = IRRemoteConfigType::ActionType;
    const auto &ir = _config.ir;

    if (_irLearnMode) {
        // the code was captured after the action has been queued
        __LDBG_printf("IR action skipped, learn mode");
        return;
    }

    if (code == ir.getCode(ActionType::BRIGHTNESS_UP) || code == ir.getCode(ActionType::BRIGHTNESS_DOWN)) {
        // the first frame is a click, the repeats change the level in smaller steps
        const int step = repeat ? kBrightnessChangeHold : kBrightnessChangeClick;
        if (code == ir.getCode(ActionType::BRIGHTNESS_UP)) {
            __LDBG_printf("IR brightness %u + %d", _targetBrightness, step);
            setBrightness(std::min<int>(255, _targetBrightness + step));
        }
        else {
            __LDBG_printf("IR brightness %u - %d", _targetBrightness, step);
            setBrightness(std::max<int>(1, _targetBrightness - step));
        }
        return;
    }

    if (code == ir.getCode(ActionType::POWER)) {
        if (!repeat) {
            __LDBG_printf("IR power on=%u", !_config.enabled);
            _setState(!_config.enabled);
        }
        return;
    }

    if (code == ir.getCode(ActionType::NEXT_ANIMATION)) {
        if (!repeat) {
            __LDBG_printf("IR next animation");
            nextAnimation();
        }
        return;
    }

    // color channels, a button held down ramps the level
    for(uint8_t channel = 0; channel < IRRemoteConfigType::kNumChannels; channel++) {
        const auto up = static_cast<ActionType>(static_cast<uint8_t>(ActionType::RED_UP) + channel);
        const auto down = static_cast<ActionType>(static_cast<uint8_t>(ActionType::RED_DOWN) + channel);
        if (code == ir.getCode(up) || code == ir.getCode(down)) {
            const bool isUp = (code == ir.getCode(up));
            Color color = _getColor();
            const int value = (isUp ? 1 : -1) * static_cast<int>(ir.step);
            switch(channel) {
                case 0:
                    color.red() = static_cast<uint8_t>(std::clamp<int>(color.red() + value, 0, 255));
                    break;
                case 1:
                    color.green() = static_cast<uint8_t>(std::clamp<int>(color.green() + value, 0, 255));
                    break;
                default:
                    color.blue() = static_cast<uint8_t>(std::clamp<int>(color.blue() + value, 0, 255));
                    break;
            }
            __LDBG_printf("IR color %06x (%c%u)", static_cast<uint32_t>(color), isUp ? '+' : '-', ir.step);
            setColorAndRefresh(color);
            _saveState();
            return;
        }
    }

    // color buttons
    for(uint8_t i = 0; i < IRRemoteConfigType::kNumColors; i++) {
        const auto action = static_cast<ActionType>(static_cast<uint8_t>(ActionType::COLOR_1) + i);
        if (code == ir.getCode(action)) {
            if (!repeat) {
                __LDBG_printf("IR color %06x", ir.getColor(action));
                // only the color is changed, the animation keeps running
                setColorAndRefresh(Color(ir.getColor(action)));
                _saveState();
            }
            return;
        }
    }
}

#if ESP32

// ------------------------------------------------------------------------
// ESP32 software NEC receiver
// ------------------------------------------------------------------------
//
// The output of the IR receiver module (TSOP/VS1838B) is idle high and pulled low while the 38kHz
// carrier is received, a falling edge starts a mark/burst and a rising edge ends it.
//
// FastLED's RMT driver claims all RMT channels and memory blocks, IRremoteESP8266's ESP32 backend
// is RMT based and cannot be used, therefore the remote control is decoded in software.
//
// The interrupt only records the time between two edges into a buffer, the pulse lengths are
// decoded by the 150ms timer callback (_irDecodeNec()). This is a second implementation of the
// NEC protocol in addition to IRremoteESP8266, it only supports NEC.
//
// NEC protocol, all durations are microseconds:
//   lead      mark 9000 + space 4500
//   bits      mark 560 + space (560 = 0, 1690 = 1), LSB first, 32 bits
//   stop      mark 560
//   repeat    mark 9000 + space 2250 + mark 560 (sent every 110ms while a key is held)
//
// The recorded pulses contain the idle time before and after a frame, those are not NEC pulses
// and get skipped while searching for the next lead mark.

namespace {

    constexpr uint16_t kNecLeadMark = 9000;
    constexpr uint16_t kNecLeadSpace = 4500;
    constexpr uint16_t kNecRepeatSpace = 2250;
    constexpr uint16_t kNecBitMark = 560;
    constexpr uint16_t kNecBitZeroSpace = 560;
    constexpr uint16_t kNecBitOneSpace = 1690;

    // the NEC timings have a tolerance of +/-30%, the software sampling adds jitter
    constexpr uint8_t kNecTolerance = 30;

    inline uint16_t necMin(uint16_t value)
    {
        return static_cast<uint16_t>(value - (value * kNecTolerance / 100));
    }

    inline uint16_t necMax(uint16_t value)
    {
        return static_cast<uint16_t>(value + (value * kNecTolerance / 100));
    }

    inline bool isNecPulse(uint16_t duration, uint16_t value)
    {
        return duration >= necMin(value) && duration <= necMax(value);
    }

}

ClockPlugin::NecResult ClockPlugin::_irParseNecFrame(const uint16_t *edges, uint16_t count, uint16_t index, uint16_t &next, uint32_t &code, bool &repeat)
{
    if (!isNecPulse(edges[index], kNecLeadMark)) {
        return NecResult::INVALID;
    }
    if (count - index < 2) {
        return NecResult::INCOMPLETE;
    }
    // the lead space separates a repeat frame (2250us) from a data frame (4500us)
    if (isNecPulse(edges[index + 1], kNecRepeatSpace)) {
        if (count - index < 3) {
            return NecResult::INCOMPLETE;
        }
        if (!isNecPulse(edges[index + 2], kNecBitMark)) {
            return NecResult::INVALID;
        }
        repeat = true;
        next = index + 3;
        return NecResult::VALID;
    }
    if (!isNecPulse(edges[index + 1], kNecLeadSpace)) {
        return NecResult::INVALID;
    }
    if (count - index < kNecFrameEdges) {
        return NecResult::INCOMPLETE;
    }
    uint32_t value = 0;
    for(uint8_t bit = 0; bit < 32; bit++) {
        if (!isNecPulse(edges[index + 2 + bit * 2], kNecBitMark)) {
            return NecResult::INVALID;
        }
        const uint16_t space = edges[index + 3 + bit * 2];
        if (isNecPulse(space, kNecBitZeroSpace)) {
            // bit is zero
        }
        else if (isNecPulse(space, kNecBitOneSpace)) {
            value |= 1UL << bit;
        }
        else {
            return NecResult::INVALID;
        }
    }
    if (!isNecPulse(edges[index + kNecFrameEdges - 1], kNecBitMark)) {
        return NecResult::INVALID; // stop bit
    }
    // the command is repeated inverted in standard and extended NEC frames, a frame without it is
    // not NEC (or corrupt). a valid code is never 0 either, see IRRemoteConfigType::kNoCode
    if ((static_cast<uint8_t>(value >> 24) ^ static_cast<uint8_t>(value >> 16)) != 0xff) {
        return NecResult::INVALID;
    }
    code = value;
    repeat = false;
    next = index + kNecFrameEdges;
    return NecResult::VALID;
}

void IRAM_ATTR ClockPlugin::_irPinISR()
{
    auto &plugin = getInstance();

    const uint32_t now = micros();
    const uint32_t duration = now - plugin._irLastEdge;
    if (duration < kIRGlitchFilter) {
        return;
    }
    plugin._irLastEdge = now;

    if (plugin._irOverflow || plugin._irEdgeCount >= kIREdgeBufferSize) {
        plugin._irOverflow = true;
        return;
    }
    plugin._irEdges[plugin._irEdgeCount] = duration > 0xffffU ? 0xffffU : static_cast<uint16_t>(duration);
    plugin._irEdgeCount++;
}

void ClockPlugin::_irReset()
{
    InterruptLock lock;
    _irEdgeCount = 0;
    _irOverflow = false;
    _irLastEdge = micros();
}

void ClockPlugin::_irDecodeNec()
{
    if (!_irEdgeCount && !_irOverflow) {
        return;
    }

    // the frame has been received completely if the input did not change for a while, otherwise
    // the recorded pulses end in the middle of a frame and the tail is kept for the next call
    const bool idle = static_cast<uint32_t>(micros() - _irLastEdge) >= kIRFrameIdleTime;

    uint16_t edges[kIREdgeBufferSize];
    uint16_t count;
    bool overflow;
    {
        InterruptLock lock;
        count = _irEdgeCount;
        overflow = _irOverflow;
        for(uint16_t i = 0; i < count; i++) {
            edges[i] = _irEdges[i];
        }
    }
    if (overflow) {
        __LDBG_printf("IR pulse buffer overflow, %u pulses", count);
    }

    uint16_t index = 0;
    while (index < count) {
        uint16_t next = 0;
        uint32_t code = 0;
        bool repeat = false;
        const auto result = _irParseNecFrame(edges, count, index, next, code, repeat);
        if (result == NecResult::INVALID) {
            // not a NEC pulse, search for the next lead mark
            index++;
            continue;
        }
        if (result == NecResult::INCOMPLETE) {
            // the frame is not complete yet, keep it if it is still being received
            break;
        }
        if (repeat) {
            _irRepeatCount++;
            __LDBG_printf("IR repeat");
            _irRemoteCallback(_irLastCode, true);
        }
        else {
            _irLastCode = code;
            _irFrameCount++;
            __LDBG_printf("IR %08x", code);
            _irRemoteCallback(code, false);
        }
        index = next;
    }

    // remove the processed pulses and keep the incomplete frame for the next call
    const uint16_t retain = idle ? 0 : static_cast<uint16_t>(count - index);
    InterruptLock lock;
    const uint16_t appended = static_cast<uint16_t>(_irEdgeCount - count);
    if (!retain) {
        for(uint16_t i = 0; i < appended; i++) {
            _irEdges[i] = _irEdges[count + i];
        }
    }
    else {
        for(uint16_t i = 0; i < appended; i++) {
            _irEdges[retain + i] = _irEdges[count + i];
        }
        for(uint16_t i = 0; i < retain; i++) {
            _irEdges[i] = edges[index + i];
        }
    }
    _irEdgeCount = retain + appended;
    _irOverflow = false;
}

void ClockPlugin::beginIRReceiver()
{
    endIRReceiver();
    if (!_config.ir.enabled) {
        __LDBG_printf("IR receiver disabled");
        return;
    }
    __LDBG_printf("begin (software receiver pin %u)", IOT_LED_MATRIX_IR_REMOTE_PIN);
    pinMode(IOT_LED_MATRIX_IR_REMOTE_PIN, INPUT_PULLUP);
    _irReset();
    attachInterrupt(digitalPinToInterrupt(IOT_LED_MATRIX_IR_REMOTE_PIN), _irPinISR, CHANGE);
    _Timer(_irTimer).add(Event::milliseconds(150), true, [this](Event::CallbackTimerPtr) {
        _irLearnTimeoutCheck();
        _irDecodeNec();
    });
}

void ClockPlugin::endIRReceiver()
{
    __LDBG_printf("end");
    _Timer(_irTimer).remove();
    detachInterrupt(digitalPinToInterrupt(IOT_LED_MATRIX_IR_REMOTE_PIN));
    _irReset();
}

#else

void ClockPlugin::beginIRReceiver()
{
    if (_irReceiver) {
        endIRReceiver();
    }
    _irReceiver = new IRrecv(IOT_LED_MATRIX_IR_REMOTE_PIN);
    __LDBG_printf("begin v" _IRREMOTEESP8266_VERSION_STR);
    // _irReceiver->setTolerance(25);
    _irReceiver->enableIRIn();
    _Timer(_irTimer).add(Event::milliseconds(150), true, [this](Event::CallbackTimerPtr) {
        _irLearnTimeoutCheck();
        if (_irReceiver->decode(&_irResults)) {
            if (_irResults.repeat) {
                _irRepeatCount++;
                __LDBG_printf("IR repeat");
                _irRemoteCallback(_irLastCode, true);
            }
            else {
                if (_irResults.value) {
                    _irLastCode = _irResults.value;
                    _irFrameCount++;
                }
                __LDBG_printf("IR %08x", _irResults.value);
                _irRemoteCallback(_irResults.value, false);
            }
            _irReceiver->resume();
        }
    });
}

void ClockPlugin::endIRReceiver()
{
    __LDBG_printf("end");
    _Timer(_irTimer).remove();
    if (_irReceiver) {
        _irReceiver->disableIRIn();
        delete _irReceiver;
        _irReceiver = nullptr;
    }
}

#endif

#endif
