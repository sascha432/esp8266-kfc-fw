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

static int __toint(const char *s, uint8_t base = 10) {
    if (*s == 'x') {
        return strtoul(s + 1, 0, 16);
    } else if (*s == '0' && *(s + 1) == 'x') {
        return strtoul(s + 2, 0, 16);
    } else {
        return strtoul(s, 0, base);
    }
}

void i2cscanner_device_error(Stream &output) {
    output.println(F("+I2C: An error occured"));
}

#endif

class I2CScannerPlugin : public PluginComponent {
public:
    I2CScannerPlugin() {
        REGISTER_PLUGIN(this, "I2CScannerPlugin");
    }
    PGM_P getName() const;

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
    bool atModeHandler(Stream &serial, const String &command, int8_t argc, char **argv) override;
#endif
};

static I2CScannerPlugin plugin;

PGM_P I2CScannerPlugin::getName() const {
    return PSTR("i2c_scan");
}

#if AT_MODE_SUPPORTED

void I2CScannerPlugin::atModeHelpGenerator() {
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(SCANI2C));
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(SCAND));
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(I2CS));
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(I2CST));
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(I2CSR));
#ifdef _LIB_ADAFRUIT_INA219_
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(I2CINA219));
#endif
#ifdef __CCS811_H__
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(I2CCCS811));
#endif
#ifdef __BME680_H__
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(I2CBME680));
#endif
#ifdef __BME280_H__
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(I2CBME280));
#endif
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(I2CLM75A));
}

bool I2CScannerPlugin::atModeHandler(Stream &serial, const String &command, int8_t argc, char **argv) {
    if (constexpr_String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(I2CS))) {
        if (argc == 1 && constexpr_String_equalsIgnoreCase(argv[0], PSTR("reset"))) {
            serial.print(F("+I2CS: "));
            config.initTwoWire(true, &serial);
        }
        else if (argc < 2) {
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
    } else if (constexpr_String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(I2CST))) {
        if (argc < 1) {
            at_mode_print_invalid_arguments(serial);
        }
        else {
            int tmp;
            Wire.beginTransmission(tmp = __toint(argv[0], 16));
            serial.printf_P(PSTR("+I2CST: address=%02x"), tmp);
            if (argc > 1) {
                serial.print(',');
            }
            for(uint8_t i = 1; i < argc; i++) {
                tmp = __toint(argv[i], 16);
                Wire.write((uint8_t)tmp);
                serial.printf_P(PSTR(" %02x"), tmp);
            }
            auto result = Wire.endTransmission();
            serial.printf_P(PSTR(", result = %u (%02x)\n"), result, result);
        }
        return true;
    } else if (constexpr_String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(I2CSR))) {
        if (argc != 2) {
            at_mode_print_invalid_arguments(serial);
        }
        else {
            int address, count;
            address = __toint(argv[0], 16);
            count = __toint(argv[1], 16);
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
    else if (constexpr_String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(SCANI2C))) {
        scanPorts(serial);
        return true;
    }
    else if (constexpr_String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(SCAND))) {
        serial.println(F("Scanning I2C bus for devices..."));
        check_if_exist_I2C(serial);
        return true;
    }
#ifdef _LIB_ADAFRUIT_INA219_
    else if (constexpr_String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(I2CINA219))) {
        if (argc < 1) {
            at_mode_print_invalid_arguments(serial);
        }
        else {
            int address = __toint(argv[0]);
            if (address == 0) {
                address = INA219_ADDRESS;
            }
            Adafruit_INA219 ina219(address);
            ina219.begin();
            serial.printf_P(PSTR("+I2CINA219: raw: Vbus %d, Vshunt %d, I %d, P %d\n"), ina219.getBusVoltage_raw(), ina219.getShuntVoltage_raw(), ina219.getCurrent_raw(), ina219.getPower_raw());
        }
        return true;
    }
#endif
#ifdef __CCS811_H__
    else if (constexpr_String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(I2CCCS811))) {
        if (argc < 1) {
            at_mode_print_invalid_arguments(serial);
        }
        else {
            Adafruit_CCS811 ccs811; // +i2cccs811=0x5a,25.07,53
            uint8_t humidity = 0;
            double temperature = 0;
            int address = __toint(argv[0]);
            if (address == 0) {
                address = CCS811_ADDRESS;
            }
            if (argc >= 2) {
                temperature = atof(argv[1]);
            }
            if (temperature == 0) {
                temperature = 25;
            }
            if (argc >= 3) {
                humidity = atoi(argv[2]);
            }
            if (humidity == 0) {
                humidity = 55;
            }
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
    else if (constexpr_String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(I2CBME680))) {
        if (argc != 1) {
            at_mode_print_invalid_arguments(serial);
        }
        else {
            Adafruit_BME680 bme680;
            int address = __toint(argv[0]);
            if (address == 0) {
                address = 0x77;
            }
            bme680.begin(address);
            serial.printf_P(PSTR("Reading BME680 at 0x%02x: %.2f °C, %.2f%%, %.2f hPa, gas %u\n"), address, bme680.readTemperature(), bme680.readHumidity(), bme680.readPressure() / 100.0, bme680.readGas());
        }
        return true;
    }
#endif
#ifdef __BME280_H__
    else if (constexpr_String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(I2CBME280))) {
        if (argc != 1) {
            at_mode_print_invalid_arguments(serial);
        }
        else {
            Adafruit_BME280 bme280;
            int address = __toint(argv[0]);
            if (address == 0) {
                address = 0x76;
            }
            bme280.begin(address, &Wire);
            serial.printf_P(PSTR("Reading BME280 at 0x%02x: %.2f °C, %.2f%%, %.2f hPa\n"), address, bme280.readTemperature(), bme280.readHumidity(), bme280.readPressure() / 100.0);
        }
        return true;
    }
#endif
    else if (constexpr_String_equalsIgnoreCase(command, PROGMEM_AT_MODE_HELP_COMMAND(I2CLM75A))) {
        if (argc != 1) {
            at_mode_print_invalid_arguments(serial);
        }
        else {
            int address;
            address = __toint(argv[0]);
            if (address == 0) {
                address = 0x48;
            }
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
