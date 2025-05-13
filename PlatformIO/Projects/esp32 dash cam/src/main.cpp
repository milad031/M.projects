#include <Arduino.h>
#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiManager.h>
#include <GyverButton.h>
#include <EEPROM.h>
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_defs.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"
#include <SD_MMC.h>


#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"

#define VERSION					"1.2.1"
#include "definitions.h"
						
// user definitions
#define BTN_REC_PIN				12
#define BTN_IS_TOUCH			0
#define WIFIMGR_TIMEOUT			120
#define MAGIC_NUMBER			77
#define AUTOSTART_REC			1
#define DEFAULT_FRAMESIZE 		FRAMESIZE_SVGA
#define DEVICE_NAME				"SKYNET.EYE001"
#define AVILENGTH				300

static const char devname[] = DEVICE_NAME;
String 		ESP_SSID = "ESP_" + String(WIFI_getChipId(), HEX);
const char* ESP_PWD = "12345678";
uint8_t esp_in_mode = 2;

uint8_t framesize;
uint8_t quality;

bool fl_start_rec = false;
bool fl_recording = false;
bool fl_sd_mounted = false;
bool fl_oldestDirFound = false;

int diskspeed = 0;

String curr_path;
char oldestDir[50];
uint32_t folder_num, file_num;
char fname[100];
char idxfname[100];
FILE *avifile = NULL;
FILE *idxfile = NULL;

#if BTN_IS_TOUCH ==
GButton btnRec(BTN_REC_PIN, HIGH_PULL, NORM_OPEN);
//GButton btnRec(BTN_REC_PIN);
#else
GButton btnRec(BTN_REC_PIN, LOW_PULL, NORM_OPEN);
#endif


// Video
int avi_length = AVILENGTH;
long avi_start_time = 0;
long avi_end_time = 0;
camera_fb_t * fb_curr = NULL;
camera_fb_t * fb_next = NULL;
//camera_fb_t * fb_for_stream = NULL;
//camera_fb_t fb_tmp;
long bp, ap, bw, aw, totalp, totalw;
float avgp, avgw;
int normal_jpg, extend_jpg, bad_jpg;
uint32_t frame_cnt, startms, elapsedms, uVideoLen;
unsigned long movie_size, jpeg_size, idx_offset;

#define BUFFSIZE 512
uint8_t buf[BUFFSIZE];
uint8_t zero_buf[4] = {0x00, 0x00, 0x00, 0x00};
uint8_t dc_buf[4] = {0x30, 0x30, 0x64, 0x63};	// "00dc"
uint8_t idx1_buf[4] = {0x69, 0x64, 0x78, 0x31};	// "idx1"

uint8_t  cif_w[2] = {0x90, 0x01};
uint8_t  cif_h[2] = {0x28, 0x01};
uint8_t  vga_w[2] = {0x80, 0x02};
uint8_t  vga_h[2] = {0xE0, 0x01};
uint8_t svga_w[2] = {0x20, 0x03};
uint8_t svga_h[2] = {0x58, 0x02};
uint8_t  xga_w[2] = {0x00, 0x04};
uint8_t  xga_h[2] = {0x00, 0x03};
uint8_t sxga_w[2] = {0x00, 0x05};
uint8_t sxga_h[2] = {0x00, 0x04};
uint8_t uxga_w[2] = {0x40, 0x06};
uint8_t uxga_h[2] = {0xB0, 0x04}; 

void major_fail() {
  Serial.println(" ");
  for  (int i = 0;  i < 10; i++) {
    for (int j = 0; j < 3; j++) {
      digitalWrite(33, LOW);   delay(150);
      digitalWrite(33, HIGH);  delay(150);
    }
    delay(1000);
    for (int j = 0; j < 3; j++) {
      digitalWrite(33, LOW);  delay(500);
      digitalWrite(33, HIGH); delay(500);
    }
    delay(1000);
    Serial.print("Major Fail  "); Serial.print(i); Serial.print(" / "); Serial.println(10);
  }
  ESP.restart();
}


