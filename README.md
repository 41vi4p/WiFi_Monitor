# WiFi_Monitor

Advanced WiFi security monitoring and analysis tool built on ESP32-C6 with real-time threat detection, network analysis, and visual feedback systems.

## 🔍 Main Project: WiFi Security Monitor

**WiFi_Monitor** - Enterprise-grade WiFi network security analysis and monitoring system.

**Core Features:**
- Deauthentication attack detection
- Beacon spam monitoring
- Channel activity analysis
- Real-time graphs and statistics
- 1.47" LCD display with live updates
- RGB LED status indicators

**Hardware Requirements:**
- ESP32-C6 board
- 1.47" ST7789 LCD (172x320)
- WS2812 RGB LED

## � Screenshots

![WiFi Monitor Dashboard 1](images/Screenshot%20from%202026-05-19%2021-28-57.png)

![WiFi Monitor Dashboard 2](images/Screenshot%20from%202026-05-19%2021-29-07.png)


## 📹 Demo & Resources

- **Presentation**: [View on Canva](https://canva.link/z7m6m2u5pvj6czr)
- **Video Demo**: [Watch on Google Drive](https://drive.google.com/file/d/1nqEuqVf62X9uQBtQGYM96E0nbY2Anv80/view?usp=sharing)

## �📁 Additional Projects

### 👥 Person Counter (`count_persons/`)
**WiFi-based Occupancy Counter** - Complimentary module for room occupancy detection.

**Features:**
- Promiscuous mode WiFi scanning
- Unique device detection and tracking
- Person count estimation
- "Project Cell" branded boot animation
- Color-coded occupancy levels (Green/Yellow/Red)
- Real-time device count graphs

**Hardware Requirements:**
- ESP32-C6 board
- 1.47" ST7789 LCD (172x320)
- WS2812 RGB LED

### ⌨️ Bluetooth Ducky (`Bluetooth_ducky/`)
**Bluetooth Keyboard Emulator** - HID device for security demonstrations.

**Features:**
- Bluetooth Low Energy (BLE) keyboard emulation
- Command-line interface for automation
- Screenshot capture
- Application launching
- Customizable typing delays

## 🚀 Getting Started

### Prerequisites
- Arduino IDE with ESP32 board support
- ESP32-C6 development board
- Required libraries (install via Arduino Library Manager):
  - `Adafruit GFX Library`
  - `Adafruit ST7789`
  - `ESP32 BLE Keyboard`

### Hardware Setup
1. Connect the 1.47" LCD to ESP32-C6 GPIO pins:
   - MISO: GPIO5, MOSI: GPIO6, SCLK: GPIO7
   - CS: GPIO14, DC: GPIO15, RST: GPIO21, BL: GPIO22
2. Connect WS2812 RGB LED to GPIO8
3. Upload the desired sketch using Arduino IDE

### Installation
1. Clone this repository
2. Open the desired `.ino` file in Arduino IDE
3. Install required libraries
4. Select ESP32-C6 board and upload

## 📊 Technical Details

### WiFi Monitoring
- **Promiscuous Mode**: Captures all WiFi frames on all channels
- **Channel Hopping**: Scans 13 WiFi channels every 250ms
- **Device Tracking**: Maintains device lists with MAC addresses and RSSI
- **Attack Detection**: Identifies deauth attacks, beacon spam, and unusual activity

### Display System
- **Resolution**: 172x320 pixels
- **Driver**: ST7789 SPI interface
- **Optimization**: Flicker-free updates with delta rendering
- **Boot Animation**: Custom "Project Cell" branding

### RGB LED Feedback
- **Blue**: Startup/initialization
- **Green**: Normal operation/low occupancy
- **Yellow**: Medium occupancy
- **Red**: High occupancy/alert conditions

## 🔧 Configuration

### Pin Definitions
```cpp
// LCD Pins
#define TFT_MISO  5
#define TFT_MOSI  6
#define TFT_SCLK  7
#define TFT_CS    14
#define TFT_DC    15
#define TFT_RST   21
#define TFT_BL    22

// RGB LED
#define RGB_PIN   8
```

### Timing Constants
- Channel hop interval: 250ms
- Display update: 1000ms
- Device timeout: 60000ms (60 seconds)

## 🎯 Use Cases

### Cybersecurity Education
- Demonstrate WiFi attack detection
- Show Bluetooth HID vulnerabilities
- Illustrate device tracking techniques

### Smart Building Applications
- Occupancy monitoring
- Space utilization analytics
- Automated lighting/environmental controls

### Security Research
- Network traffic analysis
- Device fingerprinting
- Wireless security testing

## 📈 Performance

- **WiFi Channels**: 13 (2.4GHz)
- **Device Capacity**: 100 unique devices
- **Update Rate**: 1Hz display refresh
- **Channel Scan**: ~3.25 seconds full cycle
- **Memory Usage**: ~50KB RAM

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch
3. Commit your changes
4. Push to the branch
5. Open a Pull Request

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

The MIT License is a permissive open-source license that allows you to use, modify, and distribute this software freely, as long as you include a copy of the original license and copyright notice.

## ⚠️ Disclaimer

These tools are designed for educational purposes and authorized security testing only. Unauthorized use of these tools may violate laws and regulations. The authors are not responsible for any misuse or damage caused by these tools.

## 🏷️ Tags

`ESP32-C6`, `WiFi Security`, `Bluetooth`, `IoT`, `Cybersecurity`, `Person Counter`, `Promiscuous Mode`, `Arduino`</content>
<parameter name="filePath">/home/davidporathur/Documents/cyber_presentation/README.md