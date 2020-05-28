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
#include "EnigmaIOTjsonController.h"
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

#define BLUE_LED LED_BUILTIN

EnigmaIOTjsonController *controller;

struct blindControlerHw_t config = {
    .upRelayPin = 5,
    .downRelayPin = 4,
    .upButton = 0,
    .downButton = 2,
    .fullTravellingTime = 15000,
    .notifPeriod = 15000 / 20,
    .keepAlivePeriod = 15000 * 2,
    .ON_STATE = HIGH
};

void connectEventHandler () {
    Serial.println ("Connected");
}

void disconnectEventHandler (nodeInvalidateReason_t reason) {
    Serial.printf ("Disconnected. Reason %d", reason);
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


void setup() {
    Serial.begin (115200);
    delay (1000);
    Serial.println ();

    controller = (EnigmaIOTjsonController*)new BlindController ();

    EnigmaIOTNode.setLed (BLUE_LED);
    //pinMode (BLUE_LED, OUTPUT);
    //digitalWrite (BLUE_LED, HIGH); // Turn on LED
    EnigmaIOTNode.onConnected (connectEventHandler);
    EnigmaIOTNode.onDisconnected (disconnectEventHandler);
    EnigmaIOTNode.onDataRx (processRxData);

    EnigmaIOTNode.begin (&Espnow_hal, NULL, NULL, true, false);

    controller->sendDataCallback (sendUplinkData);
    controller->begin (&config);
}


void loop() {
    controller->loop ();
    EnigmaIOTNode.handle ();
}
