// ESP32 firmware for the Spinning Cat Speaker.
//
// Streams a WAV audio file and a synchronized control file from SD card.
// Audio is sent to the MAX98357A Class D amplifier over I2S. Each 20 ms
// control frame is forwarded to the MSPM0G3507 over I2C so it can update
// the DC motor speed and WS2812 LED animation in time with the music.
//
// SD card files required on the card root:
//   /song.wav   — 44.1 kHz, 16-bit, mono PCM (standard 44-byte header)
//   /song.ctrl  — binary: CtrlHeader followed by N × ControlFrame structs

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include "driver/i2s.h"

// ── Pin assignments ──────────────────────────────────────────────────────────

#define SD_CS_PIN 5       // SPI chip-select for ADA254 microSD breakout

// I2S audio output to MAX98357A
#define I2S_BCLK 26
#define I2S_LRC  25
#define I2S_DOUT 27   // moved from GPIO 22 to avoid conflict with I2C SCL

// I2C to MSPM0G3507
#define I2C_SDA  21
#define I2C_SCL  22
#define MSP_ADDR 0x48   // 7-bit I2C address of the MSPM0 target

// ── Audio format constants ───────────────────────────────────────────────────

#define AUDIO_SAMPLE_RATE      44100
#define AUDIO_BITS_PER_SAMPLE  16
#define AUDIO_CHANNELS         1
#define WAV_HEADER_SIZE        44   // standard RIFF/PCM WAV header is 44 bytes

// Bytes per second of raw PCM audio — used to convert bytes sent → playback time
#define AUDIO_BYTES_PER_SECOND \
    ((AUDIO_SAMPLE_RATE * AUDIO_BITS_PER_SAMPLE / 8) * AUDIO_CHANNELS)

// ── Control file layout ──────────────────────────────────────────────────────

// 16-byte binary header at the start of /song.ctrl
struct CtrlHeader {
    char     magic[4];      // must equal "CTRL"
    uint32_t version;
    uint32_t frameCount;    // total number of ControlFrame entries
    uint32_t frameDtUs;     // microseconds between frames (typically 20000 = 20 ms)
};

// One frame sent to MSPM0 per frameDtUs interval
struct ControlFrame {
    uint8_t motor;      // raw motor PWM target (0-255)
    uint8_t pos;        // LED comet head position (0-34 for a 35-LED ring)
    uint8_t bright;     // LED brightness (0-255)
    uint8_t tail_len;   // comet tail length in pixels (passed through; MSPM0 uses fixed falloff)
    uint8_t sparkle;    // reserved / sparkle intensity (unused in current MSPM0 firmware)
};

// ── Global state ─────────────────────────────────────────────────────────────

File audioFile;
File ctrlFile;

CtrlHeader ctrlHeader;
uint32_t currentFrameIndex = 0;
uint64_t nextFrameTimeUs   = 0;   // playback timestamp for the next pending frame
uint64_t audioBytesSent    = 0;   // running total used to derive playback position

// ── I2S setup ────────────────────────────────────────────────────────────────

// Configure I2S for 44.1 kHz mono transmit; DMA buffers smooth bursts from SD reads
void setupI2S() {
    i2s_config_t i2s_config = {
        .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate          = AUDIO_SAMPLE_RATE,
        .bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format       = I2S_CHANNEL_FMT_ONLY_LEFT,   // mono; MAX98357A bridges both outputs
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags     = 0,
        .dma_buf_count        = 8,
        .dma_buf_len          = 256,
        .use_apll             = false,
        .tx_desc_auto_clear   = true,   // send silence instead of repeating old data on underrun
        .fixed_mclk           = 0
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num   = I2S_BCLK,
        .ws_io_num    = I2S_LRC,
        .data_out_num = I2S_DOUT,
        .data_in_num  = I2S_PIN_NO_CHANGE
    };

    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pin_config);
    i2s_zero_dma_buffer(I2S_NUM_0);
}

// ── SD helpers ───────────────────────────────────────────────────────────────

// Mount the SPI SD card and log card type and size to serial
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

// Seek past the WAV header to the first PCM sample and reset audioBytesSent
bool openAudioFile() {
    audioFile = SD.open("/song.wav", FILE_READ);
    if (!audioFile) {
        Serial.println("Failed to open /song.wav");
        return false;
    }

    // Skip the 44-byte RIFF/PCM header to reach raw PCM samples
    audioFile.seek(WAV_HEADER_SIZE);
    audioBytesSent = 0;
    Serial.println("Opened /song.wav");
    return true;
}

// Validate the CtrlHeader magic and log frame count / interval to serial
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
        Serial.println("Invalid control file magic — regenerate with spinny_kitty_spinning_testing.py");
        return false;
    }

    Serial.printf("CTRL version: %lu  frames: %lu  dt: %lu us\n",
        (unsigned long)ctrlHeader.version,
        (unsigned long)ctrlHeader.frameCount,
        (unsigned long)ctrlHeader.frameDtUs);

    currentFrameIndex = 0;
    nextFrameTimeUs   = 0;
    return true;
}

// ── I2C communication with MSPM0 ─────────────────────────────────────────────

// Transmit one 5-byte ControlFrame to the MSPM0 over I2C; returns false on bus error
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

// Read the next ControlFrame from ctrlFile; returns false at EOF or read error
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

// Convert bytes written to the I2S DMA into microseconds — more accurate than millis()
// because it follows the actual audio pipeline rather than wall-clock time
uint64_t getPlaybackTimeUs() {
    return (audioBytesSent * 1000000ULL) / AUDIO_BYTES_PER_SECOND;
}

// Close and re-open both files from the start to loop the song
void restartPlayback() {
    if (audioFile) audioFile.close();
    if (ctrlFile)  ctrlFile.close();

    openAudioFile();
    openControlFile();

    Serial.println("Playback restarted");
}

// ── Arduino entry points ──────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    delay(2000);  // let USB-serial enumerate before printing
    Serial.println("Starting Spinning Cat Speaker...");

    if (!initSDCard()) {
        Serial.println("SD init FAILED — check wiring (GPIO5 CS) and card format (FAT32)");
        while (1);
    }
    Serial.println("SD init OK");

    Wire.begin(I2C_SDA, I2C_SCL);
    Serial.println("I2C init OK");

    setupI2S();
    Serial.println("I2S init OK");

    if (!openAudioFile() || !openControlFile()) {
        while (1);
    }

    Serial.println("Starting playback...");
}

void loop() {
    static uint8_t audioBuffer[512];

    // i2s_write blocks until the DMA accepts the data, so the loop naturally
    // runs at real-time audio rate without any explicit delay
    size_t bytesRead = audioFile.read(audioBuffer, sizeof(audioBuffer));
    if (bytesRead > 0) {
        size_t bytesWritten = 0;
        i2s_write(I2S_NUM_0, audioBuffer, bytesRead, &bytesWritten, portMAX_DELAY);
        audioBytesSent += bytesWritten;
    } else {
        restartPlayback();
        return;
    }

    // Dispatch any control frames whose scheduled time has arrived; multiple
    // frames may be due if an audio write stalled longer than one frame period
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