void delete_oldest() {
  float empty_space_kB = 1.0 * (SD_MMC.totalBytes() - SD_MMC.usedBytes()) / 1024;
  while (empty_space_kB <= avi_length * 375 * 6) {
    Serial.printf("Not enough space. Free %.1f MB of %.1f MB ... Deleting oldest boot.\n", empty_space_kB/1024, 1.0*SD_MMC.totalBytes()/1024/1024);
    if (!fl_oldestDirFound) { 
      int oldest = INT_MAX;
      File f = SD_MMC.open("/");
      if (f.isDirectory()) {
        File file = f.openNextFile();
        while (file) {
          if (file.isDirectory()) {
            char foldname[50];
            strcpy(foldname, file.name());
            foldname[0] = 0x20;
            int i = atoi(foldname);
            if (i < oldest) {
              strcpy (oldestDir, file.name());
              oldest = i;
            }
          }
          file = f.openNextFile();
        }
        fl_oldestDirFound = true;
        Serial.printf("Oldest directory: name is '%s', number is '%d'\n", oldestDir, oldest);
        f.close();
      }
    }
    deleteFileInDir(oldestDir);
    empty_space_kB = 1.0 * (SD_MMC.totalBytes() - SD_MMC.usedBytes()) / 1024;
  }
}


void deleteFileInDir(const char * val) {
  char oldestname[50];
  File f = SD_MMC.open(val);
  if (!f) {
    Serial.printf("Failed to open dir '%s' during deleting", val);
    return;
  }
  if (f.isDirectory()) {
    File file;
    do {
      file = f.openNextFile();
    } while (file.isDirectory());
    if (!file && (strcmp(val, curr_path.c_str()) != 0)) {
      if (SD_MMC.rmdir(val)) {
        fl_oldestDirFound = false;
        Serial.printf("Oldest dir %s is empty. Removed.\n", val);
      }
      else {
        Serial.println("Smth wrong with file deletion...");
      }
      return;
    }

    strcpy(oldestname, file.name());
    file = f.openNextFile();
    while (file) {
      if (!file.isDirectory()) {
        if (strcmp(file.name(), oldestname) < 0) {
            strcpy(oldestname, file.name());
        }
      }
      file = f.openNextFile();
    }
    Serial.printf("File '%s' will be deleted. ", oldestname);
    if (SD_MMC.remove(oldestname)) {
      Serial.println("Deleted.");
    } else {
      Serial.println(" FAILED.");
    }
    f.close();
    //Remove the dir
	if (strcmp(val, curr_path.c_str()) != 0) {
      if (SD_MMC.rmdir(val)) {
        fl_oldestDirFound = false;
        Serial.printf("Oldest dir %s is now empty. Removed\n", val);
      }
      else {
        Serial.println("Dir not removed - there are stiil files here or error");
      }
    }
  }
}


#define WIFICHECK_INTERVAL    30000L
void check_connection() {
  static ulong checkwifi_timeout = 0;
  static ulong current_millis;
  current_millis = millis();
  if (current_millis > checkwifi_timeout) {
    if ( (WiFi.status() != WL_CONNECTED) ) {
      Serial.println("\nWiFi lost. Trying to reconnect...");
	  WiFiManager wifiManager;
      wifiManager.autoConnect( &(ESP_SSID+"_Manager")[0], ESP_PWD );
    }
    checkwifi_timeout = current_millis + WIFICHECK_INTERVAL;
  }
}
void startCameraServer();
static esp_err_t init_sdcard() {
  esp_err_t ret;
  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
    .format_if_mount_failed = false,
    .max_files = 8,
  };
  sdmmc_card_t *card;
  const char mount_point[] = MOUNT_POINT;
  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
  slot_config.width = 1;
  host.flags = SDMMC_HOST_FLAG_1BIT;
  host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;
  diskspeed = host.max_freq_khz;
  
  ret = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &card);
  
  if (ret != ESP_OK) {
    if (ret == ESP_FAIL) {
      Serial.println("Failed to mount SD card filesystem.");
      Serial.println("Do you have an SD Card installed?");
    }
    else {
      Serial.printf("Failed to initialize the card (%s). Make sure SD card lines have pull-up resistors in place.\n", esp_err_to_name(ret));
    }
    return ret;
  }
  Serial.println("SD card properties:");
  sdmmc_card_print_info(stdout, card);
  if (!SD_MMC.begin()) {
    Serial.println("Could not SD_MMC.begin() - folders won't be created");
    //major_fail();
	ret = ESP_FAIL;
  }
  return ret;
}


