#include <Arduino.h>
#include "sd_functions.h"

void setup() {
  Serial.begin(115200);
  delay(2000);
  if (initSDCard()) {
    Serial.println("SD init OK");
  } else {
    Serial.println("SD init FAILED — check wiring and card format (FAT32)");
  }
}

void loop() {
  // code
}