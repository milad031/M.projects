#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "Milad";
const char* password = "69894411";

WebServer server(80);

void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected.");
  Serial.print("ESP32 IP: ");
  Serial.println(WiFi.localIP());

  server.on("/ok", HTTP_GET, []() {
    server.send(200, "text/plain", "esp32 is running!");
  });

  server.on("/led", HTTP_POST, []() {
    if (server.hasArg("plain")) {
      String body = server.arg("plain");
      Serial.println("POST body:");
      Serial.println(body);
      server.send(200, "text/plain", "Received: " + body);
    } else {
      server.send(400, "text/plain", "No body received");
    }
  });

  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();
}
