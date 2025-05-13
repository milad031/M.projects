#include <Arduino.h>
#include "esp_camera.h"
#include <WiFi.h>
#include "SD_MMC.h"
#include "FS.h"
#include "soc/soc.h"           // Disable brownout problems
#include "soc/rtc_cntl_reg.h"    // Disable brownout problems

// مطمئن شو در فایل camera_pins.h تنها این مدل فعال است:
#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"

// تنظیمات AP
const char* ssid = "ESP32_AP";
const char* password = "password";

// متغیرهای وب سرور (در صورت استفاده از Streaming)
httpd_handle_t server = NULL;

// متغیرهای ضبط ویدیو
bool recording = false;    // وضعیت ضبط (روشن/خاموش)
File videoFile;            // شی فایل ضبط ویدیو

// تابع راه‌اندازی وب سرور (در پروژه اصلی وجود داشته باشد)
void startCameraServer();  // فرض کنید این تابع در جای دیگر یا فایل جداگانه پیاده شده است

// تابع ضبط یک فریم به SD کارت
void recordFrameToSD() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    return;
  }
  
  uint8_t *jpg_buf = NULL;
  size_t jpg_buf_len = 0;
  
  // اگر فرمت دوربین JPEG نیست، تبدیل به JPEG انجام می‌شود
  if (fb->format != PIXFORMAT_JPEG) {
    bool jpeg_converted = frame2jpg(fb, 80, &jpg_buf, &jpg_buf_len);
    esp_camera_fb_return(fb);
    if (!jpeg_converted) {
      Serial.println("JPEG compression failed");
      return;
    }
  } else {
    jpg_buf = fb->buf;
    jpg_buf_len = fb->len;
    esp_camera_fb_return(fb);
  }
  
  // اگر فایل ضبط باز است، فریم به آن اضافه می‌شود
  if (videoFile) {
    // افزودن جداکننده اختیاری برای تفکیک فریم‌ها (در صورت نیاز)
    videoFile.write((const uint8_t*)"\r\n--frame\r\n", strlen("\r\n--frame\r\n"));
    
    // افزودن هدرهای اختیاری به هر فریم
    char header[64];
    int headerLen = snprintf(header, sizeof(header),
                             "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n",
                             jpg_buf_len);
    videoFile.write((const uint8_t*)header, headerLen);
    
    // نوشتن داده‌های فریم
    videoFile.write(jpg_buf, jpg_buf_len);
    videoFile.flush();
  }
  
  // آزادسازی حافظه در صورت تخصیص در عملیات تبدیل
  if (fb->format != PIXFORMAT_JPEG) {
    free(jpg_buf);
  }
}

void setup() {
  // غیرفعال کردن ریست ناشی از افت ولتاژ (brownout) برای پایداری
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();
  
  // تنظیمات دوربین
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0     = Y2_GPIO_NUM;
  config.pin_d1     = Y3_GPIO_NUM;
  config.pin_d2     = Y4_GPIO_NUM;
  config.pin_d3     = Y5_GPIO_NUM;
  config.pin_d4     = Y6_GPIO_NUM;
  config.pin_d5     = Y7_GPIO_NUM;
  config.pin_d6     = Y8_GPIO_NUM;
  config.pin_d7     = Y9_GPIO_NUM;
  config.pin_xclk   = XCLK_GPIO_NUM;
  config.pin_pclk   = PCLK_GPIO_NUM;
  config.pin_vsync  = VSYNC_GPIO_NUM;
  config.pin_href   = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn   = PWDN_GPIO_NUM;
  config.pin_reset  = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }
  
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
  
  // تنظیم WiFi به صورت Access Point
  WiFi.softAP(ssid, password);
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());
  
  // راه‌اندازی وب سرور (برای استریم ممکن است بخواهی)
  startCameraServer();
  
  // Mount کردن SD کارت
  if (!SD_MMC.begin()){
    Serial.println("SD Card Mount Failed");
  } else {
    Serial.println("SD Card mounted successfully");
  }
  
  Serial.println("برای شروع/توقف ضبط، در مانیتور سریال حرف 'r' را بزنید.");
}

void loop() {
  // استفاده از ورودی سریال برای شروع یا توقف ضبط
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'r') {
      if (!recording) {
        // شروع ضبط
        Serial.println("شروع ضبط ویدیو روی SD کارت...");
        recording = true;
        // تولید نام فایل منحصر به فرد بر اساس زمان فعلی
        String filename = "/video_" + String(millis()) + ".mjpeg";
        videoFile = SD_MMC.open(filename.c_str(), FILE_WRITE);
        if (!videoFile) {
          Serial.println("خطا در باز کردن فایل برای ضبط");
          recording = false;
        } else {
          Serial.print("فایل ضبط شد: ");
          Serial.println(filename);
        }
      } else {
        // توقف ضبط
        Serial.println("توقف ضبط ویدیو...");
        recording = false;
        if (videoFile) {
          videoFile.close();
          Serial.println("فایل ویدیو بسته شد.");
        }
      }
    }
  }
  
  // اگر در حالت ضبط هستیم، فریم‌ها را به SD کارت بنویس
  if (recording) {
    recordFrameToSD();
    delay(100);   // فاصله تقریبی 100 میلی‌ثانیه بین فریم‌ها (می‌توانید تغییر دهید)
  }
  
  delay(1);
}
