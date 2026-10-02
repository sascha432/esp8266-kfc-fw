/**
 * @file lgfx_wt32_sc01.h
 *
 * LovyanGFX device configuration for the Wireless-Tag WT32-SC01 (PCB V3.2),
 * ESP32-WROVER-B + 3.5" 480x320 ST7796S (SPI, HSPI, write only) + FT6336U
 * capacitive touch (I2C, address 0x38).
 *
 * All pins come from the IOT_WT32_SC01_* build flags, see conf/envs/wt32_sc01.ini.
 *
 * Notes:
 *  - Pin assignment verified against the WT32-SC01 V3.2 schematic (in
 *    docs/WT32-SC01-datasheet.pdf, sheets 04_Main.SchDoc + 02_LCD.SchDoc):
 *    LCD_SCL=14, LCD_SDA=13, LCD_CS=15, LCD_RS=21, LCD_REST=22, LCD_LED=23,
 *    I2C_SDA=18, I2C_SCL=19, TP_INT=39 (SENSOR_VN). There is no MISO, the panel
 *    is write only.
 *  - LovyanGFX' built in autodetect is deliberately NOT used. The WT32_SC01
 *    detector (LGFX_AutoDetect_ESP32_all.hpp) is only registered in the
 *    ESP32-D0WDQ5 package list and only when LGFX_WT32_SC01 is defined, and the
 *    panel is write only, so it cannot be identified by its ID. The explicit
 *    configuration below is deterministic.
 *  - The controller GRAM is portrait 320x480, the visible area after
 *    setRotation(1) is 480x320 (landscape).
 *
 * Usage:
 *   #include "lgfx_wt32_sc01.h"
 *   static LGFX_WT32_SC01 lcd;
 *   ...
 *   lcd.init();
 *   lcd.setRotation(1);
 */
#pragma once

#if !defined(IOT_WT32_SC01_PIN_TFT_SCLK) || !defined(IOT_WT32_SC01_PIN_TOUCH_SDA)
#error "lgfx_wt32_sc01.h requires the IOT_WT32_SC01_* build flags, see conf/envs/wt32_sc01.ini"
#endif

#if defined(LGFX_AUTODETECT) || defined(LGFX_WT32_SC01)
#error "do not combine LGFX_AUTODETECT/LGFX_WT32_SC01 with lgfx_wt32_sc01.h"
#endif

#include <LovyanGFX.hpp>
#include <global.h> // KFC_TWOWIRE_SDA/SCL/CLOCK_SPEED, the I2C bus the touch shares with the sensor plugin

// Touch controller I2C clock. It matters while the touch uses the pins of `Wire` (see the
// validation below): the two sides then have to run at the same clock, because LovyanGFX applies
// its own configuration per transaction and puts the driver's back afterwards.
#ifndef IOT_WT32_SC01_TOUCH_I2C_FREQUENCY
#    define IOT_WT32_SC01_TOUCH_I2C_FREQUENCY 400000
#endif

// Panel write clock. LovyanGFX' WT32_SC01 detector uses 40MHz, TFT_eSPI's
// Setup201_WT32_SC01.h uses 27MHz. Too high a clock over the FPC shows up as a
// fuzzy/speckled image, so the safe value is the default - raise it once the
// picture is clean, override with -D IOT_WT32_SC01_TFT_SPI_FREQUENCY=...
#ifndef IOT_WT32_SC01_TFT_SPI_FREQUENCY
#    define IOT_WT32_SC01_TFT_SPI_FREQUENCY 27000000
#endif

// The touch controller is configured against the I2C bus of the Arduino `Wire` instance the
// sensor plugin uses (`KFC_TWOWIRE_SDA`/`KFC_TWOWIRE_SCL`): an ESP32 GPIO output is driven by a
// single peripheral signal, so both devices are only reachable over the same two pins and at the
// same clock. Whether the bus is shared is decided here, at compile time, from that
// configuration:
//   - pins match and clocks match -> share I2C_NUM_0 with `Wire`. LovyanGFX detects the open port
//     (`i2cIsInit()`, the `foreign_bus` path), applies its own configuration for a transaction and
//     puts the driver's back afterwards, the pins stay as `Wire.begin()` configured them
//   - pins match and clocks differ -> no configuration can use both devices, fail the build
//   - pins differ -> do not touch the `Wire` bus at all, the touch takes over its own controller
//     (I2C_NUM_1, the one LovyanGFX' own WT32_SC01 detector uses for the touch)
#if IOT_WT32_SC01_PIN_TOUCH_SDA == KFC_TWOWIRE_SDA && IOT_WT32_SC01_PIN_TOUCH_SCL == KFC_TWOWIRE_SCL
#    define IOT_WT32_SC01_TOUCH_I2C_PORT 0 // I2C_NUM_0, shared with `Wire`
#    if IOT_WT32_SC01_TOUCH_I2C_FREQUENCY != KFC_TWOWIRE_CLOCK_SPEED
#        error "the touch uses the pins of KFC_TWOWIRE_SDA/SCL, so its clock IOT_WT32_SC01_TOUCH_I2C_FREQUENCY must match KFC_TWOWIRE_CLOCK_SPEED"
#    endif
#else
#    define IOT_WT32_SC01_TOUCH_I2C_PORT 1 // I2C_NUM_1, own controller (I2C_NUM_0 belongs to `Wire`)
#endif

