/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_SENSOR_HAVE_MPU6050

#include <Arduino_compat.h>
#include <math.h>
#include <WebUISocket.h>
#include "Sensor_MPU6050.h"
#include "sensor.h"

#if DEBUG_IOT_SENSOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

namespace {

    constexpr float kDegToRad = 0.017453292519943295f;
    constexpr float kRadToDeg = 57.29577951308232f;
    // minimum length of the gravity vector in g
    constexpr float kMinGravity = 0.1f;
    // no display orientation for this sample (the rotation is kept)
    constexpr uint8_t kKeepRotation = 0xff;
    // the device counts as settled while the gyroscope magnitude is below this value (deg/s).
    // The state (and the callbacks) are only updated while it is settled, otherwise wiggling the
    // device would toggle them all the time
    constexpr float kMovementThreshold = 15.0f;
    // hysteresis of the tilt state in degrees, it has to leave the threshold by this much again
    constexpr float kTiltHysteresis = 5.0f;
    // interval limits of the serial calibration output (+MPU6050=...), not a config value
    constexpr uint16_t kMinDebugInterval = 50;
    constexpr uint16_t kMaxDebugInterval = 10000;
    constexpr uint16_t kDefaultDebugInterval = 1000;

}

Sensor_MPU6050::Sensor_MPU6050(const String &name, uint8_t address, TwoWire &wire) :
    MQTT::Sensor(MQTT::SensorType::MPU6050),
    _name(name),
    _address(address),
    _wire(&wire),
    _detected(false),
    _pendingInit(true),
    _rotation(0),
    _pendingRotation(0),
    _pendingSince(0),
    _tilted(false),
    _pendingTilted(false),
    _tiltPendingSince(0),
    _settled(false),
    _debugInitialized(false),
    _debugNextOutput(0),
    _lastSampleValid(false),
    _debugEnabled(false),
    _debugInterval(kDefaultDebugInterval),
    _temperature(NAN),
    _accelX(NAN),
    _accelY(NAN),
    _accelZ(NAN),
    _gyroX(NAN),
    _gyroY(NAN),
    _gyroZ(NAN),
    _unitX(0),
    _unitY(0),
    _unitZ(0),
    _deviation(0)
{
    REGISTER_SENSOR_CLIENT(this);
}

Sensor_MPU6050::~Sensor_MPU6050()
{
    UNREGISTER_SENSOR_CLIENT(this);
}

void Sensor_MPU6050::setRotationCallback(RotationCallback callback)
{
    _rotationCallback = std::move(callback);
    // synchronize a newly registered callback with the current state
    if (_rotationCallback && _detected) {
        _rotationCallback(_rotation);
    }
}

void Sensor_MPU6050::setTiltCallback(TiltCallback callback)
{
    _tiltCallback = std::move(callback);
    if (_tiltCallback && _detected) {
        _tiltCallback(_tilted);
    }
}

MQTT::AutoDiscovery::EntityPtr Sensor_MPU6050::getAutoDiscovery(FormatType format, uint8_t num)
{
    auto discovery = new MQTT::AutoDiscovery::Entity();
    __DBG_discovery_printf("num=%u/%u d=%p", num, getAutoDiscoveryCount(), discovery);
    auto baseTopic = MQTT::Client::getBaseTopicPrefix();
    switch(num) {
        case 0:
            if (discovery->create(this, _getId(IdType::ROTATION), format)) {
                discovery->addStateTopic(_getTopic(IdType::ROTATION));
                discovery->addValueTemplate(F("rotation"));
                discovery->addUnitOfMeasurement(FSPGM(UTF8_degree));
                discovery->addName(F("Rotation"));
                discovery->addStateClass(F("measurement"));
                discovery->addObjectId(baseTopic + F("mpu6050_rotation"));
                discovery->setEnabledByDefault(false);
                discovery->setVisibleByDefault(false);
            }
            break;
        case 1:
            if (discovery->create(MQTTComponent::ComponentType::BINARY_SENSOR, _getId(IdType::TILT), format)) {
                discovery->addStateTopic(_getTopic(IdType::TILT));
                discovery->addPayloadOnOff();
                discovery->addName(F("Tilt"));
                discovery->addStateClass(F("measurement"));
                discovery->addObjectId(baseTopic + F("mpu6050_tilt"));
                discovery->setEnabledByDefault(false);
                discovery->setVisibleByDefault(false);
            }
            break;
    }
    return discovery;
}

