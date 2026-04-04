#include <SD.h>
#include <SPI.h>
#include "sd_functions.h"

#define SD_CS_PIN 5

bool initSDCard() {
  if (!SD.begin(SD_CS_PIN)) {
    return false;
  }

  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return false;
  }

  const char* typeStr = "UNKNOWN";
  if      (cardType == CARD_MMC)  typeStr = "MMC";
  else if (cardType == CARD_SD)   typeStr = "SD";
  else if (cardType == CARD_SDHC) typeStr = "SDHC";

  Serial.printf("Card type: %s\n", typeStr);
  Serial.printf("Card size: %llu MB\n", SD.cardSize() / (1024 * 1024));
  return true;
}

bool writeToSD(const char* path, const String& data, bool append) {
  File f = SD.open(path, append ? FILE_APPEND : FILE_WRITE);
  if (!f) {
    Serial.printf("Failed to open %s for writing\n", path);
    return false;
  }
  f.println(data);
  f.close();
  return true;
}

String readFromSD(const char* path) {
  File f = SD.open(path, FILE_READ);
  if (!f) {
    Serial.printf("Failed to open %s for reading\n", path);
    return "";
  }
  String content;
  while (f.available()) {
    content += (char)f.read();
  }
  f.close();
  return content;
}
