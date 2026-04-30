#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include "driver/i2s.h"

#define SD_CS_PIN 5

// I2S audio output to MAX98357A
#define I2S_BCLK 26
#define I2S_LRC  25
#define I2S_DOUT 27   // moved from 22 to avoid I2C conflict

// I2C to MSPM0
#define I2C_SDA 21
#define I2C_SCL 22
#define MSP_ADDR 0x48

// WAV assumptions
#define AUDIO_SAMPLE_RATE 44100
#define AUDIO_BITS_PER_SAMPLE 16
#define AUDIO_CHANNELS 1
#define WAV_HEADER_SIZE 44
#define AUDIO_BYTES_PER_SECOND ((AUDIO_SAMPLE_RATE * AUDIO_BITS_PER_SAMPLE / 8) * AUDIO_CHANNELS)

File audioFile;
File ctrlFile;

struct CtrlHeader {
  char magic[4];
  uint32_t version;
  uint32_t frameCount;
  uint32_t frameDtUs;
};

struct ControlFrame {
  uint8_t motor;
  uint8_t pos;
  uint8_t bright;
  uint8_t tail_len;
  uint8_t sparkle;
};

CtrlHeader ctrlHeader;
uint32_t currentFrameIndex = 0;
uint64_t nextFrameTimeUs = 0;
uint64_t audioBytesSent = 0;

void setupI2S() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = AUDIO_SAMPLE_RATE,
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

bool openAudioFile() {
  audioFile = SD.open("/song.wav", FILE_READ);
  if (!audioFile) {
    Serial.println("Failed to open /song.wav");
    return false;
  }

  audioFile.seek(WAV_HEADER_SIZE);
  audioBytesSent = 0;
  Serial.println("Opened /song.wav");
  return true;
}

bool openControlFile() {
  ctrlFile = SD.open("/song.ctrl", FILE_READ);
  if (!ctrlFile) {
    Serial.println("Failed to open /song.ctrl");
    return false;
  }

  if (ctrlFile.read((uint8_t*)&ctrlHeader, sizeof(ctrlHeader)) != sizeof(ctrlHeader)) {
    Serial.println("Failed to read control header");
    return false;
  }

  if (strncmp(ctrlHeader.magic, "CTRL", 4) != 0) {
    Serial.println("Invalid control file magic");
    return false;
  }

  Serial.println("Opened /song.ctrl");
  Serial.printf("CTRL version: %lu\n", (unsigned long)ctrlHeader.version);
  Serial.printf("CTRL frame count: %lu\n", (unsigned long)ctrlHeader.frameCount);
  Serial.printf("CTRL frame dt (us): %lu\n", (unsigned long)ctrlHeader.frameDtUs);

  currentFrameIndex = 0;
  nextFrameTimeUs = 0;
  return true;
}

bool sendFrameToMSP(const ControlFrame& frame) {
  Wire.beginTransmission(MSP_ADDR);
  Wire.write((const uint8_t*)&frame, sizeof(frame));
  uint8_t err = Wire.endTransmission();

  if (err != 0) {
    Serial.print("I2C write error: ");
    Serial.println(err);
    return false;
  }
  return true;
}

bool readNextControlFrame(ControlFrame& frame) {
  if (currentFrameIndex >= ctrlHeader.frameCount) {
    return false;
  }

  int bytesRead = ctrlFile.read((uint8_t*)&frame, sizeof(frame));
  if (bytesRead != sizeof(frame)) {
    Serial.println("Failed to read control frame");
    return false;
  }

  currentFrameIndex++;
  return true;
}

uint64_t getPlaybackTimeUs() {
  return (audioBytesSent * 1000000ULL) / AUDIO_BYTES_PER_SECOND;
}

void restartPlayback() {
  if (audioFile) audioFile.close();
  if (ctrlFile) ctrlFile.close();

  openAudioFile();
  openControlFile();

  Serial.println("Playback restarted");
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("Starting combined audio + control playback...");

  if (!initSDCard()) {
    Serial.println("SD init FAILED");
    while (1);
  }
  Serial.println("SD init OK");

  Wire.begin(I2C_SDA, I2C_SCL);
  Serial.println("I2C init OK");

  setupI2S();
  Serial.println("I2S init OK");

  if (!openAudioFile()) {
    while (1);
  }

  if (!openControlFile()) {
    while (1);
  }

  Serial.println("Starting playback...");
}

void loop() {
  static uint8_t audioBuffer[512];

  // 1. Stream audio chunk
  size_t bytesRead = audioFile.read(audioBuffer, sizeof(audioBuffer));

  if (bytesRead > 0) {
    size_t bytesWritten = 0;
    i2s_write(I2S_NUM_0, audioBuffer, bytesRead, &bytesWritten, portMAX_DELAY);
    audioBytesSent += bytesWritten;
  } else {
    restartPlayback();
    return;
  }

  // 2. Send any control frames whose time has arrived
  uint64_t playbackTimeUs = getPlaybackTimeUs();

  while (currentFrameIndex < ctrlHeader.frameCount && playbackTimeUs >= nextFrameTimeUs) {
    ControlFrame frame;
    if (!readNextControlFrame(frame)) {
      break;
    }

    sendFrameToMSP(frame);
    nextFrameTimeUs += ctrlHeader.frameDtUs;
  }
}
