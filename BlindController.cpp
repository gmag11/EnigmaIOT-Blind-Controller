// 
// 
// 

#include "BlindController.h"
#include "debug.h"
//#include <functional>

using namespace std;
using namespace placeholders;

// Default values
constexpr auto UP_BUTTON_PIN = 0;
constexpr auto DOWN_BUTTON_PIN = 2;
constexpr auto UP_RELAY_PIN = 5;
constexpr auto DOWN_RELAY_PIN = 4;
constexpr auto ROLLING_TIME = 15000;
constexpr auto NOTIF_PERIOD_RATIO = 5;
constexpr auto KEEP_ALIVE_PERIOD_RATIO = 4;

const char* commandKey = "cmd";
const char* positionCommandValue = "pos";
const char* stateCommandValue = "state";
const char* fullUpCommandValue = "uu";
const char* fullDownCommandValue = "dd";
const char* gotoCommandValue = "go";
const char* stopCommandValue = "stop";
const char* startCommandValue = "start";
const char* resultKey = "res";
const char* positionKey = "pos";
const char* memKey = "mem";
const char* eventValue = "event";
const char* buttonKey = "but";
const char* upButtonValue = "up";
const char* downButtonValue = "down";
const char* countNumberKey = "num";


bool BlindController::processRxCommand (const uint8_t* mac, const uint8_t* buffer, uint8_t length, nodeMessageType_t command, nodePayloadEncoding_t payloadEncoding) {
	// TODO
	if (command != nodeMessageType_t::DOWNSTREAM_DATA_GET && command != nodeMessageType_t::DOWNSTREAM_DATA_SET) {
		DEBUG_WARN ("Wrong message type");
		return false;
	}
	if (payloadEncoding != MSG_PACK) {
		DEBUG_WARN ("Wrong payload encoding");
		return false;
	}

	DynamicJsonDocument doc (1000);
	uint8_t tempBuffer[MAX_MESSAGE_LENGTH];

	memcpy (tempBuffer, buffer, length);
	DeserializationError error = deserializeMsgPack (doc, tempBuffer, length);
	if (error != DeserializationError::Ok) {
		DEBUG_WARN ("Error decoding command: %s", error.c_str ());
		return false;
	}
	DEBUG_WARN ("Command: %d = %s", command, command == nodeMessageType_t::DOWNSTREAM_DATA_GET ? "GET" : "SET");
	size_t strLen = measureJson (doc) + 1;
	char* strBuffer = (char*)malloc (strLen);
	serializeJson (doc, strBuffer, strLen);
	DEBUG_WARN ("Data: %s", strBuffer);
	free (strBuffer);

	if (command == nodeMessageType_t::DOWNSTREAM_DATA_GET) {
		if (!strcmp(doc[commandKey], positionCommandValue)) {
			DEBUG_WARN ("Position = %d", getPosition ());
			if (!sendGetPosition ()) {
				DEBUG_WARN ("Error sending get position command response");
				return false;
			}
		} else if (!strcmp (doc[commandKey], stateCommandValue)) {
			DEBUG_WARN ("Status = %d", getState ());
			if (!sendGetStatus ()) {
				DEBUG_WARN ("Error sending get state command response");
				return false;
			}
		}
	} else {
		if (!strcmp (doc[commandKey], fullUpCommandValue)) { // Command full rollup
			DEBUG_WARN ("Full up request");
			if (!sendCommandResp (fullUpCommandValue, true)) {
				DEBUG_WARN ("Error sending Full rollup command response");
				return false;
			}
			fullRollup ();
		} else if (!strcmp (doc[commandKey], fullDownCommandValue)) { // Command full rolldown
			DEBUG_WARN ("Full down request");
			if (!sendCommandResp (fullDownCommandValue, true)) {
				DEBUG_WARN ("Error sending Full rolldown command response");
				return false;
			}
			fullRolldown ();
		} else if (!strcmp (doc[commandKey], gotoCommandValue)) { // Command go to position
			if (!doc.containsKey (positionKey)) {
				if (!sendCommandResp (gotoCommandValue, false)) { // ??????
					DEBUG_WARN ("Error sending go command response");
				}
				return false;
			}
			int position = doc[positionKey];
			if (!sendCommandResp (gotoCommandValue, gotoPosition (position))) {
				DEBUG_WARN ("Error sending go command response");
				return false;
			}
			DEBUG_WARN ("Go to position %d request", position);
		} else if (!strcmp (doc[commandKey], stopCommandValue)) {
			DEBUG_WARN ("Stop request");
			if (!sendCommandResp (stopCommandValue, true)) {
				DEBUG_WARN ("Error sending stop command response");
				return false;
			}
			requestStop ();
		}
	}
	return true;
}

bool BlindController::sendGetPosition () {
	const size_t capacity = JSON_OBJECT_SIZE (2);
	DynamicJsonDocument json (capacity);

	json[commandKey] = positionCommandValue;
	json[positionKey] = getPosition ();

	return sendJson (json);
}