void Sensor_MPU6050::publishState()
{
    if (isConnected()) {
        using namespace MQTT::Json;
        publish(MQTT::Client::formatTopic(_getId(IdType::ROTATION)), true, UnnamedObject(
            NamedUint32(F("rotation"), _rotation)
        ).toString());
        publish(MQTT::Client::formatTopic(_getId(IdType::TILT)), true, MQTT::Client::toBoolOnOff(_tilted));
    }
}

void Sensor_MPU6050::getValues(WebUINS::Events &array, bool timer)
{
    array.append(
        WebUINS::Values(_getId(IdType::ROTATION), WebUINS::TrimmedFloat(_rotation, 0), true),
        WebUINS::Values(_getId(IdType::TILT), _getTiltStateHtml())
    );
}

void Sensor_MPU6050::createWebUI(WebUINS::Root &webUI)
{
    webUI.appendToLastRow(WebUINS::Row(WebUINS::Sensor(_getId(IdType::ROTATION), _name, FSPGM(UTF8_degree)).setConfig(_renderConfig)));
    webUI.appendToLastRow(WebUINS::Row(WebUINS::Sensor(_getId(IdType::TILT), _name + F(" Tilt"), emptyString).setConfig(_renderConfig)));
}

void Sensor_MPU6050::getStatus(Print &output)
{
    if (!_detected) {
        output.printf_P(PSTR("MPU-6050 @ I2C address 0x%02x, not detected (WHO_AM_I mismatch)" HTML_S(br)), _address);
        return;
    }
    output.printf_P(PSTR("MPU-6050 @ I2C address 0x%02x, Rotation %u%s, %s, Temperature %.2f%s" HTML_S(br)),
        _address, _rotation, SPGM(UTF8_degree), _tilted ? PSTR("tilted") : PSTR("level"), _temperature, SPGM(UTF8_degreeC));
}

bool Sensor_MPU6050::getSensorData(String &name, StringVector &values)
{
    name = F("MPU-6050");
    values.emplace_back(PrintString(F("%u%s"), _rotation, SPGM(UTF8_degree)));
    values.emplace_back(_tilted ? F("Tilted") : F("Level"));
    return true;
}

