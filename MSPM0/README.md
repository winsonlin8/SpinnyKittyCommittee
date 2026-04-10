| MCU Pin | Function | Mode | Connection (L298N) | Description |
| :--- | :--- | :--- | :--- | :--- |
| **PA25** | Digital Out | GPIO | IN1 | Motor dir A |
| **PA8** | Digital Out | GPIO | IN2 | Motor dir B |
| **PA12** | PWM Out | TIMG0 | ENA | for PWM |
| **GND** | Ground | - | GND | shared ground |
| - | Power | - | VCC | 5V from laptop |

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

---

### MSPM0 LaunchPad Jumper Configuration

| Jumper | Setting | Notes |
|--------|--------|-------|
| J21    | BP     | Routes PA10 to header (disconnects from debugger) |
| J22    | BP     | Routes PA11 to header (disconnects from debugger) |

> **Important:** Both J21 and J22 must connect the center pin to **BP**, not **XDS**, to use UART with external devices.
