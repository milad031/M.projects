#include <Arduino.h>
#include <WiFi.h>

const char* ssid = "Milad";
const char* password = "69894411";

WiFiServer tcpServer(8080);

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
  Serial.println("TCP server started on port 8080");
}

void loop() {
  WiFiClient client = tcpServer.available();
  if (client) {
    Serial.println("Client connected");

    String request = "";
    while (client.connected()) {
      while (client.available()) {
        char c = client.read();
        request += c;
        if (request.indexOf("\r\n\r\n") != -1) break;
      }
      break;
    }

    Serial.println("Request headers:");
    Serial.println(request);

    if (request.startsWith("POST")) {
      int contentLength = 0;
      int idx = request.indexOf("Content-Length:");
      if (idx >= 0) {
        int endIdx = request.indexOf("\r\n", idx);
        String lenStr = request.substring(idx + 15, endIdx);
        lenStr.trim();
        contentLength = lenStr.toInt();
      }

      String body = "";
      while (body.length() < contentLength && client.connected()) {
        while (client.available()) {
          char c = client.read();
          body += c;
        }
      }

      Serial.print("Body: ");
      Serial.println(body);

      String result = "<html><body><h1>Unknown Command</h1></body></html>";
      if (body.indexOf("cmd=ON") >= 0) {
        digitalWrite(ledPin, HIGH);
        result = "<html><body><h1>LED is ON</h1></body></html>";
      } else if (body.indexOf("cmd=OFF") >= 0) {
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