void Sensor_MPU6050::createConfigureForm(AsyncWebServerRequest *request, FormUI::Form::BaseForm &form)
{
    using AccelRangeType = KFCConfigurationClasses::Plugins::SensorConfigNS::MPU6050AccelRange;
    using GyroRangeType = KFCConfigurationClasses::Plugins::SensorConfigNS::MPU6050GyroRange;

    auto &cfg = Plugins::Sensor::getWriteableConfig().mpu6050;
    auto &group = form.addCardGroup(F("mpu6050"), F("MPU-6050"), true);

    form.addObjectGetterSetter(F("mpu_ar"), cfg, cfg.get_int_accel_range, cfg.set_int_accel_range);
    form.addFormUI(F("Accelerometer Range"), FormUI::List(
        AccelRangeType::RANGE_2G, F("2 g"),
        AccelRangeType::RANGE_4G, F("4 g"),
        AccelRangeType::RANGE_8G, F("8 g"),
        AccelRangeType::RANGE_16G, F("16 g")
    ));

    form.addObjectGetterSetter(F("mpu_gr"), cfg, cfg.get_int_gyro_range, cfg.set_int_gyro_range);
    form.addFormUI(F("Gyroscope Range"), FormUI::List(
        GyroRangeType::RANGE_250, F("250 deg/s"),
        GyroRangeType::RANGE_500, F("500 deg/s"),
        GyroRangeType::RANGE_1000, F("1000 deg/s"),
        GyroRangeType::RANGE_2000, F("2000 deg/s")
    ));

    form.addObjectGetterSetter(F("mpu_axo"), FormGetterSetter(cfg, accel_offset_x));
    form.addFormUI(F("Accelerometer Offset X"), FormUI::Suffix(F("g")));
    cfg.addRangeValidatorFor_accel_offset_x(form);

    form.addObjectGetterSetter(F("mpu_ayo"), FormGetterSetter(cfg, accel_offset_y));
    form.addFormUI(F("Accelerometer Offset Y"), FormUI::Suffix(F("g")));
    cfg.addRangeValidatorFor_accel_offset_y(form);

    form.addObjectGetterSetter(F("mpu_azo"), FormGetterSetter(cfg, accel_offset_z));
    form.addFormUI(F("Accelerometer Offset Z"), FormUI::Suffix(F("g")));
    cfg.addRangeValidatorFor_accel_offset_z(form);

    form.addObjectGetterSetter(F("mpu_gxo"), FormGetterSetter(cfg, gyro_offset_x));
    form.addFormUI(F("Gyroscope Offset X"), FormUI::Suffix(F("deg/s")));
    cfg.addRangeValidatorFor_gyro_offset_x(form);

    form.addObjectGetterSetter(F("mpu_gyo"), FormGetterSetter(cfg, gyro_offset_y));
    form.addFormUI(F("Gyroscope Offset Y"), FormUI::Suffix(F("deg/s")));
    cfg.addRangeValidatorFor_gyro_offset_y(form);

    form.addObjectGetterSetter(F("mpu_gzo"), FormGetterSetter(cfg, gyro_offset_z));
    form.addFormUI(F("Gyroscope Offset Z"), FormUI::Suffix(F("deg/s")));
    cfg.addRangeValidatorFor_gyro_offset_z(form);

    form.addObjectGetterSetter(F("mpu_iax"), cfg, cfg.get_bits_invert_accel_x, cfg.set_bits_invert_accel_x);
    form.addFormUI(F("Invert Accelerometer X"), FormUI::BoolItems());

    form.addObjectGetterSetter(F("mpu_iay"), cfg, cfg.get_bits_invert_accel_y, cfg.set_bits_invert_accel_y);
    form.addFormUI(F("Invert Accelerometer Y"), FormUI::BoolItems());

    form.addObjectGetterSetter(F("mpu_iaz"), cfg, cfg.get_bits_invert_accel_z, cfg.set_bits_invert_accel_z);
    form.addFormUI(F("Invert Accelerometer Z"), FormUI::BoolItems());

    form.addObjectGetterSetter(F("mpu_igx"), cfg, cfg.get_bits_invert_gyro_x, cfg.set_bits_invert_gyro_x);
    form.addFormUI(F("Invert Gyroscope X"), FormUI::BoolItems());

    form.addObjectGetterSetter(F("mpu_igy"), cfg, cfg.get_bits_invert_gyro_y, cfg.set_bits_invert_gyro_y);
    form.addFormUI(F("Invert Gyroscope Y"), FormUI::BoolItems());

    form.addObjectGetterSetter(F("mpu_igz"), cfg, cfg.get_bits_invert_gyro_z, cfg.set_bits_invert_gyro_z);
    form.addFormUI(F("Invert Gyroscope Z"), FormUI::BoolItems());

    form.addObjectGetterSetter(F("mpu_ro"), cfg, cfg.get_bits_rotation_offset, cfg.set_bits_rotation_offset);
    form.addFormUI(F("Rotation Offset"), FormUI::List(
        0, F("0"),
        1, F("90"),
        2, F("180"),
        3, F("270")
    ));

    // a value of 0 cannot be told apart from "not set", a zero filled blob (the parameter was
    // created by a form render) falls back to the default
    form.addCallbackGetterSetter<uint8_t>(F("mpu_tt"), [&cfg](uint8_t &value, FormUI::Field::BaseField &field, bool store) {
        if (store) {
            cfg.tilt_threshold = value;
        }
        else {
            value = cfg.tilt_threshold;
            if (value < ConfigType::kMinValueFor_tilt_threshold || value > ConfigType::kMaxValueFor_tilt_threshold) {
                value = ConfigType::kDefaultValueFor_tilt_threshold;
            }
        }
        return true;
    });
    form.addFormUI(F("Tilt Threshold"),
        FormUI::PlaceHolder(ConfigType::kDefaultValueFor_tilt_threshold),
        FormUI::MinMax(ConfigType::kMinValueFor_tilt_threshold, ConfigType::kMaxValueFor_tilt_threshold),
        FormUI::Type::NUMBER_RANGE,
        FormUI::Suffix(FSPGM(UTF8_degree))
    );
    form.addValidator(FormUI::Validator::Range(ConfigType::kMinValueFor_tilt_threshold, ConfigType::kMaxValueFor_tilt_threshold, false));

    form.addCallbackGetterSetter<uint16_t>(F("mpu_si"), [&cfg](uint16_t &value, FormUI::Field::BaseField &field, bool store) {
        if (store) {
            cfg.sample_interval = value;
        }
        else {
            value = cfg.sample_interval;
            if (value < ConfigType::kMinValueFor_sample_interval || value > ConfigType::kMaxValueFor_sample_interval) {
                value = ConfigType::kDefaultValueFor_sample_interval;
            }
        }
        return true;
    });
    form.addFormUI(F("Sample Interval"),
        FormUI::PlaceHolder(ConfigType::kDefaultValueFor_sample_interval),
        FormUI::MinMax(ConfigType::kMinValueFor_sample_interval, ConfigType::kMaxValueFor_sample_interval),
        FormUI::Type::NUMBER_RANGE,
        FormUI::Suffix(F("ms"))
    );
    form.addValidator(FormUI::Validator::Range(ConfigType::kMinValueFor_sample_interval, ConfigType::kMaxValueFor_sample_interval, false));

    form.addCallbackGetterSetter<uint16_t>(F("mpu_dt"), [&cfg](uint16_t &value, FormUI::Field::BaseField &field, bool store) {
        if (store) {
            cfg.debounce_time = value;
        }
        else {
            value = cfg.debounce_time;
            if (value < ConfigType::kMinValueFor_debounce_time || value > ConfigType::kMaxValueFor_debounce_time) {
                value = ConfigType::kDefaultValueFor_debounce_time;
            }
        }
        return true;
    });
    form.addFormUI(F("Debounce Time"),
        FormUI::PlaceHolder(ConfigType::kDefaultValueFor_debounce_time),
        FormUI::MinMax(ConfigType::kMinValueFor_debounce_time, ConfigType::kMaxValueFor_debounce_time),
        FormUI::Type::NUMBER_RANGE,
        FormUI::Suffix(F("ms"))
    );
    form.addValidator(FormUI::Validator::Range(ConfigType::kMinValueFor_debounce_time, ConfigType::kMaxValueFor_debounce_time, false));

    group.end();
}

