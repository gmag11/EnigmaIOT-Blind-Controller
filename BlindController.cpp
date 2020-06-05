// 
// 
// 

#include "BlindController.h"
#include "debug.h"
//#include <functional>

using namespace std;
using namespace placeholders;

// Default values
//struct blindControlerHw_t config = {
//    .upRelayPin = 12,
//    .downRelayPin = 14,
//    .upButton = 4,
//    .downButton = 5,
//    .fullTravellingTime = fullTravelTime,
//    .notifPeriod = 0, // Not used here
//    .keepAlivePeriod = 0, // Not used here
//    .ON_STATE = HIGH
//};

constexpr auto UP_RELAY_PIN = 12;
constexpr auto DOWN_RELAY_PIN = 14;
constexpr auto UP_BUTTON_PIN = 4;
constexpr auto DOWN_BUTTON_PIN = 3;
constexpr auto ROLLING_TIME = 30000;
constexpr auto NOTIF_PERIOD_RATIO = 5;
constexpr auto KEEP_ALIVE_PERIOD_RATIO = 4;
constexpr auto ON_STATE_DEFAULT = HIGH;

constexpr auto CONFIG_FILE = "/blindcont.json"; ///< @brief blind controller configuration file name

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

void BlindController::defaultConfig () {
	config.upRelayPin = UP_RELAY_PIN;
	config.downRelayPin = DOWN_RELAY_PIN;
	config.upButton = UP_BUTTON_PIN;
	config.downButton = DOWN_BUTTON_PIN;
	config.fullTravellingTime = ROLLING_TIME;
	config.notifPeriod = config.fullTravellingTime / NOTIF_PERIOD_RATIO;
	config.keepAlivePeriod = config.fullTravellingTime * KEEP_ALIVE_PERIOD_RATIO;
	config.ON_STATE = ON_STATE_DEFAULT;
}

void BlindController::begin (void* data) {
	blindControlerHw_t *data_p = (blindControlerHw_t*)data;

	if (data_p) {
		DEBUG_WARN ("Load user config from parameter. Not using stored data");
		config.downButton = data_p->downButton;
		config.downRelayPin = data_p->downRelayPin;
		config.fullTravellingTime = data_p->fullTravellingTime;
		config.ON_STATE = data_p->ON_STATE;
		OFF_STATE = !config.ON_STATE;
		config.upButton = data_p->upButton;
		config.upRelayPin = data_p->upRelayPin;
		config.keepAlivePeriod = config.fullTravellingTime * KEEP_ALIVE_PERIOD_RATIO;
		config.notifPeriod = config.fullTravellingTime / NOTIF_PERIOD_RATIO;
	}

	configurePins ();

	sendStartAnouncement ();

	DEBUG_INFO ("Finish begin");

}

