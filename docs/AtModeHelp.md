# AT Mode Commands

After the command help takes a lot of FLASH memory, it has been moved to this document here

## TODO

copy help

## Common Commands

### TODO

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

``LMVIEW=<interval in ms|0=disable>,<client_id>``

Display LEDs over http2serial