void Sensor_MPU6050::setup()
{
    _readConfig();
    _init();
    _reset();
}

void Sensor_MPU6050::reconfigure(PGM_P source)
{
    _readConfig();
    // apply the new ranges from the loop task, the sampling timer calls _init()
    _pendingInit = true;
    // print the header again with the new configuration
    _debugInitialized = false;
}

void Sensor_MPU6050::shutdown()
{
    _Timer(_timer).remove();
    _detected = false;
    _rotationCallback = nullptr;
    _tiltCallback = nullptr;
}

void Sensor_MPU6050::_readConfig()
{
    _cfg = Plugins::Sensor::getConfig().mpu6050;
    // a value of 0 cannot be told apart from "not set", a zero filled blob (the parameter was
    // created by a form render) falls back to the default
    if (_cfg.tilt_threshold < ConfigType::kMinValueFor_tilt_threshold || _cfg.tilt_threshold > ConfigType::kMaxValueFor_tilt_threshold) {
        _cfg.tilt_threshold = ConfigType::kDefaultValueFor_tilt_threshold;
    }
    if (_cfg.sample_interval < ConfigType::kMinValueFor_sample_interval || _cfg.sample_interval > ConfigType::kMaxValueFor_sample_interval) {
        _cfg.sample_interval = ConfigType::kDefaultValueFor_sample_interval;
    }
    if (_cfg.debounce_time < ConfigType::kMinValueFor_debounce_time || _cfg.debounce_time > ConfigType::kMaxValueFor_debounce_time) {
        _cfg.debounce_time = ConfigType::kDefaultValueFor_debounce_time;
    }
}

