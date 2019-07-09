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

bool i2cscanner_at_mode_command_handler(Stream &serial, const String &command, int8_t argc, char **argv) {
    if (command.length() == 0) {
        serial.print(F(
          " AT+SCANI2C\n"
          "    Scan for I2C bus and devices\n"
        ));
    } else if (command.equalsIgnoreCase(F("SCANI2C"))) {
        scanPorts(serial);
        return true;
    }
    return false;
}

void add_plugin_i2cscanner_plugin() {
    Plugin_t plugin;

    init_plugin(F("i2scanner"), plugin, false, false, 200);

    plugin.atModeCommandHandler = i2cscanner_at_mode_command_handler;
    register_plugin(plugin);
}

#endif
