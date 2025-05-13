#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <server.h>
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

};
void loop() {
  server.handleClient();
}