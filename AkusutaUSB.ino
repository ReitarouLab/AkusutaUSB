#include <Arduino.h>
#include "USB.h"
#include "USBMSC.h"
#include <SPI.h>
#include <SD.h>

// ===== ピン定義 =====
#define SD_CS   10
#define SD_MOSI 11
#define SD_SCK  12
#define SD_MISO 13

#define DISK_PATH "/disk.img"
#define DISK_SIZE_MB 1024
#define SECTOR_SIZE 512

USBMSC MSC;
File diskFile;

// ===== 読み込み =====
int32_t onRead(uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize) {
  if (!diskFile) return -1;

  uint32_t addr = (lba * SECTOR_SIZE) + offset;
  if (!diskFile.seek(addr)) return -1;

  return diskFile.read((uint8_t*)buffer, bufsize);
}

// ===== 書き込み =====
int32_t onWrite(uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize) {
  if (!diskFile) return -1;

  uint32_t addr = (lba * SECTOR_SIZE) + offset;
  if (!diskFile.seek(addr)) return -1;

  size_t written = diskFile.write(buffer, bufsize);
  return written;
}

// ===== イジェクト処理 =====
bool onStartStop(uint8_t power_condition, bool start, bool load_eject) {
  if (load_eject) {
    Serial.println("Eject detected → flush");
    diskFile.flush();
  }
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("Start");

  // ===== CS安定化 =====
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  delay(10);

  // ===== SPI初期化 =====
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

  // ===== SD初期化（40MHz）=====
  if (!SD.begin(SD_CS, SPI, 40000000)) {
    Serial.println("SD init failed!");
    while (1);
  }
  Serial.println("SD OK");

  // ===== disk.img作成 =====
  if (!SD.exists(DISK_PATH)) {
    Serial.println("Creating disk image...");

    File f = SD.open(DISK_PATH, FILE_WRITE);
    if (!f) {
      Serial.println("Create failed!");
      while (1);
    }

    uint32_t size = DISK_SIZE_MB * 1024 * 1024;

    if (!f.seek(size - 1)) {
      Serial.println("Seek failed!");
      while (1);
    }

    if (f.write((uint8_t)0) != 1) {
      Serial.println("Write failed!");
      while (1);
    }

    f.close();
    Serial.println("disk.img created");
  }

  // ===== 再オープン（重要）=====
  diskFile = SD.open(DISK_PATH, "r+");

  if (!diskFile) {
    Serial.println("disk open failed!");
    while (1);
  }

  uint32_t disk_size = diskFile.size();

  if (disk_size == 0) {
    Serial.println("disk size ERROR!");
    while (1);
  }

  uint32_t block_count = disk_size / SECTOR_SIZE;

  Serial.printf("Disk size: %u bytes\n", disk_size);

  // ===== USB MSC設定 =====
  MSC.vendorID("ESP32");
  MSC.productID("SD_CARD");
  MSC.productRevision("1.0");

  MSC.onRead(onRead);
  MSC.onWrite(onWrite);
  MSC.onStartStop(onStartStop);

  MSC.mediaPresent(true);
  MSC.begin(block_count, SECTOR_SIZE);

  USB.begin();

  Serial.println("USB Ready");
}

void loop() {
}
