 /**
  Author: sascha_lammers@gmx.de
*/

void check_if_exist_I2C(TwoWire &wire, Print &output, uint8_t startAddress, uint8_t endAddress, uint32_t delayMillis = 10)
{
    wire.setClock(100000);
    int nDevices = 0;
    for (auto address = startAddress; address < endAddress; address++ ) {
        // The i2c_scanner uses the return value of
        // the Write.endTransmission to see if
        // a device did acknowledge to the address.
        wire.beginTransmission(address);
        uint8_t error = wire.endTransmission(true);
        if (error == 0 || error == 3) { // 0=ACK, 3=NACK for data
            output.printf_P(PSTR("I2C device found at address 0x%02x (%u)\n"), address, error);
            nDevices++;
        }
        else if (error == 4) {
            output.printf_P(PSTR("Unknown error at address 0x%02x\n"), address);
        }
        if (delayMillis) {
            delay(delayMillis);
        }
    }
    if (nDevices == 0) {
        output.println(F("No I2C devices found"));
    }
}

void scanI2C(Print &output, int8_t sda, int8_t scl, uint8_t startAddress, uint8_t endAddress)
{
    TwoWire *wire;
    if (sda == 0xff || scl == 0xff || sda == scl || isFlashInterfacePin(sda) || isFlashInterfacePin(scl)) {
        output.printf_P(PSTR("Scanning (SDA : SCL) - "));
        wire = &config.initTwoWire(false, &output);
    }
    else {
        output.printf_P(PSTR("Scanning (SDA : SCL) - GPIO%u : GPIO%u - "), sda, scl);
        Wire.begin(sda, scl);
        wire = &Wire;
    }
    check_if_exist_I2C(*wire, output, startAddress, endAddress);
    config.initTwoWire(true);
}

#define I2C_SCANNER_SKIP_PORT(i) (i == 1 || i == 3 || isFlashInterfacePin(i))

void scanPorts(Print &output, uint8_t startAddress, uint8_t endAddress)
{
    output.println(F("\nScanning all PIN pairs..."));
    for (uint8_t i = 0; i <= 16; i++) {
        if (I2C_SCANNER_SKIP_PORT(i)) {
            continue;
        }
        for (uint8_t j = 0; j <= 16; j++) {
            if (i == j || I2C_SCANNER_SKIP_PORT(j)) {
                continue;
            }
            scanI2C(output, i, j, startAddress, endAddress);
        }
    }
}
