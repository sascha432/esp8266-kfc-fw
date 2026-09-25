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
            cfg.freq_write = 40000000;
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
            cfg.i2c_port = 1; // I2C_NUM_1
            cfg.i2c_addr = IOT_WT32_SC01_TOUCH_I2C_ADDRESS;
            cfg.pin_sda = IOT_WT32_SC01_PIN_TOUCH_SDA;
            cfg.pin_scl = IOT_WT32_SC01_PIN_TOUCH_SCL;
            cfg.pin_int = IOT_WT32_SC01_PIN_TOUCH_INT;
            cfg.pin_rst = -1;
            cfg.freq = 400000;
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