bool BlindController::sendGetStatus () {
	const size_t capacity = JSON_OBJECT_SIZE (3);
	DynamicJsonDocument json (capacity);

	json[commandKey] = stateCommandValue;
	json[stateCommandValue] = (int)getState ();
	json[positionKey] = getPosition ();

	return sendJson (json);
}

bool BlindController::sendCommandResp (const char* command, bool result) {
	const size_t capacity = JSON_OBJECT_SIZE (2);
	DynamicJsonDocument json (capacity);

	json[commandKey] = command;
	json[resultKey] = (int)result;

	return sendJson (json);
}

void BlindController::processBlindEvent (blindState_t state, int8_t position) {
	DEBUG_WARN ("State: %s. Position %d", stateToStr (state), position);

	const size_t capacity = JSON_OBJECT_SIZE (5);
	DynamicJsonDocument json (capacity);

	json[stateCommandValue] = (int)state;
	json[positionKey] = position;
	json[memKey] = ESP.getFreeHeap ();

	sendJson (json);
}

bool BlindController::sendButtonPress (button_t button, int count) {
	const size_t capacity = JSON_OBJECT_SIZE (3);
	DynamicJsonDocument json (capacity);

	json[commandKey] = eventValue;
	if (button == button_t::DOWN_BUTTON)
		json[buttonKey] = downButtonValue;
	else
		json[buttonKey] = upButtonValue;
	json[countNumberKey] = (int)count;

	return sendJson (json);
}

bool BlindController::sendStartAnouncement () {
	const size_t capacity = JSON_OBJECT_SIZE (1);
	DynamicJsonDocument json (capacity);

	json[commandKey] = startCommandValue;

	return sendJson (json);
}


void BlindController::callbackUpButton (uint8_t pin, uint8_t event, uint8_t count, uint16_t length) {
	DEBUG_WARN ("Up button. Event %d Count %d", event, count);
	if (event == EVENT_PRESSED) {
		sendButtonPress (button_t::UP_BUTTON, count);
		DEBUG_DBG ("Up button pressed. Count %d", count);
		if (count == 1) { // First button press
			DEBUG_INFO ("Call simple roll up");
			positionRequest = -1; // Request undefined position
			travellingTime = -1;
			blindState = rollingUp;
			movingUp = false; // Not moving down yet
			DEBUG_DBG ("--- STATE: Rolling up");
		} else if (count == 2) { // Second button press --> full roll up
			DEBUG_INFO ("Call full roll up");
			fullRollup ();
		}
	}
	if (event == EVENT_RELEASED && positionRequest == -1) { // Check button release on undefined position request
		DEBUG_INFO ("Stop rolling up");
		blindState = stopped;
		movingUp = false;
		DEBUG_DBG ("--- STATE: Stopped");
		processBlindEvent (blindState, position);
	//} else if (event == EVENT_PRESSED) {
	//	if (count == 2) { // Second button press --> full roll up
	//		DEBUG_INFO ("Call full roll up");
	//		fullRollup ();
	//	}
	}
}

void BlindController::callbackDownButton (uint8_t pin, uint8_t event, uint8_t count, uint16_t length) {
	DEBUG_WARN ("Down button. Event %d Count %d", event, count);
	if (event == EVENT_PRESSED) {
		sendButtonPress (button_t::DOWN_BUTTON, count);
		DEBUG_DBG ("Down button pressed. Count %d", count);
		if (count == 1) { // First button press
			DEBUG_INFO ("Call simple roll down");
			positionRequest = -1; // Request undefined position
			travellingTime = -1;
			blindState = rollingDown; 
			movingDown = false; // Not moving down yet
			DEBUG_DBG ("--- STATE: Rolling down");
		} else if (count == 2) { // Second button press --> full roll down
			DEBUG_INFO ("Call full roll down");
			fullRolldown ();
		}
	}
	if (event == EVENT_RELEASED && positionRequest == -1) { // Check button release on undefined position request
		DEBUG_INFO ("Stop rollinging down");
		blindState = stopped;
		movingDown = false;
		DEBUG_DBG ("--- STATE: Stopped");
		processBlindEvent (blindState, position);
	//} else if (event == EVENT_PRESSED) {
	//	if (count == 2) { // Second button press --> full roll down
	//		DEBUG_INFO ("Call full roll down");
	//		fullRolldown ();
	//	}
	}

}

void BlindController::begin (void* data) {
	blindControlerHw_t *data_p = (blindControlerHw_t*)data;

	config.downButton = data_p->downButton;
	config.downRelayPin = data_p->downRelayPin;
	config.fullTravellingTime = data_p->fullTravellingTime;
	config.ON_STATE = data_p->ON_STATE;
	OFF_STATE = !config.ON_STATE;
	config.upButton = data_p->upButton;
	config.upRelayPin = data_p->upRelayPin;
	config.keepAlivePeriod = config.fullTravellingTime * KEEP_ALIVE_PERIOD_RATIO;
	config.notifPeriod = config.fullTravellingTime / NOTIF_PERIOD_RATIO;

	upButton = new DebounceEvent (config.upButton, std::bind (&BlindController::callbackUpButton, this, _1, _2, _3, _4), BUTTON_PUSHBUTTON | BUTTON_DEFAULT_HIGH | BUTTON_SET_PULLUP, 50, 500);
	downButton = new DebounceEvent (config.downButton, std::bind (&BlindController::callbackDownButton, this, _1, _2, _3, _4), BUTTON_PUSHBUTTON | BUTTON_DEFAULT_HIGH | BUTTON_SET_PULLUP, 50, 500);

	pinMode (config.upRelayPin, OUTPUT);
	pinMode (config.downRelayPin, OUTPUT);

	sendStartAnouncement ();

	DEBUG_INFO ("Finish begin");

}

