#define LED_PIN 12
void setupServer() {
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
        for (int i = 0; i < 5; i++) {
          digitalWrite(LED_PIN, HIGH);
          delay(300);
          digitalWrite(LED_PIN, LOW);
          delay(300);
        }
        server.send(200, "text/plain", "LED blinked");
      } else {
        server.send(400, "text/plain", "Unknown command");
      }

      Serial.println("Body: " + body);
    } else {
      server.send(400, "text/plain", "No body received");
    }
  });
}