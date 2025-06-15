# SIF Vehicle Data Logger

## Files
- **logger.c** - logging SIF data to CSV format
- **dash.c** -  TFT display dashboard using known data (older)
- **logger.py** - Python dashboard with real-time plots and GUI

### Test Commands (dash.c only)
- `test` - Start automatic test cycle
- `rpm=####` - Set RPM manually (0-8000)  
- `battery=##` - Set battery % (0-100)
- `mode=#` - Set speed mode (1-3)
- `reverse` - Toggle reverse mode
- `stop` - Stop test mode


### Requirements
```bash
pip install pyserial matplotlib pandas numpy
```

### Usage
1. Run: `python logger.py`
2. Enter COM port (e.g., COM6)
3. Click "Connect" 
4. Real-time dashboard will display

## Available Data

| Parameter | Source | Description | Range/Units |
|-----------|--------|-------------|-------------|
| **Battery %** | B9 | Battery charge percentage | 0-100% |
| **Load Voltage** | B10 | Current load voltage | Raw value |
| **RPM** | B7,B8 | Motor RPM | 0-8000+ RPM |
| **Speed Mode** | B4[0:2] | Current speed mode | 1-3 |
| **Reverse** | B5 | Reverse mode active | B5 == 4 |
| **Brake** | B4[5] | Brake pedal pressed | Boolean |
| **Regen** | B4[3] | Regenerative braking | Boolean |
| **Current** | B6 | Motor current | Raw value |
| **Direction** | B2 | Movement direction | Raw value |

## Raw SIF Bytes (B0-B11)

| Byte | Known Function |
|------|----------------|
| B0 | Unknown |
| B1 | Voltage related |
| B2 | Direction/movement |
| B3 | Unknown |
| B4 | Status bits (brake, regen, mode) |
| B5 | Reverse indicator |
| B6 | Current measurement |
| B7-B8 | RPM (16-bit value) |
| B9 | Battery percentage |
| B10 | Load voltage |
| B11 | CRC checksum |

## Power States
- **IDLE** - RPM = 0
- **COAST** - RPM > 0, no load voltage
- **LOAD** - RPM > 0, load voltage present  
- **REGEN** - Regenerative braking active