void BlindController::configurePins () {
	if (upButton) {
		delete(upButton);
	}
	if (downButton) {
		delete(downButton);
	}
	upButton = new DebounceEvent (config.upButton, std::bind (&BlindController::callbackUpButton, this, _1, _2, _3, _4), BUTTON_PUSHBUTTON | BUTTON_DEFAULT_HIGH | BUTTON_SET_PULLUP, 50, 500);
	downButton = new DebounceEvent (config.downButton, std::bind (&BlindController::callbackDownButton, this, _1, _2, _3, _4), BUTTON_PUSHBUTTON | BUTTON_DEFAULT_HIGH | BUTTON_SET_PULLUP, 50, 500);

	pinMode (config.upRelayPin, OUTPUT);
	pinMode (config.downRelayPin, OUTPUT);
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

void BlindController::configManagerStart (EnigmaIOTNodeClass* node) {
	enigmaIotNode = node;

	char upRelayStr[10];
	itoa (config.upRelayPin, upRelayStr, 10);
	upRelayPinParam = new AsyncWiFiManagerParameter ("upRelayPinParam", "Up Relay Pin", upRelayStr, 10, "required type=\"number\" min=\"0\" max=\"16\" step=\"1\"");

	char downRelayStr[10];
	itoa (config.downRelayPin, downRelayStr, 10);
	downRelayPinParam = new AsyncWiFiManagerParameter ("downRelayPinParam", "Down Relay Pin", downRelayStr, 10, "required type=\"number\" min=\"0\" max=\"16\" step=\"1\"");
	
	char upButtonStr[10];
	itoa (config.upButton, upButtonStr, 10);
	upButtonParam = new AsyncWiFiManagerParameter ("upButtonParam", "Up Button Pin", upButtonStr, 10, "required type=\"number\" min=\"0\" max=\"16\" step=\"1\"");
	
	char downButtonStr[10];
	itoa (config.downButton, downButtonStr, 10);
	downButtonParam = new AsyncWiFiManagerParameter ("downButtonParam", "Down Button Pin", downButtonStr, 10, "required type=\"number\" min=\"0\" max=\"16\" step=\"1\"");
	
	char fullTravelTimeParamStr[10];
	itoa (config.fullTravellingTime/1000, fullTravelTimeParamStr, 10);
	fullTravelTimeParam = new AsyncWiFiManagerParameter ("fullTravelTimeParam", "Full Travel Time", fullTravelTimeParamStr, 10, "required type=\"number\" min=\"0\" max=\"3600\" step=\"1\"");
	
	char notifPeriodTimeStr[10];
	itoa (config.notifPeriod/1000, notifPeriodTimeStr, 10);
	notifPeriodTimeParam = new AsyncWiFiManagerParameter ("notifPeriodTimeParam", "Notification Period", notifPeriodTimeStr, 10, "required type=\"number\" min=\"0\" max=\"3600\" step=\"1\"");

	char keepAlivePeriodTimeStr[10];
	itoa (config.keepAlivePeriod / 1000, keepAlivePeriodTimeStr, 10);
	keepAlivePeriodTimeParam = new AsyncWiFiManagerParameter ("keepAlivePeriodTimeParam", "Keep Alive Period", keepAlivePeriodTimeStr, 10, "required type=\"number\" min=\"0\" max=\"3600\" step=\"1\"");

	char onStateStr[10];
	itoa (config.notifPeriod / 1000, onStateStr, 10);
	onStateParam = new AsyncWiFiManagerParameter ("onStateParam", "Relay Pin On State", onStateStr, 10, "required type=\"number\" min=\"0\" max=\"1\" step=\"1\"");

	

	enigmaIotNode->addWiFiManagerParameter (upRelayPinParam);
	enigmaIotNode->addWiFiManagerParameter (downRelayPinParam);
	enigmaIotNode->addWiFiManagerParameter (upButtonParam);
	enigmaIotNode->addWiFiManagerParameter (downButtonParam);
	enigmaIotNode->addWiFiManagerParameter (fullTravelTimeParam);
	enigmaIotNode->addWiFiManagerParameter (notifPeriodTimeParam);
	enigmaIotNode->addWiFiManagerParameter (keepAlivePeriodTimeParam);
	enigmaIotNode->addWiFiManagerParameter (onStateParam);
}

void BlindController::configManagerExit (bool status) {
	DEBUG_DBG ("==== Blind Controller Configuration result ====");
	DEBUG_DBG ("Up Relay pin: %s", upRelayPinParam->getValue());
	DEBUG_DBG ("Down Relay pin: %s", downRelayPinParam->getValue ());
	DEBUG_DBG ("Up Button pin: %s", upButtonParam->getValue ());
	DEBUG_DBG ("Down Button pin: %s", downButtonParam->getValue ());
	DEBUG_DBG ("Full travelling time: %s ms", fullTravelTimeParam->getValue ());
	DEBUG_DBG ("Notification period time: %s", notifPeriodTimeParam->getValue ());
	DEBUG_DBG ("Keep Alive period time: %s", keepAlivePeriodTimeParam->getValue ());
	DEBUG_DBG ("On Relay state: %s", onStateParam->getValue ());

	if (status) {
		config.upRelayPin = atoi (upRelayPinParam->getValue ());
		config.downRelayPin = atoi (downRelayPinParam->getValue ());
		config.upButton = atoi (upButtonParam->getValue ());
		config.downButton = atoi (downButtonParam->getValue ());
		config.fullTravellingTime = atoi (fullTravelTimeParam->getValue ());
		config.notifPeriod = atoi (notifPeriodTimeParam->getValue ());
		config.keepAlivePeriod = atoi (keepAlivePeriodTimeParam->getValue ());
		config.ON_STATE = atoi (onStateParam->getValue ());


		if (!saveConfig ()) {
			DEBUG_ERROR ("Error writting MQTT config to filesystem.");
		} else {
			DEBUG_INFO ("Configuration stored");
		}
	} else {
		DEBUG_DBG ("Configuration does not need to be saved");
	}

	delete (upRelayPinParam);
	delete (downRelayPinParam);
	delete (upButtonParam);
	delete (downButtonParam);
	delete (fullTravelTimeParam);
	delete (notifPeriodTimeParam);
	delete (keepAlivePeriodTimeParam);
	delete (onStateParam);
}

bool BlindController::loadConfig () {
	//SPIFFS.remove (CONFIG_FILE); // Only for testing
	bool json_correct = false;

	if (!SPIFFS.begin ()) {
		DEBUG_WARN ("Error starting filesystem. Formatting");
		SPIFFS.format ();
	}

	if (SPIFFS.exists (CONFIG_FILE)) {
		DEBUG_DBG ("Opening %s file", CONFIG_FILE);
		File configFile = SPIFFS.open (CONFIG_FILE, "r");
		if (configFile) {
			size_t size = configFile.size ();
			DEBUG_DBG ("%s opened. %u bytes", CONFIG_FILE, size);
			DynamicJsonDocument doc (512);
			DeserializationError error = deserializeJson (doc, configFile);
			if (error) {
				DEBUG_ERROR ("Failed to parse file");
			} else {
				DEBUG_DBG ("JSON file parsed");
			}

			if (doc.containsKey ("upRelayPin") && doc.containsKey ("downRelayPin")
				&& doc.containsKey ("upButton") && doc.containsKey ("downButton")
				&& doc.containsKey ("fullTravellingTime") && doc.containsKey ("notifPeriod")
				&& doc.containsKey ("keepAlivePeriod") && doc.containsKey ("onState")) {
				json_correct = true;
			}

			config.upRelayPin = doc["upRelayPin"].as<int> ();
			config.downRelayPin = doc["downRelayPin"].as<int> ();
			config.upButton = doc["upButton"].as<int> ();
			config.downButton = doc["downButton"].as<int> ();
			config.fullTravellingTime = doc["fullTravellingTime"].as<int> ();
			config.notifPeriod = doc["notifPeriod"].as<int> ();
			config.keepAlivePeriod = doc["keepAlivePeriod"].as<int> ();
			config.ON_STATE = doc["onState"].as<int> ()==1?HIGH:LOW;
			
			configFile.close ();
			if (json_correct) {
				DEBUG_INFO ("Blind controller configuration successfuly read");
			} else {
				DEBUG_WARN ("Blind controller configuration error");
			}
			DEBUG_DBG ("==== Blind Controller  Configuration ====");
			DEBUG_DBG ("Up Relay pin: %s", config.upRelayPin);
			DEBUG_DBG ("Down Relay pin: %s", config.downRelayPin);
			DEBUG_DBG ("Up Button pin: %s", config.upButton);
			DEBUG_DBG ("Down Button pin: %s", config.downButton);
			DEBUG_DBG ("Full travelling time: %s ms", config.fullTravellingTime);
			DEBUG_DBG ("Notification period time: %s", config.notifPeriod);
			DEBUG_DBG ("Keep Alive period time: %s", config.keepAlivePeriod);
			DEBUG_DBG ("On Relay state: %s", config.ON_STATE);

			size_t jsonLen = measureJsonPretty (doc) + 1;
			char *output=(char*)malloc(jsonLen);
			serializeJsonPretty (doc, output, jsonLen);

			DEBUG_DBG ("JSON file %s", output.c_str ());

			free (output);

		} else {
			DEBUG_WARN ("Error opening %s", CONFIG_FILE);
		}
	} else {
		DEBUG_WARN ("%s do not exist", CONFIG_FILE);
	}

	return json_correct;
}

bool BlindController::saveConfig () {
	if (!SPIFFS.begin ()) {
		DEBUG_WARN ("Error opening filesystem");
	}
	DEBUG_DBG ("Filesystem opened");

	File configFile = SPIFFS.open (CONFIG_FILE, "w");
	if (!configFile) {
		DEBUG_WARN ("Failed to open config file %s for writing", CONFIG_FILE);
		return false;
	} else {
		DEBUG_DBG ("%s opened for writting", CONFIG_FILE);
	}

	DynamicJsonDocument doc (512);

	doc["upRelayPin"] = config.upRelayPin;
	doc["downRelayPin"] = config.downRelayPin;
	doc["upButton"] = config.upButton;
	doc["downButton"] = config.downButton;
	doc["fullTravellingTime"] = config.fullTravellingTime;
	doc["notifPeriod"] = config.notifPeriod;
	doc["keepAlivePeriod"] = config.keepAlivePeriod;
	doc["onState"] = config.ON_STATE;

	if (serializeJson (doc, configFile) == 0) {
		DEBUG_ERROR ("Failed to write to file");
		configFile.close ();
		//SPIFFS.remove (CONFIG_FILE);
		return false;
	}

	size_t jsonLen = measureJsonPretty (doc) + 1;
	char* output = (char*)malloc (jsonLen);
	serializeJsonPretty (doc, output, jsonLen);

	DEBUG_DBG ("%s", output.c_str ());

	free (output);

	configFile.flush ();
	size_t size = configFile.size ();

	//configFile.write ((uint8_t*)(&mqttgw_config), sizeof (mqttgw_config));
	configFile.close ();
	DEBUG_DBG ("Blind controller configuration saved to flash. %u bytes", size);
	return true;
}