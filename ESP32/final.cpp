#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include "driver/i2s.h"


#define SD_CS_PIN 5


#define I2S_BCLK 26
#define I2S_LRC  25
#define I2S_DOUT 27


#define BTN_PP_PIN 33
#define I2C_SDA 21
#define I2C_SCL 22
#define MSP_ADDR 0x48


#define CMD_PLAY  0x01
#define CMD_PAUSE 0x02


File audioFile;
bool isPlaying = false;


/* button debounce */
bool lastStableState = HIGH;
bool lastRawState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelayMs = 30;


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


bool sendCommand(uint8_t cmd) {
 Wire.beginTransmission(MSP_ADDR);
 Wire.write(cmd);
 uint8_t err = Wire.endTransmission();


 Serial.print("Sent command 0x");
 if (cmd < 16) Serial.print("0");
 Serial.print(cmd, HEX);
 Serial.print("  err=");
 Serial.println(err);


 return (err == 0);
}


void handleButton() {
 bool rawState = digitalRead(BTN_PP_PIN);


 if (rawState != lastRawState) {
   lastDebounceTime = millis();
   lastRawState = rawState;
 }


 if ((millis() - lastDebounceTime) > debounceDelayMs) {
   if (rawState != lastStableState) {
     lastStableState = rawState;


     if (lastStableState == LOW) {
       isPlaying = !isPlaying;


       if (isPlaying) {
         Serial.println("BTN_PP PRESSED -> PLAY");
         sendCommand(CMD_PLAY);
       } else {
         Serial.println("BTN_PP PRESSED -> PAUSE");
         sendCommand(CMD_PAUSE);
       }
     }
   }
 }
}


void setup() {
 Serial.begin(115200);
 delay(1000);
 Serial.println("ESP32 audio + button + I2C test");


 pinMode(BTN_PP_PIN, INPUT);   // external PCB pull-up
 Wire.begin(I2C_SDA, I2C_SCL);
 Wire.setClock(100000);


 if (!initSDCard()) {
   Serial.println("SD init FAILED");
   while (1);
 }
 Serial.println("SD init OK");


 setupI2S();


 audioFile = SD.open("/song.wav", FILE_READ);
 if (!audioFile) {
   Serial.println("Failed to open /song.wav");
   while (1);
 }


 audioFile.seek(44);   // skip WAV header
 Serial.println("Opened /song.wav");
 Serial.println("Press button to play/pause");


 isPlaying = false;
}


void loop() {
 handleButton();


 if (!isPlaying) {
   delay(5);
   return;
 }


 static uint8_t buffer[512];
 size_t bytesRead = audioFile.read(buffer, sizeof(buffer));


 if (bytesRead > 0) {
   size_t bytesWritten = 0;
   i2s_write(I2S_NUM_0, buffer, bytesRead, &bytesWritten, portMAX_DELAY);
 } else {
   Serial.println("Playback finished, rewinding");
   audioFile.seek(44);
   isPlaying = false;
   sendCommand(CMD_PAUSE);
 }
}

