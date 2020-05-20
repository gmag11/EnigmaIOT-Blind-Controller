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

#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <Curve25519.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h>
#include <ESPAsyncTCP.h>
#include <Hash.h>
#include <DNSServer.h>

#include <SerialCommands.h>

#define BLUE_LED LED_BUILTIN

void cmd_up_cb (SerialCommands* sender);
void cmd_down_cb (SerialCommands* sender);
void cmd_pos_cb (SerialCommands* sender);
void cmd_stop_cb (SerialCommands* sender);
char serial_buffer[32];
SerialCommands commands (&Serial, serial_buffer, sizeof (serial_buffer), "\r\n", " ");
SerialCommand cmd_up ("UU", cmd_up_cb);
SerialCommand cmd_down ("DD", cmd_down_cb);
SerialCommand cmd_pos ("POS", cmd_pos_cb);
SerialCommand cmd_stop ("STOP", cmd_stop_cb);

BlindController blindController;

void connectEventHandler () {
    Serial.println ("Connected");
}

void disconnectEventHandler (nodeInvalidateReason_t reason) {
    Serial.printf ("Disconnected. Reason %d", reason);
}

void processRxData (const uint8_t* mac, const uint8_t* buffer, uint8_t length, nodeMessageType_t command, nodePayloadEncoding_t payloadEncoding) {
    // TODO
}

void cmd_unrecognized (SerialCommands* sender, const char* cmd) {
    sender->GetSerial ()->printf ("Unrecognized command [%s]\n", cmd);
}

void cmd_up_cb (SerialCommands* sender) {
    sender->GetSerial ()->println ("UP COMMAND");
    blindController.fullRollup ();
}

void cmd_down_cb (SerialCommands* sender) {
    sender->GetSerial ()->println ("DOWN COMMAND");
    blindController.fullRolldown ();
}

void cmd_pos_cb (SerialCommands* sender) {
    char* pos_str = sender->Next ();
    if (pos_str == NULL) {
        sender->GetSerial ()->println ("ERROR NO POSITION");
        return;
    }
    int pos = atoi (pos_str);
    if (pos > 100 || pos < 0) {
        sender->GetSerial ()->printf ("WRONG POSITION %d\n", pos);
        return;
    }
    sender->GetSerial ()->printf ("POS COMMAND: %d\n", pos);
    blindController.gotoPosition (pos);
}

void cmd_stop_cb (SerialCommands* sender) {
    sender->GetSerial ()->println ("STOP COMMAND");
    blindController.requestStop ();
}


void processEvent (blindState_t state, int8_t position) {
    Serial.printf ("State: %s. Position %d\n", blindController.stateToStr (state), position);

    const size_t capacity = JSON_OBJECT_SIZE (5);
    DynamicJsonDocument json (capacity);

    json["state"] = (int)state;
    json["pos"] = position;

    int len = measureMsgPack (json) + 1;
    uint8_t* buffer = (uint8_t*)malloc (len);
    len = serializeMsgPack (json, (char*)buffer, len);

    Serial.printf ("Trying to send: %s\n", printHexBuffer (
        buffer, len));

    if (!EnigmaIOTNode.sendData (buffer, len, MSG_PACK)) {
        Serial.println ("---- Error sending data");
    } else {
        Serial.println ("---- Data sent");
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

    blindController.setEventManager (processEvent);
    blindController.begin ();

    commands.SetDefaultHandler (cmd_unrecognized);
    commands.AddCommand (&cmd_up);
    commands.AddCommand (&cmd_down);
    commands.AddCommand (&cmd_pos);
    commands.AddCommand (&cmd_stop);
}


void loop() {
    commands.ReadSerial ();
    blindController.loop ();
    EnigmaIOTNode.handle ();
}
