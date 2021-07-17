/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_SENSOR_HAVE_CCS811

#include "Sensor_CCS811.h"
#include "Sensor_LM75A.h"
#include "Sensor_BME280.h"
#include "Sensor_BME680.h"
#include "sensor.h"

// #undef DEBUG_IOT_SENSOR
// #define DEBUG_IOT_SENSOR 1

#if DEBUG_IOT_SENSOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

Sensor_CCS811::Sensor_CCS811(const String &name, uint8_t address) :
    MQTT::Sensor(MQTT::SensorType::CCS811),
    _name(name),
    _address(address),
    _state(_readStateFile())
{
    REGISTER_SENSOR_CLIENT(this);
    config.initTwoWire();
    _ccs811.begin(_address);
    if (_state) {
        _ccs811.setBaseline(_state.baseline);
    }
    else {
        // save first state after 5min.
        _state = SensorFileEntry(_address, 0, time(nullptr) - IOT_SENSOR_CCS811_SAVE_STATE_INTERVAL + 60);
    }
    setUpdateRate(1); // faster update rate until valid data is available
}

Sensor_CCS811::~Sensor_CCS811()
{
    UNREGISTER_SENSOR_CLIENT(this);
}

MQTT::AutoDiscovery::EntityPtr Sensor_CCS811::getAutoDiscovery(FormatType format, uint8_t num)
{
    auto discovery = new MQTT::AutoDiscovery::Entity();
    switch(num) {
        case 0:
            if (discovery->create(this, _getId(F("eco2")), format)) {
                discovery->addStateTopic(MQTTClient::formatTopic(_getId()));
                discovery->addUnitOfMeasurement(F("ppm"));
                discovery->addValueTemplate(F("eCO2"));
            }
            break;
        case 1:
            if (discovery->create(this, _getId(F("tvoc")), format)) {
                discovery->addStateTopic(MQTTClient::formatTopic(_getId()));
                discovery->addUnitOfMeasurement(F("ppb"));
                discovery->addValueTemplate(F("TVOC"));
            }
            break;
    }
    return discovery;
}

void Sensor_CCS811::getValues(WebUINS::Events &array, bool timer)
{
    auto &sensor = _readSensor();
    array.append(WebUINS::Values(_getId(F("eco2")), sensor.eCO2, sensor.available == 1));
    array.append(WebUINS::Values(_getId(F("tvoc")), sensor.TVOC, sensor.available == 1));
}

void Sensor_CCS811::createWebUI(WebUINS::Root &webUI)
{
    static constexpr auto renderType = IOT_SENSOR_CCS811_RENDER_TYPE;
    {
        auto sensor = WebUINS::Sensor(_getId(F("eco2")),  _name + F(" CO2"), F("ppm"), renderType);
        #ifdef IOT_SENSOR_CCS811_RENDER_HEIGHT
            sensor.append(WebUINS::NamedString(J(height), IOT_SENSOR_CCS811_RENDER_HEIGHT));
        #endif
        webUI.appendToLastRow(WebUINS::Row(sensor));
    }
    {
        auto sensor = WebUINS::Sensor(_getId(F("tvoc")),  _name + F(" TVOC"), F("ppb"), renderType);
        #ifdef IOT_SENSOR_CCS811_RENDER_HEIGHT
            sensor.append(WebUINS::NamedString(J(height), IOT_SENSOR_CCS811_RENDER_HEIGHT));
        #endif
        webUI.appendToLastRow(WebUINS::Row(sensor));
    }
}

void Sensor_CCS811::getStatus(Print &output)
{
    output.printf_P(PSTR("CCS811 @ I2C address 0x%02x" HTML_S(br)), _address);
}

void Sensor_CCS811::publishState()
{
    if (isConnected()) {
        auto &sensor = _readSensor();
        if (sensor.available == 1) {
            using namespace MQTT::Json;
            publish(_getTopic(), true, UnnamedObject(NamedUnsignedShort(F("eCO2"), sensor.eCO2), NamedUnsignedShort(F("TVOC"), sensor.TVOC)).toString());
        }
    }
}

Sensor_CCS811::SensorData &Sensor_CCS811::_readSensor()
{
// use temperature and humidity for compensation
#if IOT_SENSOR_HAVE_LM75A || IOT_SENSOR_HAVE_BME280 || IOT_SENSOR_HAVE_BME680
    auto &sensors = SensorPlugin::getSensors();
    for(auto sensor: sensors) {
        #if IOT_SENSOR_HAVE_BME280
            if (sensor->getType() == SensorType::BME280) {
                auto data = reinterpret_cast<Sensor_BME280 *>(sensor)->_readSensor();
                __LDBG_printf("CCS811: BME280: humidity %u, temperature %.3f", (uint8_t)data.humidity, data.temperature);
                _ccs811.setEnvironmentalData(data.humidity, data.temperature);
            }
        #elif IOT_SENSOR_HAVE_BME680
            if (sensor->getType() == SensorType::BME680) {
                auto data = reinterpret_cast<Sensor_BME680 *>(sensor)->_readSensor();
                __LDBG_printf("CCS811: BME680: humidity %u, temperature %.3f", (uint8_t)data.humidity, data.temperature);
                _ccs811.setEnvironmentalData(data.humidity, data.temperature);
            }
        #elif IOT_SENSOR_HAVE_LM75A
            if (sensor->getType() == SensorType::LM75A) {
                auto temperature = reinterpret_cast<Sensor_BME680 *>(sensor)->_readSensor();
                __LDBG_printf("CCS811: LM75A: humidity fixed 55, temperature %.3f", temperature);
                _ccs811.setEnvironmentalData(55, temperature);
            }
        #endif
    }
#else
    _ccs811.setEnvironmentalData(55, 25);
#endif

    bool available = _ccs811.available();
    uint8_t error = available ? _ccs811.readData() : 0;
    if (!available || error) {
        if (error) {
            if (_sensor.errors < 0xff) {
                _sensor.errors++;
            }
            if (_sensor.errors > 10) {
                // too many errors, mark as unavailable
                _sensor.available = 0;
            }
        }
        __LDBG_printf("CCS811 0x%02x: available=%u error=%d/%d #%u", _address, available, _ccs811.checkError(), error, _sensor.errors);
        return _sensor;
    }

    _sensor.errors = 0;
    _sensor.eCO2 = _ccs811.geteCO2();
    _sensor.TVOC = _ccs811.getTVOC();
    if (_sensor.eCO2 && _sensor.TVOC && _sensor.available == -1) { // if the first sensor data is available, set to default update rate
        setUpdateRate(DEFAULT_UPDATE_RATE);
        _sensor.available = 1;
    }
    _state.baseline = _ccs811.getBaseline();
    _writeStateFile();

    __LDBG_printf("CCS811 0x%02x: available=%d/%d eCO2 %uppm TVOC %uppb error=%d baseline=%u",
        _address, _sensor.available, available, _sensor.eCO2, _sensor.TVOC, _ccs811.checkError(), _state.baseline
    );

    return _sensor;
}

