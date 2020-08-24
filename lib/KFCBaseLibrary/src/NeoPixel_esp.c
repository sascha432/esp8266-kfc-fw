// This is a mash-up of the Due show() code + insights from Michael Miller's
// ESP8266 work for the NeoPixelBus library: github.com/Makuna/NeoPixelBus
// Needs to be a separate .c file to enforce ICACHE_RAM_ATTR execution.
// GPIO16 support https://github.com/sascha432/esp8266-kfc-fw/blob/master/lib/KFCBaseLibrary/src/NeoPixel_esp.c

#if (defined(ESP8266) || defined(ESP32)) && HAVE_NEOPIXEL

#include <Arduino.h>
#ifdef ESP8266
#include <eagle_soc.h>
#include <user_interface.h>
#endif

#include <push_optimize.h>
#pragma GCC optimize ("O2")

void NeoPixel_fillColor(uint8_t *pixels, uint16_t numBytes, uint32_t color)
{
    uint8_t *ptr = pixels;
    for(uint8_t i = 0; i < numBytes; i += 3) {
        *ptr++ = color >> 8;
        *ptr++ = color >> 16;
        *ptr++ = color;
    }
}

#if defined(ESP8266)

// compensation for if (pin == ...)
// high 400-410ns low 800-810ns period 1300ns 769kHz
#define COMP_CYCLES     3

#define gpio_set_level_high() \
    if (pin == 16) { \
        WRITE_PERI_REG(RTC_GPIO_OUT, gpio_set); \
    } else { \
        GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, pinMask); \
    }

#define gpio_set_level_low() \
    if (pin == 16) { \
        WRITE_PERI_REG(RTC_GPIO_OUT, gpio_clear); \
    } else { \
        GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, pinMask); \
    }

#else

#define COMP_CYCLES                     0

#define gpio_set_level_high()           gpio_set_level(pin, HIGH)
#define gpio_set_level_low()            gpio_set_level(pin, LOW)

#endif

#define CYCLES_800_T0H  (F_CPU / 2500000) // 0.4us
#define CYCLES_800_T1H  (F_CPU / 1250000) // 0.8us
#define CYCLES_800      (F_CPU / 800000) // 1.25us per bit
#define CYCLES_400_T0H  (F_CPU / 2000000) // 0.5uS
#define CYCLES_400_T1H  (F_CPU / 833333) // 1.2us
#define CYCLES_400      (F_CPU / 400000) // 2.5us per bit


static uint32_t _getCycleCount(void) __attribute__((always_inline));
static inline uint32_t _getCycleCount(void)
{
    uint32_t ccount;
    __asm__ __volatile__("rsr %0,ccount"
                         : "=a"(ccount));
    return ccount;
}

// ~213 byte IRAM~
// 162 byte

#if defined(ESP8266)
void ICACHE_RAM_ATTR espShow(uint8_t pin, uint8_t *pixels, uint32_t numBytes, uint8_t *p, uint8_t *end, uint32_t time0, uint32_t time1, uint32_t period, uint32_t pinMask, uint32_t gpio_clear, uint32_t gpio_set)
#else
void ICACHE_RAM_ATTR espShow(uint8_t pin, uint8_t *pixels, uint32_t numBytes, uint8_t *p, uint8_t *end, uint32_t time0, uint32_t time1, uint32_t period)
#endif
{
    uint8_t pix, mask;
    uint32_t c, t;
    uint32_t startTime = 0;

    pix = *p++;
    mask = 0x80;

    for (t = time0;; t = time0) {
        if (pix & mask)
            t = time1; // Bit high duration
        while (((c = _getCycleCount()) - startTime) < period)
            ; // Wait for bit start
        gpio_set_level_high();
        startTime = c; // Save start time
        while (((c = _getCycleCount()) - startTime) < t)
            ; // Wait high duration
        gpio_set_level_low();
        if (!(mask >>= 1)) { // Next bit/byte
            if (p >= end)
                break;
            pix = *p++;
            mask = 0x80;
        }
    }
    while ((_getCycleCount() - startTime) < period)
        ; // Wait for last bit
}

void NeoPixel_espShow(uint8_t pin, uint8_t *pixels, uint32_t numBytes, boolean is800KHz)
{
#ifdef ESP8266
    system_update_cpu_freq(SYS_CPU_80MHZ);
#endif

    noInterrupts();

    uint8_t *p, *end;
    uint32_t time0, time1, period, pinMask;

    pinMask = _BV(pin);
    p = pixels;
    end = p + numBytes;

#ifdef NEO_KHZ400
    if (is800KHz) {
#endif
        time0 = CYCLES_800_T0H - COMP_CYCLES;
        time1 = CYCLES_800_T1H - COMP_CYCLES;
        period = CYCLES_800;
#ifdef NEO_KHZ400
    } else { // 400 KHz bitstream
        time0 = CYCLES_400_T0H - COMP_CYCLES;
        time1 = CYCLES_400_T1H - COMP_CYCLES;
        period = CYCLES_400;
    }
#endif

#ifdef ESP8266
    // TODO gpio_set_level_high() / gpio_set_level_low() could be replaced with a single write operator without the
    // if, but that seems to mess with the number of clock cycles. needs some disassembling to figure out what the
    // compiler is doing....
    // WRITE_PERI_REG(gpio_set_address, gpio_set);
    // WRITE_PERI_REG(gpio_clear_address, gpio_clear);

    // uint32_t gpio_clear_address;
    // uint32_t gpio_set_address;
    uint32_t gpio_clear;
    uint32_t gpio_set;
    if (pin == 16) {
        // gpio_clear_address = RTC_GPIO_OUT;
        // gpio_set_address = RTC_GPIO_OUT;
        gpio_clear = (READ_PERI_REG(RTC_GPIO_OUT) & 0xfffffffeU);
        gpio_set = gpio_clear | 1;

    }
    else {
        // gpio_clear_address = PERIPHS_GPIO_BASEADDR + GPIO_OUT_W1TC_ADDRESS;
        // gpio_set_address = PERIPHS_GPIO_BASEADDR + GPIO_OUT_W1TS_ADDRESS;
        gpio_set = pinMask;
        gpio_clear = pinMask;

    }
#endif

#if defined(ESP8266)
    espShow(pin, pixels, numBytes, p, end, time0, time1, period, pinMask, gpio_clear, gpio_set);
#else
    espShow(pin, pixels, numBytes, p, end, time0, time1, period);
#endif

    interrupts();
}

#endif

#include <pop_optimize.h>
