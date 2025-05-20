#include <Arduino.h>
#include <WiFi.h>


const char* ssid = "Milad";
const char* password = "69894411";


WiFiServer tcpServer(333);

const int ledPin = 12;

void setup() {
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.println(WiFi.localIP());

  tcpServer.begin();
  Serial.println("TCP server started");
}

void loop() {
  WiFiClient client = tcpServer.available();

  if (client) {
    Serial.println("Client connected");
    String command = "";

    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        command += c;

        if (c == '\n') {
          command.trim();

          if (command == "ON") {
            digitalWrite(ledPin, HIGH);
            client.println("LED is ON");
          } else if (command == "OFF") {
            digitalWrite(ledPin, LOW);
            client.println("LED is OFF");
          } else {
            client.println("Unknown command");
          }

          command = "";
        }
      }
    }
    client.stop();
    Serial.println("Client disconnected");
  }
}
