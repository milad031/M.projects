#include <WiFi.h>
#include <time.h>

const char* ssid     = "Milad";
const char* password = "69894411";

// تنظیم NTP برای ایران (UTC+3:30)
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 12600;
const int daylightOffset_sec = 0;

const char* weekDaysFa[] = {
  "یک‌شنبه", "دو‌شنبه", "سه‌شنبه", "چهار‌شنبه", "پنج‌شنبه", "جمعه", "شنبه"
};

const char* persianMonths[] = {
  "فروردین", "اردیبهشت", "خرداد", "تیر", "مرداد", "شهریور",
  "مهر", "آبان", "آذر", "دی", "بهمن", "اسفند"
};

// تابع تبدیل تاریخ میلادی به شمسی
void gregorianToJalali(int gy, int gm, int gd, int &jy, int &jm, int &jd) {
  int g_d_m[] = {0,31,59,90,120,151,181,212,243,273,304,334};
  int gy2 = (gm > 2) ? (gy + 1) : gy;
  long days = 355666 + (365 * gy) + ((gy2 + 3) / 4) - ((gy2 + 99) / 100) + ((gy2 + 399) / 400) + gd + g_d_m[gm - 1];
  jy = -1595 + (33 * (days / 12053)); days %= 12053;
  jy += 4 * (days / 1461); days %= 1461;
  if (days > 365) { jy += (days - 1) / 365; days = (days - 1) % 365; }
  int jm_arr[] = {0,31,62,93,124,155,186,216,246,276,306,336};
  for (jm = 1; jm <= 12 && days >= jm_arr[jm]; jm++);
  jm--;
  jd = days - jm_arr[jm] + 1;
  jm++;
}

String toPersianNumber(int num) {
  String persianDigits[] = {"۰","۱","۲","۳","۴","۵","۶","۷","۸","۹"};
  String result = "";
  String temp = String(num);
  for (int i = 0; i < temp.length(); i++) {
    if (isdigit(temp[i])) {
      result += persianDigits[temp[i] - '0'];
    } else {
      result += temp[i];
    }
  }
  return result;
}

void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected.");

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("در حال دریافت زمان از اینترنت...");
  delay(2000);
}

void loop() {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    int gy = timeinfo.tm_year + 1900;
    int gm = timeinfo.tm_mon + 1;
    int gd = timeinfo.tm_mday;

    int jy, jm, jd;
    gregorianToJalali(gy, gm, gd, jy, jm, jd);

    String weekDay = weekDaysFa[timeinfo.tm_wday];
    String monthFa = persianMonths[jm - 1];

    int hour = timeinfo.tm_hour;
    int minute = timeinfo.tm_min;
    int second = timeinfo.tm_sec;

    String finalString = weekDay + " " +
                         toPersianNumber(jd) + " " +
                         monthFa + " " +
                         toPersianNumber(jy) + " - ساعت " +
                         toPersianNumber(hour) + ":" +
                         toPersianNumber(minute) + ":" +
                         toPersianNumber(second);

    Serial.println(finalString);
  } else {
    Serial.println("زمان در دسترس نیست");
  }

  delay(1000);
}
