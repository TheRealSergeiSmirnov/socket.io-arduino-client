/*
	socket.io-arduino-client: a Socket.IO client for the Arduino

	Based on Bill Roy's Arduino Socket.IO Client (which is based on 
	Kevin Rohling's Arduino WebSocket Client) with event handling by
	@dantaex

	Copyright 2014 Quentin Pigné

	Permission is hereby granted, free of charge, to any person
	obtaining a copy of this software and associated documentation
	files (the "Software"), to deal in the Software without
	restriction, including without limitation the rights to use,
	copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the
	Software is furnished to do so, subject to the following
	conditions:
	
	The above copyright notice and this permission notice shall be
	included in all copies or substantial portions of the Software.
	
	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
	EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
	OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
	NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
	HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
	WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
	FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
	OTHER DEALINGS IN THE SOFTWARE.
*/

#include "SocketIOClient.h"

//Event handling attributes
HashType <char*, void(*)(EthernetClient client, char* data)> SocketIOClient::hashRawArray[HASH_SIZE];
HashMap  <char*, void(*)(EthernetClient client, char* data)> SocketIOClient::eventHandlers = HashMap<char*, void(*)(EthernetClient client, char* data)>(hashRawArray, HASH_SIZE);

//By default constructor, called when a client is instantiated
SocketIOClient::SocketIOClient() {
	//At the beginning, the number of handled event is null
	nbEvent = 0;	
}

/*
	Connection methods
*/

bool SocketIOClient::connect(char* theHostname, int thePort, char* theResource, char* theNsp) {
	//Ethernet connection as a client, if it fails, socket connection cannot be done
	if(!client.connect(theHostname, thePort)) return false;
	hostname = theHostname;
	port = thePort;
	resource = theResource;
	nsp = theNsp;

	//Send handshake to start the socket connection
	client.print(F("GET /"));
	client.print(resource);
	client.println(F("/1/ HTTP/1.1"));
	client.print(F("Host: "));
	client.print(hostname);
	client.print(F(":"));
	client.println(port);
	client.println(F("Origin: Arduino"));
	client.println();

	//Check for the server's response
	if(!waitForInput()) return false;

	//Check for the "HTTP/1.1 200 OK" response
	readInput();
	if(atoi(&databuffer[9]) != 200) {
		while(client.available()) readInput();
		client.stop();
		return false;
	}

	//Go to the line [sid:transport:timeout:available_transports]
	//corresponding to the last line of the response
	eatHeader(); //Consume the rest of the header
	readInput(); //Consume the next line (empty)

	//Get the SID in the response
	char* iptr = databuffer;
	char* optr = sid;
	while(*iptr && (*iptr != ':') && (optr < &sid[SID_LEN - 2])) *optr++ = *iptr++;
	*optr = 0;

	Serial.print(F("Connected. SID = "));
	Serial.println(sid);

	//Consume the rest of the response
	while(client.available()) readInput();

	//Stop the connection
	client.stop();
	delay(1000);

	//Reconnect on WebSocket connection
	//Serial.print(F("WebSocket Connect... "));
	if(!client.connect(hostname, port)) {
		Serial.print(F("Reconnect failed."));
		return false;
	}
	Serial.println(F("Reconnected."));

	//Construction of the protocol switching request
	client.print(F("GET /"));
	client.print(resource);
	client.print(F("/1/websocket/"));
	client.print(sid);
	client.println(F(" HTTP/1.1"));
	client.print(F("Host: "));
	client.print(hostname);
	client.print(F(":"));
	client.println(port);
	client.println(F("Upgrade: WebSocket"));
	client.println(F("Connection: Upgrade"));
	client.println(F("Sec-WebSocket-Key: x3JJHMbDL1EzLkh9GBhXDw=="));
	client.println(F("Sec-WebSocket-Version: 13"));
	client.println(F("Origin: ArduinoSocketIOClient"));
	client.println();

	//Check for the server's response
	if(!waitForInput()) return false;
	
	Serial.println();

	//Check for the "HTTP/1.1 101 Switching Protocols" response
	readInput();
	if(atoi(&databuffer[9]) != 101) {
		while (client.available()) readInput();
		client.stop();
		return false;
	}

	//Connection to the namespace if any
	if(nsp != "/") {
		client.print((char)129);
		client.print((char)(3 + strlen(nsp)));
		client.print(F("1::"));
		client.print(nsp);
	}

	eatHeader(); //Consume the rest of the header
	monitor(); //Treat the response as input

	return true;
}

