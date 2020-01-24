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

#include "kfc_fw_config.h"
#include "i2c_scanner.h"
#include "plugins.h"

#if IOT_SENSOR_HAVE_INA219
#include "Adafruit_INA219.h"
#ifndef _LIB_ADAFRUIT_INA219_
#define _LIB_ADAFRUIT_INA219_
#endif
#endif
#if IOT_SENSOR_HAVE_BME280
#include "Adafruit_BME280.h"
#ifndef __BME280_H__
#define __BME280_H__
#endif
#endif
#if IOT_SENSOR_HAVE_BME680
#include "Adafruit_BME680.h"
#ifndef __BME680_H__
#define __BME680_H__
#endif
#endif
#if IOT_SENSOR_HAVE_CCS811
#include "Adafruit_CCS811.h"
#ifndef __CCS811_H__
#define __CCS811_H__
#endif
#endif

void check_if_exist_I2C(Print &output);

uint8_t portArray[] = { 16, 5, 4, 0, 2, 12, 13 };
const char *portMap[] = { "GPIO16", "GPIO5", "GPIO4", "GPIO0", "GPIO2", "GPIO12", "GPIO13" };

void scanPorts(Print &output) {
    output.println(F("\n\nI2C Scanner to scan for devices on each port pair D0 to D7"));
    for (uint8_t i = 0; i < sizeof(portArray); i++) {
        for (uint8_t j = 0; j < sizeof(portArray); j++) {
            if (i != j) {
                output.printf_P(PSTR("Scanning (SDA : SCL) - %s : %s - "), portMap[i], portMap[j]);
                Wire.begin(portArray[i], portArray[j]);
                check_if_exist_I2C(output);
            }
        }
    }
}

void check_if_exist_I2C(Print &output) {
    uint8_t error, address;
    int nDevices;

    nDevices = 0;
    for (address = 1; address < 127; address++ )  {

        // The i2c_scanner uses the return value of
        // the Write.endTransmisstion to see if
        // a device did acknowledge to the address.
        Wire.beginTransmission(address);
        error = Wire.endTransmission();

        if (error == 0 || error == 3) { // 0=ACK, 3=NACK for data
            output.printf_P(PSTR("I2C device found at address 0x%02x\n"), address);
            nDevices++;
        }
        else if (error == 4) {
            output.printf_P(PSTR("Unknown error at address 0x%02x\n"), address);
        }
    } //for loop
    if (nDevices == 0) {
        output.println(F("No I2C devices found"));
    }
    else {
        output.println(F("**********************************\n"));
    }
    //delay(1000);           // wait 1 seconds for next scan, did not find it necessary
}

#if AT_MODE_SUPPORTED

#include "at_mode.h"

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(SCANI2C, "SCANI2C", "Scan for I2C bus and devices (might cause a crash)");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PNPN(SCAND, "SCAND", "Scan for devices");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(I2CS, "I2CS", "<SDA,SCL[,speed=100kHz[,clock strech limit=usec]]>", "Setup I2C bus. Use reset to restore defaults");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(I2CST, "I2CST", "<address,byte[,byte][,...]>", "Send data to slave (hex encoded: ff or 0xff)");
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(I2CSR, "I2CSR", "<address,count>", "Request data from slave");
#ifdef _LIB_ADAFRUIT_INA219_
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(I2CINA219, "I2CINA219", "<address>", "Read INA219 sensor");
#endif
#ifdef __CCS811_H__
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(I2CCCS811, "I2CCCS811", "<address>[,<temperature=25>][,<humidity=55>]", "Read CCS811 sensor");
#endif
#ifdef __BME680_H__
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(I2CBME680, "I2CBME680", "<address>", "Read BME680 sensor");
#endif
#ifdef __BME280_H__
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(I2CBME280, "I2CBME280", "<address>", "Read BME280 sensor");
#endif
PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(I2CLM75A, "I2CLM75A", "<address>", "Read temperature from LM75A");

void i2cscanner_device_error(Stream &output) {
    output.println(F("+I2C: An error occured"));
}

#endif

class I2CScannerPlugin : public PluginComponent {
public:
    I2CScannerPlugin() {
        REGISTER_PLUGIN(this, "I2CScannerPlugin");
    }
    virtual PGM_P getName() const {
        return PSTR("i2c_scan");
    }

    PluginPriorityEnum_t getSetupPriority() const {
        return MIN_PRIORITY;
    }

    void setup(PluginSetupMode_t mode) override {
        config.initTwoWire();
    }

#if AT_MODE_SUPPORTED
    bool hasAtMode() const override {
        return true;
    }
    void atModeHelpGenerator() override;
    bool atModeHandler(AtModeArgs &args) override;
#endif
};

static I2CScannerPlugin plugin;

#if AT_MODE_SUPPORTED

void I2CScannerPlugin::atModeHelpGenerator() {
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(SCANI2C), getName());
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(SCAND), getName());
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(I2CS), getName());
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(I2CST), getName());
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(I2CSR), getName());
#ifdef _LIB_ADAFRUIT_INA219_
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(I2CINA219), getName());
#endif
#ifdef __CCS811_H__
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(I2CCCS811), getName());
#endif
#ifdef __BME680_H__
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(I2CBME680), getName());
#endif
#ifdef __BME280_H__
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(I2CBME280), getName());
#endif
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(I2CLM75A), getName());
}

