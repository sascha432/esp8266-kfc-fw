/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_BLINDS_CTRL

#pragma once

#include <Arduino_compat.h>
#include <WebUIComponent.h>
#include <MicrosTimer.h>
#include "kfc_fw_config.h"
#include "blinds_ctrl.h"
#include "../mqtt/mqtt_client.h"

PROGMEM_STRING_DECL(blinds_controller_channel1);
PROGMEM_STRING_DECL(blinds_controller_channel2);
PROGMEM_STRING_DECL(blinds_controller_channel1_sensor);
PROGMEM_STRING_DECL(blinds_controller_channel2_sensor);

class BlindsControl : public MQTTComponent, public WebUIInterface {
public:
    typedef enum {
        UNKNOWN = 0,
        OPEN = 1,
        CLOSED = 2,
    } StateEnum_t;

    typedef struct BlindsControllerChannel Channel_t;

public:
    BlindsControl();

    virtual void createAutoDiscovery(MQTTAutoDiscovery::Format_t format, MQTTComponent::MQTTAutoDiscoveryVector &vector) override;
    virtual uint8_t getAutoDiscoveryCount() const override;

    virtual void getValues(JsonArray &array);
    virtual void setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState);

    void setChannel(uint8_t channel, StateEnum_t state);

protected:
    void _loopMethod();

// private:
protected:
    static PGM_P _stateStr(StateEnum_t state);
    const __FlashStringHelper *_getStateStr(uint8_t channel) const;

    void _clearAdc();
    void _updateAdc();

    void _setMotorSpeed(uint8_t channel, uint16_t speed, bool direction);
    void _stop();

    void _publishState();
    void _readConfig();

// private:
protected:
    StateEnum_t _state[2];
    Channel_t _channels[2];
    uint8_t _activeChannel;

    MillisTimer _motorTimeout;

    uint32_t _adcIntegral;
    uint16_t _currentLimitCounter;
    MicrosTimer _currentTimer;
};

#endif
