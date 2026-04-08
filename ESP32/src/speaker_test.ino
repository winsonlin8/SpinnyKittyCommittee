// Code used to play a fil called song.wav on SD card from speaker

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include "driver/i2s.h"

#define SD_CS_PIN 5

#define I2S_BCLK 26
#define I2S_LRC  25
#define I2S_DOUT 22

File audioFile;

void setupI2S() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = 44100,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = 0,
    .dma_buf_count = 8,
    .dma_buf_len = 256,
    .use_apll = false,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_BCLK,
    .ws_io_num = I2S_LRC,
    .data_out_num = I2S_DOUT,
    .data_in_num = I2S_PIN_NO_CHANGE
  };

  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);
  i2s_zero_dma_buffer(I2S_NUM_0);
}

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

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("Starting...");

  if (!initSDCard()) {
    Serial.println("SD init FAILED");
    while (1);
  }
  Serial.println("SD init OK");

  setupI2S();

  audioFile = SD.open("/song.wav", FILE_READ);
  if (!audioFile) {
    Serial.println("Failed to open /sound.wav");
    while (1);
  }

  Serial.println("Opened /song.wav");

  // Skip 44-byte WAV header
  audioFile.seek(44);
  Serial.println("Starting playback...");
}

void loop() {
  static uint8_t buffer[512];
  size_t bytesRead = audioFile.read(buffer, sizeof(buffer));

  if (bytesRead > 0) {
    size_t bytesWritten = 0;
    i2s_write(I2S_NUM_0, buffer, bytesRead, &bytesWritten, portMAX_DELAY);
  } else {
    Serial.println("Playback finished, restarting...");
    audioFile.seek(44);
  }
}
