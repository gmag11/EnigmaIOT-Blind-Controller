// BlindController.h

#ifndef _BLINDCONTROLLER_h
#define _BLINDCONTROLLER_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif

#ifdef ESP32
#include <SPIFFS.h>
#endif

#include <EnigmaIOTjsonController.h>
#include <DebounceEvent.h>

struct blindControlerHw_t {
	int upRelayPin;
	int downRelayPin;
	uint8_t upButton;
	uint8_t downButton;
	clock_t fullTravellingTime;
	clock_t notifPeriod;
	clock_t keepAlivePeriod;
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

#define CONTROLLER_CLASS_NAME BlindController
static const char* CONTROLLER_NAME = "Blind controller";

class CONTROLLER_CLASS_NAME : EnigmaIOTjsonController {
protected:
	blindControlerHw_t config;
	int OFF_STATE;
	DebounceEvent* upButton;
	DebounceEvent* downButton;
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

	AsyncWiFiManagerParameter* upRelayPinParam; ///< @brief Configuration field for up relay pin
	AsyncWiFiManagerParameter* downRelayPinParam; ///< @brief Configuration field for down relay pin
	AsyncWiFiManagerParameter* upButtonParam; ///< @brief Configuration field for up button pin
	AsyncWiFiManagerParameter* downButtonParam; ///< @brief Configuration field for down button pin
	AsyncWiFiManagerParameter* fullTravelTimeParam; ///< @brief Configuration field for full travel time
	AsyncWiFiManagerParameter* notifPeriodTimeParam; ///< @brief Configuration field for notification period time
	AsyncWiFiManagerParameter* keepAlivePeriodTimeParam; ///< @brief Configuration field for keep alive time
	AsyncWiFiManagerParameter* onStateParam; ///< @brief Configuration field for on state value for relay pins

public:
	void setup (EnigmaIOTNodeClass* node, void* data = NULL);
	bool processRxCommand (const uint8_t* mac, const uint8_t* buffer, uint8_t length, nodeMessageType_t command, nodePayloadEncoding_t payloadEncoding);
	void loop ();
	~CONTROLLER_CLASS_NAME ();
	/**
	 * @brief Called when wifi manager starts config portal
	 * @param enigmaIotGw Pointer to EnigmaIOT gateway instance
	 */
	void configManagerStart ();

	/**
	 * @brief Called when wifi manager exits config portal
	 * @param status `true` if configuration was successful
	 */
	void configManagerExit (bool status);

	/**
	 * @brief Loads output module configuration
	 * @return Returns `true` if load was successful. `false` otherwise
	 */
	bool loadConfig ();
	
	void connectInform () {
		sendStartAnouncement ();
	}

protected:
	/**
	  * @brief Saves output module configuration
	  * @return Returns `true` if save was successful. `false` otherwise
	  */
	bool saveConfig ();

	void defaultConfig ();
	void configurePins ();

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
	const char* stateToStr (int state) {
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
	void setTravelTime (int travelTime);
	void callbackUpButton (uint8_t pin, uint8_t event, uint8_t count, uint16_t length);
	void callbackDownButton (uint8_t pin, uint8_t event, uint8_t count, uint16_t length);
	bool sendButtonPress (button_t button, int count);
	int8_t timeToPos (time_t movementTime) {
		return movementTime * 100 / config.fullTravellingTime;
	}
	time_t movementToTime (int8_t movement);
	void sendPosition ();

	bool sendGetTravelTime ();
	bool sendGetPosition ();
	bool sendGetStatus ();
	bool sendCommandResp (const char* command, bool result);
	void processBlindEvent (blindState_t state, int8_t position);

    bool sendStartAnouncement () {
        // You can send a 'hello' message when your node starts. Useful to detect unexpected reboot
        const size_t capacity = JSON_OBJECT_SIZE (10);
        DynamicJsonDocument json (capacity);
        json["status"] = "start";
        json["device"] = CONTROLLER_NAME;
        char version_buf[10];
        snprintf (version_buf, 10, "%d.%d.%d",
                  ENIGMAIOT_PROT_VERS[0], ENIGMAIOT_PROT_VERS[1], ENIGMAIOT_PROT_VERS[2]);
        json["version"] = String (version_buf);

        return sendJson (json);
    }
};

#endif

