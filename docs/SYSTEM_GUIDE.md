# Integrated WiFi Sniffer & Analyzer System

A comprehensive WiFi security monitoring and analysis system with real-time device detection, attack identification, and location estimation.

## System Overview

This integrated system consists of:

1. **ESP32-C6 Firmware** - Captures WiFi traffic, detects attacks, and sends data to laptop
2. **Python Dashboard** - Analyzes and visualizes network data on your computer
3. **Real-time Communication** - Serial connection between ESP32 and laptop

## Features

### 🔍 Device Detection
- Detects all active WiFi devices on the network
- Tracks MAC addresses, signal strength (RSSI), and packet counts
- Identifies probe requests and beacon frames
- Signal strength classification (Excellent, Good, Fair, Weak, Very Weak)

### 🛡️ Attack Detection
- Deauthentication attacks
- Beacon frame spam
- Probe request floods
- Disassociation attacks
- Real-time alert system with visual/audio indicators

### 📍 Location Estimation
- RSSI-based distance estimation (Friis Free Space Equation)
- Historical RSSI tracking
- Multi-point signal strength analysis
- Relative positioning of devices

### 📊 Analytics
- Real-time statistics dashboard
- Attack timeline visualization
- RSSI distribution analysis
- Signal strength heatmaps
- Device activity logging

### 🎨 Dual Display
- **ESP32 LCD**: Real-time activity summary and attack alerts
- **Laptop Dashboard**: Detailed analysis with graphs and statistics

## Hardware Requirements

### ESP32-C6 Setup
- ESP32-C6 microcontroller
- 1.47" ST7789 LCD display (172x320)
- WS2812 RGB LED
- USB cable for serial connection

### Connection Details
```
ESP32 GPIO Pins:
- GPIO 5: LCD MISO
- GPIO 6: LCD MOSI  
- GPIO 7: LCD SCLK
- GPIO 14: LCD CS
- GPIO 15: LCD DC
- GPIO 21: LCD RST
- GPIO 22: LCD Backlight (PWM)
- GPIO 8: RGB LED
- GPIO 2: Serial RX
- GPIO 1: Serial TX
```

### Laptop
- Any Linux/Mac/Windows computer with USB port
- Python 3.7+

## Software Installation

### 1. Arduino IDE Setup (ESP32)

```bash
# Install ESP32 board support in Arduino IDE:
# 1. Go to Preferences → Additional Board Manager URLs
# 2. Add: https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
# 3. Board Manager → Search "ESP32" → Install
# 4. Select Board: "ESP32-C6"
```

Required Libraries (Install via Library Manager):
- `Adafruit GFX Library`
- `Adafruit ST7789 Library`

### 2. Python Setup (Laptop)

```bash
# Install dependencies
pip install -r requirements.txt

# Or install manually
pip install matplotlib pyserial numpy
```

## Usage

### Step 1: Upload Firmware to ESP32

1. Connect ESP32-C6 to your computer via USB
2. Open `esp_sniffer/esp_sniffer.ino` in Arduino IDE
3. Select Board: ESP32-C6
4. Select Port: (your USB port)
5. Click Upload (⏵)

### Step 2: Start Laptop Dashboard

```bash
# Make dashboard executable (Linux/Mac)
chmod +x laptop_dashboard.py

# Run the dashboard
python3 laptop_dashboard.py
```

### Step 3: Connect to ESP32

1. In the dashboard window, check your serial port (usually `/dev/ttyUSB0` on Linux, `COM3` on Windows)
2. Click "Connect" button
3. Wait for status to show "Connected"
4. Dashboard will start receiving and displaying data

## Dashboard Interface

### Left Panel
- **Connection Status**: Shows connection state and any errors
- **Network Statistics**: Real-time counts of devices, packets, attacks
- **Attack Status**: Alert area showing detected attacks
- **Top Devices**: List of closest/strongest signal devices with:
  - MAC address
  - Current RSSI (signal strength in dBm)
  - Signal quality classification
  - Estimated distance

### Right Panel (Graphs)
1. **Active Devices Over Time**: Line graph showing device count trend
2. **Attack Timeline**: Bar chart showing attack detection pattern
3. **RSSI Distribution**: Histogram showing signal strength distribution
4. **Closest Devices**: Horizontal bar chart of strongest signals

## Data Format

### ESP32 → Laptop Serial Protocol

```
$DATA|DEV:num|ACT:num|PKT:count|ATTACK:0/1|TYPE:attack_type|
BEA:count|PRQ:count|PRR:count|DEA:count|
DEVICES:MAC|RSSI|Packets|Channel|ProbeReqs;MAC|...|END
```

