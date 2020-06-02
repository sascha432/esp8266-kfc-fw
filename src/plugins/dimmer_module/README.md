# Firmware flashing

## ESP8266 Serial

Connect VCC, GND, RST, GPIO0, RX, TX and **ATmega RST (to GND)**

```
upload_speed = 921600
monitor_speed = 57600
board = esp12e
```

## ATMega ISP

Connect VCC, GND, MISO, MOSI, SCK, RST and **ESP RST (to GND)**

### Fuses

Change `-patmega328pb` to `-patmega328p` for 328P

`avrdude -C avrdude.conf -v -patmega328pb -carduino -PCOMx -D -b19200 -e -Ulock:w:0x3F:m -Uefuse:w:0xFF:m -Uhfuse:w:0xDA:m -Ulfuse:w:0xFF:m`

### Boot loader

The bootloader is for ATmega328P but works with ATmega328PB and identifies as such.

`avrdude -C avrdude.conf -v -patmega328pb -carduino -PCOMx -b19200 "-Uflash:w:atmega_328p_and_pb_8mhz.hex:i" -Ulock:w:0x0F:m`

_Note_: Using the Arduino IDE requires some modifications for 328PB

## ATMega Serial

Connect VCC, GND, RX(to TX), TX(to RX), RST and **ESP RST (to GND)**

After the boot loader has been flashed, it can be programmed with avrdude (Arduino IDE, PlatformIO...) or over the WebUI.

```
board = pro8MHzatmega328
monitor_speed = 57600
ArduinoNano / 328P Old Bootloader
```
