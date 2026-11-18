# HLK-LD2410C on `presense_ld2410_1`, notes on the LD2450

Wiring of the presence sensor board: a D1 mini (ESP8266, USB-C) with an HLK-LD2410C presence radar. The pins are named
like on the NodeMCU (D0 - D8) and the wiring is the same on one, its VIN is the 5V pin of the D1 mini. The env is
`presense_ld2410_1` (`conf/envs/environmental_sensor.ini`, it extends `presense_sensor`). The commands, the AT console
and the WebUI pages are documented in [AtModeHelp.md](../AtModeHelp.md) (`+LD2410C`, `+LD2450`).

The radar uses the hardware UARTs of the ESP8266, SoftwareSerial does not work at its baud rates: UART1 TX sends the
commands, UART0 RX receives the module while the firmware talks to it.

A board has one LD24xx radar, see [One radar per board](#one-radar-per-board). The notes on the HLK-LD2450 are in
[LD2450](#ld2450).

## Overview

``` text
                       D1 mini (ESP8266)
                    +--------------------+
  5 V (USB) --------| 5V                 |---- VCC  LD2410C
                    | GND                |---- GND  LD2410C
                    |                    |
                    | D4  GPIO2  UART1 TX|---> RX   LD2410C
                    | D7  GPIO13 UART0 RX|<--- TX   LD2410C
                    | D2  GPIO4          |<--- OUT  LD2410C
                    +--------------------+
```

## D1 mini pins

| D1 mini | GPIO | Function | Connected to | Notes |
| ------- | ---- | -------- | ------------ | ----- |
| D2 | GPIO4 | input | OUT of the LD2410C | presence output of the module, high (3.3 V) while it detects someone. D2 is the default I2C SDA, I2C is disabled (`DISABLE_TWO_WIRE=1`) |
| D4 | GPIO2 | UART1 TX | RX of the LD2410C | boot strap pin, must be high at boot (10 kΩ pull-up on the board). The boot ROM sends its messages here as well (about 74880 baud), the radar ignores them. The blue LED of the ESP-12 module flickers with the commands, the firmware does not use it |
| D7 | GPIO13 | UART0 RX while the firmware talks to the radar | TX of the LD2410C | |
| TX / RX | GPIO1 / GPIO3 | USB serial console | - | paused while the radar uses UART0, see [Serial console](#serial-console) |
| 5V | - | 5 V from USB | VCC of the LD2410C | 220 µF (6.3 V) to GND for the current peaks of the ESP8266. The module needs more than 200 mA, average 79 mA |
| GND | - | ground | GND of the LD2410C | |

D0, D1, D3, D5, D6 and D8 are not used. The UART and the OUT pin of the module use 3.3 V levels, they connect to the
ESP8266 directly.

## LD2410C

| Pin | Connected to |
| --- | ------------ |
| VCC | 5V |
| GND | GND |
| TX | D7 (GPIO13) |
| RX | D4 (GPIO2) |
| OUT | D2 (GPIO4) |

The module keeps its configuration in its own flash: the baud rate, the max. moving and still gate, the nobody duration
and the move/still value of the 9 gates (0.75 m each). A read switches it to engineering mode, its frames then carry the
energy of every gate for the live view until it restarts.

**Baud rate.** The module runs at its default 256000 baud (`IOT_SENSOR_LD2410C_BAUD`). A module that does not answer
at this rate is searched on all rates it supports (9600 to 460800 baud) and set back to it with command 0x00A1 and a
restart: a module that was set to another rate is moved at the first boot, nothing has to be done by hand.

**Factory reset.** `+LD2410C=factory` sets the module back to its defaults, 256000 baud and Bluetooth on. The next read
(the boot read or `+LD2410C=read`) switches Bluetooth off and restarts the module (`IOT_SENSOR_LD24XX_BLUETOOTH_OFF`,
default 1).

## Serial console

UART0 is the USB serial console and receives the radar as well:

- While a command runs (about 100 ms), the RX signal of UART0 comes from D7 instead of the USB chip. The console
  ignores its input, its output is held (up to 1 KB) and sent when the command has finished, TX stays high meanwhile.
- A module that does not answer is searched on all baud rates, which pauses the console for about 3 seconds. The boot
  read tries a missing module up to 3 times.
- The monitor (`+LD2410C=mon`) and the live view of `/ld2410c.html` keep the port open, the console is paused all that
  time. The WebSocket console of the WebUI is not affected.

## Checking the wiring

- `+LD2410C=read` prints the firmware version and the baud rate. A wired module answers with `0 retries`.
- `+LD2410C=out` prints the level of D2 and the presence.
- `+LD2410C=mon,10,raw` prints the received bytes as hex: only `00` bytes means the module uses a slower baud rate than
  the firmware expects, the read finds it.
- `+LD2410C=tx,5` sends a continuous `0x55` pattern on D4 for 5 seconds: a multimeter shows a mid level (about 1.6 V)
  on D4 and on the RX pin of the module. 0 or 3.3 V means the pin is not driven.
- `+LD2410C=mon,30,gates` prints the energy of every gate, see [Installation](#installation).
- The log (serial console `+LOGGER=`, syslog, log file) has the result of every command the firmware sends by itself
  (boot read, saved form, baud rate change).

## Firmware settings

`presense_ld2410_1` and its base `presense_sensor` set these flags:

| Flag | Value | Meaning |
| ---- | ----- | ------- |
| `IOT_SENSOR_HAVE_LD2410C` | `1` | LD2410C |
| `IOT_SENSOR_LD2410C_OUT_PIN` | `4` | OUT of the LD2410C on D2 |
| `DISABLE_TWO_WIRE` | `1` | no I2C, D2 is the OUT pin of the LD2410C |

## WebUI and MQTT

- `/sensor.html`: the card of the LD2410C, max. moving/still gate and nobody duration
- `/ld2410c.html`: move/still value of the 9 gates with a live view: the energy of every gate as a bar below its
  values, the peak since the page was opened and a button that raises the values of the gates whose peak reached them
- MQTT: the presence (the OUT pin) as a binary sensor with the device class `occupancy`, state topic
  `<base topic>/ld2410c`

## Installation

The module is mounted on the wall, 1.5 - 2 m high, with the antenna side (the side with the square antenna patches)
facing the room and nothing metallic in front of it. It detects within +-60 degrees around the normal of the antenna
plane, up to 6 m in 8 gates of 0.75 m (5 m in the manual's diagram). A housing must not contain metal; the manual
recommends 12.4 or 18.6 mm (1 or 1.5 wavelengths, +-1.2 mm) between the antenna and the inside of the cover.

**Walls do not stop it.** 24 GHz passes drywall, wood, glass and plastic, and the LD2410C detects breathing: a person
or a vibrating device behind a wall, above or below the room, or behind the module (back lobe) keeps it on "detected"
in an empty room. The live view on `/ld2410c.html` shows which gates see it, as red still bars at their distance.
Limit the max. gates to the room (0.75 m each) or raise the still values of those gates; a metal plate behind the
module blocks the back lobe, which the manual recommends against movement behind the radar. To tell front from back,
hold a metal sheet (a baking tray, aluminum foil) over the front of the housing: energy that stays comes from behind.

**Orientation**, seen from the room (looking at the antenna side), from the module manual: the module lies
horizontally with its pins at the top. It reports no angle, the orientation sets the shape of its detection area.

``` text
        LD2410C (manual figure 9/10)

                    up
        +---------------------------+
        |  o    o    o    o    o    |  pins at the top
        |  TX   RX  OUT  GND  VCC   |
        |                           |
        |  [##]               [##]  |  2 antenna patches
        |          [chip]           |  left and right
        |                           |
        +---------------------------+
                   down
```

Source: HLK-LD2410C manual V1.00, chapters 5.4 and 8, figures 9 and 10.

## One radar per board

The LD24xx radars cannot be mixed on one board, more than one of `IOT_SENSOR_HAVE_LD2420`, `IOT_SENSOR_HAVE_LD2410C`
and `IOT_SENSOR_HAVE_LD2450` is a compile error:

- GPIO13 is the only RX pin the radars can use.
- Two 24 GHz radars close to each other disturb each other: users report false presence of an LD2410/LD2412 next to an
  active LD2450, and about 15 cm between the modules as the distance where it stops.

## LD2450

The HLK-LD2450 tracking radar uses the same pins as the LD2410C (TX on D7, RX on D4) on its own board,
`IOT_SENSOR_HAVE_LD2450=1`. The envs are `presense_ld2450_2` (NodeMCU 0.9, OTA) and `presense_serial` (the same
board over USB).

**Presence.** The LD2450 has no presence output, the targets it reports are the presence. With
`IOT_SENSOR_LD2450_PRESENCE=1` (set in `presense_ld2450_2` and `presense_serial`) the firmware reads the target
frames all the time:

- WebUI: the tile of the radar shows Detected (with the number of targets), Clear, or Unknown before the first frame
- MQTT: the presence as a binary sensor with the device class `occupancy`, state topic `<base topic>/ld2450`, and the
  number of targets (0-3) as a sensor, state topic `<base topic>/ld2450/targets`
- only targets inside the area of the module count: x -3..+3 m and y 0..6 m. A target outside it is a false
  detection, `+LD2450=mon` marks it with `outside the area` and the live view (`/ld2450-radar.html`, a fixed 7 m
  view with the area shaded) draws it as a grey hollow dot, at the edge if it is farther than 7 m
- `/sensor.html`: the presence timeout (1-600 s, default 5 s). A target switches the presence on at once, it is
  cleared when no frame had a target for that long, also when the module stops reporting. The module has no hold
  time of its own and its target count changes with every frame: the number of targets is published at most once
  per second
- the USB serial console is paused after the boot read, see [Serial console](#serial-console): UART0 receives the
  radar all the time. The WebSocket console of the WebUI is not affected, a command takes the port for its duration

**Bluetooth has to be off on the LD2450.** Its Bluetooth module is the reason the LD2450 stopped reporting: with
Bluetooth on, the LD2450 (firmware 2.04.23101915) switches its radar off about 150 s after it started (power-on or its
own restart). It sends no target frames anymore, after a while it answers no command either (status `not responding`),
and the board current drops by about 150 mA. Only a power cycle starts it again, a reset of the ESP8266 does not: the
module has no reset pin. Measured on 2026-11-16: 5 runs with Bluetooth on stopped 150-160 s after the start of the
module, with Bluetooth off it kept reporting; on 2026-11-17 two factory resets (Bluetooth on) stopped it again after
155-158 s. The wiring, the supply and resets of the ESP8266 were not the cause. A new module and a factory reset have
Bluetooth on: the firmware switches it off at the first read and restarts the module
(`IOT_SENSOR_LD24XX_BLUETOOTH_OFF`, default 1).

**Orientation of the LD2450:** it stands upright, seen from the room. It measures x (sideways) and y (distance) in the
horizontal plane only in this position, and the zone areas are defined in these coordinates. The manual names the angle
direction (positive to the left as seen from the room) but not the sign of x: after mounting, stand about 1 m to one
side and check the sign of x with `+LD2450=mon,5` before entering zone areas.

``` text
        LD2450 (manual figure 6)

               up
        +-------------+
        |  [##]  [##] |  2 x 2 antenna patches at the top
        |  [##]  [##] |
        |             |
        |  HLK [chip] |  radar chip right
        |             |
        |  [##]       |  2 antenna patches at the bottom
        |  [##]       |
        +-------------+
              down
  positive angle <-   -> negative angle
```

Source: HLK-LD2450 instruction manual V1.00, chapter 7, figures 5-7.
