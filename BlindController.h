// BlindController.h

#ifndef _BLINDCONTROLLER_h
#define _BLINDCONTROLLER_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif

#include "EnigmaIOTjsonController.h"
#include <DebounceEvent.h>

struct blindControlerHw_t {
	int upRelayPin;
	int downRelayPin;
	int upButton;
	int downButton;
	time_t fullTravellingTime;
	time_t notifPeriod;
	time_t keepAlivePeriod;
	int ON_STATE;
};

typedef enum {
	rollingUp = 1,
	rollingDown = 2,
	stopped = 4,
	error = 0
} blindState_t;

typedef enum {
	UP_BUTTON,
	DOWN_BUTTON
} button_t;

#if defined ESP8266 || defined ESP32
#include <functional>
//typedef std::function<void (blindState_t state, uint8_t position)> stateNotify_cb_t;
#else
#error This code only supports ESP8266 or ESP32 platforms
#endif

class BlindController : EnigmaIOTjsonController
{
 protected:
	 blindControlerHw_t config;
	 int OFF_STATE;
	 DebounceEvent* upButton;
     DebounceEvent *downButton;
	 int8_t position = -1;
	 int8_t positionRequest = -1;
	 int8_t originalPosition;
	 int8_t finalPosition;
	 blindState_t blindState = stopped;
	 time_t travellingTime = -1;
	 time_t blindStartedMoving;
	 bool movingUp = false;
	 bool movingDown = false;
	 //sendJson_cb sendJson; // Defined on parent class

 public:
	 void begin (/*struct blindControlerHw_t*/void *data);
	 void begin ();
	 bool processRxCommand (const uint8_t* mac, const uint8_t* buffer, uint8_t length, nodeMessageType_t command, nodePayloadEncoding_t payloadEncoding);
	 void loop ();
	 ~BlindController ();

protected:
	void rollup ();
	void rolldown ();
	void stop ();
	void fullRollup ();
	void fullRolldown ();
	bool gotoPosition (int pos);
	int8_t getPosition () {
		return position;
	}
	blindState_t getState () {
		return blindState;
	}
	char* stateToStr (int state) {
		switch (state) {
		case rollingUp:
			return "Rolling up";
		case rollingDown:
			return "Rolling down";
		case stopped:
			return "Stopped";
		}
	}
	void requestStop ();
	void callbackUpButton (uint8_t pin, uint8_t event, uint8_t count, uint16_t length);
	void callbackDownButton (uint8_t pin, uint8_t event, uint8_t count, uint16_t length);
	bool sendButtonPress (button_t button, int count);
	int8_t timeToPos (time_t movementTime) {
		return movementTime * 100 / config.fullTravellingTime;
	}
	time_t movementToTime (int8_t movement);
	void sendPosition ();

	bool sendGetPosition ();
	bool sendGetStatus ();
	bool sendCommandResp (const char* command, bool result);
	bool sendStartAnouncement ();
	void processBlindEvent (blindState_t state, int8_t position);
};

#endif

