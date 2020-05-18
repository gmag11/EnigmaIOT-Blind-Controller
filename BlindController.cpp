// 
// 
// 

#include "BlindController.h"
#include <functional>

using namespace std;
using namespace placeholders;

// Default values
constexpr auto UP_BUTTON = 0;
constexpr auto DOWN_BUTTON = 2;
constexpr auto UP_RELAY = 5;
constexpr auto DOWN_RELAY = 4;
constexpr auto ROLLING_TIME = 5000;

void BlindController::callbackUpButton (uint8_t pin, uint8_t event, uint8_t count, uint16_t length) {
	DEBUG_VERBOSE ("Up button. Event %d Count %d", event, count);
	//switch (blindState) {
	//case rollingDown:
	//case stopped:
	if (event == EVENT_PRESSED) {
		DEBUG_DBG ("Up button pressed. Count %d", count);
		if (count == 1) {
			DEBUG_INFO ("Call simple roll up");
			positionRequest = -1;
			travellingTime = -1;
			blindState = rollingUp;
			DEBUG_DBG ("--- STATE: Rolling up");
		}
	}
//    break;
//case rollingUp:
	if (event == EVENT_RELEASED && positionRequest == -1) {
		DEBUG_INFO ("Stop rolling up");
		blindState = stopped;
		movingUp = false;
		DEBUG_DBG ("--- STATE: Stopped");
	} else if (event == EVENT_PRESSED) {
		if (count == 2) {
			DEBUG_INFO ("Call full roll up");
			fullRollup ();
		}
	}
//    break;
//}

}

void BlindController::callbackDownButton (uint8_t pin, uint8_t event, uint8_t count, uint16_t length) {
	DEBUG_VERBOSE ("Down button. Event %d Count %d", event, count);
	//switch (blindState) {
	//case rollingUp:
	//case stopped:
	if (event == EVENT_PRESSED) {
		DEBUG_DBG ("Down button pressed. Count %d", count);
		if (count == 1) {
			DEBUG_INFO ("Call simple roll down");
			positionRequest = -1;
			travellingTime = -1;
			blindState = rollingDown;
			movingDown = false;
			DEBUG_DBG ("--- STATE: Rolling down");
		}
	}
//    break;
//case rollingDown:
	if (event == EVENT_RELEASED && positionRequest == -1) {
		DEBUG_INFO ("Stop rollinging down");
		blindState = stopped;
		movingDown = false;
		DEBUG_DBG ("--- STATE: Stopped");
	} else if (event == EVENT_PRESSED) {
		if (count == 2) {
			DEBUG_INFO ("Call full roll down");
			fullRolldown ();
		}
	}
//    break;
//}

}

void BlindController::begin (blindControlerHw_t* data) {
	config.downButton = data->downButton;
	config.downRelayPin = data->downRelayPin;
	config.fullTravellingTime = data->fullTravellingTime;
	config.ON_STATE = data->ON_STATE;
	config.OFF_STATE = !config.ON_STATE;
	config.upButton = data->upButton;
	config.upRelayPin = data->upRelayPin;

	upButton = new DebounceEvent (config.upButton, std::bind (&BlindController::callbackUpButton, this, _1, _2, _3, _4), BUTTON_PUSHBUTTON | BUTTON_DEFAULT_HIGH | BUTTON_SET_PULLUP, 50, 200);
	downButton = new DebounceEvent (config.downButton, std::bind (&BlindController::callbackDownButton, this, _1, _2, _3, _4), BUTTON_PUSHBUTTON | BUTTON_DEFAULT_HIGH | BUTTON_SET_PULLUP, 50, 200);

	pinMode (config.upRelayPin, OUTPUT);
	pinMode (config.downRelayPin, OUTPUT);

	DEBUG_INFO ("Call begin");

}

