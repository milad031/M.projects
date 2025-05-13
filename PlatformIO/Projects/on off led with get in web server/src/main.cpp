#include <Arduino.h>
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
    body.trim();

    if (body == "on") {
      digitalWrite(LED_PIN, HIGH);
      server.send(200, "text/plain", "LED turned ON");
    } else if (body == "off") {
      digitalWrite(LED_PIN, LOW);
      server.send(200, "text/plain", "LED turned OFF");
    } else if (body == "blink") {
      for (int i = 0; i < 10; i++) {
        digitalWrite(LED_PIN, HIGH);
        delay(300);
        digitalWrite(LED_PIN, LOW);
        delay(300);
      }
      server.send(200, "text/plain", "LED blinked 5 times");
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