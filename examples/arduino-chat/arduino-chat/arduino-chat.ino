#include <SPI.h>
#include <Ethernet.h>

#include "SocketIOClient.h"

SocketIOClient client;

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
char hostname[] = "";

// Socket.io "chat_message" event handler
void chat_message(EthernetClient ethclient, char *data ){
  Serial.print("Message : ");
  Serial.println(data);
}

void setup() {
  Serial.begin(9600);
  Ethernet.begin(mac);
  Serial.print("Arduino is on ");
  Serial.println(Ethernet.localIP());

  if(client.connect(hostname, 3000, "socket.io", "/chat_room")) {
    Serial.println("Socket.IO connected !");
  } else {
    Serial.println("Socket.IO not connected.");
  }

  //Event hanlders
  client.setEventHandler("chat_message",  chat_message);

  //Say hello! to the server
  client.emit("chat_message", "Arduino here, hello!");
}

void loop() {
  client.monitor();
}
