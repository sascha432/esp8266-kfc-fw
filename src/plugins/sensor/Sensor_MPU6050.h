/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if IOT_SENSOR_HAVE_MPU6050

#include <Arduino_compat.h>
#include <Wire.h>
#include <EventScheduler.h>
#include <functional>
#include "WebUIComponent.h"
#include "plugins.h"
#include "MQTTSensor.h"

#ifndef IOT_SENSOR_MPU6050_RENDER_TYPE
#    define IOT_SENSOR_MPU6050_RENDER_TYPE WebUINS::SensorRenderType::ROW
#endif

// InvenSense MPU-6050 (3 axis accelerometer, 3 axis gyroscope and an on-chip temperature sensor).
// The accelerometer is used as a display rotation sensor (0/90/180/270 degrees) and as a binary
// tilt sensor ("not resting in a stable orientation"). The registers are accessed directly over
// I2C, no external library is required. A serial calibration output is available through the AT
// command +MPU6050, see atModeHandler().
class Sensor_MPU6050 : public MQTT::Sensor {
public:
    using Plugins = KFCConfigurationClasses::PluginsType;
    using ConfigType = KFCConfigurationClasses::Plugins::SensorConfigNS::MPU6050ConfigType;

    // I2C address with AD0 low. The MPU-6050 answers with the same value in the WHO_AM_I register
    // (0x75), which is how the chip is detected
    static constexpr uint8_t kI2CAddress = 0x68;

    static constexpr uint8_t REG_SMPLRT_DIV = 0x19;
    static constexpr uint8_t REG_CONFIG = 0x1a;
    static constexpr uint8_t REG_GYRO_CONFIG = 0x1b;
    static constexpr uint8_t REG_ACCEL_CONFIG = 0x1c;
    static constexpr uint8_t REG_ACCEL_XOUT_H = 0x3b;
    static constexpr uint8_t REG_PWR_MGMT_1 = 0x6b;
    static constexpr uint8_t REG_WHO_AM_I = 0x75;

    enum class IdType : uint8_t {
        ROTATION,
        TILT,
    };

    using RotationCallback = std::function<void(uint8_t rotation)>;
    using TiltCallback = std::function<void(bool tilted)>;

public:
    Sensor_MPU6050(const String &name, uint8_t address = IOT_SENSOR_HAVE_MPU6050, TwoWire &wire = Wire);
    virtual ~Sensor_MPU6050();

    virtual AutoDiscovery::EntityPtr getAutoDiscovery(FormatType format, uint8_t num) override;
    virtual uint8_t getAutoDiscoveryCount() const override;

    virtual void publishState() override;
    virtual void getValues(WebUINS::Events &array, bool timer) override;
    virtual void createWebUI(WebUINS::Root &webUI) override;
    virtual void getStatus(Print &output) override;
    virtual bool getSensorData(String &name, StringVector &values) override;

    virtual bool hasForm() const override;
    virtual void createConfigureForm(AsyncWebServerRequest *request, FormUI::Form::BaseForm &form) override;
    virtual void setup() override;
    virtual void reconfigure(PGM_P source) override;
    virtual void shutdown() override;

    #if AT_MODE_SUPPORTED
        virtual bool atModeHandler(AtModeArgs &args) override;
    #endif

    uint8_t getAddress() const {
        return _address;
    }

    // display rotation in degrees, one of 0/90/180/270
    uint8_t getRotation() const {
        return _rotation;
    }

    // true while the device is not resting in a stable orientation
    bool isTilted() const {
        return _tilted;
    }

    // false if the MPU-6050 did not answer (WHO_AM_I mismatch)
    bool isDetected() const {
        return _detected;
    }

    // Register a callback that is invoked when the display rotation changes (0/90/180/270 degrees).
    // An empty std::function unregisters it. The sensor invokes it from the main loop task and calls
    // a newly registered callback immediately with the current state so a display can synchronize.
    void setRotationCallback(RotationCallback callback);

    // Register a callback that is invoked when the binary tilt state changes.
    void setTiltCallback(TiltCallback callback);

    bool hasCallbacks() const {
        return _rotationCallback || _tiltCallback;
    }

private:
    struct RawData {
        int16_t accelX;
        int16_t accelY;
        int16_t accelZ;
        int16_t temperature;
        int16_t gyroX;
        int16_t gyroY;
        int16_t gyroZ;
    };

    void _readConfig();
    bool _init();
    void _reset();
    void _update();
    bool _writeRegister(uint8_t reg, uint8_t value);
    bool _readRegisters(uint8_t reg, uint8_t *buffer, uint8_t length);
    bool _readRaw(RawData &data);
    float _accelScale() const;
    float _gyroScale() const;
    String _getId(IdType type) const;
    String _getTopic(IdType type) const;
    String _getTiltStateHtml() const;
    void _publishWebUI();
    void _serialDebugHeader();
    void _serialDebug(bool rotationChanged, bool tiltChanged);

    String _name;
    uint8_t _address;
    TwoWire *_wire;
    ConfigType _cfg;

    Event::Timer _timer;
    bool _detected;
    bool _pendingInit;

    uint8_t _rotation;
    uint8_t _pendingRotation;
    uint32_t _pendingSince;
    bool _tilted;
    bool _pendingTilted;
    uint32_t _tiltPendingSince;
    bool _settled;

    RotationCallback _rotationCallback;
    TiltCallback _tiltCallback;

    // serial calibration output
    bool _debugInitialized;
    uint32_t _debugNextOutput;
    bool _lastSampleValid;

    // serial calibration output, controlled by the AT command +MPU6050 (not a config value),
    // always available regardless of the DEBUG build flag
    bool _debugEnabled;
    uint16_t _debugInterval;

    // last converted values, for getStatus() and the serial debug output
    float _temperature;
    float _accelX;
    float _accelY;
    float _accelZ;
    float _gyroX;
    float _gyroY;
    float _gyroZ;
    float _unitX;
    float _unitY;
    float _unitZ;
    float _deviation;
};

inline uint8_t Sensor_MPU6050::getAutoDiscoveryCount() const
{
    return 2;
}

inline bool Sensor_MPU6050::hasForm() const
{
    return true;
}

#endif
