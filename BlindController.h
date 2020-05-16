// BlindController.h

#ifndef _BLINDCONTROLLER_h
#define _BLINDCONTROLLER_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif

#include <DebounceEvent.h>

typedef struct {
public:
	int upRelayPin;
	int downRelayPin;
	int upButton;
	int downButton;
	time_t fullUpTime;
	time_t fullDownTime;
	int ON_STATE = HIGH;
protected:
	int OFF_STATE = !ON_STATE;
	friend class BlindController;
} blindControlerHw_t;

typedef enum {
	rollingUp = 1,
	rollingDown = 2,
	stopped = 3,
	error = 0
} blindState_t;

class BlindController
{
 protected:
	 blindControlerHw_t config;
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

 public:
	 void begin (blindControlerHw_t *data);
	 void begin ();
	 void fullRollup ();
	 void fullRolldown ();
	 void gotoPosition (int8_t pos);
	 int8_t getPosition () {
		 return position;
	 }
	 blindState_t getState () {
		 return blindState;
	 }
	 void loop ();

protected:
	void rollup ();
	void rolldown ();
	void stop ();
	void callbackUpButton (uint8_t pin, uint8_t event, uint8_t count, uint16_t length);
	void callbackDownButton (uint8_t pin, uint8_t event, uint8_t count, uint16_t length);
	int8_t timeToPos (time_t movementTime) {
		return movementTime * 100 / config.fullUpTime;
	}
};

#endif

