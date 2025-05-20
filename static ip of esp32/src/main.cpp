#include <WiFi.h>

const char* ssid = "no andishan";
const char* password = "12amirmilad12";

IPAddress local_IP(192, 168, 1, 184);
IPAddress gateway(0, 0, 0, 0);
IPAddress subnet(255, 255, 255, 0);

void setup() {
  Serial.begin(115200);

  if (!WiFi.config(local_IP, gateway, subnet)) {
    Serial.println("");
  }

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void loop() {
}
