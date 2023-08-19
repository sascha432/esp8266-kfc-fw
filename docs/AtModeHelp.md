# AT Mode Commands

After the command help takes a lot of FLASH memory, it has been moved to this document here. Most commands without DEBUG MODE are listed.

## Common Commands

### `+AT?`

Print `OK`

### `+REM=...`

Ignore comment

### `+DSLP=[<milliseconds>[,<mode>]]`

Enter deep sleep

### `+RST=[<s>]`

Reset device. If `s` is provided, it will reboot in SAFE MODE.

### `+LOAD`

Discard changes and load settings from EEPROM/NVS

### `+STORE`

Store current settings in EEPROM/NVS

### `+DUMP=[<dirty|json>]`

Dump config information and data

  - `dirty` show modified entries only
  - `json` use JSON as output format

### `+FACTORY`

Restore factory settings (but do not store in EEPROM/NVS)

### `+FSR`

Restore factory settings, store in EEPROM/NVS and reboot device

### `+NVS=<format|stats|dump>`

- `dump` dumps the NVS partition debug info. Only available if compiled in
- `stats` display NVS statistics
- `format` formats the NVS partition, restores factory settings and stores it. Reboot the device to apply all new settings. Formatting a NVS partition that has been used for quite some time can free up some heap. The status page shows the amount of heap being used for it and should not exceed 1.5-2KB for a 32KB partition

``` test
NVS flash storage max. size 32KB, 86.6% in use
NVS init partition memory usage 1312 byte
Stored items 34, size 0.60KB
```

For the ESP8266 and its custom NVS implementation, esptool.py can be used to clear a full or corrupted NVS partition that prevents the device to boot. Usually this is  detected automatically, and the entire `nvs` partition is formatted. The current custom implementation of NVS for NONOS_SDK has not been tested very well

**Note:** The region in the example can be different depending on the flash layout being used (`eagle...ld`, name `nvs`)

***TODO:*** Implement a command for PIO for format NVS

``` bash
python .platformio/packages/tool-esptoolpy/esptool.py erase_region 4009984 32768
```

For the ESP32, check the documentation for NVS

### `+ATMODE=<1|0>`

Enable/disable AT Mode if compiled in

### `+HEAP=<interval[,umm]>`

Display heap usage every interval (can be 1s or 1000ms, 0 shows it once). If `umm` is added, the umm heap statistics are displayed (ESP8266 only)

### `+GPIO=<interval>`

Display GPIO pin states every interval (can be 1s or 1000ms, 0 shows it once)

### `+PWM=<pin>,<input|input_pullup|high|low|waveform|level=0-1023[,<frequency=100-40000Hz>[,<duration/ms>]]`

Control PIN input, output and PWM state. Setting a PIN high or low implicitly sets it to output.

### `+DLY=<milliseconds>`

Call delay(milliseconds)

### `+CAT=<filename>`

Display file contents

### `+MD=<directory>`

Create directory

### `+RM=<path>`

Remove file or directory

### `+TOUCH=<filename>`

Open file in append write mode. This will change the last modification time if the file system supports mtime or create the file if it does not exist. All directories will be created if they do not exist

### `+RN=<path>,<new path>`

Rename file or directory

### `+LS=[<directory>[,<hidden=true|false>,<subdirs=true|false>]]`

List files and directories

### `+LSR=[<directory>]`

List files and directories using FS.openDir(). This will not display read only virtual files.

### `+AOTA=<start|stop>`

Start/stop Arduino OTA (requires about 1K RAM to run)

### `+PLG=<list|start|stop|add-blacklist|add|remove>[,<name>]`

Plugin management. If a plugin malfunctions, it can be blacklisted in SAFE MODE.

### `+RTC=[set]`

Display RTC status or set RTC time from current time. Only available with a real time clock (i.e. DS3231)

### `+WIFI=<command>,[args]`

Run WiFi command

 - reset                                       Reset WiFi connection
 - on                                          Enable WiFi station mode
 - off                                         Disable WiFi station mode
 - list[,<show passwords>]                     List WiFi networks.
 - cfg,[<...>]                                 Configure WiFi network
 - ap_on                                       Enable WiFi AP mode
 - ap_off                                      Disable WiFi AP mode
 - ap_standby                                  Set AP to stand-by mode (turns AP mode on if station mode cannot connect)
 - diag                                        Print diagnostic information
 - stl                                         List available WiFi stations
 - next                                        Switch to next WiFi station
 - stop_ping                                   Stop pinging the gateway (Only if compiled in)

### `+LED=<slow,fast,flicker,off,solid,sos,pattern>,[,color=0xff0000|pattern=10110...][,pin]`

Set internal LED mode or an LED on a certain PIN.

### `+NEOPX=<pin>,<num>,<r>,<g>,<b>`

