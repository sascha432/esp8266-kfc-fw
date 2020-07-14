/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_SENSOR_HAVE_HLW8032

#include <LoopFunctions.h>
#include "Sensor_HLW8032.h"
#include "sensor.h"
#include "MicrosTimer.h"

#if DEBUG_IOT_SENSOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#if IOT_SENSOR_HLW8032_PF

static volatile unsigned long energyCounter = 0;

void ICACHE_RAM_ATTR Sensor_HLW8032_callbackPF() {
    energyCounter++;
}

#endif

Sensor_HLW8032::Sensor_HLW8032(const String &name, uint8_t pinRx, uint8_t pinTx, uint8_t pinPF) : Sensor_HLW80xx(name), _pinPF(pinPF), _serial(pinTx, pinRx), _lastData(0)
{
    REGISTER_SENSOR_CLIENT(this);

#if IOT_SENSOR_HLW8032_PF
    pinMode(_pinPF, INPUT);
    attachInterrupt(digitalPinToInterrupt(_pinPF), Sensor_HLW8032_callbackPF, CHANGE);
#else
    _pinPF = pinTx; // use TX pin as identifier
#endif

    _serial.begin(4800);
    _serial.onReceive([this](int available) {
        _onReceive(available);
    });
}

Sensor_HLW8032::~Sensor_HLW8032()
{
#if IOT_SENSOR_HLW8032_PF
    detachInterrupt(digitalPinToInterrupt(_pinPF));
#endif
    UNREGISTER_SENSOR_CLIENT(this);
}


void Sensor_HLW8032::getStatus(PrintHtmlEntitiesString &output) {
    output.printf_P(PSTR("Power Monitor HLW8032" HTML_S(br)));
}

MQTTSensorSensorType Sensor_HLW8032::getType() const {
    return MQTTSensorSensorType::HLW8032;
}

String Sensor_HLW8032::_getId(const __FlashStringHelper *type) {
    PrintString id(F("hlw8032_0x%02x"), _pinPF);
    if (type) {
        id.write('_');
        id.print(FPSTR(type));
    }
    return id;
}

void Sensor_HLW8032::_onReceive(int available) {

    if (_buffer.length()) {
        auto last = get_time_diff(_lastData, millis());
        if (last > IOT_SENSOR_HLW8032_SERIAL_INTERVAL / 2) { // discard old data
            _buffer.clear();
        }
    }
    _lastData = millis();

    while(_serial.available()) {
        _buffer.write(_serial.read());
    }

    if (_buffer.length() >= sizeof(HLW8032SerialData_t)) {
        HLW8032SerialData_t *data = reinterpret_cast<HLW8032SerialData_t *>(_buffer.getChar());

        // TODO verify checksum
        // copy values etc...

#if !IOT_SENSOR_HLW8032_PF
    // copy PF from data
#endif

        _buffer.clear();
    }

#if IOT_SENSOR_HLW8032_PF
    _energyCounter = energyCounter;
#endif
}

#endif
