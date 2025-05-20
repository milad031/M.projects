#include <Arduino.h>
#include <WiFi.h>

const char* ssid = "acer";
const char* password = "12amirmilad12";

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
  Serial.println("\nWiFi connected. IP address: ");
  Serial.println(WiFi.localIP());

  tcpServer.begin();
}

void loop() {
  WiFiClient client = tcpServer.available();
  if (client) {
    Serial.println("Client connected");

    String request = "";
    unsigned long timeout = millis() + 3000;

    while (client.connected() && millis() < timeout) {
      while (client.available()) {
        char c = client.read();
        request += c;
        timeout = millis() + 3000;
        if (request.indexOf("\r\n\r\n") != -1) break;
      }
      if (request.indexOf("\r\n\r\n") != -1) break;
    }

    int headerEnd = request.indexOf("\r\n\r\n");
    String header = request.substring(0, headerEnd);
    String body = request.substring(headerEnd + 4);

    Serial.println("Request headers:");
    Serial.println(header);

    int contentLength = 0;
    int idx = header.indexOf("Content-Length:");
    if (idx >= 0) {
      int endIdx = header.indexOf("\r\n", idx);
      String lenStr = header.substring(idx + 15, endIdx);
      lenStr.trim();
      contentLength = lenStr.toInt();
    }

    timeout = millis() + 3000;
    while (body.length() < contentLength && client.connected() && millis() < timeout) {
      while (client.available()) {
        char c = client.read();
        body += c;
        timeout = millis() + 3000;
      }
      delay(1);
    }

    Serial.print("Body: ");
    Serial.println(body);

    if (header.substring(0,4).equalsIgnoreCase("POST")) {
      String result = "<html><body><h1>han??? :/</h1></body></html>";
      if (body.indexOf("led=on") >= 0) {
        digitalWrite(ledPin, HIGH);
        result = "<html><body><h1>LED is ON</h1></body></html>";
      } else if (body.indexOf("led=off") >= 0) {
        digitalWrite(ledPin, LOW);
        result = "<html><body><h1>LED is OFF</h1></body></html>";
      }

      String response = "HTTP/1.1 200 OK\r\n";
      response += "Content-Type: text/html\r\n";
      response += "Content-Length: " + String(result.length()) + "\r\n";
      response += "Connection: close\r\n\r\n";
      response += result;

      client.print(response);
    } else {
      String response = "HTTP/1.1 405 Method Not Allowed\r\nContent-Length: 0\r\n\r\n";
      client.print(response);
    }

    delay(10);
    client.stop();
    Serial.println("Client disconnected");
  }
}


