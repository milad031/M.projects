#include <Arduino.h>
#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <ESPAsyncWebServer.h>
#include "SD_MMC.h"
#include "FS.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_http_server.h"
#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"
const char* ssid     = "milad";
const char* password = "password";
static bool sd_ok = true;
static bool recording = false;
static File videoFile;
#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";
void startCameraServer();
void recordFrameToSD();

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  delay(100);
  Serial.println("esp32 zabt tasvir");
  if (!SD_MMC.begin()) {
    Serial.println("cart sd shenasaei nashod");
    sd_ok = false;
  } else if (SD_MMC.cardType() == CARD_NONE) {
    Serial.println("edame bedoone cart sd");
    sd_ok = false;
  } else {
    Serial.println("cart sd aamaade");
  }
  camera_config_t config = {};
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  if (psramFound()) {
    config.frame_size   = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count     = 2;
  } else {
    config.frame_size   = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count     = 1;
  }
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("doorbin loud nashod", err);
    while (true) delay(1000);
  }
  sensor_t* s = esp_camera_sensor_get();
  s->set_framesize(s, FRAMESIZE_VGA);
  Serial.println("✅ doorbin amade");

  WiFi.softAP(ssid, password);
  Serial.printf("ap amade", ssid, WiFi.softAPIP().toString().c_str());

  startCameraServer();

  Serial.println("barye zabt tasvir dokme R  ra feshar dahid");
}

void loop() {
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'r') {
      if (!recording) {
        String fn = "/video_" + String(millis()) + ".mjpeg";
        if (sd_ok) {
          videoFile = SD_MMC.open(fn.c_str(), FILE_WRITE);
          if (videoFile) {
            recording = true;
            Serial.printf("📹 Recording to %s\n", fn.c_str());
          } else {
            Serial.println(" ");
          }
        } else {
          Serial.println("sd card amade nist-zabt nashod");
        }
      } else {
        recording = false;
        if (videoFile) {
          videoFile.close();
          Serial.println(" zabt motovaghef");
        }
      }
    }
  }
  static uint32_t last = 0;
  if (recording && millis() - last >= 100) {
    last = millis();
    recordFrameToSD();
  }
}

void recordFrameToSD() {
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("aks gereftan ba moshkel roo be roo shod");
    return;
  }
  uint8_t* buf = nullptr;
  size_t len = 0;
  bool freed = false;

  if (fb->format != PIXFORMAT_JPEG) {
    if (!frame2jpg(fb, 80, &buf, &len)) {
      Serial.println("convert aks ba moshkel roo be roo shod");
      esp_camera_fb_return(fb);
      return;
    }
    esp_camera_fb_return(fb);
    freed = true;
  } else {
    buf = fb->buf;
    len = fb->len;
  }

  // Write MJPEG boundary and header
  videoFile.write((const uint8_t*)_STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
  char hdr[64];
  int hlen = snprintf(hdr, sizeof(hdr), _STREAM_PART, (unsigned)len);
  videoFile.write((uint8_t*)hdr, hlen);
  videoFile.write(buf, len);
  videoFile.flush();

  // Cleanup
  if (freed) free(buf);
  else esp_camera_fb_return(fb);

  Serial.printf("fram ba hajme %u bytes zakhire shod\n", (unsigned)len);
}

void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.max_uri_handlers = 2;
  httpd_handle_t server = NULL;

  httpd_uri_t index_uri = {
    .uri      = "/",
    .method   = HTTP_GET,
    .handler  = [](httpd_req_t* req) -> esp_err_t {
      const char* html = "<html><body><h1>ESP32-CAM Stream</h1><img src=\"/stream\"></body></html>";
      httpd_resp_set_type(req, "text/html");
      return httpd_resp_send(req, html, strlen(html));
    },
    .user_ctx = NULL
  };

  httpd_uri_t stream_uri = {
    .uri      = "/stream",
    .method   = HTTP_GET,
    .handler  = [](httpd_req_t* req) -> esp_err_t {
      camera_fb_t* fb;
      esp_err_t res;
      httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
      while (true) {
        fb = esp_camera_fb_get();
        if (!fb) {
          res = ESP_FAIL;
        } else {
          size_t jpglen;
          const uint8_t* jpgbuf;
          if (fb->format != PIXFORMAT_JPEG) {
            bool ok = frame2jpg(fb, 80, (uint8_t**)&jpgbuf, &jpglen);
            esp_camera_fb_return(fb);
            if (!ok) break;
          } else {
            jpglen = fb->len;
            jpgbuf = fb->buf;
            esp_camera_fb_return(fb);
          }
          if (httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY)) != ESP_OK) break;
          char part2[64];
          int l2 = snprintf(part2, sizeof(part2), _STREAM_PART, jpglen);
          if (httpd_resp_send_chunk(req, part2, l2) != ESP_OK) break;
          if (httpd_resp_send_chunk(req, (const char*)jpgbuf, jpglen) != ESP_OK) break;
        }
      }
      return ESP_OK;
    },
    .user_ctx = NULL
  };

  if (httpd_start(&server, &config) == ESP_OK) {
    httpd_register_uri_handler(server, &index_uri);
    httpd_register_uri_handler(server, &stream_uri);
    Serial.println("web server aamaade");
  } else {
    Serial.println("web server ba khataa roo be roo shod");
  }
}
