/*
 Name:		EnigmaIOT_Blind_Controller.ino
 Created:	5/10/2020 6:13:20 PM
 Author:	gmartin
*/

#if !defined ESP8266 && !defined ESP32
#error Node only supports ESP8266 or ESP32 platform
#endif

#include <Arduino.h>
#include <DebounceEvent.h>
#include <EnigmaIOTjsonController.h>
#include <FailSafe.h>
#include "BlindController.h"

#include <EnigmaIOTNode.h>
#include <espnow_hal.h>
#include <CayenneLPP.h>
#include <ArduinoJson.h>

#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <ESPAsyncTCP.h> // Comment to compile for ESP32
#include <Hash.h>
#elif defined ESP32
#include <WiFi.h>
#include <SPIFFS.h>
//#include <AsyncTCP.h> // Comment to compile for ESP8266
#include <SPIFFS.h>
#include <Update.h>
#include <driver/adc.h>
#include "esp_wifi.h"
#endif
#include <ArduinoJson.h>
#include <Curve25519.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h>
#include <DNSServer.h>
#include <FS.h>

#ifndef LED_BUILTIN
#define LED_BUILTIN 2 // ESP32 boards normally have a LED in GPIO3 or GPIO5
#endif // !LED_BUILTIN

//#define USE_SERIAL // Don't forget to set DEBUG_LEVEL to NONE if serial is disabled
#ifndef USE_SERIAL
#define BLUE_LED 3
#else
#define BLUE_LED LED_BUILTIN
#endif // USE_SERIAL

EnigmaIOTjsonController* controller;

const auto fullTravelTime = 30000;
#define RESET_PIN 13

const time_t BOOT_FLAG_TIMEOUT = 10000; // Time in ms to reset flag
const int MAX_CONSECUTIVE_BOOT = 3; // Number of rapid boot cycles before enabling fail safe mode
const int LED = LED_BUILTIN; // Number of rapid boot cycles before enabling fail safe mode
const int FAILSAFE_RTC_ADDRESS = 0; // If you use RTC memory adjust offset to not overwrite other data

void connectEventHandler () {
    controller->connectInform ();
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
	controller->configManagerStart ();
}

void setup () {

#ifdef USE_SERIAL
	Serial.begin (115200);
	delay (1000);
	Serial.println ();
#endif
    
    FailSafe.checkBoot (MAX_CONSECUTIVE_BOOT, LED, FAILSAFE_RTC_ADDRESS); // Parameters are optional
    if (FailSafe.isActive ()) { // Skip all user setup if fail safe mode is activated
        return;
    }

	controller = (EnigmaIOTjsonController*)new CONTROLLER_CLASS_NAME ();

	EnigmaIOTNode.setLed (BLUE_LED);
	EnigmaIOTNode.setResetPin (RESET_PIN);
	EnigmaIOTNode.onConnected (connectEventHandler);
	EnigmaIOTNode.onDisconnected (disconnectEventHandler);
	EnigmaIOTNode.onDataRx (processRxData);
	EnigmaIOTNode.enableClockSync (false);
	EnigmaIOTNode.onWiFiManagerStarted (wifiManagerStarted);
	EnigmaIOTNode.onWiFiManagerExit (wifiManagerExit);
	EnigmaIOTNode.enableBroadcast ();

	if (!controller->loadConfig ()) {
		DEBUG_WARN ("Error reading config file");
		if (SPIFFS.format ())
			DEBUG_WARN ("SPIFFS Formatted");
	}

	EnigmaIOTNode.begin (&Espnow_hal, NULL, NULL, true, false);

	uint8_t macAddress[ENIGMAIOT_ADDR_LEN];
#ifdef ESP8266
	if (wifi_get_macaddr (STATION_IF, macAddress))
#elif defined ESP32
	if ((esp_wifi_get_mac (WIFI_IF_STA, macAddress) == ESP_OK))
#endif
	{
		EnigmaIOTNode.setNodeAddress (macAddress);
		char macStr[ENIGMAIOT_ADDR_LEN * 3];
		DEBUG_DBG ("Node address set to %s", mac2str (macAddress, macStr));
	} else {
		DEBUG_WARN ("Node address error");
	}

	controller->sendDataCallback (sendUplinkData);
	controller->setup (&EnigmaIOTNode);

	DEBUG_DBG ("END setup");
}

void loop () {
    FailSafe.loop (BOOT_FLAG_TIMEOUT); // Use always this line

    if (FailSafe.isActive ()) { // Skip all user loop code if Fail Safe mode is active
        return;
    }

    controller->loop ();
	EnigmaIOTNode.handle ();
}
