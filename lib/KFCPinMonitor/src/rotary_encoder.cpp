/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <MicrosTimer.h>
#include "pin_monitor.h"

#if DEBUG_PIN_MONITOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using namespace PinMonitor;

RotaryEncoderPin::RotaryEncoderPin(uint8_t pin, RotaryEncoderDirection direction, RotaryEncoder *encoder, ActiveStateType state) :
    Pin(pin, encoder, StateType::UP_DOWN, state),
    _direction(direction)
{
    #if DEBUG
        if (pin >= NUM_DIGITAL_PINS) {
            __DBG_panic("invalid pin=%u", pin);
        }
    #endif
}

RotaryEncoderPin::~RotaryEncoderPin()
{
    if (_direction == RotaryEncoderDirection::LAST) {
        delete reinterpret_cast<RotaryEncoder *>(const_cast<void *>(getArg()));
    }
}

void RotaryEncoderPin::loop()
{
    // if (_direction == RotaryEncoderDirection::LAST) {
    //     // reinterpret_cast<RotaryEncoder *>(const_cast<void *>(getArg()))->loop();
    // }
}

void RotaryEncoder::attachPins(uint8_t pin1, uint8_t pin2)
{
    pinMonitor.attachPinType<RotaryEncoderPin>(HardwarePinType::ROTARY, pin1, RotaryEncoderDirection::LEFT, this, _activeState);
    pinMonitor.attachPinType<RotaryEncoderPin>(HardwarePinType::ROTARY, pin2, RotaryEncoderDirection::RIGHT, this, _activeState);
    static_assert(RotaryEncoderDirection::RIGHT == RotaryEncoderDirection::LAST, "LAST must be added last");
    _mask1 = _BV(pin1);
    _mask2 = _BV(pin2);
    pinMode(pin1, PinMonitor::pinMonitor.getPinMode());
    pinMode(pin2, PinMonitor::pinMonitor.getPinMode());
}

// https://github.com/buxtronix/arduino/blob/master/libraries/Rotary/Rotary.cpp

/* Rotary encoder handler for arduino. v1.1
 *
 * Copyright 2011 Ben Buxton. Licenced under the GNU GPL Version 3.
 * Contact: bb@cactii.net
 *
 * A typical mechanical rotary encoder emits a two bit gray code
 * on 3 output pins. Every step in the output (often accompanied
 * by a physical 'click') generates a specific sequence of output
 * codes on the pins.
 *
 * There are 3 pins used for the rotary encoding - one common and
 * two 'bit' pins.
 *
 * The following is the typical sequence of code on the output when
 * moving from one step to the next:
 *
 *   Position   Bit1   Bit2
 *   ----------------------
 *     Step1     0      0
 *      1/4      1      0
 *      1/2      1      1
 *      3/4      0      1
 *     Step2     0      0
 *
 * From this table, we can see that when moving from one 'click' to
 * the next, there are 4 changes in the output code.
 *
 * - From an initial 0 - 0, Bit1 goes high, Bit0 stays low.
 * - Then both bits are high, halfway through the step.
 * - Then Bit1 goes low, but Bit2 stays high.
 * - Finally at the end of the step, both bits return to 0.
 *
 * Detecting the direction is easy - the table simply goes in the other
 * direction (read up instead of down).
 *
 * To decode this, we use a simple state machine. Every time the output
 * code changes, it follows state, until finally a full steps worth of
 * code is received (in the correct order). At the final 0-0, it returns
 * a value indicating a step in one direction or the other.
 *
 * It's also possible to use 'half-step' mode. This just emits an event
 * at both the 0-0 and 1-1 positions. This might be useful for some
 * encoders where you want to detect all positions.
 *
 * If an invalid state happens (for example we go from '0-1' straight
 * to '1-0'), the state machine resets to the start until 0-0 and the
 * next valid codes occur.
 *
 * The biggest advantage of using a state machine over other algorithms
 * is that this has inherent debounce built in. Other algorithms emit spurious
 * output with switch bounce, but this one will simply flip between
 * sub-states until the bounce settles, then continue along the state
 * machine.
 * A side effect of debounce is that fast rotations can cause steps to
 * be skipped. By not requiring debounce, fast rotations can be accurately
 * measured.
 * Another advantage is the ability to properly handle bad state, such
 * as due to EMI, etc.
 * It is also a lot simpler than others - a static state table and less
 * than 10 lines of logic.
 */