Set NeoPixel colors for a given PIN if available

### `+METRICS`

Displays versions of the SDK, framework, libraries, memory addresses and a lot more. Available in DEBUG mode only

### `+PING=<target[,count=4[,timeout=5000]]>`

Ping host or IP address if compiled in

### `PWM=<pin>,<input|input_pullup|waveform|level=0-1024>[,<frequency=100-40000Hz>[,<duration/ms>]]`

PWM output on PIN, min./max. level set it to LOW/HIGH" using digitalWrite

### `+RD?`

Show reset detector status

### `+RD`

Clear reset detector status

### `+SAVECRASH=<info|list|print|clear|format>`

Run command

 - `info` show statistics
 - `list` list crash logs
 - `print` show stack trace for a crash log
 - `clear` clear all data (marks used sectors as invalid, they will be re-used and formated once no free sectors are available)
 - `format` format all savecrash data sectors

``` text
+savecrash=info
+SAVECRASH: entries=1 size=1068 capacity=124620
+SAVECRASH: free space=123492 largest block=4020
```

``` text
+savecrash=list
<001> 2023-07-25T04:51:45 PDT reason=LoadStoreErrorCause epc1=0x00000003 epc2=0x40101675 epc3=0x00000000 excvaddr=0x402b9437 depc=0x400287c1 version=0.0.8.b13160
 md5=eddacbc1184a156d22844c6a9e387742 stack-size=1008
 ```

 ``` text
+savecrash=print,1

--------------- CUT HERE FOR EXCEPTION DECODER ---------------

Firmware 0.0.8.b13160
MD5 eddacbc1184a156d22844c6a9e387742
Timestamp 2023-07-25T04:51:45 PDT

Exception (3):
epc1=0x40101675 epc2=0x00000000 epc3=0x402b9437 excvaddr=0x400287c1 depc=0x00000000

>>>stack>>>
sp: 3ffff970 end: 3fffffd0 offset: 0270
3ffffbe0:  00000000 3ffffcb0 3fff1b0c 3fff0550
3ffffbf0:  3ffffc90 40264a60 00000020 401018fc
...
3fffffc0:  feefeffe feefeffe 3fffdab0 4010078d
<<<stack<<<

--------------- CUT HERE FOR EXCEPTION DECODER ---------------

```

``` text
+savecrash=clear
+SAVECRASH: SaveCrash logs erased
+savecrash=info
+SAVECRASH: entries=0 size=0 capacity=124620
+SAVECRASH: free space=124560 largest block=4020
```

## I2C Bus

### `+I2CS=<pin-sda>,<pin-scl>[,<speed=100000>,<clock-stretch=45000>,<start|stop>]`

Configure I2C Bus

### `+I2CTM=<address>,<data,...>`

Transmit data to slave

### `+I2CRQ=<address>,<length>`

Request data from slave

### `+I2CSCAN=[<start-address=1>][,<end-address=127>][,<sda=4|list|any|no-init>,<scl=5>]`

Scan I2C Bus for devices.

- `list` display all PIN pairs that will be scanned
- `any` probes all available PINs for I2C devices. The UART used for debug output and the Flash SPI GPIOs are excluded
- `no-init` skips re-initializing the I2C Bus

**NOTE:** Available only if compiled in

## 7 Segment Clock / LED Matrix / LED Strip

### `+LMTESTP=<#color>[,<time=500ms>]`

Test peak values. WARNING! This command will bypass all protections and limits

### `+LMC=<command>[,<options>]`

Run command