void BlindController::begin () {
	blindControlerHw_t defaultConfig;

	defaultConfig.downButton = DOWN_BUTTON;
	defaultConfig.downRelayPin = DOWN_RELAY;
	defaultConfig.fullTravellingTime = ROLLING_TIME;
	defaultConfig.upButton = UP_BUTTON;
	defaultConfig.upRelayPin = UP_RELAY;

	DEBUG_INFO ("Call default begin");

	begin (&defaultConfig);
}

void BlindController::fullRollup () {
	DEBUG_DBG ("Configure full roll up");
	positionRequest = 100;
	travellingTime = config.fullTravellingTime * 1.1;
	blindState = rollingUp;
	DEBUG_DBG ("--- STATE: Rolling up");
}

void BlindController::fullRolldown () {
	DEBUG_DBG ("Configure full roll down");
	positionRequest = 0;
	travellingTime = config.fullTravellingTime * 1.1;
	blindState = rollingDown;
	DEBUG_DBG ("--- STATE: Rolling down");
}

void BlindController::gotoPosition (int8_t pos) {
	// TODO
	positionRequest = pos;
	if (pos > position) {
		blindState = rollingUp;
		travellingTime = movementToTime (pos - position);
		DEBUG_DBG ("--- STATE: Rolling up to position %d", pos);
	} else if (pos < position) {
		blindState = rollingDown;
		travellingTime = movementToTime (position - pos);
		DEBUG_DBG ("--- STATE: Rolling down to position %d", pos);
	}
}

void  BlindController::rollup () {
	time_t timeMoving;

	if (!movingUp) {
		DEBUG_DBG ("Started roll up. Position Request %d. Original position %d", positionRequest, position);
		movingUp = true;
		blindStartedMoving = millis ();
		originalPosition = position;
		finalPosition = positionRequest;
		digitalWrite (config.downRelayPin, config.OFF_STATE);
		digitalWrite (config.upRelayPin, config.ON_STATE);
	}
	timeMoving = millis () - blindStartedMoving;
	if (position != -1 && position < 100) {
		position = originalPosition + timeToPos (timeMoving);
		if (position > 100) {
			position = 100;
		}
	}
	if (travellingTime > 0 && timeMoving > travellingTime) {
		DEBUG_DBG ("Stopped roll up");
		if (positionRequest == 100) {
			position = 100;
		}
		//movingUp = false;
		blindState = stopped;
		DEBUG_DBG ("--- STATE: Stopped");
	}
}

void BlindController::rolldown () {
	time_t timeMoving;

	if (!movingDown) {
		DEBUG_DBG ("Started roll down. Position Request %d. Original position %d", positionRequest, position);
		movingDown = true;
		blindStartedMoving = millis ();
		originalPosition = position;
		finalPosition = positionRequest;
		digitalWrite (config.upRelayPin, config.OFF_STATE);
		digitalWrite (config.downRelayPin, config.ON_STATE);
	}
	timeMoving = millis () - blindStartedMoving;
	if (position != -1 && position > 0) {
		position = originalPosition - timeToPos (timeMoving);
		if (position < 0) {
			position = 0;
		}
	}
	if (travellingTime > 0 && timeMoving > travellingTime) {
		DEBUG_DBG ("Stopped roll down");
		if (positionRequest == 0) {
			position = 0;
		}
		//movingUp = false;
		blindState = stopped;
		DEBUG_DBG ("--- STATE: Stopped");
	}

}

void BlindController::stop () {
	digitalWrite (config.upRelayPin, config.OFF_STATE);
	digitalWrite (config.downRelayPin, config.OFF_STATE);
	movingDown = false;
	movingUp = false;
}

void BlindController::showPosition () {
	static time_t lastShowedPos;
	switch (blindState) {
	case rollingUp:
	case rollingDown:
		if (millis () - lastShowedPos > 1000) {
			lastShowedPos = millis ();
			DEBUG_INFO ("Position: %d", position);
		}
	}
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

	showPosition ();
}

