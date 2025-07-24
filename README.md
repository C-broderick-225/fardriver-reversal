# CheapDashEbike

A custom ESP32-S2-based dashboard and data logger for electric vehicles, designed for the Lolin S2 Mini board. This project features a real-time graphical display, SIF data parsing, and serial logging, with a focus on extensibility and clarity.

## Features

- **Real-time vehicle data display** on a 240x320 TFT (ST7789) using Adafruit GFX libraries.
- **SIF (Serial Interface) data acquisition** via hardware interrupt, with CRC validation.
- **Python-based and SIF-based data source switching** for flexible development and testing.
- **Comprehensive UI**: speed, RPM, battery, voltage, current, brake/regen/reverse indicators, and rev limiter warning.
- **Serial logging** of all raw and processed data for external analysis.
- **Debug and status commands** via serial interface.
- **Unit testing support** via PlatformIO.

## Hardware

- **Board:** Lolin S2 Mini (ESP32-S2)
- **Display:** ST7789 240x320 TFT (uses Adafruit GFX and ST7789 libraries)
- **SIF Input:** Pin 4 (interrupt-driven)
- **Other Pins:** TFT_CS (12), TFT_DC (13), TFT_RST (14), SPI (7, 11)

## Directory Structure

```
.
├── src/                # Main application source code
│   ├── main.cpp        # Entry point, hardware setup, main loop
│   ├── logic.cpp/h     # Vehicle data parsing, state management
│   ├── ui.cpp/h        # Display/UI logic
│   └── CMakeLists.txt  # Source build config
├── include/            # Project-wide header files
│   └── README          # Header file usage guide
├── lib/                # Private libraries (add your own here)
│   └── README          # Library usage guide
├── test/               # Unit tests and test runner
│   ├── logger.py       # (Example) Python logger
│   └── README          # Unit testing info
├── platformio.ini      # PlatformIO project config
├── sdkconfig.lolin_s2_mini # ESP-IDF/Arduino SDK config
├── CMakeLists.txt      # Project build config
├── .vscode/            # VSCode settings (optional)
└── .gitignore
```

## Getting Started

### Prerequisites

- PlatformIO (VSCode extension recommended)
- Lolin S2 Mini board
- ST7789 240x320 TFT display
- (Optional) Python for test logging

### Build & Upload

1. Connect your Lolin S2 Mini via USB.
2. Install dependencies:
   - Adafruit GFX Library
   - Adafruit ST7735 and ST7789 Library
   - Adafruit BusIO
3. Build and upload using PlatformIO:
   ```
   pio run --target upload
   ```
4. Open the serial monitor:
   ```
   pio device monitor
   ```

### Serial Commands

- `DEBUG_ON` / `DEBUG_OFF` — Enable/disable debug output
- `STATUS` — Print SIF packet count and data source
- `SIF_ON` / `SIF_OFF` — Switch between SIF and Python data sources

## Code Overview

- **main.cpp**: Sets up hardware, handles interrupts, manages main loop, and serial commands.
- **logic.cpp/h**: Parses SIF and Python data, manages vehicle state, and provides data to UI.
- **ui.cpp/h**: Draws and updates the dashboard display, including gauges, indicators, and warnings.

## Testing

- Place unit tests in the `test/` directory.
- See `test/README` for details.
- Run tests with:
  ```
  pio test
  ```

## Customization

- Add your own libraries in `lib/your_library_name/`.
- Place shared headers in `include/`.
- Adjust hardware pin assignments and display settings in `main.cpp` as needed.

## License

[Specify your license here, e.g., MIT, GPL, etc.] 