- vis[ualizer],<type>
- br[ightness],<level>
- co[lor],<#RGB|r,b,g>
- pr[int],<display=00:00:00>
- tem[perature],<value>
- lo[op],<enable|disable>
- met[hod]<fast|neo|neoex> (FastLED|Adafruit NeoPixel|NeoPixelEx)
- ani[mation][,<solid|gradient|rainbow|rainbow-fastled|flash|color-fade|fire|interleaved>][,blend_time=4000ms]
- out[put],<on|off>
- dit[her],<on|off>
- co[lor],<#color>
- map,<rows>,<cols>,<reverse_rows>,<reverse_columns>,<rotate>,<interleaved>
- cl[ear]
- res[et][,<pixels=116>]
- test,<1=pixel order|2=clock|3=row/col>[,#color=#330033][,<brightness=128>][,<speed=100ms>]
- get[,<range>]
- set,<range(=0-7)>,[#color]

### `+LMVIEW=<interval in ms|0=disable>,<client_id>`

Display LEDs over http2serial. Requires a proxy that listens to UDP and sends the data back over a web socket. An example can be found in `scripts\udp2ws_proxy`

## Weather Station / Clock / LED Matrix

### `+WSSET=<touchpad|timeformat24h|metric|tft|scroll|stats|lock|unlock|screen|screens>,<on|off|options>`

Enable/disable function

### `+WSBL=<level=0-1023>`

Set backlight level

### `+WSU=<i|f>`

Update weather info/forecast

### `+WSM=<date YYYY-MM-DD>[,<days>]`

Show Moon Phase for given date. Only available in DEBUG MODE

### `+MDNSR`=<service>,<proto>,[<wait=3000ms>]`

Query MDNS

### `+MDNSR=<stop|start|enable|disable|zeroconf>`

Configure MDNS

## Blinds Controller

### `+BCME=<open|close|stop|tone|imperial|init>[,<channel>][,<tone_frequency>,<tone_pwm_value>]`

Open, close a channel, stop motor or run tone test, play imperial march, set initial state

## Remote Control

### `+RCDSLP=<time in seconds|0>`

Disable auto sleep or set timeout in seconds

### `+RCBAT=<interval in seconds|0>`

Send battery status every n seconds to MQTT and to UDP server, 10 times in a row. If the time is 0, it is sent once.

### `+BCAP=<voltage>`

Calculate battery capacity for given voltage

### `+BCTAB=<from>,<to-voltage>[,<true=charging|false>]`

Create a table for the given voltage range

## MQTT

### `+MQTT=con|dis|set|top|auto|list`

Manage MQTT

  con[nect]                                   Connect to server
  dis[connect][,<true|false>]                 Disconnect from server and enable/disable auto reconnect
  set,<enable,disable>                        Enable or disable MQTT
  top[ics]                                    List subscribed topics
  auto[discovery][,restart][,force]           Publish auto discovery
  list[,<full|crc|file>]                      List auto discovery (file = /.logs/mqtt_auto_discovery.json)

## Weather Station

### `+WSSET=<touchpad|timeformat24h|metric|tft|scroll|stats|lock|unlock|screen|screens>,<on|off|options>`

### `+WSBL=<level=0-1023>`

Set backlight level

### `+WSU=<i|f>`

Update weather info/forecast

## STK500v1 Programmer

### `+STK500V1F=<filename>,[<0=Serial/1=Serial1>[,<0=disable/1=logger/2=serial/3=serial2http/4=file>]]`

Flash .HEX file to AVR MCU over Serial Port (requires bootloader)

__NOTE__: If the process fails, check if the voltage does not drop too much during the process

For example: `+STK500V1F=/stk500v1/atomicsun-firmware-2.2.3-328pb.hex`

``` text
+STK500V1F: Flashing /stk500v1/atomicsun-firmware-2.2.3-328pb.hex on Serial

Input file validated. 27750 bytes to write...
Connected to bootloader...
Device signature = 0x1e950f

Writing: | ################################################## | Complete

Reading: | ################################################## | Complete

27750 bytes verified
Programming successful (22 seconds)
Done
```

### `+STK500V1S=<atmega328p/0x1e1234/...>`

Set signature (available names in `/stk500v1/atmega.csv`)

### `+STK500V1S?`

Display signature

### `Debug log file`

Debug log file is stored in `/stk500v1/debug.log`

## Dimmer

### `+DIMG`

Get level(s)

### `+DIMS=<channel>,<level>[,<time>]`

Set level

### `+DIMCF=reset|weeprom|info|print|write|factory|zc`

## Configure dimmer firmware (ATMega MCU)

### `+DIMCF=reset`

Hard reset MCU

### `+DIMCF=weeprom`

Write EEPROM

### `+DIMCF=info`

Display dimmer info and config over serial port

``` text
+REM=MOSFET Dimmer 2.2.3 Nov 22 2022 17:50:49 Author sascha_lammers@gmx.de
+REM=sig=1e-95-16,fuses=l:ff,h:da,e:f5,MCU=ATmega328PB@8Mhz,gcc=7.3.0
+REM=options=EEPROM=62,NTC=A0,int.temp,VCC,fading_events=1,proto=UART,addr=17,mode=T,timer1=8/1.00,lvls=8192,pins=6,8,9,10,cubic=0,range=0-8192
+REM=values=restore=1,f=60.342Hz,vref11=1.100,NTC=27.20/+0.00,int.temp=36/ofs=88/gain=156,max.temp=75,metrics=5,VCC=3.229,min.on-time=300,min.off=300,ZC=110,sw.on-time=0/0
```

### `+DIMCF=write`

Write EEPROM and configuration

### `+DIMCF=factory`

Restore ATMega factory settings

### `+DIMCF=zc,<value>`

Set zero crossing offset (16bit) in CPU cycles (16MHz = 62.5ns). The EEPROM must be written to store the value permanently

### `+DIMCF=zc,<+-value>`

Increase or decrease zero crossing offset
