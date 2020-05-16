// 
// 
// 

#include "BlindController.h"
#include <functional>

using namespace std;
using namespace placeholders;

// Default values
constexpr auto UP_BUTTON = 5;
constexpr auto DOWN_BUTTON = 4;
constexpr auto UP_RELAY = 11;
constexpr auto DOWN_RELAY = 12;
constexpr auto ROLLING_TIME = 30000;

void BlindController::callbackUpButton (uint8_t pin, uint8_t event, uint8_t count, uint16_t length) {
    switch (blindState) {
    case rollingDown:
    case stopped:
        if (event == EVENT_PRESSED) {
            if (count <= 2) {
                if (count == 2) {
                    fullRollup ();
                } else {
                    positionRequest = -1;
                    blindState = rollingUp;
                }
            }
        }
        break;
    case rollingUp:
        if (event == EVENT_RELEASED && positionRequest == -1) {
            blindState = stopped;
        }
        break;
    }

}

void BlindController::callbackDownButton (uint8_t pin, uint8_t event, uint8_t count, uint16_t length) {
    switch (blindState) {
    case rollingUp:
    case stopped:
        if (event == EVENT_PRESSED) {
            if (count <= 2) {
                if (count == 2) {
                    fullRolldown ();
                } else {
                    positionRequest = -1;
                    blindState = rollingDown;
                }
            }
        }
        break;
    case rollingDown:
        if (event == EVENT_RELEASED && positionRequest == -1) {
            blindState = stopped;
        }
        break;
    }

}

void BlindController::begin (blindControlerHw_t *data) {
    config.downButton = data->downButton;
    config.downRelayPin = data->downRelayPin;
    config.fullDownTime = data->fullDownTime;
    config.fullUpTime = data->fullUpTime;
    config.ON_STATE = data->ON_STATE;
    config.OFF_STATE = !config.ON_STATE;
    config.upButton = data->upButton;
    config.upRelayPin = data->upRelayPin;

    upButton = new DebounceEvent (config.upButton, std::bind (&BlindController::callbackUpButton, this, _1, _2, _3, _4), BUTTON_PUSHBUTTON | BUTTON_DEFAULT_HIGH | BUTTON_SET_PULLUP, 50, 500);
    downButton = new DebounceEvent (config.downButton, std::bind (&BlindController::callbackDownButton, this, _1, _2, _3, _4), BUTTON_PUSHBUTTON | BUTTON_DEFAULT_HIGH | BUTTON_SET_PULLUP, 50, 500);
}

void BlindController::begin () {
    blindControlerHw_t defaultConfig;

    defaultConfig.downButton = DOWN_BUTTON;
    defaultConfig.downRelayPin = DOWN_RELAY;
    defaultConfig.fullDownTime = ROLLING_TIME;
    defaultConfig.fullUpTime = ROLLING_TIME;
    defaultConfig.upButton = UP_BUTTON;
    defaultConfig.upRelayPin = UP_RELAY;

    begin (&defaultConfig);
}

void BlindController::fullRollup () {
    positionRequest = 100;
    blindState = rollingUp;
}

void BlindController::fullRolldown () {
    positionRequest = 0;
    blindState = rollingDown;
}

void BlindController::gotoPosition (int8_t pos) {
    positionRequest = pos;
    if (pos > position) {
        blindState = rollingUp;
    } else if (pos < position) {
        blindState = rollingDown;
    }
}

void  BlindController::rollup () {
    if (!movingUp) {
        movingUp = true;
        blindStartedMoving = millis ();
        originalPosition = position;
        finalPosition = positionRequest;
        digitalWrite (config.downRelayPin, config.OFF_STATE);
        digitalWrite (config.upRelayPin, config.ON_STATE);
    }
    travellingTime = millis () - blindStartedMoving;
    if (position > 100) {
        position = originalPosition + timeToPos (travellingTime);
    } else {
        position = 100;
        movingUp = false;
        blindState = stopped;
    }
}

void BlindController::rolldown () {
    if (!movingDown) {
        movingDown = true;
        blindStartedMoving = millis ();
        originalPosition = position;
        finalPosition = positionRequest;
        digitalWrite (config.upRelayPin, config.OFF_STATE);
        digitalWrite (config.downRelayPin, config.ON_STATE);
    }
    travellingTime = millis () - blindStartedMoving;
    if (position < 0) {
        position = originalPosition - timeToPos (travellingTime);
    } else {
        position = 0;
        movingDown = false;
        blindState = stopped;
    }

}

void BlindController::stop () {
    digitalWrite (config.upRelayPin, config.OFF_STATE);
    digitalWrite (config.downRelayPin, config.OFF_STATE);
}


void BlindController::loop () {
    if (upButton)
        upButton->loop ();
    if (downButton)
        downButton->loop ();

    switch (blindState) {
    case stopped:
        stop ();
        break;
    case rollingUp:
        rollup ();
        break;
    case rollingDown:
        rolldown ();
        break;
    default:
        break;
    }
}