void Sensor_MPU6050::_reset()
{
    _Timer(_timer).remove();
    auto interval = _cfg.sample_interval;
    if (interval < ConfigType::kMinValueFor_sample_interval || interval > ConfigType::kMaxValueFor_sample_interval) {
        interval = ConfigType::kDefaultValueFor_sample_interval;
    }
    __LDBG_printf("sampling interval %ums", interval);
    _Timer(_timer).add(Event::milliseconds(interval), true, [this](Event::CallbackTimerPtr) {
        _update();
    });
}

bool Sensor_MPU6050::_init()
{
    _detected = false;

    uint8_t whoAmI = 0;
    // the WHO_AM_I register returns the I2C address with AD0 low
    if (!_readRegisters(REG_WHO_AM_I, &whoAmI, 1) || whoAmI != kI2CAddress) {
        __LDBG_printf("MPU-6050 @ 0x%02x not detected (WHO_AM_I=0x%02x)", _address, whoAmI);
        return false;
    }

    _writeRegister(REG_PWR_MGMT_1, 0x80);   // device reset
    delay(100);
    _writeRegister(REG_PWR_MGMT_1, 0x01);   // wake up, clock source = PLL with X axis gyroscope
    _writeRegister(REG_SMPLRT_DIV, 0x04);   // 1kHz / (1 + 4) = 200Hz
    _writeRegister(REG_CONFIG, 0x03);       // digital low pass filter ~44Hz
    _writeRegister(REG_ACCEL_CONFIG, static_cast<uint8_t>(_cfg.accel_range) << 3);
    _writeRegister(REG_GYRO_CONFIG, static_cast<uint8_t>(_cfg.gyro_range) << 3);

    _detected = true;
    __LDBG_printf("MPU-6050 @ 0x%02x initialized (WHO_AM_I=0x%02x)", _address, whoAmI);
    return true;
}

