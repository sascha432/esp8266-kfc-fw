# AT Mode Commands

After the command help takes a lot of FLASH memory, it has been moved to this document here. Most commands without DEBUG MODE are listed.

## Common Commands

### ``+AT?``

Print ``OK``

### ``+REM=...``

Ignore comment

### ``+DSLP=[<milliseconds>[,<mode>]]``

Enter deep sleep

### ``+RST=[<s>]``

Reset device. If ``s`` is provided, it will reboot in SAFE MODE.

### ``+LOAD``

Discard changes and load settings from EEPROM

### ``+STORE``

Store current settings in EEPROM

### ``+FACTORY``

Restore factory settings (but do not store in EEPROM)

### ``+FSR``

Restore factory settings, store in EEPROM and reboot device

### ``+ATMODE=<1|0>``

Enable/disable AT Mode if compiled in

### ``+HEAP=<interval>``

Display heap usage every interval (can be 1s or 1000ms, 0 shows it once)

### ``+GPIO=<interval>``

Display GPIO pin states every interval (can be 1s or 1000ms, 0 shows it once)

### ``+PWM=<pin>,<input|input_pullup|high|low|waveform|level=0-1023[,<frequency=100-40000Hz>[,<duration/ms>]]``

Control PIN input, output and PWM state. Setting a PIN high or low implicitly sets it to output.

### ``+DLY=<milliseconds>``

Call delay(milliseconds)

### ``+CAT=<filename>``

Display file contents

### ``+RM=<filename>``

Remove file from FS

### ``+RN=<filename>,<new filename>``

Rename file or directory

### ``+LS=[<directory>[,<hidden=true|false>,<subdirs=true|false>]]``

List files and directories

### ``+LSR=[<directory>]``

List files and directories using FS.openDir(). This will not display read only virtual files.

### ``+AOTA=<start|stop>``

Start/stop Arduino OTA (required about 1K RAM to run)

### ``+PLG=<list|start|stop|add-blacklist|add|remove>[,<name>]``

Plugin management. If a plugin malfunctions, it can be blacklisted in SAFE MODE.

### ``+RTC=[set]``

Display RTC status or set RTC time from current time. Only available with a real time clock (i.e. DS3231)

### ``+WIFI=<command>,[args]``

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

### ``+LED=<slow,fast,flicker,off,solid,sos,pattern>,[,color=0xff0000|pattern=10110...][,pin]``

Set internal LED mode or an LED on a certain PIN.

### ``+NEOPX=<pin>,<num>,<r>,<g>,<b>``

Set NeoPixel colors for a given PIN if available

### ``+METRICS``

Displays versions of the SDK, framework, libraries, memory addresses and a lot more. Available in DEBUG mode only

### ``+PING=<target[,count=4[,timeout=5000]]>``

Ping host or IP address if compiled in

### ``+I2CS=<pin-sda>,<pin-scl>[,<speed=100000>,<clock-stretch=45000>,<start|stop>]``

Configure I2C Bus

### ``+I2CTM=<address>,<data,...>``

Transmsit data to slave

### ``+I2CRQ=<address>,<length>``

Request data from slave

### ``+I2CSCAN=[<start-address=1>][,<end-address=127>][,<sda=4|any|no-init>,<scl=5>]``

Scan I2C Bus. If ANY is passed as third argument, all available PINs are probed for I2C devices. Available only if compiled in

### ``PWM=<pin>,<input|input_pullup|waveform|level=0-1024>[,<frequency=100-40000Hz>[,<duration/ms>]]

PWM output on PIN, min./max. level set it to LOW/HIGH" using digitalWrite


## 7 Segment Clock

### ``+LMTESTP=<#color>[,<time=500ms>]``

Test peak values. WARNING! This command will bypass all protections and limits

### ``+LMC=<command>[,<options>]``

Run command

- vis[ualizer],<type>
- br[ightness],<level>
- co[lor],<#RGB|r,b,g>
- pr[int],<display=00:00:00>
- tem[perature],<value>
- lo[op],<enable|disable>
- met[hod]<fast[led]|neo[pixel]|neo_rep[eat]>
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

