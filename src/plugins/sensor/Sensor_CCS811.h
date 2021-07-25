/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if IOT_SENSOR_HAVE_CCS811

#include <Arduino_compat.h>
#include <Wire.h>
#include <vector>
#include "WebUIComponent.h"
#include "plugins.h"
#include "MQTTSensor.h"
#include <Adafruit_CCS811.h>

#ifndef IOT_SENSOR_CCS811_RENDER_TYPE
#define IOT_SENSOR_CCS811_RENDER_TYPE WebUINS::SensorRenderType::COLUMN
#endif

#ifndef IOT_SENSOR_CCS811_RENDER_HEIGHT
// #define IOT_SENSOR_CCS811_RENDER_HEIGHT F("15rem")
#endif

// save state interval in seconds
#ifndef IOT_SENSOR_CCS811_SAVE_STATE_INTERVAL
#define IOT_SENSOR_CCS811_SAVE_STATE_INTERVAL 900
#endif

class Sensor_CCS811 : public MQTT::Sensor {
public:
    struct SensorData {
        int8_t available;   // -1 init, 0 N/A, 1 available
        uint8_t errors;
        uint16_t eCO2;      // ppm
        uint16_t TVOC;      // ppb
        SensorData() : available(-1), errors(0), eCO2(0), TVOC(0) {}
    };

    struct __attribute_packed__ SensorFileEntry {
        uint32_t crc;
        uint8_t address;
        uint16_t baseline;
        uint32_t timestamp;
        SensorFileEntry() : crc(~0U), address(0), baseline(0), timestamp(0) {}
        SensorFileEntry(uint8_t _address, uint16_t _baseline, uint32_t _timestamp) : crc(~0U), address(_address), baseline(_baseline), timestamp(_timestamp) {}
        operator bool() const {
            return address != 0 && crc == calcCrc();
        }
        operator uint8_t *() {
            return reinterpret_cast<uint8_t *>(this);
        }
        operator const uint8_t *() const {
            return reinterpret_cast<const uint8_t *>(this);
        }
        uint32_t calcCrc() const {
            return crc32(&address, sizeof(*this) - offsetof(SensorFileEntry, address));
        }
    };

    const uint8_t DEFAULT_UPDATE_RATE = MQTT::Sensor::DEFAULT_UPDATE_RATE;

public:
    Sensor_CCS811(const String &name, uint8_t address = CCS811_ADDRESS);
    virtual ~Sensor_CCS811();

    virtual AutoDiscovery::EntityPtr getAutoDiscovery(FormatType format, uint8_t num) override;
    virtual uint8_t getAutoDiscoveryCount() const override;

    virtual void publishState() override;
    virtual void getValues(WebUINS::Events &array, bool timer) override;
    virtual void createWebUI(WebUINS::Root &webUI) override;
    virtual void getStatus(Print &output) override;

private:
    String _getId(const __FlashStringHelper *type = nullptr);
    String _getTopic(const __FlashStringHelper *type = nullptr);
    SensorData &_readSensor();

    SensorFileEntry _readStateFile() const;
    void _writeStateFile();
    File _createStateFile() const;

private:
    String _name;
    SensorData _sensor;
    uint8_t _address;
    SensorFileEntry _state;
    Adafruit_CCS811 _ccs811;
};

inline uint8_t Sensor_CCS811::getAutoDiscoveryCount() const
{
    return 2;
}

inline String Sensor_CCS811::_getTopic(const __FlashStringHelper *type)
{
    return MQTT::Client::formatTopic(_getId(type));
}

#endif