//Server unreachable if it takes more than 30sec to answer
bool SocketIOClient::waitForInput() {
	unsigned long now = millis();
	while(!client.available() && ((millis() - now) < 30000UL)) {}
	return client.available();
}

//Consume the header
void SocketIOClient::eatHeader() {
	//Consume lines to the empty line between the end of the header and the beginning of the response body
	while(client.available()) {
		readInput();
		if(strlen(databuffer) == 0) break;
	}
}

/*
	Event handling methods
	Event data format = 5:::{"name":"event_name","args":[]}
*/

//Map an event name and its handler function
void SocketIOClient::setEventHandler(char* eventName,  void (*handler)(EthernetClient client, char* data)) {
	if(nbEvent < HASH_SIZE) eventHandlers[nbEvent++](eventName, handler);
	else Serial.println('Max number of events reached');
}

//Event data emitting method
void SocketIOClient::emit(char* event, char* data) {
	//According to the WebSocket RFC, first byte is 129
	client.print((char)129);
	//Second byte is the length of data
	if(nsp != "/") {
		client.print((char)(27 + strlen(nsp) + strlen(event) + strlen(data)));
	} else {
		client.print((char)(27 + strlen(event) + strlen(data)));
	}
	//Data at the end (socket.io data frame encoded)
	client.print("5::");
	if(nsp != "/") client.print(nsp);
	client.print(":{\"name\":\"");
	client.print(event);
	client.print("\",\"args\":[\"");
	client.print(data);
	client.print("\"]}");
}

/*
	Monitoring for incoming data
	https://github.com/automattic/socket.io-protocol
*/

void SocketIOClient::monitor(){
	*databuffer = 0;

	//If Ethernet client is disconnected, try to reconnect
	if(!client.connected()) {
		if(!client.connect(hostname, port)) return;
	}

	//Stop the method if no data
	if(!client.available()) return;

	while(client.available()) {
		readInput();
		dataptr = databuffer;
		switch(databuffer[0]) {
			case '1': //connect: [1::]
				break;

			case '2': //heartbeat: [2::]
				client.print((char)129);
				client.print((char)3);
				client.print("2::");
				break;

			case '5': //event: [5:::{"name":"event_name","args":[]}]
				//We only use the json part of data
				while(*dataptr != '{') dataptr++;
				//Getting the name of the event
				char* evtnm;
				evtnm = getName(dataptr);
				//Get the event handler function and call it
				void (*evhand)(EthernetClient client, char *data );
				if(eventHandlers.getFunction(evtnm , &evhand)) {
					evhand(client, dataptr);
				}
				break;

			default:
				Serial.print("Drop ");
				Serial.println(dataptr);
		}
	}
}

//Put incoming data's first line into the data buffer
void SocketIOClient::readInput() {
	//dataptr pointer points to the beginning of the buffer
	dataptr = databuffer;
	char c = client.read();
	if(c != -127) {
		while(client.available() && c != '\r') {
			*dataptr++ = c;
			c = client.read();
		}
		client.read();
	} else {
		int length = client.read();
		if(length == 126) {
			length = client.read();
			length = length << 8;
			length = length | client.read();
		}
		for(int i = 0; i < length; i++) {
			c = client.read();
			*dataptr++ = c;
		}
	}
	*dataptr = 0;
}

//Get the name of the incoming event
char* SocketIOClient::getName(char* dataptr) {
	//Copying data not to be modified by json parsing
	char json[strlen(dataptr)];
	strcpy(json, dataptr);
	//Parsing json
	StaticJsonBuffer<200> jsonBuffer;
	JsonObject& jObj = jsonBuffer.parseObject(json);
	//Getting the event name and return it
	const char* name = jObj["name"];
	return (char*)name;
}