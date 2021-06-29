# GPIO behavior during reboot and wakeup from deep sleep

This was tested with a NodeMCU board. 3.3V

Low < 0.3V
High > 3V
High goes down arround 80-95ms >2.55V

## NodeMCU / GPIO pins

Depending on the levelshifter, GPIO 1, 3, 9 and 10 can be used. GPIO 9 and 10 require to run the flash memory in DIO mode.

  NodeMCU | PIN | Description |
|---|---|---|
| D0 | GPIO16 | floating (or connected to reset, directly or via low value series resistor = pullup) |
| D1 | GPIO5 | floating
| D2 | GPIO4 | floating
| D3 | GPIO0 | pullup
| D4 | GPIO2 | pullup
| D5 | GPIO14 | floating
| D6 | GPIO12 | floating
| D7 | GPIO13 | floating
| D8 | GPIO15 | pulldown
| RX | GPIO3 | serial2usb converter
| TX | GPIO1 | serial2usb converter
| S2 | GPIO9 | flash memory (QIO mode)
| S3 | GPIO10 | flash memory (QIO mode)
| A0 | ADC0/TOUT | floating


## External Reset

A0 - low
D0 - high
D1 - low
D2 - low
D3 = high, 18-72ms 1.8V, 72-80 low, high
D4 = high, 17.6-48.8ms 16Khz PWM with a short low period (10uF, 10k>2.75V, 1K>1.6V, 200R>1.3V), high
D5 = high
D6 = high
D7 = high
D8 = low, high during reset, @19ms low
RX = high
TX = same as D4 (sending some serial data @ 78600baud)
S3 = high
S2 = low with random PWM high
S1 = same as S2
SC = high with random PWM low
S0 = same as S2
SK = high

## Wakeup From Deep Sleep (10s)

The only difference is D0 when sending the reset signal, and D8

A0 - low
D0 - 220us low durin reset, high
D1 - low
D2 - low
D3 - high, 18-72ms 1.8V, 72-80 low, high
D4 - high, 17.6-48.8ms 16Khz PWM with a short low period (10uF, 10k>2.75V, 1K>1.6V, 200R>1.3V), high
D5 - high
D6 - high
D7 - high
D8 - low, from reset to 36ms 0.8V, low, 12us high @200ms
RX - high
TX = same as D4 (sending some serial data @ 78600baud)
