/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_BLINDS_CTRL

#pragma once

#include <Arduino_compat.h>
#include <WebUIComponent.h>
#include <MicrosTimer.h>
#include <FunctionalInterrupt.h>
#include "blinds_ctrl.h"
#include "BlindsChannel.h"

PROGMEM_STRING_DECL(blinds_controller_channel1);
PROGMEM_STRING_DECL(blinds_controller_channel2);
PROGMEM_STRING_DECL(blinds_controller_channel1_sensor);
PROGMEM_STRING_DECL(blinds_controller_channel2_sensor);

class BlindsControl : public MQTTComponent, public WebUIInterface {
public:
    typedef enum {
        NONE =          0xff,
        CHANNEL1 =      0,
        CHANNEL2,
        CHANNEL_SIZE,
    } ChannelEnum_t;

    class ChannelAction {
    public:
        ChannelAction() : _state(BlindsChannel::UNKNOWN), _channel(NONE) {
        }

        void set(BlindsChannel::StateEnum_t state, uint8_t channel) {
            _state = state;
            _channel = (ChannelEnum_t)channel;
        }

        bool isSet() const {
            return _channel != NONE;
        }

        uint8_t getChannel() const {
            return (uint8_t)_channel;
        }

        BlindsChannel::StateEnum_t getState() const {
            return _state;
        }

        void clear() {
            _channel = NONE;
        }

    private:
        BlindsChannel::StateEnum_t _state;
        ChannelEnum_t _channel;
    };

public:
    BlindsControl();

    virtual void createAutoDiscovery(MQTTAutoDiscovery::Format_t format, MQTTComponent::MQTTAutoDiscoveryVector &vector) override;
    virtual uint8_t getAutoDiscoveryCount() const override {
        return 2;
    }
    virtual void onConnect(MQTTClient *client) override;

    virtual void getValues(JsonArray &array);
    virtual void setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState);

    void setChannel(uint8_t channel, BlindsChannel::StateEnum_t state);

protected:
    void _loopMethod();

protected:
    const __FlashStringHelper *_getStateStr(uint8_t channel) const;

    void _setup();

    void _clearAdc();
    void _updateAdc();

    void _setMotorSpeed(uint8_t channel, uint16_t speed, bool direction);
    void _stop();

    void _publishState(MQTTClient *client = nullptr);
    void _loadState();
    void _saveState();
    void _readConfig();

protected:
    String _getTopic() const;

    std::array<BlindsChannel, ChannelEnum_t::CHANNEL_SIZE> _channels;
    uint8_t _swapChannels: 1;
    uint8_t _channel0Dir: 1;
    uint8_t _channel1Dir: 1;
    uint8_t _activeChannel;
    ChannelAction _action;

    MillisTimer _motorTimeout;

    uint32_t _adcIntegral;
    uint16_t _currentLimitCounter;
    MicrosTimer _currentTimer;

#if IOT_BLINDS_CTRL_RPM_PIN
protected:
    void _rpmReset();
    void _rpmIntCallback(const InterruptInfo &info);
    uint16_t _getRpm();
    bool _hasStalled() const;

protected:
    MicrosTimer _rpmTimer;
    MicrosTimer _rpmLastInterrupt;
    uint32_t _rpmTimeIntegral;
    uint32_t _rpmCounter;
#endif
};

#endif