void BlindController::begin () {
	blindControlerHw_t defaultConfig;

	defaultConfig.downButton = DOWN_BUTTON_PIN;
	defaultConfig.downRelayPin = DOWN_RELAY_PIN;
	defaultConfig.fullTravellingTime = ROLLING_TIME;
	defaultConfig.upButton = UP_BUTTON_PIN;
	defaultConfig.upRelayPin = UP_RELAY_PIN;

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

bool BlindController::gotoPosition (int pos) {
	int currentPosition = position;

	if (pos <= 0) {
		pos = 0;
		currentPosition = 100; // Force full rolling down
	} else if (pos >= 100) {
		pos = 100;
		currentPosition = 0; // Force full rolling up
	} else if (position == -1) {
		DEBUG_WARN ("Position not calibrated. Pos = %d", pos);
		return false;
	}
	stop ();
	positionRequest = pos;
	if (pos > currentPosition) {
		blindState = rollingUp;
		DEBUG_WARN ("--- STATE: Rolling up to position %d", pos);
		if (pos < 100) {
			travellingTime = movementToTime (pos - position);
		} else {
			fullRollup ();
		}
	} else if (pos < currentPosition) {
		blindState = rollingDown;
		DEBUG_WARN ("--- STATE: Rolling down to position %d", pos);
		if (pos > 0) {
			travellingTime = movementToTime (position - pos);
		} else {
			fullRolldown ();
		}
	} else {
		DEBUG_WARN ("Requested = Current position");
		processBlindEvent (blindState, position);
	}
	return true;
}

void  BlindController::rollup () {
	time_t timeMoving;

	if (!movingUp) {
		DEBUG_DBG ("Started roll up. Position Request %d. Original position %d", positionRequest, position);
		movingDown = false;
		movingUp = true;
		blindStartedMoving = millis ();
		originalPosition = position;
		finalPosition = positionRequest;
		digitalWrite (config.downRelayPin, OFF_STATE);
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
		blindState = stopped;
		DEBUG_DBG ("--- STATE: Stopped");
		processBlindEvent (blindState, position);
	} else {
		if (timeMoving > config.fullTravellingTime * 1.1) {
			blindState = stopped;
			position = 100;
			DEBUG_DBG ("--- STATE: Stopped");
			processBlindEvent (blindState, position);
		}
	}
}

void BlindController::rolldown () {
	time_t timeMoving;

	if (!movingDown) {
		DEBUG_DBG ("Started roll down. Position Request %d. Original position %d", positionRequest, position);
		movingUp = false;
		movingDown = true;
		blindStartedMoving = millis ();
		originalPosition = position;
		finalPosition = positionRequest;
		digitalWrite (config.upRelayPin, OFF_STATE);
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
		blindState = stopped;
		DEBUG_DBG ("--- STATE: Stopped");
		processBlindEvent (blindState, position);
	} else {
		if (timeMoving > config.fullTravellingTime * 1.1) {
			blindState = stopped;
			position = 0;
			DEBUG_DBG ("--- STATE: Stopped");
			processBlindEvent (blindState, position);
		}
	}

}

void BlindController::requestStop () {
	DEBUG_DBG ("Configure stop");
	blindState = stopped;
	DEBUG_DBG ("--- STATE: Stopped");
	processBlindEvent (blindState, position);
}

void BlindController::stop () {
	digitalWrite (config.upRelayPin, OFF_STATE);
	digitalWrite (config.downRelayPin, OFF_STATE);
	movingDown = false;
	movingUp = false;
}

void BlindController::sendPosition () {
	static time_t lastShowedPos;
	switch (blindState) {
	case rollingUp:
	case rollingDown:
		if (millis () - lastShowedPos > config.notifPeriod) {
			lastShowedPos = millis ();
			DEBUG_INFO ("Position: %d", position);
			processBlindEvent (blindState, position);
		}
		break;
	case stopped:
		if (millis () - lastShowedPos > config.keepAlivePeriod) {
			lastShowedPos = millis ();
			DEBUG_INFO ("Position: %d", position);
			processBlindEvent (blindState, position);
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

	sendPosition ();
}

time_t BlindController::movementToTime (int8_t movement) {
	time_t calculatedTime = movement * config.fullTravellingTime / 100;
	if (movement >= config.fullTravellingTime) {
		calculatedTime = config.fullTravellingTime * 1.1;
	}
	DEBUG_DBG ("Desired movement: %d. Calculated time: %d", movement, calculatedTime);
	return calculatedTime;
}

BlindController::~BlindController () {
	delete(upButton);
	delete(downButton);
	sendData = 0;
}