void buttonsTick() {
  btnRec.tick();
  if (btnRec.isSingle()) {
    fl_start_rec = !fl_start_rec;
	Serial.printf("Rec button pressed. fl_start_rec = %d\n", fl_start_rec);
  }
}


#define AVIOFFSET 240
const int avi_header[AVIOFFSET] PROGMEM = {
  0x52, 0x49, 0x46, 0x46, 0xD8, 0x01, 0x0E, 0x00, 0x41, 0x56, 0x49, 0x20, 0x4C, 0x49, 0x53, 0x54,
  0xD0, 0x00, 0x00, 0x00, 0x68, 0x64, 0x72, 0x6C, 0x61, 0x76, 0x69, 0x68, 0x38, 0x00, 0x00, 0x00,
  0xA0, 0x86, 0x01, 0x00, 0x80, 0x66, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00,
  0x64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x80, 0x02, 0x00, 0x00, 0xe0, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4C, 0x49, 0x53, 0x54, 0x84, 0x00, 0x00, 0x00,
  0x73, 0x74, 0x72, 0x6C, 0x73, 0x74, 0x72, 0x68, 0x30, 0x00, 0x00, 0x00, 0x76, 0x69, 0x64, 0x73,
  0x4D, 0x4A, 0x50, 0x47, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x73, 0x74, 0x72, 0x66,
  0x28, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x80, 0x02, 0x00, 0x00, 0xe0, 0x01, 0x00, 0x00,
  0x01, 0x00, 0x18, 0x00, 0x4D, 0x4A, 0x50, 0x47, 0x00, 0x84, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x49, 0x4E, 0x46, 0x4F,
  0x10, 0x00, 0x00, 0x00, 0x6A, 0x61, 0x6D, 0x65, 0x73, 0x7A, 0x61, 0x68, 0x61, 0x72, 0x79, 0x20,
  0x76, 0x31, 0x30, 0x20, 0x4C, 0x49, 0x53, 0x54, 0x00, 0x01, 0x0E, 0x00, 0x6D, 0x6F, 0x76, 0x69,
};

uint16_t remnant = 0;

static esp_err_t start_avi() {
  Serial.println("Start rec");
  sprintf(fname, MOUNT_POINT"/%d/%s.%d.%03d.avi",  folder_num, devname, folder_num, file_num);
  sprintf(idxfname, MOUNT_POINT"/%d/idx.tmp", folder_num);
  
  file_num++;
  avifile = fopen(fname, "w");
  idxfile = fopen(idxfname, "w");
  if (avifile == NULL) {
    Serial.println("Unable create avifile");
    major_fail();
  }
  if (idxfile == NULL) {
    Serial.println("Unable create idxfile");
    major_fail();
  }

  for (int i = 0; i < AVIOFFSET; i++) {
    char ch = pgm_read_byte(&avi_header[i]);
    buf[i] = ch;
  }

  size_t err = fwrite(buf, 1, AVIOFFSET, avifile);
  if (framesize == 10) {
    fseek(avifile, 0x40, SEEK_SET);
    err = fwrite(uxga_w, 1, 2, avifile);
    fseek(avifile, 0xA8, SEEK_SET);
    err = fwrite(uxga_w, 1, 2, avifile);
    fseek(avifile, 0x44, SEEK_SET);
    err = fwrite(uxga_h, 1, 2, avifile);
    fseek(avifile, 0xAC, SEEK_SET);
    err = fwrite(uxga_h, 1, 2, avifile);
  }
  else if (framesize == 9) {
    fseek(avifile, 0x40, SEEK_SET);
    err = fwrite(sxga_w, 1, 2, avifile);
    fseek(avifile, 0xA8, SEEK_SET);
    err = fwrite(sxga_w, 1, 2, avifile);
    fseek(avifile, 0x44, SEEK_SET);
    err = fwrite(sxga_h, 1, 2, avifile);
    fseek(avifile, 0xAC, SEEK_SET);
    err = fwrite(sxga_h, 1, 2, avifile);
  }
  else if (framesize == 8) {
    fseek(avifile, 0x40, SEEK_SET);
    err = fwrite(xga_w, 1, 2, avifile);
    fseek(avifile, 0xA8, SEEK_SET);
    err = fwrite(xga_w, 1, 2, avifile);
    fseek(avifile, 0x44, SEEK_SET);
    err = fwrite(xga_h, 1, 2, avifile);
    fseek(avifile, 0xAC, SEEK_SET);
    err = fwrite(xga_h, 1, 2, avifile);
  }
  else if (framesize == 7) {
    fseek(avifile, 0x40, SEEK_SET);
    err = fwrite(svga_w, 1, 2, avifile);
    fseek(avifile, 0xA8, SEEK_SET);
    err = fwrite(svga_w, 1, 2, avifile);
    fseek(avifile, 0x44, SEEK_SET);
    err = fwrite(svga_h, 1, 2, avifile);
    fseek(avifile, 0xAC, SEEK_SET);
    err = fwrite(svga_h, 1, 2, avifile);
  }
  else if (framesize == 6) {
    fseek(avifile, 0x40, SEEK_SET);
    err = fwrite(vga_w, 1, 2, avifile);
    fseek(avifile, 0xA8, SEEK_SET);
    err = fwrite(vga_w, 1, 2, avifile);
    fseek(avifile, 0x44, SEEK_SET);
    err = fwrite(vga_h, 1, 2, avifile);
    fseek(avifile, 0xAC, SEEK_SET);
    err = fwrite(vga_h, 1, 2, avifile);
  }
  else if (framesize == 5) {
    fseek(avifile, 0x40, SEEK_SET);
    err = fwrite(cif_w, 1, 2, avifile);
    fseek(avifile, 0xA8, SEEK_SET);
    err = fwrite(cif_w, 1, 2, avifile);
    fseek(avifile, 0x44, SEEK_SET);
    err = fwrite(cif_h, 1, 2, avifile);
    fseek(avifile, 0xAC, SEEK_SET);
    err = fwrite(cif_h, 1, 2, avifile);
  }

  fseek(avifile, AVIOFFSET, SEEK_SET);

  startms = millis();
  totalp = 0;
  totalw = 0;
  jpeg_size = 0;
  movie_size = 0;
  uVideoLen = 0;
  idx_offset = 4;
  bad_jpg = 0;
  extend_jpg = 0;
  normal_jpg = 0;
} // end of start avi