void Sensor_MPU6050::_update()
{
    if (_pendingInit) {
        _pendingInit = false;
        _init();
    }
    if (!_detected) {
        if (_debugEnabled && !_debugInitialized) {
            _debugInitialized = true;
            _debugNextOutput = 0;
            _serialDebugHeader();
            Serial.printf_P(PSTR("+MPU6050: NOT detected, no answer at I2C address 0x%02x (WHO_AM_I must be 0x68)\n"), static_cast<unsigned>(_address));
        }
        return;
    }

    RawData raw;
    if (!_readRaw(raw)) {
        return;
    }

    float accelScale = _accelScale();
    float gyroScale = _gyroScale();

    float ax = raw.accelX / accelScale - _cfg.accel_offset_x;
    float ay = raw.accelY / accelScale - _cfg.accel_offset_y;
    float az = raw.accelZ / accelScale - _cfg.accel_offset_z;
    float gx = raw.gyroX / gyroScale - _cfg.gyro_offset_x;
    float gy = raw.gyroY / gyroScale - _cfg.gyro_offset_y;
    float gz = raw.gyroZ / gyroScale - _cfg.gyro_offset_z;

    if (_cfg.invert_accel_x) {
        ax = -ax;
    }
    if (_cfg.invert_accel_y) {
        ay = -ay;
    }
    if (_cfg.invert_accel_z) {
        az = -az;
    }
    if (_cfg.invert_gyro_x) {
        gx = -gx;
    }
    if (_cfg.invert_gyro_y) {
        gy = -gy;
    }
    if (_cfg.invert_gyro_z) {
        gz = -gz;
    }

    _accelX = ax;
    _accelY = ay;
    _accelZ = az;
    _gyroX = gx;
    _gyroY = gy;
    _gyroZ = gz;
    _temperature = raw.temperature / 340.0f + 36.53f;

    bool rotationChanged = false;
    bool tiltChanged = false;

    float length = sqrtf(ax * ax + ay * ay + az * az);
    // the first sample after the device reset reads all zero
    _lastSampleValid = length > kMinGravity;
    if (_lastSampleValid) {
        float ux = ax / length;
        float uy = ay / length;
        float uz = az / length;
        _unitX = ux;
        _unitY = uy;
        _unitZ = uz;

        // the gyroscope tells whether the device is at rest. A new rotation/tilt is only latched
        // while it is settled, a wiggle just cancels the pending state and the timers restart
        // once it comes to rest again
        float gyroLength = sqrtf(gx * gx + gy * gy + gz * gz);
        _settled = gyroLength <= kMovementThreshold;
        if (!_settled) {
            _pendingRotation = _rotation;
            _pendingTilted = _tilted;
            _pendingSince = millis();
            _tiltPendingSince = _pendingSince;
        }
        else {
            float cosEnter = cosf(kDegToRad * static_cast<float>(_cfg.tilt_threshold));
            float cosLeave = cosf(kDegToRad * (static_cast<float>(_cfg.tilt_threshold) - kTiltHysteresis));

            uint8_t candidate = kKeepRotation;
            float axis;
            if (fabsf(uz) >= fmaxf(fabsf(ux), fabsf(uy))) {
                // lying flat, no display orientation -> not resting in a stable orientation
                axis = fmaxf(fabsf(ux), fabsf(uy));
            }
            else if (fabsf(ux) >= fabsf(uy)) {
                candidate = (ux >= 0) ? 0 : 180;
                axis = fabsf(ux);
            }
            else {
                candidate = (uy >= 0) ? 90 : 270;
                axis = fabsf(uy);
            }
            _deviation = kRadToDeg * acosf(fminf(1.0f, axis));

            // hysteresis: enter the tilted state above the threshold, leave it below it again
            bool tilted = _tilted ? (axis <= cosLeave) : (axis < cosEnter);
            if (tilted != _tilted) {
                if (tilted != _pendingTilted) {
                    _pendingTilted = tilted;
                    _tiltPendingSince = millis();
                }
                else if (static_cast<int32_t>(millis() - _tiltPendingSince) >= static_cast<int32_t>(_cfg.debounce_time)) {
                    _tilted = tilted;
                    tiltChanged = true;
                }
            }
            else {
                _pendingTilted = _tilted;
            }

            if (candidate != kKeepRotation) {
                uint8_t target = static_cast<uint8_t>((candidate + _cfg.rotation_offset * 90) % 360);
                if (target != _rotation) {
                    if (target != _pendingRotation) {
                        _pendingRotation = target;
                        _pendingSince = millis();
                    }
                    else if (static_cast<int32_t>(millis() - _pendingSince) >= static_cast<int32_t>(_cfg.debounce_time)) {
                        _rotation = target;
                        rotationChanged = true;
                    }
                }
                else {
                    _pendingRotation = _rotation;
                }
            }
        }
    }

    if (rotationChanged && _rotationCallback) {
        _rotationCallback(_rotation);
    }
    if (tiltChanged && _tiltCallback) {
        _tiltCallback(_tilted);
    }
    if (rotationChanged || tiltChanged) {
        publishState();
        _publishWebUI();
    }

    _serialDebug(rotationChanged, tiltChanged);
}

