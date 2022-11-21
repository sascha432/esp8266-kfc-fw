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

### ``+TORE``

Store current settings in EEPROM

### ``+FACTORY``

Restore factory settings (but do not store in EEPROM)

### ``+FSR``

Restore factory settings, store in EEPROM and reboot device

### ``+ATMODE=<1|0>``

Enable/disable AT Mode if compiled in

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

Flash .HEX file to AVR MCU over serial boot loader

For example: ``+STK500V1F=/stk500v1/atomicsun-firmware-2.2.3-328pb.hex``

### ``+STK500V1S=<atmega328p/0x1e1234/...>``

Set signature (/stk500v1/atmega.csv)

### ``+STK500V1S?``

Display signature

### ``Debug log file``

Debug log file (/stk500v1/debug.log)

## Dimmer

### ``+DIMG``

Get level(s)

### ``+DIMS=<channel>,<level>[,<time>]``

Set level

### ``+DIMCF=<reset|weeprom|info|print|write|factory|zc,value>``

Configure dimmer firmware

#### ``+DIMCF=zc,value``

Set zero crossing offset

