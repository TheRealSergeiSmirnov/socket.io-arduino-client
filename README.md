# Socket.IO Arduino Client
### An Arduino client for connecting and messaging with Socket.io servers

Based on Bill Roy's Arduino Socket.IO Client (which is based on Kevin Rohling's Arduino WebSocket Client) with event handling by @dantaex. Kevin's, Bill's and Dan's documentation is reproduced hereinafter, with changes as needed.

Along the way, all uses of the String class were replaced with fixed char buffers so as to use less memory.

## Caveats

This library doesn't support every inch of the Websocket specifications, most notably the use of a Sec-Websocket-Key. Also, because Arduino doesn't support SSL, this library also doesn't support the use of Websockets over https. If you're interested in learning more about the Websocket specifications, I recommend checking out the [Wikipedia Page](http://en.wikipedia.org/wiki/WebSocket). Now that I've got that out of the way, I've been able to successfully use this to connect to several hosted Websocket services, including: [echo.websocket.org](http://websocket.org/echo.html) and [pusherapp.com](http://pusherapp.com).

## Installation instructions

Clone this repo into your Arduino Sketchbook directory under libraries, then restart the Arduino IDE so that it notices the new library.  Now, under File\Examples you should see SocketIOClient.

## How to use this library

```
EthernetClient ethClient;
SocketIOClient client(ethClient);
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
char hostname[] = "148.XXX.XX.XX";
int port = 3000;

// Socket.io "hello" EVENT handler
void hello(Client& client, char* data) {
    Serial.print("[hello] event happening: ");
    Serial.println(data);
    client.emit("goodbye", "Arduino here, goodbye!");
}

// Socket.io "goodbye" EVENT handler
void goodbye(Client& client, char* data) {
    Serial.print("[goodbye] event happening: ");
    Serial.println(data);
    Serial.println("That is all.");
}

void setup() {
    Serial.begin(9600);
    Ethernet.begin(mac);

    if(!client.connect(hostname, port)) {
        Serial.println(F("Not connected."));
    }

    //Event hanlders
    client.setEventHandler("hello", hello);
    client.setEventHandler("goodbye", goodbye);

    //Say hello to the server
    client.emit("hello", "Arduino here, hello!");
}

void loop() {
    client.monitor();
}

```