// #include "Arduino.h"
// #include "Rotary.h"

/*
 * The below state table has, for each state (row), the new state
 * to set based on the next encoder output. From left to right in,
 * the table, the encoder outputs are 00, 01, 10, 11, and the value
 * in that position is the new state to set.
 */

#define R_START 0x0

	// Values returned by 'process'
// No complete step yet.
#define DIR_NONE 0x0
// Clockwise step.
#define DIR_CW 0x10
// Anti-clockwise step.
#define DIR_CCW 0x20

//#define HALF_STEP

#ifdef HALF_STEP
// Use the half-step state table (emits a code at 00 and 11)
#define R_CCW_BEGIN 0x1
#define R_CW_BEGIN 0x2
#define R_START_M 0x3
#define R_CW_BEGIN_M 0x4
#define R_CCW_BEGIN_M 0x5
constexpr uint32_t ttable[6][4] PROGMEM = {
  // R_START (00)
  {R_START_M,            R_CW_BEGIN,     R_CCW_BEGIN,  R_START},
  // R_CCW_BEGIN
  {R_START_M | DIR_CCW, R_START,        R_CCW_BEGIN,  R_START},
  // R_CW_BEGIN
  {R_START_M | DIR_CW,  R_CW_BEGIN,     R_START,      R_START},
  // R_START_M (11)
  {R_START_M,            R_CCW_BEGIN_M,  R_CW_BEGIN_M, R_START},
  // R_CW_BEGIN_M
  {R_START_M,            R_START_M,      R_CW_BEGIN_M, R_START | DIR_CW},
  // R_CCW_BEGIN_M
  {R_START_M,            R_CCW_BEGIN_M,  R_START_M,    R_START | DIR_CCW},
};
#else
// Use the full-step state table (emits a code at 00 only)
#define R_CW_FINAL 0x1
#define R_CW_BEGIN 0x2
#define R_CW_NEXT 0x3
#define R_CCW_BEGIN 0x4
#define R_CCW_FINAL 0x5
#define R_CCW_NEXT 0x6

const uint8_t ttable_P[7][4] PROGMEM = {
  // R_START
  {R_START,    R_CW_BEGIN,  R_CCW_BEGIN, R_START},
  // R_CW_FINAL
  {R_CW_NEXT,  R_START,     R_CW_FINAL,  R_START | DIR_CW},
  // R_CW_BEGIN
  {R_CW_NEXT,  R_CW_BEGIN,  R_START,     R_START},
  // R_CW_NEXT
  {R_CW_NEXT,  R_CW_BEGIN,  R_CW_FINAL,  R_START},
  // R_CCW_BEGIN
  {R_CCW_NEXT, R_START,     R_CCW_BEGIN, R_START},
  // R_CCW_FINAL
  {R_CCW_NEXT, R_CCW_FINAL, R_START,     R_START | DIR_CCW},
  // R_CCW_NEXT
  {R_CCW_NEXT, R_CCW_FINAL, R_CCW_BEGIN, R_START},
};
#endif


void RotaryEncoder::processEvent(const Interrupt::Event &eventData)
{
    // uint8_t value = ((eventData.gpiRegValue() & _mask1) != 0) | (((eventData.gpiRegValue() & _mask2) != 0) << 1);
    uint8_t value = (eventData.gpiRegValue() & _mask1) ? 0b01 : 0b00;
    if (eventData.gpiRegValue() & _mask2) {
        value |= 0b10;
    }
    uint8_t state = (_activeState == ActiveStateType::ACTIVE_LOW) ? value ^ 0b11 : value;

    uint8_t ttable[7][4];
    memcpy_P(ttable, ttable_P, sizeof(ttable));

    // __DBG_printf("rotary state=0b%u%u _state=0x%02x/%u -> 0x%02x", state&1, state>>1, _state&0xf, state, ttable[_state & 0xf][state]);

    _state = ttable[_state & 0xf][state];
    if (_state >= DIR_CW) {
        event(static_cast<EventType>(_state), eventData.getTime());
    }
}
