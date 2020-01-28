// This is a mash-up of the Due show() code + insights from Michael Miller's
// ESP8266 work for the NeoPixelBus library: github.com/Makuna/NeoPixelBus
// Needs to be a separate .c file to enforce ICACHE_RAM_ATTR execution.
// GPIO16 support

#if (defined(ESP8266) || defined(ESP32)) && HAVE_NEOPIXEL

#include <Arduino.h>
#ifdef ESP8266
#include <eagle_soc.h>
#endif

#pragma GCC optimize ("O2")

void NeoPixel_setColor(uint8_t *pixels, uint32_t color)
{
    //GRB
    *pixels++ = color >> 8;
    *pixels = color >> 16;
    *pixels++ = color;
}

void NeoPixel_fillColor(uint8_t *pixels, uint16_t numBytes, uint32_t color)
{
    uint8_t *ptr = pixels;
    for(uint8_t i = 0; i < numBytes; i += 3) {
        *ptr++ = color >> 8;
        *ptr++ = color >> 16;
        *ptr++ = color;
    }
}

static uint32_t _getCycleCount(void) __attribute__((always_inline));
static inline uint32_t _getCycleCount(void)
{
    uint32_t ccount;
    __asm__ __volatile__("rsr %0,ccount"
                         : "=a"(ccount));
    return ccount;
}

void ICACHE_RAM_ATTR NeoPixel_espShow(uint8_t pin, uint8_t *pixels, uint32_t numBytes, boolean is800KHz) {

#define CYCLES_800_T0H (F_CPU / 2500000) // 0.4us
#define CYCLES_800_T1H (F_CPU / 1250000) // 0.8us
#define CYCLES_800 (F_CPU / 800000) // 1.25us per bit
#define CYCLES_400_T0H (F_CPU / 2000000) // 0.5uS
#define CYCLES_400_T1H (F_CPU / 833333) // 1.2us
#define CYCLES_400 (F_CPU / 400000) // 2.5us per bit

    uint8_t *p, *end, pix, mask;
    uint32_t t, time0, time1, period, c, startTime, pinMask;

    pinMask = _BV(pin);
    p = pixels;
    end = p + numBytes;
    pix = *p++;
    mask = 0x80;
    startTime = 0;

#ifdef NEO_KHZ400
    if (is800KHz) {
#endif
        time0 = CYCLES_800_T0H;
        time1 = CYCLES_800_T1H;
        period = CYCLES_800;
#ifdef NEO_KHZ400
    } else { // 400 KHz bitstream
        time0 = CYCLES_400_T0H;
        time1 = CYCLES_400_T1H;
        period = CYCLES_400;
    }
#endif

#ifdef ESP8266
    if (pin == 16) {
        // GPIO16 requires some pre-calculations to be fast enough
        uint32_t gpio_clear = (READ_PERI_REG(RTC_GPIO_OUT) & (uint32)0xfffffffe);
        uint32_t gpio_set = gpio_clear |= 1;

        for (t = time0;; t = time0) {
            if (pix & mask)
                t = time1; // Bit high duration
            while (((c = _getCycleCount()) - startTime) < period)
                ; // Wait for bit start
            WRITE_PERI_REG(RTC_GPIO_OUT, gpio_set);
            startTime = c; // Save start time
            while (((c = _getCycleCount()) - startTime) < t)
                ; // Wait high duration
            WRITE_PERI_REG(RTC_GPIO_OUT, gpio_clear);
            if (!(mask >>= 1)) { // Next bit/byte
                if (p >= end)
                    break;
                pix = *p++;
                mask = 0x80;
            }
        }
    }
    else {

        for (t = time0;; t = time0) {
            if (pix & mask)
                t = time1; // Bit high duration
            while (((c = _getCycleCount()) - startTime) < period)
                ; // Wait for bit start
            GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, pinMask);       // Set high
            startTime = c; // Save start time
            while (((c = _getCycleCount()) - startTime) < t)
                ; // Wait high duration
            GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, pinMask); // Set low
            if (!(mask >>= 1)) { // Next bit/byte
                if (p >= end)
                    break;
                pix = *p++;
                mask = 0x80;
            }
        }
    }

#else
    for (t = time0;; t = time0) {
        if (pix & mask)
            t = time1; // Bit high duration
        while (((c = _getCycleCount()) - startTime) < period)
            ; // Wait for bit start
        gpio_set_level(pin, HIGH);
        startTime = c; // Save start time
        while (((c = _getCycleCount()) - startTime) < t)
            ; // Wait high duration
        gpio_set_level(pin, LOW);
        if (!(mask >>= 1)) { // Next bit/byte
            if (p >= end)
                break;
            pix = *p++;
            mask = 0x80;
        }
    }
#endif

    while ((_getCycleCount() - startTime) < period)
        ; // Wait for last bit

}

void ICACHE_RAM_ATTR espShow(uint8_t pin, uint8_t *pixels, uint32_t numBytes, boolean is800KHz) {
    NeoPixel_espShow(pin, pixels, numBytes, is800KHz);
}

#endif