Example:
```
$DATA|DEV:42|ACT:12|PKT:5234|ATTACK:0|TYPE:None|
BEA:45|PRQ:23|PRR:18|DEA:0|
DEVICES:AA:BB:CC:DD:EE:FF|-45|234|6|12;
11:22:33:44:55:66|-62|189|11|5|END
```

## Understanding the Data

### RSSI (Received Signal Strength Indicator)
- Range: -100 dBm (weakest) to -30 dBm (strongest)
- Typically in range: -90 to -40 dBm
- Used to estimate distance and proximity

### Distance Estimation
Formula: `Distance(m) = 10^((TxPower - RSSI) / (10*N))`
- TxPower: -40 dBm (typical WiFi)
- N: 2 (free space path loss exponent)
- Example: RSSI = -60 dBm → ~10m distance

### Signal Strength Classification
| RSSI Range | Classification | Quality |
|-----------|-----------------|---------|
| -50 to 0  | Excellent       | 5 bars  |
| -50 to -60| Good            | 4 bars  |
| -60 to -70| Fair            | 3 bars  |
| -70 to -80| Weak            | 2 bars  |
| < -80     | Very Weak       | 1 bar   |

## Attack Detection

### Deauthentication Attack
- Multiple deauth frames detected per second
- Indicator: Sudden device disconnections
- Alert: "DEAUTH ATTACK"

### Beacon Spam
- Excessive beacon frames (>100/s)
- Multiple SSIDs being advertised
- Alert: "BEACON SPAM"

### Probe Request Flood
- High volume of probe requests (>50/s)
- Devices searching for networks
- Alert: "PROBE FLOOD"

### Disassociation Attack
- Multiple disassoc frames (>5/s)
- Similar to deauth but different frame type
- Alert: "DISASSOC ATTACK"

## Troubleshooting

### No Serial Connection
```bash
# List available serial ports (Linux)
ls -la /dev/tty*

# Check ESP32 driver
dmesg | grep usb

# Change port in dashboard
# Common ports: /dev/ttyUSB0, /dev/ttyUSB1, /dev/ttyACM0
```

### Dashboard Not Receiving Data
1. Check ESP32 serial monitor output (Arduino IDE)
2. Verify baud rate: 115200
3. Ensure USB cable is data-capable (not charge-only)
4. Try different USB port

### No Devices Detected
1. Ensure WiFi networks are nearby
2. Check Serial output shows promiscuous mode enabled
3. Verify channel hopping (should see CH:1-13 cycling)

### LCD Not Displaying
1. Check SPI pin connections
2. Verify SPI clock speed (try lowering to 10MHz)
3. Check backlight GPIO 22 and TFT_BL

## Performance Considerations

### Channel Hopping
- Hops channels every 300ms
- Scans all 13 WiFi channels (2.4 GHz)
- Allows detection of devices on any channel

### Device Decay
- Devices marked inactive after 45 seconds without packets
- Keeps memory usage manageable
- Can track up to 200 devices simultaneously

### Data Rate
- Updates dashboard every 1 second
- Sends detailed data to laptop every 500ms
- Optimal for real-time analysis without overwhelming

## Advanced Features

### Location Estimation
Estimated distance is calculated from RSSI and displayed for each device. Multiple measurements allow rough triangulation if you have reference points.

### Suspicious Device Detection
- Devices sending >50 probe requests are marked suspicious
- Potential rogue access points or network scanning tools

### Attack Pattern Analysis
- Dashboard tracks 60-second history
- Multiple attacks within 5 seconds = HIGH severity
- Single attacks = MEDIUM severity

## File Structure

```
cyber_presentation/
├── esp_sniffer/
│   └── esp_sniffer.ino          # Main ESP32 firmware (integrated)
├── count_persons/
│   └── count_persons.ino        # Reference: Person counting
├── esp_monitor/
│   └── esp_monitor.ino          # Reference: Attack detection
├── laptop_dashboard.py           # Python analysis dashboard
├── requirements.txt              # Python dependencies
└── README.md                     # This file
```

## Security Notes

⚠️ **Important**: This tool is for authorized network monitoring and educational purposes only. 
- Only use on networks you own or have explicit permission to monitor
- Unauthorized network monitoring may violate laws in your jurisdiction
- Store collected data securely
- Respect privacy of network users

## Future Enhancements

- [ ] Kalman filtering for more accurate distance estimation
- [ ] Multi-point triangulation for precise location
- [ ] Export data to CSV/JSON formats
- [ ] Network topology visualization
- [ ] Machine learning for attack pattern recognition
- [ ] Mobile app for remote monitoring
- [ ] Cloud data storage and analysis

## License

Educational and research use. See LICENSE file for details.

## Support

For issues or questions:
1. Check troubleshooting section above
2. Review serial output on ESP32
3. Verify hardware connections
4. Check Python console for error messages

---

**Project Cell** - Integrated WiFi Analysis System v1.0
