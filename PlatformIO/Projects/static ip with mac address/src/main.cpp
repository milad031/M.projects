#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
extern "C" {
  #include "tcpip_adapter.h"
}

IPAddress apIP(192, 168, 10, 1);
IPAddress gateway(192, 168, 10, 1);
IPAddress subnet(255, 255, 255, 0);

struct Device {
  uint8_t mac[6];
  IPAddress allowedIP;
};

Device allowedDevices[] = {
  {{0x16, 0x01, 0x3F, 0xCD, 0xFF, 0xAC}, IPAddress(192, 168, 10, 85)},
  {{0xc6, 0xb8, 0x10, 0x03, 0xac, 0xa7}, IPAddress(192, 168, 10, 86)}
};
const int deviceCount = sizeof(allowedDevices) / sizeof(Device);

bool isAuthorized(const uint8_t *mac) {
  for (int i = 0; i < deviceCount; i++) {
    if (memcmp(mac, allowedDevices[i].mac, 6) == 0) {
      return true;
    }
  }
  return false;
}

void blockDevice(const uint8_t *staMac) {
  uint8_t apMac[6];
  WiFi.softAPmacAddress(apMac);

  uint8_t deauthFrame[26] = {
    0xC0, 0x00,
    0x00, 0x00,
    staMac[0], staMac[1], staMac[2], staMac[3], staMac[4], staMac[5],
    apMac[0], apMac[1], apMac[2], apMac[3], apMac[4], apMac[5],
    apMac[0], apMac[1], apMac[2], apMac[3], apMac[4], apMac[5],
    0x00, 0x00,
    0x07, 0x00
  };
  esp_wifi_80211_tx(WIFI_IF_AP, deauthFrame, sizeof(deauthFrame), true);
  Serial.println("→ ip dastgah eshtebah");
}

void setup() {
  Serial.begin(115200);

  WiFi.softAPConfig(apIP, gateway, subnet);
  WiFi.softAP("ESP32_STATIC_AP", "11111111", 1, 0);
  tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP);

  Serial.println("AP Started (no DHCP): " + WiFi.softAPIP().toString());
}

void loop() {
  wifi_sta_list_t staList;
  esp_wifi_ap_get_sta_list(&staList);

  for (int i = 0; i < staList.num; i++) {
    const uint8_t *mac = staList.sta[i].mac;
    Serial.print("MAC motasel shode: ");
    for (int j = 0; j < 6; j++) {
      Serial.printf("%02X", mac[j]);
      if (j < 5) Serial.print(":");
    }
    if (isAuthorized(mac)) {
      Serial.println(" → mojaz");
    } else {
      Serial.println(" → gheir mojaz");
      blockDevice(mac);
    }
  }
  delay(10000);
}