bool I2CScannerPlugin::atModeHandler(AtModeArgs &args)
{
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(I2CS))) {
        if (args.requireArgs(1, 4)) {
            if (args.equalsIgnoreCase(0, F("reset"))) {
                auto &serial = args.getStream();
                serial.print(F("+I2CS: "));
                config.initTwoWire(true, &serial);
            }
            else if (args.requireArgs(2, 4)) {
                int sda = args.toNumber(0, KFC_TWOWIRE_SDA);
                int scl = args.toNumber(1, KFC_TWOWIRE_SCL);
                uint32_t speed = args.toNumber(2, 100) * 1000UL;
                uint32_t setClockStretchLimit = args.toInt(3);
                Wire.setClock(speed);
                if (setClockStretchLimit) {
                    Wire.setClockStretchLimit(setClockStretchLimit);
                }
                args.printf_P(PSTR("SDA=%d, SCL=%d, speed=%ukHz, setClockStretchLimit=%u"), sda, scl, speed / 1000U, setClockStretchLimit);
            }
        }
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(I2CST))) {
        if (args.requireArgs(1)) {
            auto &serial = args.getStream();
            int address = args.toNumber(0);
            Wire.beginTransmission(address);
            serial.printf_P(PSTR("+I2CST: address=%02x"), address);
            if (args.size() > 1) {
                serial.print(',');
            }
            for(uint8_t i = 1; i < args.size(); i++) {
                int tmp = args.toNumber(i);
                Wire.write((uint8_t)tmp);
                serial.printf_P(PSTR(" %02x"), tmp);
            }
            auto result = Wire.endTransmission();
            serial.printf_P(PSTR(", result = %u (%02x)\n"), result, result);
        }
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(I2CSR))) {
        if (args.requireArgs(2, 2)) {
            auto &serial = args.getStream();
            int address = args.toNumber(0);
            int count = args.toNumber(1);
            serial.printf_P(PSTR("+I2CSR: address=%02x, count=%d, "), address, count);
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
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(SCANI2C))) {
        scanPorts(args.getStream());
        return true;
    }
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(SCAND))) {
        args.print(F("Scanning I2C bus for devices..."));
        check_if_exist_I2C(args.getStream());
        return true;
    }
#ifdef _LIB_ADAFRUIT_INA219_
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(I2CINA219))) {
        if (args.requireArgs(1, 1)) {
            int address = args.toNumber(0, INA219_ADDRESS);
            Adafruit_INA219 ina219(address);
            ina219.begin();
            args.printf_P(PSTR("raw: Vbus %d, Vshunt %d, I %d, P %d"), ina219.getBusVoltage_raw(), ina219.getShuntVoltage_raw(), ina219.getCurrent_raw(), ina219.getPower_raw());
        }
        return true;
    }
#endif
#ifdef __CCS811_H__
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(I2CCCS811))) {
        if (args.requireArgs(1, 3)) {
            Adafruit_CCS811 ccs811; // +i2cccs811=0x5a,25.07,53
            int address = args.toNumber(0, CCS811_ADDRESS);
            double temperature = args.toDouble(1, 25.0);
            uint8_t humidity = (uint8_t)args.toIntMinMax(2, 10, 99, 55);
            ccs811.begin(address);
            ccs811.setEnvironmentalData(humidity, temperature);
            auto available = ccs811.available();
            auto result = ccs811.readData();
            auto error = ccs811.checkError();
            serial.printf_P(PSTR("Reading CCS811 at 0x%02x: available()=%u, readData()=%u, checkError()=%u, eCO2 %u, TVOC %u, calculateTemperature %f\n"), address, available, result, error, ccs811.geteCO2(), ccs811.getTVOC(), ccs811.calculateTemperature());
        }
        return true;
    }
#endif
#ifdef __BME680_H__
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(I2CBME680))) {
        if (args.requireArgs(1, 1)) {
            Adafruit_BME680 bme680;
            int address = args.toNumber(0, 0x77);
            bme680.begin(address);
            serial.printf_P(PSTR("Reading BME680 at 0x%02x: %.2f °C, %.2f%%, %.2f hPa, gas %u\n"), address, bme680.readTemperature(), bme680.readHumidity(), bme680.readPressure() / 100.0, bme680.readGas());
        }
        return true;
    }
#endif
#ifdef __BME280_H__
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(I2CBME280))) {
        if (args.requireArgs(1, 1)) {
            Adafruit_BME280 bme280;
            int address = args.toNumber(0, 0x76);
            bme280.begin(address, &Wire);
            serial.printf_P(PSTR("Reading BME280 at 0x%02x: %.2f °C, %.2f%%, %.2f hPa\n"), address, bme280.readTemperature(), bme280.readHumidity(), bme280.readPressure() / 100.0);
        }
        return true;
    }
#endif
    else if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(I2CLM75A))) {
        if (args.requireArgs(1, 1)) {
            int address = args.toNumber(0, 0x48);
            auto &serial = args.getStream();
            serial.printf_P(PSTR("Reading LM75A at 0x%02x: "), address);
            Wire.beginTransmission(address);
            Wire.write(0x00);
            if (Wire.endTransmission() == 0 && Wire.requestFrom(address, 2) == 2) {
                auto temp = (((uint8_t)Wire.read() << 8) | (uint8_t)Wire.read()) / 256.0;
                serial.printf_P(PSTR("%.2f °C\n"), temp);
            }
            else {
                serial.println();
                i2cscanner_device_error(serial);
            }
        }
        return true;
    }
    return false;
}

#endif

#endif
