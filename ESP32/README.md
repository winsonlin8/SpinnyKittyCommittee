# ESP32 Firmware

This directory contains the PlatformIO project for the ESP32 microcontroller.

## Prerequisites

- [VS Code](https://code.visualstudio.com/) with the [PlatformIO IDE extension](https://marketplace.visualstudio.com/items?itemName=platformio.platformio-ide), **or**
- PlatformIO CLI: `pip install platformio`

## Directory Structure

```
ESP32/
├── src/           # Source files (.cpp)
├── include/       # Header files (.h)
├── lib/           # Local libraries
├── test/          # Unit tests
└── platformio.ini # Project configuration
```

## Building and Uploading

### VS Code

1. Open the `ESP32/` folder as your workspace root in VS Code.
2. PlatformIO will auto-detect `platformio.ini` and configure the project.
3. Use the bottom toolbar:
   - **Checkmark (✓)** — Build
   - **Right arrow (→)** — Upload

### CLI

Run these commands from inside the `ESP32/` directory:

```bash
pio run              # Build only
pio run -t upload    # Build and upload
```

## Serial Monitor

The firmware outputs debug info over serial at **115200 baud**. To view it:

```bash
pio device monitor
```

Or click the plug icon in VS Code's bottom toolbar.

## Wiring

### ADA254 — MicroSD Card Breakout

| ADA254 Pin | ESP32 Pin | Notes           |
|------------|-----------|-----------------|
| 3V         | 3.3V      | Power           |
| GND        | GND       |                 |
| CLK        | GPIO 18   | VSPI SCLK       |
| DO         | GPIO 19   | VSPI MISO       |
| DI         | GPIO 23   | VSPI MOSI       |
| CS         | GPIO 5    | Chip select     |

---
### MAX98357A — I2S Audio Amplifier

| MAX98357A Pin | ESP32 Pin | Notes |
|---------------|-----------|-------|
| VIN           | 5V / VIN  | Power for amplifier; 5V gives better speaker output |
| GND           | GND       | Common ground |
| BCLK          | GPIO 26   | I2S bit clock |
| LRC / WS      | GPIO 25   | I2S word select / left-right clock |
| DIN           | GPIO 22   | I2S audio data from ESP32 |

---

### Speaker — 4Ω 3W

| Speaker Wire | Connect To     | Notes |
|--------------|----------------|-------|
| Wire 1       | MAX98357A OUT+ | Speaker output |
| Wire 2       | MAX98357A OUT- | Speaker output |

> **Important:** Do **not** connect either speaker terminal to GND.

---

> ### ICS-43434 — I2S Microphone

| ICS-43434 Pin | ESP32 Pin | Notes |
|---------------|-----------|-------|
| 3V            | 3.3V      | Power (do not use 5V) |
| GND           | GND       | Common ground |
| BCLK          | GPIO 14   | I2S bit clock |
| LRCL          | GPIO 15   | I2S word select (left/right clock) |
| DOUT          | GPIO 32   | I2S data from mic to ESP32 |
| SEL           | GND       | Selects LEFT channel (matches current code) |

### ESP32 ↔ MSPM0G3507 — UART Communication

| ESP32 Pin        | MSPM0G3507 Pin | Notes |
|------------------|----------------|-------|
| GPIO 17 (TX)     | PA11 (RX)      | ESP32 transmits → MSPM0 receives |
| GPIO 16 (RX)     | PA10 (TX)      | ESP32 receives ← MSPM0 transmits |
| GND              | GND            | Common ground (required) |

### ESP32 ↔ MSPM0G3507 — PWM Backup Communication

| ESP32 Pin | MSPM0G3507 Pin | Notes |
|-----------|----------------|-------|
| GPIO 4    | PA12           | One-way PWM/control signal from ESP32 to MSPM0 |
| GND       | GND            | Common ground (required) |

### MSPM0 LaunchPad Jumper Configuration

| Jumper | Setting | Notes |
|--------|--------|-------|
| J21    | BP     | Routes PA10 to header (disconnects from debugger) |
| J22    | BP     | Routes PA11 to header (disconnects from debugger) |

> **Important:** Both J21 and J22 must connect the center pin to **BP**, not **XDS**, to use UART with external devices.

### ESP32 ↔ MSPM0G3507 — I2C Communication

| ESP32 Pin | MSPM0G3507 Pin | Function | Notes |
|-----------|----------------|----------|-------|
| GPIO21    | PA0            | SDA      | I2C data line |
| GPIO22    | PA1            | SCL      | I2C clock line |
| GND       | GND            | Ground   | Common ground required |

### We Need Pull Up Resistors!

| Signal | Recommended Pull-Up | Notes |
|--------|----------------------|-------|
| SDA    | 2.2 kΩ to 3.3V       | Matches proven LaunchPad setup |
| SCL    | 2.2 kΩ to 3.3V       | Matches proven LaunchPad setup |

---

---

### Microphone Notes

- The ICS-43434 is a **digital I2S microphone**, not analog.
- `DOUT` sends audio data **to** the ESP32 (input device).
- The `SEL` pin determines which channel the mic outputs:
  - `GND` → Left channel  
  - `3.3V` → Right channel  

> Current firmware uses `I2S_CHANNEL_FMT_ONLY_LEFT`, so `SEL` must be connected to **GND**.

- The microphone must be powered at **3.3V**, not 5V.
- Ensure a **common ground** with the ESP32 and other components.

---

### Power Notes

| Device | Power Source | Notes |
|--------|--------------|-------|
| ESP32 | USB from laptop | Used for programming and testing |
| ADA254 MicroSD Breakout | ESP32 3.3V | SD breakout powered at 3.3V |
| MAX98357A | ESP32 5V / VIN | Better volume than powering from 3.3V |

<!-- To add a new device, copy the template below and fill it in:

### <Device Name> — <Description>

| <Device> Pin | ESP32 Pin | Notes |
|--------------|-----------|-------|
| ...          | ...       | ...   |

-->

## Adding Libraries

Add dependencies to `platformio.ini` under `lib_deps`:

```ini
lib_deps =
    SPI
    SD
```

PlatformIO will automatically download them on the next build.