class LGFX_WT32_SC01 : public lgfx::LGFX_Device
{
    lgfx::Panel_ST7796 _panel_instance;
    lgfx::Bus_SPI _bus_instance;
    lgfx::Light_PWM _light_instance;
    lgfx::Touch_FT5x06 _touch_instance;

public:
    LGFX_WT32_SC01()
    {
        // SPI bus, HSPI, 3 wire (write only panel), DMA
        {
            auto cfg = _bus_instance.config();
            cfg.spi_host = HSPI_HOST;
            cfg.spi_mode = 0;
            cfg.spi_3wire = true;
            cfg.use_lock = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;
            cfg.pin_sclk = IOT_WT32_SC01_PIN_TFT_SCLK;
            cfg.pin_mosi = IOT_WT32_SC01_PIN_TFT_MOSI;
            cfg.pin_miso = IOT_WT32_SC01_PIN_TFT_MISO; // -1, LCD_RD is not connected
            cfg.pin_dc = IOT_WT32_SC01_PIN_TFT_DC;
            cfg.freq_write = IOT_WT32_SC01_TFT_SPI_FREQUENCY;
            cfg.freq_read = 16000000;
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }
        // panel, do not set the memory size to the landscape size, it is the
        // addressable area of the controller (320x480, portrait)
        {
            auto cfg = _panel_instance.config();
            cfg.pin_cs = IOT_WT32_SC01_PIN_TFT_CS;
            cfg.pin_rst = IOT_WT32_SC01_PIN_TFT_RST;
            cfg.pin_busy = -1;
            cfg.memory_width = 320;
            cfg.memory_height = 480;
            cfg.readable = false;
            cfg.invert = false;
            // LovyanGFX writes `rgb_order ? MAD_RGB(0x00) : MAD_BGR(0x08)` into MADCTL
            // (Panel_LCD.inl:169), so rgb_order = false SETS the BGR bit: the name is
            // the opposite of the bit. This panel needs the BGR bit set - Teneppa's
            // WT32-SC01 module uses MADCTL 0b01001010 (bit3 set) and LovyanGFX' own
            // WT32_SC01 detector leaves rgb_order at its default false as well.
            // Getting this wrong swaps red and blue only (green stays green).
            cfg.rgb_order = false;
            cfg.dlen_16bit = false;
            cfg.bus_shared = false;
            _panel_instance.config(cfg);
        }
        // backlight, PWM channel 7 like the LovyanGFX WT32_SC01 detector
        {
            auto cfg = _light_instance.config();
            cfg.pin_bl = IOT_WT32_SC01_PIN_TFT_BL;
            cfg.invert = !IOT_WT32_SC01_TFT_BL_ACTIVE_HIGH;
            cfg.freq = 12000;
            cfg.pwm_channel = 7;
            _light_instance.config(cfg);
            _panel_instance.setLight(&_light_instance);
        }
        // touch, FT6336U
        {
            auto cfg = _touch_instance.config();
            // I2C_NUM_0 while the touch is on the pins of `Wire` (`KFC_TWOWIRE_SDA`/`SCL`), i.e.
            // the touch and the MPU-6050 of the sensor plugin sit on the one I2C bus the
            // WT32-SC01 exposes. I2C0 is used on purpose: an ESP32 GPIO output is driven by a
            // single peripheral signal, so two controllers on the same two pins would fight
            // over the routing and only the last one configured could talk. LovyanGFX detects
            // that the port is already open through `Wire` (`i2cIsInit()`, the `foreign_bus`
            // path) and shares it instead of taking it over - it applies its own configuration
            // for a transaction and puts the driver's back afterwards, the pins and their
            // pull-ups stay as `Wire.begin()` configured them. Both sides run in the main loop
            // task, so no transaction of one can interleave with the other.
            // A different pin configuration cannot share the bus: the touch then takes over a
            // controller of its own and this is I2C_NUM_1 (see the top of this file).
            cfg.i2c_port = IOT_WT32_SC01_TOUCH_I2C_PORT;
            cfg.i2c_addr = IOT_WT32_SC01_TOUCH_I2C_ADDRESS;
            cfg.pin_sda = IOT_WT32_SC01_PIN_TOUCH_SDA;
            cfg.pin_scl = IOT_WT32_SC01_PIN_TOUCH_SCL;
            // GPIO34..39 are input only and have NO internal pull-up/pull-down on the ESP32, and
            // the WT32-SC01 does not populate a pull-up on the FT6336U INT line either (only the
            // 0R R18 to SENSOR_VN/IO39). Touch_FT5x06 uses the pin as a release gate:
            //   if (_flg_released != gpio_in(pin_int)) { ... }
            //   if (_flg_released) return 0;
            // With a floating pin that gate latches "released" and getTouch() never reports a
            // touch, i.e. the touch screen appears dead although the controller answers on I2C.
            // pin_int < 0 makes the driver poll the controller over I2C instead (one short read
            // per LVGL input period, 30 ms) which is reliable on this board.
            cfg.pin_int = -1; // -D IOT_WT32_SC01_PIN_TOUCH_INT is documented but not usable, see above
            cfg.pin_rst = -1;
            // while the bus is shared with `Wire` the clock has to be the one Wire.setClock()
            // uses (KFC_TWOWIRE_CLOCK_SPEED, enforced by the guard at the top of this file)
            cfg.freq = IOT_WT32_SC01_TOUCH_I2C_FREQUENCY;
            // `bus_shared` refers to the bus of the PANEL (SPI), not to the I2C bus: the touch
            // must not make LovyanGFX end/begin a transaction on the panel bus around a read
            cfg.bus_shared = false;
            cfg.offset_rotation = 0;
            cfg.x_min = 0;
            cfg.x_max = 319;
            cfg.y_min = 0;
            cfg.y_max = 479;
            _touch_instance.config(cfg);
            _panel_instance.setTouch(&_touch_instance);
        }
        setPanel(&_panel_instance);
    }
};
