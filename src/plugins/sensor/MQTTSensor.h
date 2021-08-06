/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#ifndef DEBUG_IOT_SENSOR
#define DEBUG_IOT_SENSOR 0
#endif

#include <Arduino_compat.h>
#include <PrintHtmlEntitiesString.h>
#include <vector>
#include <KFCJson.h>
#include <KFCForms.h>
#include "../mqtt/mqtt_client.h"
#include "../mqtt/mqtt_json.h"
#include "EnumBitset.h"

#if DEBUG_IOT_SENSOR
#    define REGISTER_SENSOR_CLIENT(sensor)                                   \
        {                                                                    \
            ::printf("registerClient %s:%u\n", __BASENAME_FILE__, __LINE__); \
            registerClient(sensor);                                          \
        }
#else
#    define REGISTER_SENSOR_CLIENT(sensor) registerClient(sensor);
#endif
#define UNREGISTER_SENSOR_CLIENT(sensor) unregisterClient(sensor);

namespace MQTT {

    enum class SensorType : uint8_t {
        MIN = 0,
        UNKNOWN = 0,
        LM75A,
        BME280,
        BME680,
        CCS811,
        HLW8012,
        HLW8032,
        BATTERY,
        DS3231,
        INA219,
        DHTxx,
        DIMMER_METRICS,
        MOTION,
        AMBIENT_LIGHT,
        SYSTEM_METRICS,
        MAX
    };

    class Sensor : public MQTTComponent {
    public:
        static constexpr uint16_t DEFAULT_UPDATE_RATE = 5;
        static constexpr uint16_t DEFAULT_MQTT_UPDATE_RATE = 30;

        using SensorType = MQTT::SensorType;
        using SensorPtr = MQTT::SensorPtr;
        using NamedArray = MQTT::Json::NamedArray;
        using SensorConfig = WebUINS::SensorConfig;

        Sensor(SensorType type);
        virtual ~Sensor();

        // REGISTER_SENSOR_CLIENT(this) must be called in the constructor of the sensor
        template <class T>
        void registerClient(T sensor) {
            Client::registerComponent(sensor);
        }

        // UNREGISTER_SENSOR_CLIENT(this) must be called in the destructor of the sensor
        template <class T>
        void unregisterClient(T sensor) {
            MQTT::Client::unregisterComponent(sensor);
        }

        virtual void onConnect() override;

        // using MQTT update rate
        // client is valid and connected
        virtual void publishState() = 0;
        // using update rate
        virtual void getValues(WebUINS::Events &array, bool timer) = 0;
        // create webUI for sensor
        virtual void createWebUI(WebUINS::Root &webUI) = 0;
        // return status of the sensor for status.html
        virtual void getStatus(Print &output) = 0;

        virtual bool getSensorData(String &name, StringVector &values) {
            return false;
        }

        virtual bool hasForm() const {
            return false;
        }

        virtual void createConfigureForm(AsyncWebServerRequest *request, FormUI::Form::BaseForm &form) {}
        virtual void configurationSaved(FormUI::Form::BaseForm *form) {}
        virtual void setup() {}
        virtual void reconfigure(PGM_P source) {}
        virtual void shutdown() {}

    #if AT_MODE_SUPPORTED
        virtual void atModeHelpGenerator() {
        }
        virtual bool atModeHandler(AtModeArgs &args) {
            return false;
        }
    #endif

        void timerEvent(WebUINS::Events *array, bool mqttIsConnected);

        void setUpdateRate(uint16_t updateRate);
        void setMqttUpdateRate(uint16_t updateRate);
        void setNextMqttUpdate(uint16_t delay);
        void forceUpdate();
        void forceMqttUpdate();

        SensorType getType() const;
        int16_t getOrderId() const;
        void invertOrderId();
        void setConfig(const SensorConfig &config);

    protected:
        uint16_t _updateRate;
        uint16_t _mqttUpdateRate;

        int16_t _getNextOrderId() const;

        static int16_t _orderIdCounter;

    private:
        uint32_t _nextUpdate;
        uint32_t _nextMqttUpdate;
        int16_t _orderId;

    protected:
        SensorType _type;
        SensorConfig _renderConfig;
    };

    inline void Sensor::setUpdateRate(uint16_t updateRate)
    {
        _updateRate = updateRate;
        forceUpdate();
    }

    inline void Sensor::setMqttUpdateRate(uint16_t updateRate)
    {
        _mqttUpdateRate = updateRate;
        forceMqttUpdate();
    }

    inline void Sensor::setNextMqttUpdate(uint16_t delay)
    {
        _nextMqttUpdate = time(nullptr) + delay;
    }

    inline void Sensor::forceUpdate()
    {
        _nextUpdate = 0;
    }

    inline void Sensor::forceMqttUpdate()
    {
        _nextMqttUpdate = 0;
    }

    inline Sensor::SensorType Sensor::getType() const
    {
        return _type;
    }

    inline int16_t Sensor::getOrderId() const
    {
        return _orderId;
    }

    inline void Sensor::invertOrderId()
    {
        _orderId = -_orderId;
    }

    inline int16_t Sensor::_getNextOrderId() const
    {
        _orderIdCounter += 100;
        return _orderIdCounter;
    }

    inline void Sensor::setConfig(const SensorConfig &config)
    {
        _renderConfig = config;
    }

}