``+LMVIEW=<interval in ms|0=disable>,<client_id>``

Display LEDs over http2serial

## Weather Station / Clock / LED Matrix

### ``+WSSET=<touchpad|timeformat24h|metric|tft|scroll|stats|lock|unlock|screen|screens>,<on|off|options>``

Enable/disable function

### ``+WSBL=<level=0-1023>``

Set backlight level

### ``+WSU=<i|f>``

Update weather info/forecast

### ``+WSM=<date YYYY-MM-DD>[,<days>]``

Show Moon Phase for given date. Only available in DEBUG MODE

### ``+MDNSR`=<service>,<proto>,[<wait=3000ms>]``

Query MDNS

### ``+MDNSR=<stop|start|enable|disable|zeroconf>``

Configure MDNS

## Blinds Controller

### ``+BCME=<open|close|stop|tone|imperial|init>[,<channel>][,<tone_frequency>,<tone_pwm_value>]``

Open, close a channel, stop motor or run tone test, play imperial march, initial state

## MQTT

### ``+MQTT=con|dis|set|top|auto|list``

Manage MQTT

  con[nect]                                   Connect to server
  dis[connect][,<true|false>]                 Disconnect from server and enable/disable auto reconnect
  set,<enable,disable>                        Enable or disable MQTT
  top[ics]                                    List subscribed topics
  auto[discovery][,restart][,force]           Publish auto discovery
  list[,<full|crc|file>]                      List auto discovery (file = /.logs/mqtt_auto_discovery.json)

## Weather Station

### ``+WSSET=<touchpad|timeformat24h|metric|tft|scroll|stats|lock|unlock|screen|screens>,<on|off|options>``

### ``+WSBL=<level=0-1023>``

Set backlight level

### ``+WSU=<i|f>``

Update weather info/forecast

## STK500v1 Programmer

### ``+STK500V1F=<filename>,[<0=Serial/1=Serial1>[,<0=disable/1=logger/2=serial/3=serial2http/4=file>]]``

Flash .HEX file to AVR MCU over Serial Port (requires bootloader)

__NOTE__: If the process fails, check if the voltage does not drop too much during the process

For example: ``+STK500V1F=/stk500v1/atomicsun-firmware-2.2.3-328pb.hex``

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

### ``+STK500V1S=<atmega328p/0x1e1234/...>``

Set signature (available names in `/stk500v1/atmega.csv`)

### ``+STK500V1S?``

Display signature

### ``Debug log file``

Debug log file is stored in ``/stk500v1/debug.log``

## Dimmer

### ``+DIMG``

Get level(s)

### ``+DIMS=<channel>,<level>[,<time>]``

Set level

### ``+DIMCF=reset|weeprom|info|print|write|factory|zc``

Configure dimmer firmware

#### ``+DIMCF=reset``

Hard reset MCU

#### ``+DIMCF=weeprom``

Write EEPROM

#### ``+DIMCF=info``

Display dimmer info and config over serial port

``` text
+REM=MOSFET Dimmer 2.2.3 Nov 22 2022 17:50:49 Author sascha_lammers@gmx.de
+REM=sig=1e-95-16,fuses=l:ff,h:da,e:f5,MCU=ATmega328PB@8Mhz,gcc=7.3.0
+REM=options=EEPROM=62,NTC=A0,int.temp,VCC,fading_events=1,proto=UART,addr=17,mode=T,timer1=8/1.00,lvls=8192,pins=6,8,9,10,cubic=0,range=0-8192
+REM=values=restore=1,f=60.342Hz,vref11=1.100,NTC=27.20/+0.00,int.temp=36/ofs=88/gain=156,max.temp=75,metrics=5,VCC=3.229,min.on-time=300,min.off=300,ZC=110,sw.on-time=0/0
```

#### ``+DIMCF=write``

Write EEPROM and configuration

#### ``+DIMCF=factory``

Restore factory settings

#### ``+DIMCF=zc,<value>``

Set zero crossing offset (16bit) in CPU cycles (16MHz = 62.5ns). The EEPROM must be written to store the value permanently

#### ``+DIMCF=zc,<+-value>``

Increase or decrease zero crossing offset

