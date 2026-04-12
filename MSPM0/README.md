| MCU Pin | Function | Mode | Connection (L298N) | Description |
| :--- | :--- | :--- | :--- | :--- |
| **PA25** | Digital Out | GPIO | IN1 | Motor dir A |
| **PA8** | Digital Out | GPIO | IN2 | Motor dir B |
| **PA22** | PWM Out | TIMG0 | ENA (outer pin) | for PWM |
| **GND** | Ground | - | GND | shared ground |
| - | Power | - | VCC | 5V from power supply |
| - | motor+ | - | OUT1 | output signal |
| - | motor- | - | OUT2 | output signal |

| Jumper on L298N | Status | 
| :--- | :--- |
| **ENA Jumper** | **Removed** | 
| **5V Enable** | **Inserted** |

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

---

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
