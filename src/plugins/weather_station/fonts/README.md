# How to build the fonts

## Requirements

 - Adafruit GFX Library/fontconvert
 - gcc
 - make
 - freetype2-devel, libfreetype6-dev (those have been tested)
 - cygwin or WSL2 to create them with Windows

## Compiling fontconvert

Goto directory and run make ``src/plugins/weather_station/fonts/fontconvert``. Further information about the software can be found at [https://github.com/sascha432/Adafruit-GFX-Library](https://github.com/sascha432/Adafruit-GFX-Library)

``` bash
sascha:~/Documents/PlatformIO/Projects/esp8266-kfc-fw/src/plugins/weather_station/fonts/fontconvert$ make
gcc -Wall -I/usr/local/include/freetype2 -I/usr/include/freetype2 -I/usr/include fontconvert.c -lfreetype -o fontconvert
fontconvert.c: In function ‘main’:
fontconvert.c:129:3: warning: implicit declaration of function ‘FT_Property_Set’ [-Wimplicit-function-declaration]
  129 |   FT_Property_Set(library, "truetype", "interpreter-version",
      |   ^~~~~~~~~~~~~~~
strip fontconvert
sascha:~/Documents/PlatformIO/Projects/esp8266-kfc-fw/src/plugins/weather_station/fonts/fontconvert$ ls -alh fontconvert
-rwxrwxr-x 1 sascha sascha 15K Sep 27 10:21 fontconvert
```

## Creating the fonts

Change directory to ``src/plugins/weather_station/fonts`` and run ``bash ./makefonts``. This will create a directory ``output`` with a variety of fonts and different sizes

``` bash
sascha:~/Documents/PlatformIO/Projects/esp8266-kfc-fw/src/plugins/weather_station/fonts$ bash ./makefonts 
Creating fonts in "/home/sascha/Documents/PlatformIO/Projects/esp8266-kfc-fw/src/plugins/weather_station/fonts/output"
Using "/home/sascha/Documents/PlatformIO/Projects/esp8266-kfc-fw/src/plugins/weather_station/fonts/fontconvert/fontconvert"
DejaVuSans_Bold 2 OK
DejaVuSans_Bold 3 OK
DejaVuSans_Bold 4 OK
DejaVuSans_Bold 5 OK
DejaVuSans_Bold 6 OK
DejaVuSans_Bold 7 OK
DejaVuSans_Bold 8 OK
DejaVuSans_Bold 9 OK
DejaVuSans_Bold 10 OK
DejaVuSans_Bold 11 OK
DejaVuSans_Bold 12 OK
DejaVuSans_Bold 13 OK
DejaVuSans_Bold 14 OK
DejaVuSans_Bold 15 OK
DejaVuSans 2 OK
DejaVuSans 3 OK
DejaVuSans 4 OK
DejaVuSans 5 OK
DejaVuSans 6 OK
DejaVuSans 7 OK
DejaVuSans 8 OK
DejaVuSans 9 OK
DejaVuSans 10 OK
DejaVuSans 11 OK
DejaVuSans 12 OK
DejaVuSans 13 OK
DejaVuSans 14 OK
DejaVuSans 15 OK
Dialog_Bold 2 OK
Dialog_Bold 3 OK
Dialog_Bold 4 OK
Dialog_Bold 5 OK
Dialog_Bold 6 OK
Dialog_Bold 7 OK
Dialog_Bold 8 OK
Dialog_Bold 9 OK
Dialog_Bold 10 OK
Dialog_Bold 11 OK
Dialog_Bold 12 OK
Dialog_Bold 13 OK
Dialog_Bold 14 OK
Dialog_Bold 15 OK
Dialog 2 OK
Dialog 3 OK
Dialog 4 OK
Dialog 5 OK
Dialog 6 OK
Dialog 7 OK
Dialog 8 OK
Dialog 9 OK
Dialog 10 OK
Dialog 11 OK
Dialog 12 OK
Dialog 13 OK
Dialog 14 OK
Dialog 15 OK
simple_small_pixels 2 OK
simple_small_pixels 3 OK
simple_small_pixels 4 OK
simple_small_pixels 5 OK
simple_small_pixels 6 OK
simple_small_pixels 7 OK
simple_small_pixels 8 OK
simple_small_pixels 9 OK
simple_small_pixels 10 OK
simple_small_pixels 11 OK
simple_small_pixels 12 OK
simple_small_pixels 13 OK
simple_small_pixels 14 OK
simple_small_pixels 15 OK
Copying headers
Done.
```