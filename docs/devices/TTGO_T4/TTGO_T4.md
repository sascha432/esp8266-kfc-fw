# TTGO T4 v1.3 (LilyGO)

2.4" ESP32 display board (240x320 TFT, no touch), with microSD slot and on-board
power management.

Files in this folder:

- `TTGO_T4.md` - this document
- `LILYGO TTGO Backlight_04-600x600w.jpg` - vendor pinout sheet for the T4 v1.3
- `ESP32-WROVER-B_datasheet.pdf` - datasheet of the module on the board
- `ILI9341_datasheet.pdf` - datasheet of the display controller

Vendor demos: https://github.com/LilyGO/TTGO-T4-DEMO

## Specs

| Item | Value |
|---|---|
| Module | ESP32-WROVER-B (ESP32, Xtensa LX6 dual-core @ 240 MHz) |
| Flash / PSRAM | 4 MB flash / 8 MB PSRAM - **verify on your unit**, no LilyGO datasheet states it (PSRAM is present: reference projects build with `BOARD_HAS_PSRAM`) |
| Radio | Wi-Fi + Bluetooth Classic / BLE |
| USB | USB-C via USB-UART bridge (no native USB) |
| Panel | 2.4" TFT, 240x320 (the vendor pinout sheet labels it 2.2") |
| Display interface | 4-wire SPI |
| Touch | none |
| Camera | none |
| Audio | none |
| Storage | microSD slot (SPI) |
| Power | IP5306-I2C power bank/charger IC (battery + boost), `VBAT` on the header |
| Buttons | 3 on-board buttons (37/38/39) + BOOT (0) |
| PlatformIO env | `ttgo_t4_debug` (`board = esp32dev`) |
| Debug probe | no USB-JTAG in the ESP32 -> external probe (`debug_tool = esp-prog`) |

## Display

| Item | Value |
|---|---|
| Controller | ILI9341 |
| Bus | 4-wire SPI (reference projects run it at 80 MHz) |
| Resolution / color | 240x320, RGB565 |

| Vendor sheet label | Signal | GPIO |
|---|---|---|
| SCL | SCK | 18 |
| RST | RST | 5 |
| CS | CS | 27 |
| RS | DC | 32 |
| SDI | MOSI | 23 |
| SDO | MISO | 12 |
| BK | Backlight | 4 |

These pins are taken from the vendor pinout sheet in this folder (which also matches the
working PlatformIO port `mordaha/TTGO-T4-v1.3-HELLO_WORLD`). Note that LilyGO's own
`TTGO-T4-DEMO` Arduino sketch for this board is inconsistent: `T4_9341_NEW.ino` drives
DC on GPIO 26, while the pinout sheet and the third-party port use GPIO 32.

## Touch

Not supported - no touch panel or touch controller. The "IP5306 / SDA 21 / SCL 22" entry
on the vendor sheet is the I2C bus of the power-management IC, not a touch bus.

## Camera

Not supported.

## Audio

Not supported - no codec/DAC/amplifier on the board.

## Storage / other

| Item | Value |
|---|---|
| microSD, SPI | CS 13, MOSI 15, SCK 14, MISO 2 |
| Buttons | Button1 = 38, Button2 = 37, Button3 = 39 |
| Power management | IP5306-I2C, SDA 21 / SCL 22 (battery fuel gauge, charge state) |
| Header | S_VP / S_VN, IO35, IO34, IO32, IO25, IO0, IO19, IO4, VCC_IO, VBAT, GND, RST |

## Build notes

- PlatformIO: `board = esp32dev` with PSRAM enabled, see `sdkconfig.ttgo_t4`
  (the ESP32 PSRAM cache workaround matters here - `CONFIG_SPIRAM_CACHE_WORKAROUND`).
- GUI: LVGL via `lib_deps` (env uses LVGL 9.x; switch to `${lvgl8.lib_deps}` /
  `${lvgl8.build_flags}` for LVGL 8.x).
- Driver stack: `esp_lcd` SPI panel (ILI9341) + LVGL port. No touch driver, but the
  backlight on GPIO 4 should be driven (LEDC) if dimming is wanted.
- The 240x320 framebuffer fits in internal RAM; the PSRAM is only needed for larger
  draw buffers or assets.

## Sources

- Vendor pinout sheet for T4 v1.3 (image in this folder) - display, SD, buttons, IP5306, header
- https://github.com/LilyGO/TTGO-T4-DEMO - vendor demos (`T4_9341_NEW/T4_9341_NEW.ino`; DC differs, see above)
- https://github.com/mordaha/TTGO-T4-v1.3-HELLO_WORLD - working PlatformIO port (TFT_eSPI flags incl. DC 32, backlight 4)
- https://github.com/gshorten/TTGO_T4_Speeduino_Meter - button pins, I2C usage
