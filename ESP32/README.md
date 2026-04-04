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
