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

BlindController blindController;

void connectEventHandler () {
    Serial.println ("Connected");
}

void disconnectEventHandler (nodeInvalidateReason_t reason) {
    Serial.printf ("Disconnected. Reason %d", reason);
}

bool sendJson (DynamicJsonDocument json) {
    int len = measureMsgPack (json) + 1;
    uint8_t* buffer = (uint8_t*)malloc (len);
    len = serializeMsgPack (json, (char*)buffer, len);

    Serial.printf ("Trying to send: %s\n", printHexBuffer (
        buffer, len));
    bool result = EnigmaIOTNode.sendData (buffer, len, MSG_PACK);
    if (!result) {
        DEBUG_WARN ("---- Error sending data");
    } else {
        DEBUG_WARN ("---- Data sent");
    }
    free (buffer);
    return result;
}

void processRxData (const uint8_t* mac, const uint8_t* buffer, uint8_t length, nodeMessageType_t command, nodePayloadEncoding_t payloadEncoding) {
    if (blindController.processRxCommand (mac, buffer, length, command, payloadEncoding)) {
        DEBUG_INFO ("Command processed");
    } else {
        DEBUG_WARN ("Command error");
    }
}


void setup() {
    Serial.begin (115200);
    delay (1000);
    Serial.println ();

    EnigmaIOTNode.setLed (BLUE_LED);
    //pinMode (BLUE_LED, OUTPUT);
    //digitalWrite (BLUE_LED, HIGH); // Turn on LED
    EnigmaIOTNode.onConnected (connectEventHandler);
    EnigmaIOTNode.onDisconnected (disconnectEventHandler);
    EnigmaIOTNode.onDataRx (processRxData);

    EnigmaIOTNode.begin (&Espnow_hal, NULL, NULL, true, false);

    blindController.onJsonSend (sendJson);
    blindController.begin ();

}


void loop() {
    blindController.loop ();
    EnigmaIOTNode.handle ();
}
