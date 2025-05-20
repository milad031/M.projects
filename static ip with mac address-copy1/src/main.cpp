#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>

// تنظیمات AP
IPAddress apIP(192, 168, 10, 1);
IPAddress gateway(192, 168, 10, 1);
IPAddress subnet(255, 255, 255, 0);

struct Device {
  uint8_t mac[6];
  IPAddress allowedIP;
};

Device allowedDevices[] = {
  // مثال: گوشی تو با MAC خاص فقط اجازه داره با IP 192.168.10.85 وصل بشه
  {{0xA4, 0xCF, 0x12, 0x34, 0x56, 0x78}, IPAddress(192, 168, 10, 85)}
};

const int deviceCount = sizeof(allowedDevices) / sizeof(Device);

bool isAuthorized(uint8_t *mac) {
  for (int i = 0; i < deviceCount; i++) {
    bool match = true;
    for (int j = 0; j < 6; j++) {
      if (mac[j] != allowedDevices[i].mac[j]) {
        match = false;
        break;
      }
    }
    if (match) return true;
  }
  return false;
}

void setup() {
  Serial.begin(115200);

  // ساخت AP بدون DHCP
  WiFi.softAPConfig(apIP, gateway, subnet);
  WiFi.softAP("Private-AP", "12345678");
  Serial.println("AP Started: " + WiFi.softAPIP().toString());

  // DHCP خاموشه، فقط دستگاه‌هایی که IP دستی گرفتن می‌تونن وصل شن
}

void loop() {
  wifi_sta_list_t wifi_sta_list;
  tcpip_adapter_sta_list_t adapter_sta_list;
  esp_wifi_ap_get_sta_list(&wifi_sta_list);
  tcpip_adapter_get_sta_list(&wifi_sta_list, &adapter_sta_list);

  for (int i = 0; i < adapter_sta_list.num; i++) {
    uint8_t *mac = adapter_sta_list.sta[i].mac;

    Serial.print("Connected MAC: ");
    for (int j = 0; j < 6; j++) {
      Serial.printf("%02X", mac[j]);
      if (j < 5) Serial.print(":");
    }

    if (isAuthorized(mac)) {
      Serial.println(" → Authorized ✅");
    } else {
      Serial.println(" → Not Authorized ❌");
      // اینجا می‌تونی مثلا device رو block کنی یا ignore
    }
  }

  delay(10000);
}
