# MSPM0G3507 Firmware

This directory contains the Code Composer Studio project for the MSPM0G3507
microcontroller. It acts as an I2C target (address 0x48), receiving playback
commands from the ESP32 and driving the DC motor and WS2812 LED ring in sync
with the music.

See `i2c_target/` for the main firmware source and pre-computed animation data.

---

## Motor Driver — MSPM0G3507 ↔ L298N

| MCU Pin | Function      | Mode  | L298N Pin    | Description              |
|:--------|:--------------|:------|:-------------|:-------------------------|
| **PA25** | Digital Out  | GPIO  | IN1          | Motor direction A        |
| **PA24** | Digital Out  | GPIO  | IN2          | Motor direction B        |
| **PA22** | PWM Out      | GPIO (software PWM) | ENA (outer pin) | Speed control |
| **GND**  | Ground       | —     | GND          | Shared ground            |
| —        | Power        | —     | VCC          | 5V from power supply     |
| —        | Motor+       | —     | OUT1         | Motor output             |
| —        | Motor−       | —     | OUT2         | Motor output             |

### L298N Jumper Settings

| Jumper           | Status       | Reason |
|:-----------------|:-------------|:-------|
| **ENA Jumper**   | **Removed**  | Allows external PWM control from PA22 |
| **5V Enable**    | **Inserted** | Enables the on-board 5V regulator     |

---

## WS2812 LED Ring

| MCU Pin | Connection      | Notes                          |
|:--------|:----------------|:-------------------------------|
| **PA15** | LED data line  | 1-wire bit-bang at ~800 kHz    |
| **GND**  | LED GND        | Shared ground                  |
| —        | LED 5V         | 5V from power supply           |

The ring has 35 pixels. All WS2812 timing is handled in software; see
`i2c_target/i2c_target.c` for the bit-bang implementation.

---

## ESP32 ↔ MSPM0G3507 — I2C Communication

| ESP32 Pin | MSPM0G3507 Pin | Function | Notes                        |
|:----------|:---------------|:---------|:-----------------------------|
| GPIO 21   | PA0            | SDA      | I2C data line                |
| GPIO 22   | PA1            | SCL      | I2C clock line               |
| GND       | GND            | Ground   | Common ground required       |

**Pull-up resistors required on both lines:**

| Signal | Value  | Pull to |
|:-------|:-------|:--------|
| SDA    | 2.2 kΩ | 3.3V   |
| SCL    | 2.2 kΩ | 3.3V   |

---

## LaunchPad Jumper Configuration (for debugging)

| Jumper | Setting | Effect |
|:-------|:--------|:-------|
| J101 13:14 | ON  | Connects PA19 to XDS-110 SWDIO (required for debugging) |
| J101 15:16 | ON  | Connects PA20 to XDS-110 SWCLK (required for debugging) |

> Disconnect these jumpers when PA19/PA20 are needed for application use.