static void inline print_quartet(unsigned long i, FILE * fd) {
  uint8_t y[4];
  y[0] = i % 0x100;
  y[1] = (i >> 8) % 0x100;
  y[2] = (i >> 16) % 0x100;
  y[3] = (i >> 24) % 0x100;
  size_t i1_err = fwrite(y , 1, 4, fd);
}


static void inline print_2quartet(unsigned long i, unsigned long j, FILE * fd) {
  uint8_t y[8];
  y[0] = i % 0x100;
  y[1] = (i >> 8) % 0x100;
  y[2] = (i >> 16) % 0x100;
  y[3] = (i >> 24) % 0x100;
  y[4] = j % 0x100;
  y[5] = (j >> 8) % 0x100;
  y[6] = (j >> 16) % 0x100;
  y[7] = (j >> 24) % 0x100;
  size_t i1_err = fwrite(y , 1, 8, fd);
}


uint8_t framebuffer_static[64 * 1024 + 20];

static esp_err_t another_save_avi(camera_fb_t * fb ) {
  int fblen;
  fblen = fb->len;
  
  int fb_block_length;
  uint8_t* fb_block_start;
  
  jpeg_size = fblen;
  movie_size += jpeg_size;
  uVideoLen += jpeg_size;
  
  remnant = (4 - (jpeg_size & 0x00000003)) & 0x00000003;
  
  bw = millis();
  long frame_write_start = millis();
  
  framebuffer_static[3] = 0x63;
  framebuffer_static[2] = 0x64;
  framebuffer_static[1] = 0x30;
  framebuffer_static[0] = 0x30;
  
  int jpeg_size_rem = jpeg_size + remnant;

  framebuffer_static[4] = jpeg_size_rem % 0x100;
  framebuffer_static[5] = (jpeg_size_rem >> 8) % 0x100;
  framebuffer_static[6] = (jpeg_size_rem >> 16) % 0x100;
  framebuffer_static[7] = (jpeg_size_rem >> 24) % 0x100;
  
  fb_block_start = fb->buf;
  
  if (fblen > 64 * 1024 - 8 ) {
    fb_block_length = 64 * 1024;
    fblen = fblen - (64 * 1024 - 8);
    memcpy(framebuffer_static + 8, fb_block_start, fb_block_length - 8);
    fb_block_start = fb_block_start + fb_block_length - 8;
  }
  else {
    fb_block_length = fblen + 8  + remnant;
    memcpy(framebuffer_static + 8, fb_block_start,  fblen);
    fblen = 0;
  }

  size_t err = fwrite(framebuffer_static, 1, fb_block_length, avifile);
  if (err != fb_block_length) {
    Serial.print("Error on avi write: err = "); Serial.print(err);
    Serial.print(" len = "); Serial.println(fb_block_length);
  }

  while (fblen > 0) {
    if (fblen > 64 * 1024) {
      fb_block_length = 64 * 1024;
      fblen = fblen - fb_block_length;
    }
    else {
      fb_block_length = fblen  + remnant;
      fblen = 0;
    }

    memcpy(framebuffer_static, fb_block_start, fb_block_length);

    size_t err = fwrite(framebuffer_static, 1, fb_block_length, avifile);
    if (err != fb_block_length) {
      Serial.print("Error on avi write: err = "); Serial.print(err);
      Serial.print(" len = "); Serial.println(fb_block_length);
    }
    fb_block_start = fb_block_start + fb_block_length;
  }
  
  long frame_write_end = millis();
  print_2quartet(idx_offset, jpeg_size, idxfile);
  idx_offset = idx_offset + jpeg_size + remnant + 8;
  movie_size = movie_size + remnant;
  totalw = totalw + millis() - bw;
} // end of another_pic_avi


