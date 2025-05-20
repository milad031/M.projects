#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "Milad";
const char* password = "69894411";

WiFiServer tcpServer(333);     
WebServer httpServer(80);      

const int ledPin = 12;

void handleRoot() {
  String html = "<html><body><h1>ESP32 LED Control</h1>"
                "<p><a href=\"/on\">Turn ON</a></p>"
                "<p><a href=\"/off\">Turn OFF</a></p>"
                "</body></html>";
  httpServer.send(200, "text/html", html);
}

void handleOn() {
  digitalWrite(ledPin, HIGH);
  httpServer.send(200, "text/html", "<h1>LED is ON</h1><a href=\"/\">Back</a>");
}

void handleOff() {
  digitalWrite(ledPin, LOW);
  httpServer.send(200, "text/html", "<h1>LED is OFF</h1><a href=\"/\">Back</a>");
}

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

  httpServer.on("/", handleRoot);
  httpServer.on("/on", handleOn);
  httpServer.on("/off", handleOff);
  httpServer.begin();
  Serial.println("HTTP server started on port 80");
}

void loop() {
  httpServer.handleClient();

  WiFiClient client = tcpServer.available();
  if (client) {
    Serial.println("Client connected to TCP");

    String command = "";
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        command += c;
        if (c == '\n') {
          command.trim();
          if (command.equalsIgnoreCase("ON")) {
            digitalWrite(ledPin, HIGH);
            client.println("LED is ON");
          } else if (command.equalsIgnoreCase("OFF")) {
            digitalWrite(ledPin, LOW);
            client.println("LED is OFF");
          } else {
            client.println("Unknown command");
          }
          command = "";
        }
      }
      delay(10);
    }

    client.stop();
    Serial.println("TCP client disconnected");
  }
}
