/*
 Name:		EnigmaIOT_Blind_Controller.ino
 Created:	5/10/2020 6:13:20 PM
 Author:	gmartin
*/

#include <DebounceEvent.h>

constexpr auto UP_BUTTON = 5;
constexpr auto DOWN_BUTTON = 4;
constexpr auto UP_RELAY = 11;
constexpr auto DOWN_RELAY = 12;

constexpr auto ON_STATE = LOW;
constexpr auto OFF_STATE = !ON_STATE;

typedef enum {
    rollingUp = 1,
    rollingDown = 2,
    stopped = 3,
    error = 0
} blindState_t;

void callbackUpButton (uint8_t pin, uint8_t event, uint8_t count, uint16_t length);
void callbackDownButton (uint8_t pin, uint8_t event, uint8_t count, uint16_t length);

DebounceEvent upButton = DebounceEvent (UP_BUTTON, callbackUpButton, BUTTON_PUSHBUTTON | BUTTON_DEFAULT_HIGH | BUTTON_SET_PULLUP, 50, 500);
DebounceEvent downButton = DebounceEvent (DOWN_BUTTON, callbackDownButton, BUTTON_PUSHBUTTON | BUTTON_DEFAULT_HIGH | BUTTON_SET_PULLUP, 50, 500);

int8_t position = -1;
int8_t positionRequest = -1;
blindState_t blindState = stopped;
//time_t blindStartedMoving = 0;

void callbackUpButton (uint8_t pin, uint8_t event, uint8_t count, uint16_t length) {
    switch (blindState) {
    case rollingDown:
    case stopped:
        if (event == EVENT_PRESSED) {
            if (count <= 2) {
                if (count == 2) {
                    positionRequest = 100;
                } else {
                    positionRequest = -1;
                }
                blindState = rollingUp;
                //blindStartedMoving = millis ();
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

void callbackDownButton (uint8_t pin, uint8_t event, uint8_t count, uint16_t length) {
    switch (blindState) {
    case rollingUp:
    case stopped:
        if (event == EVENT_PRESSED) {
            if (count <= 2) {
                if (count == 2) {
                    positionRequest = 0;
                } else {
                    positionRequest = -1;
                }
                blindState = rollingDown;
                //blindStartedMoving = millis ();
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

int8_t timeToPos (time_t movementTime) {
    return movementTime * 100 / 30000;
}

void rollup () {
    static bool started = false;
    static time_t blindStartedMoving = 0;
    static int8_t originalPosition;
    static int8_t finalPosition;
    time_t movementTime;
    if (!started) {
        started = true;
        blindStartedMoving = millis ();
        originalPosition = position;
        finalPosition = positionRequest;
        digitalWrite (UP_RELAY, ON_STATE);
        digitalWrite (DOWN_RELAY, OFF_STATE);
    }
    movementTime = millis () - blindStartedMoving;
    if (position > 100) {
        position = originalPosition + timeToPos (movementTime);
    }  else {
        position = 100;
        started = false;
        blindState = stopped;
    }
}

void rolldown () {
    static bool started = false;
    static time_t blindStartedMoving = 0;
    static int8_t originalPosition;
    static int8_t finalPosition;
    time_t movementTime;
    if (!started) {
        started = true;
        blindStartedMoving = millis ();
        originalPosition = position;
        finalPosition = positionRequest;
        digitalWrite (UP_RELAY, ON_STATE);
        digitalWrite (DOWN_RELAY, OFF_STATE);
    }
    movementTime = millis () - blindStartedMoving;
    if (position < 0) {
        position = originalPosition - timeToPos (movementTime);
    } else {
        position = 0;
        started = false;
        blindState = stopped;
    }

}

void stop () {
    digitalWrite (UP_RELAY, OFF_STATE);
    digitalWrite (DOWN_RELAY, OFF_STATE);
}



void setup() {
    Serial.begin (115200);
    pinMode (UP_RELAY, OUTPUT);
    pinMode (DOWN_RELAY, OUTPUT);
}


void loop() {
    upButton.loop ();
    downButton.loop ();

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
