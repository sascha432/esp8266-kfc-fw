/**
 * Author: sascha_lammers@gmx.de
 */

//https://github.com/jainrk/i2c_port_address_scanner
/*
* i2c_port_address_scanner
* Scans ports D0 to D7 on an ESP8266 and searches for I2C device. based on the original code
* available on Arduino.cc and later improved by user Krodal and Nick Gammon (www.gammon.com.au/forum/?id=10896)
* D8 throws exceptions thus it has been left out
*
*/
#if I2CSCANNER_PLUGIN

// TODO could have some more options and cleaner code

#include <Wire.h>
#include "i2c_scanner.h"
#include "plugins.h"

void check_if_exist_I2C(Print &output);

uint8_t portArray[] = {16, 5, 4, 0, 2, 14, 12, 13};
//String portMap[] = {"D0", "D1", "D2", "D3", "D4", "D5", "D6", "D7"}; //for Wemos
String portMap[] = {"GPIO16", "GPIO5", "GPIO4", "GPIO0", "GPIO2", "GPIO14", "GPIO12", "GPIO13"};

void scanPorts(Print &output) {
    output.println("\n\nI2C Scanner to scan for devices on each port pair D0 to D7");
    for (uint8_t i = 0; i < sizeof(portArray); i++) {
        for (uint8_t j = 0; j < sizeof(portArray); j++) {
            if (i != j){
            output.print("Scanning (SDA : SCL) - " + portMap[i] + " : " + portMap[j] + " - ");
            Wire.begin(portArray[i], portArray[j]);
            check_if_exist_I2C(output);
            }
        }
    }
}

void check_if_exist_I2C(Print &output) {
  byte error, address;
  int nDevices;
  nDevices = 0;
  for (address = 1; address < 127; address++ )  {
    // The i2c_scanner uses the return value of
    // the Write.endTransmisstion to see if
    // a device did acknowledge to the address.
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0){
      output.print("I2C device found at address 0x");
      if (address < 16)
        output.print("0");
      output.print(address, HEX);
      output.println("  !");

      nDevices++;
    } else if (error == 4) {
      output.print("Unknow error at address 0x");
      if (address < 16)
        output.print("0");
      output.println(address, HEX);
    }
  } //for loop
  if (nDevices == 0)
    output.println("No I2C devices found");
  else
    output.println("**********************************\n");
  //delay(1000);           // wait 1 seconds for next scan, did not find it necessary
}

#if AT_MODE_SUPPORTED

#include "at_mode.h"

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(SCANI2C, "SCANI2C", "Scan for I2C bus and devices");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(I2CS, "I2CS", "<SDA,SCL[,speed=100kHz[,clock strech limit=usec]]>", "Setup I2C bus");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(I2CT, "I2CT", "<stop,address,byte[,byte][,byte,...]>", "Send data to slave");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(I2CR, "I2CR", "<address,count>", "Request data from slave");

static int __toint(const char *s) {
    if (*s == 'x') {
        return strtoul(s + 1, 0, 16);
    } else if (*s == '0' && *(s + 1) == 'x') {
        return strtoul(s + 2, 0, 16);
    } else {
        return atoi(s);
    }
}

void i2cscanner_setup() {
    Wire.begin(4, 5);
    Wire.setClockStretchLimit(15000);
}

bool i2cscanner_at_mode_command_handler(Stream &serial, const String &command, int8_t argc, char **argv) {
    if (command.length() == 0) {

        at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(SCANI2C));
        at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(I2CS));
        at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(I2CT));
        at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(I2CR));

    } 
    else if (constexpr_String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(I2CS))) {
        if (argc < 2) {
            at_mode_print_invalid_arguments(serial);
        } 
        else {
            int sda, scl, speed = 100, setClockStretchLimit = 0;
            Wire.begin(sda = __toint(argv[0]), scl = __toint(argv[1]));
            if (argc >= 3) {
                speed = __toint(argv[2]);
                if (argc >= 4) {
                    setClockStretchLimit = __toint(argv[3]);
                }
            }
            Wire.setClock(speed * 1e3L);
            if (setClockStretchLimit) {
                Wire.setClockStretchLimit(setClockStretchLimit);
            }
            serial.printf_P(PSTR("+I2CS: SDA=%d, SCL=%d, speed=%ukHz, setClockStretchLimit=%d\n"), sda, scl, speed, setClockStretchLimit);
        }
        return true;
    } else if (constexpr_String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(I2CT))) {
        if (argc < 1) {
            at_mode_print_invalid_arguments(serial);
        }
        else {
            int tmp;
            Wire.beginTransmission(tmp = __toint(argv[0]));
            serial.printf_P(PSTR("+I2CT: address=%02x"), tmp);
            if (argc > 1) {
                serial.print(',');
            }
            for(uint8_t i = 1; i < argc; i++) {
                tmp = __toint(argv[i]);
                Wire.write((uint8_t)tmp);
                serial.printf_P(PSTR(" %02x"), tmp);
            }
            auto result = Wire.endTransmission();
            serial.printf_P(PSTR(", result = %u (%02x)\n"), result, result);
        }
        return true;
    } else if (constexpr_String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(I2CR))) {
        if (argc != 2) {
            at_mode_print_invalid_arguments(serial);
        }
        else {
            int address, count;
            address = __toint(argv[0]);
            count = __toint(argv[1]);
            serial.printf_P(PSTR("+I2CR: address=%02x, count=%d, "), address, count);
            auto result = Wire.requestFrom(address, count);
            serial.printf_P(PSTR("result=%u, expected=%u, "), result, count);
            uint16_t word = 0;
            uint32_t qword = 0;
            int pos = 0;
            while(result--) {
                int byte = Wire.read();
                word = (word >> 8) | (byte << 8);
                qword = (qword >> 8) | (byte << 24);
                serial.printf_P(PSTR("[%02x] %#02x(%u) "), pos, byte, byte);
                if (pos > 0) {
                    serial.printf_P(PSTR("%#04x(%u) "), word, word);
                }
                if (pos > 2) {
                    serial.printf_P(PSTR("%#08x(%u) "), qword, qword);
                }
                pos++;
            }
            serial.println();
        }
        return true;
    } 
    else if (constexpr_String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(SCANI2C))) {
        scanPorts(serial);
        return true;
    }
    return false;
}

#endif

PROGMEM_PLUGIN_CONFIG_DEF(
/* pluginName               */ i2c_scan,
/* setupPriority            */ PLUGIN_MIN_PRIORITY,
/* allowSafeMode            */ false,
/* autoSetupWakeUp          */ false,
/* rtcMemoryId              */ 0,
/* setupPlugin              */ i2cscanner_setup,
// /* setupPlugin              */ nullptr,
/* statusTemplate           */ nullptr,
/* configureForm            */ nullptr,
/* reconfigurePlugin        */ nullptr,
/* reconfigure Dependencies */ nullptr,
/* prepareDeepSleep         */ nullptr,
/* atModeCommandHandler     */ i2cscanner_at_mode_command_handler
);

#endif
