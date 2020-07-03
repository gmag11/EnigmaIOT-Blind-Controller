/*
 Name:		EnigmaIOT_Blind_Controller.ino
 Created:	5/10/2020 6:13:20 PM
 Author:	gmartin
*/

#ifndef ESP8266
#error Node only supports ESP8266 platform
#endif

#include <Arduino.h>
#include <DebounceEvent.h>
#include <EnigmaIOTjsonController.h>
#include "BlindController.h"

#include <EnigmaIOTNode.h>
#include <espnow_hal.h>
#include <CayenneLPP.h>
#include <ArduinoJson.h>

#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <Curve25519.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h>
#include <ESPAsyncTCP.h>
#include <Hash.h>
#include <DNSServer.h>

#define USE_SERIAL // Don't forget to set DEBUG_LEVEL to NONE if serial is disabled
#ifndef USE_SERIAL
#define BLUE_LED 3
#else
#define BLUE_LED LED_BUILTIN
#endif // USE_SERIAL

EnigmaIOTjsonController* controller;

const auto fullTravelTime = 30000;
#define RESET_PIN 13

void connectEventHandler () {
	DEBUG_WARN ("Connected");
}

void disconnectEventHandler (nodeInvalidateReason_t reason) {
	DEBUG_WARN ("Disconnected. Reason %d", reason);
}

bool sendUplinkData (const uint8_t* data, size_t len, nodePayloadEncoding_t payloadEncoding) {
	return EnigmaIOTNode.sendData (data, len, payloadEncoding);
}

void processRxData (const uint8_t* mac, const uint8_t* buffer, uint8_t length, nodeMessageType_t command, nodePayloadEncoding_t payloadEncoding) {
	if (controller->processRxCommand (mac, buffer, length, command, payloadEncoding)) {
		DEBUG_INFO ("Command processed");
	} else {
		DEBUG_WARN ("Command error");
	}
}

void wifiManagerExit (boolean status) {
	controller->configManagerExit (status);
}

void wifiManagerStarted () {
	controller->configManagerStart (&EnigmaIOTNode);
}

void setup () {

#ifdef USE_SERIAL
	Serial.begin (115200);
	delay (1000);
	Serial.println ();
#endif

	controller = (EnigmaIOTjsonController*)new BlindController ();

	EnigmaIOTNode.setLed (BLUE_LED);
	EnigmaIOTNode.setResetPin (RESET_PIN);
	EnigmaIOTNode.onConnected (connectEventHandler);
	EnigmaIOTNode.onDisconnected (disconnectEventHandler);
	EnigmaIOTNode.onDataRx (processRxData);
	EnigmaIOTNode.enableClockSync (false);
	EnigmaIOTNode.onWiFiManagerStarted (wifiManagerStarted);
	EnigmaIOTNode.onWiFiManagerExit (wifiManagerExit);

	if (!controller->loadConfig ()) {
		DEBUG_WARN ("Error reading config file");
		if (SPIFFS.format ())
			DEBUG_WARN ("SPIFFS Formatted");
	}

	EnigmaIOTNode.begin (&Espnow_hal, NULL, NULL, true, false);

	controller->sendDataCallback (sendUplinkData);
	controller->begin ();
	DEBUG_DBG ("END setup");
}

void loop () {
	controller->loop ();
	EnigmaIOTNode.handle ();
}