String Sensor_CCS811::_getId(const __FlashStringHelper *type)
{
    PrintString id(F("ccs811_0x%02x"), _address);
    if (type) {
        id.write('_');
        id.print(type);
    }
    return id;
}

Sensor_CCS811::SensorFileEntry Sensor_CCS811::_readStateFile() const
{
    auto file = KFCFS.open(FSPGM(iot_sensor_ccs811_state_file), fs::FileOpenMode::read);
    if (file) {
        SensorFileEntry entry;
        while (file.read(entry, sizeof(entry)) == sizeof(entry)) {
            __LDBG_printf("entry=%u address=0x%02x baseline=%u", (bool)entry, entry.address, entry.baseline);
            if (entry && entry.address == _address) {
                return entry;
            }
        }
    }
    return SensorFileEntry();
}

File Sensor_CCS811::_createStateFile() const
{
    auto file = KFCFS.open(FSPGM(iot_sensor_ccs811_state_file), fs::FileOpenMode::write);
    if (!file) {
        Logger_error(F("Cannot create CCS811 state file %s"), SPGM(iot_sensor_ccs811_state_file));
    }
    return file;
}

void Sensor_CCS811::_writeStateFile()
{
    uint32_t timestamp = time(nullptr);
    if (!isTimeValid(timestamp)) {
        __LDBG_printf("invalid timestamp=%u", timestamp);
        return;
    }
    if (_state.baseline == 0) {
        __LDBG_printf("invalid baseline=0");
        return;
    }
    __LDBG_printf("timestamp=%u _state.timestamp=%u store=%u", timestamp, _state.timestamp, (timestamp > _state.timestamp + IOT_SENSOR_CCS811_SAVE_STATE_INTERVAL));
    if (timestamp > _state.timestamp + IOT_SENSOR_CCS811_SAVE_STATE_INTERVAL) {
        // update state timestamp
        _state.address = _address;
        _state.timestamp = timestamp;
        _state.crc = _state.calcCrc();

        auto file = KFCFS.open(FSPGM(iot_sensor_ccs811_state_file), fs::FileOpenMode::read);
        if (!file) {
            // create new file and store state
            if (!(file = _createStateFile())) {
                return;
            }
            file.write(_state, sizeof(_state));
            __DBG_printf("CCS811 0x%02x: created new state file baseline=%u state=%u", _address, _state.baseline, (bool)_state);
            return;
        }

        // read state file
        auto size = file.size();
        auto uniquePtr = std::unique_ptr<uint8_t[]>(new uint8_t[size]());
        auto buf = uniquePtr.get();
        if (!buf) {
            __LDBG_printf("failed to allocate memory=%u", size);
            return;
        }
        size = file.read(buf, size);
        __LDBG_printf("read size=%u file_size=%u", size, file.size());
        file.close();

        // check if state exists and has changed
        SensorFileEntry entry;
        auto begin = buf;
        auto end = buf + size;
        while(begin + sizeof(entry) <= end) {
            memcpy(&entry, begin, sizeof(entry)); // use memcpy for packed struct
            begin += sizeof(entry);
            if (entry && entry.address == _address) {
                if (entry.baseline == _state.baseline) {
                    __LDBG_printf("baseline=%u did not change", _state.baseline);
                    return;
                }
            }
        }

        // create new state file and copy record
        if (!(file = _createStateFile())) {
            return;
        }

        // rewrite file
        begin = buf;
        while(begin + sizeof(entry) <= end) {
            memcpy(&entry, begin, sizeof(entry));
            begin += sizeof(entry);
            if (entry) {
                if (entry.address != _address) {
                    file.write(entry, sizeof(entry));
                }
                else {
                    // skip
                    __LDBG_printf("old baseline=%u", entry.baseline);
                }
            }
            else {
                __LDBG_printf("invalid entry address=0x%02x baseline=%u", entry.address, entry.baseline);
            }
        }

        // write new state
        file.write(_state, sizeof(_state));
        __DBG_printf("CCS811 0x%02x: state file baseline=%u state=%u updated", _address, _state.baseline, (bool)_state);

    }
}


#endif