static esp_err_t end_avi() {
  unsigned long current_end = 0;
  current_end = ftell (avifile);
  
  if (frame_cnt <  10 ) {
    Serial.println("Recording screwed up, less than 10 frames, forget index\n");
    fclose(idxfile);
    fclose(avifile);
	int xx = remove(idxfname);
    int yy = remove(fname);
  }
  else {
    elapsedms = millis() - startms;
	float fRealFPS = (1000.0f * (float)frame_cnt) / ((float)elapsedms);
    float fmicroseconds_per_frame = 1000000.0f / fRealFPS;
    uint8_t iAttainedFPS = round(fRealFPS);
    uint32_t us_per_frame = round(fmicroseconds_per_frame);
    fseek(avifile, 4 , SEEK_SET);
    print_quartet(movie_size + 240 + 16 * frame_cnt + 8 * frame_cnt, avifile);

    fseek(avifile, 0x20 , SEEK_SET);
    print_quartet(us_per_frame, avifile);

    unsigned long max_bytes_per_sec = movie_size * iAttainedFPS / frame_cnt;

    fseek(avifile, 0x24 , SEEK_SET);
    print_quartet(max_bytes_per_sec, avifile);

    fseek(avifile, 0x30 , SEEK_SET);
    print_quartet(frame_cnt, avifile);

    fseek(avifile, 0x8c , SEEK_SET);
    print_quartet(frame_cnt, avifile);

    fseek(avifile, 0x84 , SEEK_SET);
    print_quartet((int)iAttainedFPS, avifile);

    fseek(avifile, 0xe8 , SEEK_SET);
    print_quartet(movie_size + frame_cnt * 8 + 4, avifile);

    Serial.printf("Writng the index, %d frames\n", frame_cnt);
    fseek(avifile, current_end, SEEK_SET);

    fclose(idxfile);

    size_t i1_err = fwrite(idx1_buf, 1, 4, avifile);

    print_quartet(frame_cnt * 16, avifile);

    idxfile = fopen(idxfname, "r");

    if (idxfile == NULL) {
      Serial.println("Could not open index file");
      major_fail();
    }

    char * AteBytes;
    AteBytes = (char*) malloc (8);

    for (int i = 0; i < frame_cnt; i++) {
      size_t res = fread ( AteBytes, 1, 8, idxfile);
      size_t i1_err = fwrite(dc_buf, 1, 4, avifile);
      size_t i2_err = fwrite(zero_buf, 1, 4, avifile);
      size_t i3_err = fwrite(AteBytes, 1, 8, avifile);
    }

    free(AteBytes);

    fclose(idxfile);
    fclose(avifile);
    //int xx = remove((curr_path + "idx.tmp").c_str());
    int xx = remove(idxfname);
  }
}

