#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>

const char* ssid = "esp32";
const char* password = "12345678";

IPAddress local_IP(192, 168, 52, 7);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

void setup() {
  Serial.begin(115200);

  WiFi.softAPConfig(local_IP, gateway, subnet);

  WiFi.softAP(ssid, password);

  Serial.print("Access Point IP address: ");
  Serial.println(WiFi.softAPIP());
}

void loop() {

}
