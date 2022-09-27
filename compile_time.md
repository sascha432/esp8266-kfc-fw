# Compile time Linux vs. Windows

## Hardware

- i9-12900H Laptop CPU
- 1TB SSD
- 32GB RAM

## Procedure

- ``pio run`` to get dependencies etc...
- ``pio run -t clean ;  pio run`` a couple times

## Linux (ext4)

Environment    Status    Duration
-------------  --------  ------------
ws_99          SUCCESS   00:00:40.434
hexpanel_17    SUCCESS   00:00:40.861
bme280_48      SUCCESS   00:00:31.468
blindsctrl_80l SUCCESS   00:00:34.259
rlybrd_85      SUCCESS   00:00:33.927
clockv2_100    SUCCESS   00:00:40.905

## Windows 11 (NTFS)

Environment    Status    Duration
-------------  --------  ------------