camera_fb_t *  get_good_jpeg() {
  camera_fb_t * fb;
  do {
    bp = millis();
    fb = esp_camera_fb_get();
    totalp = totalp + millis() - bp;

    int x = fb->len;
    int foundffd9 = 0;

    for (int j = 1; j <= 1025; j++) {
      if (fb->buf[x - j] != 0xD9) {
      }
      else {
        if (fb->buf[x - j - 1] == 0xFF ) {
          if (j == 1) {
            normal_jpg++;
          }
          else {
            extend_jpg++;
          }
          if (j > 1000) {
            Serial.print("Frame "); Serial.print(frame_cnt);
            Serial.print(", Len = "); Serial.print(x);
            Serial.print(", Correct Len = "); Serial.print(x - j + 1);
            Serial.print(", Extra Bytes = "); Serial.println( j - 1);
          }
          foundffd9 = 1;
          break;
        }
      }
    }

    if (!foundffd9) {
      bad_jpg++;
      Serial.print("Bad jpeg, Len = "); Serial.println(x);
      esp_camera_fb_return(fb);
    }
    else {
      break;
    }
  } while (1);
  return fb;
}

uint8_t* framebuffer;
int framebuffer_len;

void setup() {
  Serial.begin(SERIAL_BAUD);
  while (!Serial);
  Serial.setDebugOutput(true);
  Serial.printf("VERSION = %s\n", VERSION);
  
  pinMode(LED_RED, OUTPUT);
  digitalWrite(LED_RED, LED_OFF);
  
  Serial.println("Camera initialization");
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 10000000;
  config.pixel_format = PIXFORMAT_JPEG;
  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    quality = 10;
    config.fb_count = 7;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    quality = 12;
    config.fb_count = 1;
  }

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif
  //}
  
  //{camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
  
  sensor_t * s = esp_camera_sensor_get();
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);
    s->set_brightness(s, 1);
    s->set_saturation(s, -2);
  }
  s->set_framesize(s, DEFAULT_FRAMESIZE);
  framesize = s->status.framesize;

#if defined(CAMERA_MODEL_M5STACK_WIDE)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif
  Serial.print("WiFi manager initialization:");
  WiFiManager wifiManager;
  wifiManager.setDebugOutput(true);
  wifiManager.setConfigPortalTimeout(WIFIMGR_TIMEOUT);
  IPAddress cameraIP;
  if (wifiManager.getWiFiIsSaved()) {
    Serial.print("Stored: SSID = "); Serial.print(wifiManager.getWiFiSSID(true)); Serial.print(", Pass = "); Serial.println(wifiManager.getWiFiPass(true));
    Serial.println("Trying to connect to this point...");
  }
  
  wifiManager.autoConnect( &(ESP_SSID+"_Manager")[0], ESP_PWD );
  if (WiFi.status() == WL_CONNECTED) {
    esp_in_mode = 0;
    Serial.println("Connected to Wi-Fi network.");
    cameraIP = WiFi.localIP();
    //Serial.println(cameraIP);
  }
  else {
    Serial.print("Hit timeout. Connection to Wi-Fi failed: ");
    Serial.println(WiFi.status());
    Serial.print("Starting soft Access Point...");
    if (WiFi.softAP( &(ESP_SSID+"_AP")[0], ESP_PWD )) {
      Serial.print("AP created. Connect to: ");
	  Serial.println(&(ESP_SSID+"_AP")[0]);
      //Serial.print("Soft-AP IP address: ");
      cameraIP = WiFi.softAPIP();
      //Serial.println(WiFi.softAPIP());
      esp_in_mode = 1;
    }
  }
  //}
  
  //{SD card init
  esp_err_t card_err = init_sdcard();
  if (card_err == ESP_OK) {
    Serial.println("SD card mount successfully.");
	fl_sd_mounted = true;  
  }
  else {
    Serial.printf("SD Card init failed with error 0x%x\n", card_err);
	Serial.println("Video won't be record.");
    //fl_sd_mounted = false;
  }
  EEPROM.begin(EEPROM_SIZE);
  uint8_t tmp;
  folder_num = 0;
  EEPROM.get(0, tmp);
  if (tmp == MAGIC_NUMBER) {
    EEPROM.get(EEPROM_ADDR_FOLDER_NUM, folder_num);
    EEPROM.get(EEPROM_ADDR_FRAMESIZE, framesize);
    EEPROM.get(EEPROM_ADDR_QUALITY, quality);
    //s->set_framesize(s, framesize);
    s->set_framesize(s, (framesize_t)framesize);
    s->set_quality(s, quality);
    Serial.print("Found data in EEPROM. folder_num = "); Serial.println(folder_num);
  }
  else {
    EEPROM.put(0, MAGIC_NUMBER);
    EEPROM.put(EEPROM_ADDR_FOLDER_NUM, folder_num);
    EEPROM.put(EEPROM_ADDR_FRAMESIZE, framesize);
    EEPROM.put(EEPROM_ADDR_QUALITY, quality);
  }

  folder_num++;
  file_num = 1;
  EEPROM.put(EEPROM_ADDR_FOLDER_NUM, folder_num);
  EEPROM.end();
  //} EEPROM
  
  if (fl_sd_mounted) {
    char tmpstr[13];
    sprintf(tmpstr, "/%d", folder_num);
    curr_path = "";
    curr_path += String(tmpstr);
	Serial.printf("Creating dir: '%s' as current path... ", curr_path);
    if (SD_MMC.mkdir(curr_path.c_str())) {
      Serial.println("Dir created");
    }
    else {
      Serial.println("Failed to creat dir.");
    }

    if (AUTOSTART_REC) fl_start_rec = true;
  }
  
  startCameraServer();
  Serial.print("Camera Ready! Use 'http://");
  Serial.print(cameraIP);
  Serial.println("' to connect");
  
  framebuffer = (uint8_t*)ps_malloc(512 * 1024);
  frame_cnt = 0;
}

