#include <Arduino.h>
#include <WiFi.h>

const char* ssid = "Milad";
const char* password = "69894411";

WiFiServer tcpServer(333);

const int ledPin = 12;
bool blinkMode = false;
unsigned long lastBlinkTime = 0;
bool ledState = false;

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
  if (blinkMode) {
    unsigned long currentTime = millis();
    if (currentTime - lastBlinkTime >= 500) {
      ledState = !ledState;
      digitalWrite(ledPin, ledState ? HIGH : LOW);
      lastBlinkTime = currentTime;
    }
  }

  WiFiClient client = tcpServer.available();
  if (client) {
    Serial.println("Client connected");
    String command = "";

    while (client.connected()) {
      while (client.available()) {
        char c = client.read();
        if (c == '\r') continue;
        if (c == '\n') {
          command.trim();
          
          if (command == "ON") {
            blinkMode = false;
            digitalWrite(ledPin, HIGH);
            client.println("LED is ON");
          } else if (command == "OFF") {
            blinkMode = false;
            digitalWrite(ledPin, LOW);
            client.println("LED is OFF");
          } else if (command == "BLINK") {
            blinkMode = true;
            client.println("LED is BLINKING");
          } else {
            client.println("Unknown command");
          }

          command = "";
        } else {
          command += c;
        }
      }
      if (blinkMode) {
        unsigned long currentTime = millis();
        if (currentTime - lastBlinkTime >= 500) {
          ledState = !ledState;
          digitalWrite(ledPin, ledState ? HIGH : LOW);
          lastBlinkTime = currentTime;
        }
      }
    }

    client.stop();
    Serial.println("Client disconnected");
  }
}
