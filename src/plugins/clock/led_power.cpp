/**
 * Author: sascha_lammers@gmx.de
 */

#include "led_power.h"
#if ESP32
#    include <esp_attr.h>
#    include <soc/gpio_struct.h>
#endif

#if ESP32
    // the panic handler runs with the flash cache possibly disabled
#    define LED_POWER_CRASH_ATTR IRAM_ATTR
#else
    // the ESP8266 crash callback runs from flash anyway, keep the IRAM free
#    define LED_POWER_CRASH_ATTR
#endif

namespace Clock {

    namespace LedPower {

        void LED_POWER_CRASH_ATTR crashOff()
        {
            #if IOT_LED_MATRIX_STANDBY_PIN != -1
                constexpr bool kOffLevel = IOT_LED_MATRIX_STANDBY_PIN_STATE(false) == HIGH;
                #if ESP32
                    #if IOT_LED_MATRIX_STANDBY_PIN < 32
                        constexpr uint32_t kMask = 1UL << IOT_LED_MATRIX_STANDBY_PIN;
                        if (kOffLevel) {
                            GPIO.out_w1ts = kMask;
                        }
                        else {
                            GPIO.out_w1tc = kMask;
                        }
                    #else
                        constexpr uint32_t kMask = 1UL << (IOT_LED_MATRIX_STANDBY_PIN - 32);
                        if (kOffLevel) {
                            GPIO.out1_w1ts.val = kMask;
                        }
                        else {
                            GPIO.out1_w1tc.val = kMask;
                        }
                    #endif
                #elif ESP8266
                    #if IOT_LED_MATRIX_STANDBY_PIN == 16
                        if (kOffLevel) {
                            GP16O |= 1;
                        }
                        else {
                            GP16O &= ~1;
                        }
                    #else
                        constexpr uint32_t kMask = 1UL << IOT_LED_MATRIX_STANDBY_PIN;
                        if (kOffLevel) {
                            GPOS = kMask;
                        }
                        else {
                            GPOC = kMask;
                        }
                    #endif
                #endif
            #endif
        }

    }

}

#if ESP32 && IOT_LED_MATRIX_STANDBY_PIN != -1

// requires -Wl,--wrap=esp_panic_handler (conf/envs/wled_board.ini). The panic handler is called for exceptions,
// abort(), the task and the interrupt watchdog, before the core dump is written. Without the linker flag this
// function is not referenced and the LEDs stay on until the reset
extern "C" void __real_esp_panic_handler(void *info);

extern "C" void IRAM_ATTR __wrap_esp_panic_handler(void *info)
{
    Clock::LedPower::crashOff();
    __real_esp_panic_handler(info);
}

#endif
