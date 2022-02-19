/**
 * Author: sascha_lammers@gmx.de
 */

#include "BlindsControl.h"

void BlindsControl::_startToneTimer(uint32_t timeout)
{
    _stopToneTimer();
    if ((_config.play_tone_channel & 0x03) == 0) {
        __LDBG_printf("tone timer disabled");
        return;
    }

    uint8_t pins[2] = { kInvalidPin, kInvalidPin };
    auto pinPtr = &pins[0];
    if (_config.play_tone_channel & 0x01) {
        *pinPtr++ = _config.pins[0 * kChannelCount];
    }
    if (_config.play_tone_channel & 0x02) {
        *pinPtr = _config.pins[1 * kChannelCount];
    }

    // stored in the callback lambda function
    ToneSettings settings(_config.tone_frequency, _config.tone_pwm_value, pins, timeout);

    __LDBG_printf("start tone timer pin=%u,%u states=%s,%s freq=%u pwm=%u interval=%ums tone=%ums max_duration=%.1fs runtime=%u",
        settings.pin[0],
        settings.pin[1],
        _states[0]._getFPStr(),
        _states[1]._getFPStr(),
        settings.frequency,
        settings.pwmValue,
        settings.interval,
        ToneSettings::kToneDuration,
        ToneSettings::kMaxLength / 1000.0,
        settings.runtime
    );

    _setDac(PWMRANGE);

    _Timer(_toneTimer).add(ToneSettings::kTimerInterval, true, [this, settings](Event::CallbackTimerPtr timer) mutable {
        auto interval = settings.runtime ? std::clamp<uint16_t>(ToneSettings::kToneInterval - (((settings.counter * (ToneSettings::kToneInterval - ToneSettings::kToneMinInterval)) / (settings.runtime / ToneSettings::kTimerInterval)) + ToneSettings::kToneMinInterval), ToneSettings::kToneMinInterval, ToneSettings::kToneInterval) : settings.interval;
        // __LDBG_printf("_toneTimer loop=%u counter=%u/%u interval=%u interval_loops=%u", loop, settings.counter, settings.runtime / ToneSettings::kTimerInterval, interval, interval / ToneSettings::kTimerInterval);
        if (settings.loop == 0) {
            // __LDBG_printf("_toneTimer ON counter=%u interval=%u", settings.counter, interval);
            // start tone
            _playTone(settings.pin.data(), settings.pwmValue, settings.frequency);
        }
        else if (settings.loop == ToneSettings::kToneMaxLoop) {
            // stop tone
            // __LDBG_printf("_toneTimer OFF loop=%u/%u", settings.loop, (interval / ToneSettings::kTimerInterval));
            analogWrite(settings.pin[0], 0);
#if !defined(ESP8266)
            // analogWrite checks the pin
            if (settings.hasPin2())
#endif
            {
                analogWrite(settings.pin[1], 0);
            }
        }
        if (++settings.loop > (interval / ToneSettings::kTimerInterval)) {
            // __LDBG_printf("_toneTimer IVAL counter=%u", settings.counter);
            settings.loop = 0;
        }
        // max. limit
        if (++settings.counter > (ToneSettings::kMaxLength / ToneSettings::kTimerInterval)) {
            __LDBG_printf("_toneTimer max. timeout reached");
            timer->disarm();
            LoopFunctions::callOnce([this]() {
                _stop();
            });
            return;
        }
    });
}

#if HAVE_IMPERIAL_MARCH

    void BlindsControl::_playImperialMarch(uint16_t speed, int8_t zweiklang, uint8_t repeat)
    {
        _stopToneTimer();
        if ((_config.play_tone_channel & 0x03) == 0) {
            __LDBG_printf("tone timer disabled");
            return;
        }
        uint8_t channel = (_config.play_tone_channel - 1) % kChannelCount;
        uint8_t channel2 = (channel + 1) % kChannelCount;
        uint8_t index = (channel * kChannelCount) + (_states[channel].isClosed() ? 1 : 0);
        uint8_t index2 = (channel2 * kChannelCount) + (_states[channel2].isClosed() ? 1 : 0);
        struct {
            uint16_t pwmValue;
            uint16_t counter;
            uint16_t nextNote;
            uint16_t speed;
            uint8_t notePosition;
            uint8_t pin;
            uint8_t pin2;
            uint8_t repeat;
            int8_t zweiklang;
            uint8_t offDelayCounter;
        } settings = {
            (uint16_t)_config.tone_pwm_value,
            0,  // counter
            0,  // nextNote
            speed,
            0,  // notePosition
            _config.pins[index], // use channel 1, and use open/close pin that is jammed
            _config.pins[index2], // use channel 1, and use open/close pin that is jammed
            repeat,
            zweiklang,
            0   // offDelayCounter
        };

        _setDac(PWMRANGE);

        __LDBG_printf("imperial march length=%u speed=%u zweiklang=%d repeat=%u pin=%u pin2=%u", sizeof(imperial_march_notes), settings.speed, settings.zweiklang, settings.repeat, settings.pin, settings.pin2);

        _Timer(_toneTimer).add(settings.speed, true, [this, settings](Event::CallbackTimerPtr timer) mutable {

            if (settings.offDelayCounter) {
                if (--settings.offDelayCounter == 0 || settings.pwmValue <= settings.repeat) {
                    _stop();
                    return;
                }
                auto note = pgm_read_byte(imperial_march_notes + IMPERIAL_MARCH_NOTES_COUNT - 1);
                _playNote(settings.pin, settings.pwmValue, note);
                if (settings.zweiklang) {
                    _playNote(settings.pin2, settings.pwmValue, note + settings.zweiklang);
                }
                settings.pwmValue -= settings.repeat;
                return;
            }

            if (settings.notePosition >= IMPERIAL_MARCH_NOTES_COUNT) {
                if (settings.repeat-- == 0) {
                    settings.offDelayCounter = (4000 / settings.speed) + 1; // fade last node within 4 seconds
                    settings.repeat = (settings.pwmValue / settings.offDelayCounter) + 1; // decrease pwm value steps
                    return;
                }
                else {
                    switch(settings.zweiklang) {
                        case 12:
                            settings.zweiklang = -12;
                            break;
                        case 7:
                            settings.zweiklang = 12;
                            break;
                        case 5:
                            settings.zweiklang = 7;
                            break;
                        case 0:
                            settings.zweiklang = 5;
                            break;
                        case -5:
                            settings.zweiklang = 0;
                            break;
                        case -7:
                            settings.zweiklang = -5;
                            break;
                        case -12:
                            settings.zweiklang = -7;
                            break;
                        default:
                            settings.zweiklang = 0;
                            break;
                    }
                    __LDBG_printf("repeat imperial=%u zweiklang=%d", settings.repeat, settings.zweiklang);
                    settings.notePosition = 0;
                    settings.counter = 0;
                    settings.nextNote = 0;
                }
            }

            if (settings.counter++ >= settings.nextNote) {
                auto note = pgm_read_byte(imperial_march_notes + settings.notePosition);
                auto length = pgm_read_byte(imperial_march_lengths + settings.notePosition);

                analogWrite(settings.pin, 0);
                analogWrite(settings.pin2, 0);
                delay(5);

                _playNote(settings.pin, settings.pwmValue, note);
                if (settings.zweiklang) {
                    _playNote(settings.pin2, settings.pwmValue, note + settings.zweiklang);
                }

                settings.nextNote = settings.counter + length;
                settings.notePosition++;

            }
        });
    }

#endif
