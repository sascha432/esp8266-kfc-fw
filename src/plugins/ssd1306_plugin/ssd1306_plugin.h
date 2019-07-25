/**
 * Author: sascha_lammers@gmx.de
 */

#if SSD1306_PLUGIN

#pragma once

#include <Arduino_compat.h>
#include <Adafruit_SSD1306.h>

// Heltec WiFi LoRa 32
// -D SSD1306_PLUGIN_128_64=1 -D SSD1306_PLUGIN_RESET_PIN=16 -D SSD1306_PLUGIN_SCL_PIN=15

// Heltec ESP8266 WiFi Kit 8
// -D SSD1306_PLUGIN_128_32=1 -D SSD1306_PLUGIN_RESET_PIN=16

#if defined(SSD1306_PLUGIN_128_64)
#define SSD1306_PLUGIN_WIDTH                128
#define SSD1306_PLUGIN_HEIGHT               64
#elif defined(SSD1306_PLUGIN_128_32)
#define SSD1306_PLUGIN_WIDTH                128
#define SSD1306_PLUGIN_HEIGHT               32
#endif

#ifndef SSD1306_PLUGIN_RESET_PIN
#define SSD1306_PLUGIN_RESET_PIN            -1
#endif
#ifndef SSD1306_PLUGIN_I2C_ADDR
#define SSD1306_PLUGIN_I2C_ADDR             0x3c
#endif
#ifndef SSD1306_PLUGIN_SDA_PIN
#define SSD1306_PLUGIN_SDA_PIN              4
#endif
#ifndef SSD1306_PLUGIN_SCL_PIN
#define SSD1306_PLUGIN_SCL_PIN              5
#endif

#ifndef SSD1306_PLUGIN_DISPLAY_STATUS_DELAY
// delay after start before displaying wifi and date/time. 0 to disable
#define SSD1306_PLUGIN_DISPLAY_STATUS_DELAY 1000
#endif

// display WiFi SSID, IP, date and time
void ssd1306_disable_status();
void ssd1306_enable_status();

void ssd1306_clear_display();

extern Adafruit_SSD1306 Display;

#endif
