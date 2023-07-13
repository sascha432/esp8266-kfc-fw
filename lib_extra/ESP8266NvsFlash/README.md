# NVS Storage for ESP8266_NONOS_SDK

This is a port of the nvs_flash code of the ESP8266_RTOS_SDK

## Documentation

[https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html)

## Partitions

2 Partitions are supported and need to be defined in the eagle.ld file. The default names are 'nvs' and 'nvs2'. The names can be changed in 'esp_partition.cpp'. The ESP8266 does not use 'nvs' by default. Each NVS partition needs to have at least 3 sectors, but 6 or more are recommended

## Second partition

The second partition is optional and can have 0 sectors. It needs to be initialized before use with 'nvs_flash_init_partition'

## Modifications

The code is only slightly modified, but relies on a replacement of the 'esp_partition_*' functions since the NONOS_SDK does not have a readable runtime partition table. The partitions are grabbed from the eagle.ld linking file

## Encryption

Encryption is not supported
