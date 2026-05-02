# Quick Reference Card

## Serial Port Finder

```bash
# Linux
ls /dev/tty* | grep -E "(USB|ACM)"

# Mac
ls /dev/tty.usbserial*

# Find in Python
python3 utilities.py find-esp32
```

## ESP32 GPIO Pinout

| GPIO | Function | Signal |
|------|----------|--------|
| 5 | SPI MISO | LCD MISO |
| 6 | SPI MOSI | LCD MOSI |
| 7 | SPI SCLK | LCD SCLK |
| 14 | CS | LCD Chip Select |
| 15 | DC | LCD Data/Command |
| 21 | RST | LCD Reset |
| 22 | PWM | LCD Backlight |
| 8 | Data | RGB LED |
| 1 | RX | Serial RX |
| 2 | TX | Serial TX |

## Installation Commands

```bash
# Python dependencies
pip3 install matplotlib pyserial numpy

# From requirements.txt
pip3 install -r requirements.txt

# Verify installation
python3 -c "import serial, matplotlib, numpy; print('OK')"
```

## Serial Connection Test

```bash
# Screen monitor (Linux/Mac)
screen /dev/ttyUSB0 115200

# Exit: Ctrl+A then Ctrl+\

# Or use Python
python3 utilities.py test /dev/ttyUSB0
```

## Arduino IDE Setup

```
1. Tools → Board Manager
2. Search "esp32"
3. Install "esp32 by Espressif"
4. Tools → Board → ESP32-C6
5. Tools → Port → /dev/ttyUSB0
6. Tools → Upload Speed → 115200
```

## Library Installation

Arduino IDE: Sketch → Include Library → Manage Libraries

Search and install:
- `Adafruit GFX Library`
- `Adafruit ST7789 Library`

## RSSI To Distance

```
Distance (meters) = 10^((TxPower - RSSI) / (10*N))
TxPower: -40 dBm
N: 2 (free space)

Examples:
RSSI: -40 dBm → ~1.0 m
RSSI: -50 dBm → ~3.2 m
RSSI: -60 dBm → ~10.0 m
RSSI: -70 dBm → ~31.6 m
RSSI: -80 dBm → ~100 m
```

## Signal Strength Classification

| RSSI Range | Classification |
|-----------|-----------------|
| -50 to 0  | Excellent       |
| -50 to -60| Good            |
| -60 to -70| Fair            |
| -70 to -80| Weak            |
| < -80     | Very Weak       |

## WiFi Channel Frequencies

| Channel | Frequency | Region |
|---------|-----------|--------|
| 1-11    | 2412-2462 MHz | Americas |
| 1-13    | 2412-2472 MHz | Europe |
| 14      | 2484 MHz  | Japan  |

(System scans channels 1-13)

## Key Files

| File | Purpose |
|------|---------|
| `esp_sniffer.ino` | Main firmware |
| `laptop_dashboard.py` | Analysis dashboard |
| `utilities.py` | Helper tools |
| `SYSTEM_GUIDE.md` | Full documentation |
| `TROUBLESHOOTING.md` | Problem solving |

## Dashboard Commands

```python
# To find available ports
python3 utilities.py find-esp32

# To test connection
python3 utilities.py test /dev/ttyUSB0

# To run dashboard
python3 laptop_dashboard.py
```

## Expected Serial Output

```
$DATA|DEV:42|ACT:12|PKT:5234|ATTACK:0|TYPE:None|
BEA:45|PRQ:23|PRR:18|DEA:0|
DEVICES:AA:BB:CC:DD:EE:FF|-45|234|6|12;...|END
```

Translation:
- DEV: 42 unique devices
- ACT: 12 active devices
- PKT: 5234 total packets
- ATTACK: 0 (no attack)
- BEA: 45 beacons/sec
- PRQ: 23 probe requests/sec
- DEA: 0 deauth frames/sec

## Attack Severity

| Count | Severity |
|-------|----------|
| 1 | Low |
| 2-3 | Medium |
| 4+ | High |

## MAC Address Format

`AA:BB:CC:DD:EE:FF` (hex)
- First 3 octets: Manufacturer
- Last 3 octets: Device specific

## Troubleshooting Checklist

- [ ] USB cable is data cable (not charge-only)
- [ ] Serial port is correct
- [ ] Baud rate is 115200 on all interfaces
- [ ] ESP32 shows "Promiscuous mode enabled!"
- [ ] WiFi networks nearby are active
- [ ] Python 3.7+ installed
- [ ] All required libraries installed
- [ ] Dashboard port matches ESP serial port

## Common Baud Rates

| System | Baud Rate |
|--------|-----------|
| ESP32 | 115200 |
| Arduino Serial | 115200 |
| Legacy devices | 9600 |

## Memory Limits

| Component | Max |
|-----------|-----|
| Devices | 200 |
| RSSI History | 20 samples |
| Display Graph | 60 samples |
| Serial Buffer | 64 KB |

## Update Frequencies

| Component | Rate |
|-----------|------|
| LCD Display | 1000 ms |
| Serial Data | 500 ms |
| Channel Hop | 300 ms |
| RGB LED | 100 ms |
| Dashboard GUI | 500 ms |

## Performance Targets

| Metric | Target |
|--------|--------|
| Packet Processing | <1 ms |
| ESP32 CPU Usage | <10% |
| Dashboard CPU | <5% |
| Memory (ESP32) | <50 KB |
| Memory (Laptop) | <100 MB |

## Emergency Reset (ESP32)

```cpp
// Add to setup() if needed
nvs_flash_erase();
nvs_flash_init();
```

Then upload fresh firmware.

## Export Data

```python
# In utilities.py
export_to_csv(data_dict, "devices.csv")
export_to_json(analyzer_state, "analysis.json")
```

## Key Metrics to Monitor

1. **Active Devices**: How many devices are connected
2. **RSSI**: Signal strength (-30 strong, -90 weak)
3. **Packet Count**: Network activity level
4. **Attack Detection**: Security threats
5. **Channel**: Which WiFi channel used
6. **Deauth Count**: Disconnection attacks

## Data Retention

- Device timeout: 45 seconds
- Graph history: 60 seconds
- RSSI samples: 20 per device
- Active session: Until disconnect

---

## Quick Debug Commands

```bash
# Check if ESP32 is responding
screen /dev/ttyUSB0 115200

# Check for $DATA messages
screen /dev/ttyUSB0 115200 | grep DATA

# Find ESP32 USB device
lsusb | grep -i silicon

# Check serial permissions
ls -la /dev/ttyUSB0

# Fix permissions (Linux)
sudo chmod 666 /dev/ttyUSB0
```

## One-Line Help

```python
# Show all available commands
python3 utilities.py

# Find your ESP32
python3 utilities.py find-esp32

# Test if board responds
python3 utilities.py test /dev/ttyUSB0
```

---

**Print or bookmark this page for quick reference while working!**