// Calibration output for the serial console, enabled with the AT command +MPU6050. The output only
// contains values, the column layout and the calibration procedure are documented in
// docs/AtModeHelp.md
void Sensor_MPU6050::_serialDebugHeader()
{
    Serial.print(F("+MPU6050: serial calibration output enabled\n"));
    Serial.printf_P(PSTR("+MPU6050: addr=0x%02x detected=%d sample=%ums debounce=%ums debug_interval=%ums\n"),
        static_cast<unsigned>(_address), _detected ? 1 : 0,
        static_cast<unsigned>(_cfg.sample_interval), static_cast<unsigned>(_cfg.debounce_time), static_cast<unsigned>(_debugInterval));
    Serial.printf_P(PSTR("+MPU6050: accel_range=%.0fg gyro_range=%.0f%s/s rot_offset=%u%s tilt_threshold=%u%s tilt_hysteresis=%.0f%s\n"),
        2.0f * static_cast<float>(1 << static_cast<unsigned>(_cfg.accel_range)),
        250.0f * static_cast<float>(1 << static_cast<unsigned>(_cfg.gyro_range)), SPGM(UTF8_degree),
        static_cast<unsigned>(_cfg.rotation_offset * 90), SPGM(UTF8_degree),
        static_cast<unsigned>(_cfg.tilt_threshold), SPGM(UTF8_degree),
        kTiltHysteresis, SPGM(UTF8_degree));
    Serial.printf_P(PSTR("+MPU6050: invert a=[%u,%u,%u] g=[%u,%u,%u] offset a=[%.3f,%.3f,%.3f]g g=[%.1f,%.1f,%.1f]dps\n"),
        static_cast<unsigned>(_cfg.invert_accel_x), static_cast<unsigned>(_cfg.invert_accel_y), static_cast<unsigned>(_cfg.invert_accel_z),
        static_cast<unsigned>(_cfg.invert_gyro_x), static_cast<unsigned>(_cfg.invert_gyro_y), static_cast<unsigned>(_cfg.invert_gyro_z),
        _cfg.accel_offset_x, _cfg.accel_offset_y, _cfg.accel_offset_z,
        _cfg.gyro_offset_x, _cfg.gyro_offset_y, _cfg.gyro_offset_z);
}

void Sensor_MPU6050::_serialDebug(bool rotationChanged, bool tiltChanged)
{
    if (!_debugEnabled) {
        _debugInitialized = false;
        return;
    }
    if (!_debugInitialized) {
        _debugInitialized = true;
        _debugNextOutput = 0;
        _serialDebugHeader();
    }

    if (rotationChanged) {
        Serial.printf_P(PSTR("+MPU6050: event rotation -> %u%s (display callback %s)\n"),
            static_cast<unsigned>(_rotation), SPGM(UTF8_degree), _rotationCallback ? PSTR("registered") : PSTR("none"));
    }
    if (tiltChanged) {
        Serial.printf_P(PSTR("+MPU6050: event tilt -> %s (display callback %s)\n"),
            _tilted ? PSTR("tilted") : PSTR("level"), _tiltCallback ? PSTR("registered") : PSTR("none"));
    }

    uint32_t now = millis();
    if (_lastSampleValid && static_cast<int32_t>(now - _debugNextOutput) >= 0) {
        _debugNextOutput = now + _debugInterval;
        Serial.printf_P(PSTR("+MPU6050: a[%+.3f,%+.3f,%+.3f]g u[%+.2f,%+.2f,%+.2f] dev=%.1f%s rot=%u%s tilt=%s settled=%d gyro[%+.1f,%+.1f,%+.1f]dps\n"),
            _accelX, _accelY, _accelZ,
            _unitX, _unitY, _unitZ,
            _deviation, SPGM(UTF8_degree),
            static_cast<unsigned>(_rotation), SPGM(UTF8_degree),
            _tilted ? PSTR("tilted") : PSTR("level"),
            _settled ? 1 : 0,
            _gyroX, _gyroY, _gyroZ);
    }
}

bool Sensor_MPU6050::_writeRegister(uint8_t reg, uint8_t value)
{
    _wire->beginTransmission(_address);
    _wire->write(reg);
    _wire->write(value);
    return _wire->endTransmission() == 0;
}

bool Sensor_MPU6050::_readRegisters(uint8_t reg, uint8_t *buffer, uint8_t length)
{
    _wire->beginTransmission(_address);
    _wire->write(reg);
    if (_wire->endTransmission(false) != 0) {
        return false;
    }
    if (_wire->requestFrom(_address, length) != length) {
        return false;
    }
    for(uint8_t i = 0; i < length; i++) {
        buffer[i] = _wire->read();
    }
    return true;
}

