// BlindController.h

#ifndef _ENIGMAIOTJSONCONTROLLER_h
#define _ENIGMAIOTJSONCONTROLLER_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif

#include <EnigmaIOTNode.h>
#include <ArduinoJson.h>


#if defined ESP8266 || defined ESP32
#include <functional>
typedef std::function<bool (const uint8_t* data, size_t len, nodePayloadEncoding_t payloadEncoding)> sendData_cb;
#else
#error This code only supports ESP8266 or ESP32 platforms
#endif

class EnigmaIOTjsonController
{
 protected:
	 sendData_cb sendData;

 public:
	 virtual void begin () = 0;
	 virtual void begin (void* config) = 0;
	 virtual void loop () = 0;
	 virtual bool processRxCommand (
		 const uint8_t* mac, const uint8_t* buffer, uint8_t length, nodeMessageType_t command, nodePayloadEncoding_t payloadEncoding) = 0;
	 void sendDataCallback (sendData_cb cb) {
		 sendData = cb;
	 }

protected:
	virtual bool sendCommandResp (const char* command, bool result) = 0;
	virtual bool sendStartAnouncement () = 0;

	bool sendJson (DynamicJsonDocument& json) {
		int len = measureMsgPack (json) + 1;
		uint8_t* buffer = (uint8_t*)malloc (len);
		len = serializeMsgPack (json, (char*)buffer, len);

		size_t strLen = measureJson (json) + 1;
		char* strBuffer = (char*)calloc (sizeof(uint8_t), strLen);

		/*Serial.printf ("Trying to send: %s\n", printHexBuffer (
			buffer, len));*/
		serializeJson (json, strBuffer, strLen);
		DEBUG_WARN ("Trying to send: %s", strBuffer);
		bool result = false;
		if (sendData)
			result = sendData (buffer, len, MSG_PACK);
		if (!result) {
			DEBUG_WARN ("---- Error sending data");
		} else {
			DEBUG_INFO ("---- Data sent");
		}
		free (buffer);
		free (strBuffer);
		return result;
	}
};

#endif // _ENIGMAIOTJSONCONTROLLER_h

