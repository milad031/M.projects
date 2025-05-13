#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "Milad";
const char* password = "69894411";

WebServer server(80);

#define LED_PIN 12

void setup() {
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nConnected! IP:");
  Serial.println(WiFi.localIP());

  server.on("/led", HTTP_POST, []() {
    if (server.hasArg("plain")) {
      String body = server.arg("plain");

      if (body.indexOf("on") != -1) {
        digitalWrite(LED_PIN, HIGH);
        server.send(200, "text/plain", "LED turned ON");
      } else if (body.indexOf("off") != -1) {
        digitalWrite(LED_PIN, LOW);
        server.send(200, "text/plain", "LED turned OFF");
      } else {
        server.send(400, "text/plain", "Unknown command");
      }

      Serial.println("Body: " + body);
    } else {
      server.send(400, "text/plain", "No body received");
    }
  });

  server.begin();
}

void loop() {
  server.handleClient();
}