bool Sensor_MPU6050::_readRaw(RawData &data)
{
    uint8_t buffer[14];
    if (!_readRegisters(REG_ACCEL_XOUT_H, buffer, sizeof(buffer))) {
        return false;
    }
    data.accelX = static_cast<int16_t>((buffer[0] << 8) | buffer[1]);
    data.accelY = static_cast<int16_t>((buffer[2] << 8) | buffer[3]);
    data.accelZ = static_cast<int16_t>((buffer[4] << 8) | buffer[5]);
    data.temperature = static_cast<int16_t>((buffer[6] << 8) | buffer[7]);
    data.gyroX = static_cast<int16_t>((buffer[8] << 8) | buffer[9]);
    data.gyroY = static_cast<int16_t>((buffer[10] << 8) | buffer[11]);
    data.gyroZ = static_cast<int16_t>((buffer[12] << 8) | buffer[13]);
    return true;
}

float Sensor_MPU6050::_accelScale() const
{
    switch(static_cast<KFCConfigurationClasses::Plugins::SensorConfigNS::MPU6050AccelRange>(_cfg.accel_range)) {
        case KFCConfigurationClasses::Plugins::SensorConfigNS::MPU6050AccelRange::RANGE_4G:
            return 8192.0f;
        case KFCConfigurationClasses::Plugins::SensorConfigNS::MPU6050AccelRange::RANGE_8G:
            return 4096.0f;
        case KFCConfigurationClasses::Plugins::SensorConfigNS::MPU6050AccelRange::RANGE_16G:
            return 2048.0f;
        default:
            return 16384.0f;
    }
}

float Sensor_MPU6050::_gyroScale() const
{
    switch(static_cast<KFCConfigurationClasses::Plugins::SensorConfigNS::MPU6050GyroRange>(_cfg.gyro_range)) {
        case KFCConfigurationClasses::Plugins::SensorConfigNS::MPU6050GyroRange::RANGE_500:
            return 65.5f;
        case KFCConfigurationClasses::Plugins::SensorConfigNS::MPU6050GyroRange::RANGE_1000:
            return 32.8f;
        case KFCConfigurationClasses::Plugins::SensorConfigNS::MPU6050GyroRange::RANGE_2000:
            return 16.4f;
        default:
            return 131.0f;
    }
}

String Sensor_MPU6050::_getId(IdType type) const
{
    PrintString id(F("mpu6050_0x%02x_"), _address);
    id.print(type == IdType::TILT ? F("tilt") : F("rotation"));
    return id;
}

String Sensor_MPU6050::_getTopic(IdType type) const
{
    return MQTT::Client::formatTopic(_getId(type));
}

String Sensor_MPU6050::_getTiltStateHtml() const
{
    String str = F("<div class=\"pt-3\">");
    str += _tilted ? F("Tilted") : F("Level");
    str += F("</div>");
    return str;
}

void Sensor_MPU6050::_publishWebUI()
{
    if (WebUISocket::hasAuthenticatedClients()) {
        WebUISocket::broadcast(WebUISocket::getSender(), WebUINS::UpdateEvents(WebUINS::Events(
            WebUINS::Values(_getId(IdType::ROTATION), WebUINS::TrimmedFloat(_rotation, 0), true),
            WebUINS::Values(_getId(IdType::TILT), _getTiltStateHtml())
        )));
    }
}

#if AT_MODE_SUPPORTED

#include "at_mode.h"

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(MPU6050, "MPU6050", "<0=off/50-10000=interval in ms>", "MPU-6050 serial calibration output");

bool Sensor_MPU6050::atModeHandler(AtModeArgs &args)
{
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(MPU6050))) {
        // no argument (or *, any, all) only reports the current state
        if (!args.isAny(0)) {
            if (args.isFalse(0)) {
                _debugEnabled = false;
            }
            else {
                _debugEnabled = true;
                _debugInterval = static_cast<uint16_t>(args.toMillis(0, kMinDebugInterval, kMaxDebugInterval, kDefaultDebugInterval));
            }
            // print the header again with the new setting
            _debugInitialized = false;
        }
        args.print(PSTR("MPU-6050 serial calibration output %s, interval %ums"), _debugEnabled ? PSTR("on") : PSTR("off"), static_cast<unsigned>(_debugInterval));
        return true;
    }
    return false;
}

#endif

#endif