void loop() {
  buttonsTick();
  if (fl_sd_mounted) {
    //frame_cnt = frame_cnt + 1;
    if      (!fl_recording &&  fl_start_rec) {	//start a movie
      avi_start_time = millis();
	  //framesize = new_framesize;
	  //quality = new_quality;
      Serial.println("------------------------------------------------");
      Serial.printf("Start the file %d.%03d.avi\n",  folder_num, file_num);
      Serial.printf("Framesize %d, quality %d, length %d seconds\n", framesize, quality, avi_length);
      
      frame_cnt += 1;
      fb_curr = get_good_jpeg();				// should take zero time
      delete_oldest();
      start_avi();
      fl_recording = true;
      fb_next = get_good_jpeg();				// should take nearly zero time due to time spent writing header
      /*if (fb_for_stream == NULL) {
        memcpy(&fb_tmp, fb_next, sizeof *fb_next);	//fb_tmp = *fb_next;
        fb_for_stream = &fb_tmp;
      } */
      another_save_avi( fb_curr);				// put first frame in avi
      esp_camera_fb_return(fb_curr);			// get rid of first frame
      fb_curr = NULL;
    }
    else if ( (fl_recording && !fl_start_rec) || (fl_recording && (millis() > (avi_start_time+avi_length*1000))) ) { // stop a movie
      Serial.println("Stop rec  - closing the files");
      frame_cnt += 1;
      fb_curr = fb_next;
      fb_next = NULL;
      another_save_avi(fb_curr);		// save final frame of avi
      esp_camera_fb_return(fb_curr);
      fb_curr = NULL;
      
      end_avi();						// end the movie
      fl_recording = false;
      digitalWrite(LED_RED, LED_OFF);
      avi_end_time = millis();
	  float fps = frame_cnt /  (1.0*(avi_end_time - avi_start_time) /1000);
      Serial.printf("It was %d frames, avg. framerate = %.1f fps.\n", frame_cnt, fps);
      Serial.println();
      frame_cnt = 0;					// start recording again on the next request
    }
    else if ( fl_recording &&  fl_start_rec) {  // another frame of the avi
      frame_cnt += 1;
      if (frame_cnt % 24 == 0) { digitalWrite(LED_RED, !digitalRead(LED_RED)); }
      fb_curr = fb_next;				// we will write a frame, and get the camera preparing a new one
      fb_next = get_good_jpeg();		// should take near zero, unless the sd is faster than the camera, when we will have to wait for the camera
      /*if (fb_for_stream == NULL) {
        memcpy(&fb_tmp, fb_next, sizeof *fb_next);	//fb_tmp = *fb_next;
        fb_for_stream = &fb_tmp;
      } */
      another_save_avi(fb_curr);
      
      esp_camera_fb_return(fb_curr);
      fb_curr = NULL;
    }
  }

  if (esp_in_mode == 0) check_connection(); //if connected to Wi-Fi point  
}