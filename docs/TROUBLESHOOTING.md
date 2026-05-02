# Troubleshooting Guide

## Compilation Issues

### Error: "ESP32_C6" board not found

**Solution:**
```
Arduino IDE → Tools → Board Manager → Search "esp32" → Install "esp32 by Espressif"
Then Tools → Board → "ESP32-C6"
```

### Error: "Adafruit_ST7789.h not found"

**Solution:**
```
Arduino IDE → Sketch → Include Library → Manage Libraries
Search for:
  - Adafruit GFX Library
  - Adafruit ST7789 Library
Click Install on each
```

### Error: Compilation fails with undefined references

**Solution:**
```
Make sure you have the correct libraries installed.
Try: Sketch → Verify/Compile (Ctrl+R)
If still fails, try reinstalling board support:
  1. Remove ESP32: Board Manager → ESP32 → Uninstall
  2. Restart Arduino IDE
  3. Reinstall: Board Manager → esp32 → Install
```

## Upload Issues

### Error: "Failed to connect to ESP32"

```
1. Check USB cable (try different port on computer)
2. Verify ESP32 USB driver installed
3. Hold down BOOT button while uploading
4. Try lower baud rate: Tools → Upload Speed → 115200
```

### Error: "COM port not found / /dev/ttyUSB0 not recognized"

**Linux:**
```bash
# Check connected devices
lsusb

# List serial ports
ls -la /dev/tty* | grep -E "(USB|ACM)"

# Give your user permission
sudo usermod -a -G dialout $USER
# Logout and login, or: newgrp dialout
```

**Windows:**
```
Device Manager → Ports (COM & LPT)
If showing "Unknown Device" or with warning:
  1. Right-click → Update driver
  2. Browse computer → Let me pick
  3. Silicon Labs or WCH CH340
```

**Mac:**
```bash
# Find port
ls /dev/tty.usbserial*

# If nothing shows, install CH340 driver
# https://github.com/WCHSoftware/ch340-driver
```

### "Failed to open port /dev/ttyUSB0"

```bash
# Check if port is in use
sudo lsof /dev/ttyUSB0

# Close Arduino IDE Serial Monitor first
# Kill any hanging processes
sudo fuser -k /dev/ttyUSB0
```

## Runtime Issues

### LCD Remains Blank

**Check in Arduino Serial Monitor (115200 baud):**
```
Should show:
"Display initialized!"
"Promiscuous mode enabled!"
```

If shows error:
1. Verify SPI pins: GPIO 5,6,7 (MISO, MOSI, SCLK)
2. Check CS: GPIO 14, DC: GPIO 15, RST: GPIO 21, BL: GPIO 22
3. Try slower SPI frequency (edit: `tft.init(LCD_WIDTH, LCD_HEIGHT)`)

**Diagnostic:**
```cpp
// Add to setup() after tft.init()
tft.setTextColor(ST77XX_WHITE);
tft.setTextSize(1);
tft.setCursor(0, 0);
tft.println("LCD Works!");
```

### RGB LED Not Lighting

```
Check:
1. GPIO 8 connection
2. Power supply adequate (3.3V)
3. LED polarity (usually: G, V+, V-, R on WS2812)
4. Try neopixelWrite(8, 255, 0, 0); // Red test

If still not working:
- LED might be dead (test with different LED)
- Pin 8 might be reserved by something else
```

### No Devices Detected

**Possible causes:**
1. No WiFi networks nearby
2. Promiscuous mode not enabled
3. Channel hopping too fast

**Verify in Serial Monitor:**
```
Check for these lines:
"Promiscuous mode enabled!"
"Callback registered!"
"Channel set to 1"

If missing: Reset board, try again
```

**Test:**
1. Put ESP32 next to WiFi router
2. Use phone to scan networks repeatedly
3. Should see MAC addresses in serial output

### Dashboard Shows No Connection

**Step 1: Verify ESP32 is transmitting**
```bash
# Open serial monitor
screen /dev/ttyUSB0 115200

# Should see data like:
Active:5 | Total:42 | CH:3 | Deauth:0 | Probe:12
```

**Exit screen:** `Ctrl+A` then `Ctrl+\`

**Step 2: Check serial port in dashboard**
```python
# Test connection
python3 utilities.py find-esp32

# If found, update port in dashboard
# Default: /dev/ttyUSB0
```

**Step 3: Verify data format**
```bash
# Look for lines starting with $DATA
screen /dev/ttyUSB0 115200 | grep DATA

# Should see:
$DATA|DEV:...|ACT:...|END
```

### Dashboard Crashes on Connect

**Error: "list index out of range"**
```
Likely: Wrong data format
Solution: Update ESP32 firmware (latest version)
```

**Error: "serial.SerialException"**
```
Solution: 
1. Close Arduino Serial Monitor
2. Restart dashboard
3. Click Connect
```

## Python Issues

### Error: "ModuleNotFoundError: No module named 'matplotlib'"

```bash
# Install requirements
pip3 install -r requirements.txt

# Or individual packages
pip3 install matplotlib pyserial numpy
```

### Error: "Connection refused" or "Permission denied"

```bash
# On Linux, check permissions
ls -la /dev/ttyUSB0
# Should show: crw-rw-rw-- or crw-rw----

# Fix permissions
sudo chmod 666 /dev/ttyUSB0

# Or add user to dialout group
sudo usermod -a -G dialout $USER
# Logout and login
```

### Error: "Tkinter not found"

```bash
# Ubuntu/Debian
sudo apt-get install python3-tk

# Fedora
sudo dnf install python3-tkinter

# Mac (via Homebrew)
brew install python-tk
```

### GUI window appears but no updates

```
Check:
1. Serial connection established (status should be "Connected")
2. Baud rate is 115200
3. Data format correct (look in terminal for $DATA messages)

If stuck:
1. Restart dashboard
2. Close all Arduino IDE windows
3. Disconnect and reconnect USB
```

## Data/Performance Issues

### Laptop is slow or freezing

**Solution 1: Reduce graph update rate**
```python
# Edit laptop_dashboard.py, find:
UPDATE_INTERVAL = 500  # ms

# Change to:
UPDATE_INTERVAL = 1000  # ms (every second)
```

**Solution 2: Disable matplotlib rendering**
```python
# Comment out graph update calls temporarily
# This helps identify if matplotlib is the bottleneck
```

**Solution 3: Clear old device data**
```python
# In analyzer, limit device history
self.devices = {}  # Clear all devices
# Then reconnect
```

### ESP32 freezes or crashes

**Causes:**
1. Stack overflow (recursion)
2. Memory exhaustion
3. Serial buffer full

**Solutions:**
```cpp
// Add watchdog timer
#include "esp_task_wdt.h"

// In setup()
esp_task_wdt_init(30, true);  // 30 second timeout
esp_task_wdt_add(NULL);

// In loop() regularly
esp_task_wdt_reset();
```

### "Too many devices" warning

Device limit is 200. If exceeded:
```cpp
#define MAX_DEVICES 200  // Change in esp_sniffer.ino

// Or implement device aging:
if (now - devices[i].lastSeen > 60000) {
  devices[i].isActive = false;  // Mark inactive
}
```

## Network/Monitoring Issues

### Only seeing a few devices

**Causes:**
1. Devices not actively sending packets
2. Too much background noise
3. Channel hopping missing devices

**Solutions:**
1. Make sure WiFi networks nearby are active
2. Have people use WiFi (phones, laptops, etc.)
3. Position ESP32 centrally
4. Reduce channel hop interval (faster detection)

### High number of false deauth detections

**Causes:**
- Threshold too low
- Interference in environment

**Solution: Increase threshold**
```cpp
#define DEAUTH_THRESHOLD 5  // Change in esp_sniffer.ino
```

### MAC addresses showing as "00:00:00:00:00:00"

**Causes:**
- Invalid frame parsing
- Corrupted packet data

**Solution:**
```cpp
// The code filters these out in isValidMAC()
// But if you see them, check:
1. SPI clock speed (might be too high)
2. USB cable quality (might have interference)
3. WiFi antenna position
```

## Serial Communication Issues

### Garbled text in serial monitor

```
Likely: Baud rate mismatch
Check: Serial.begin() in sketch = 115200
       ESP32 board setting = 115200
       Serial monitor = 115200

If still garbled, ESP32 might be damaged
```

### Only seeing "[ESP]:" messages, no "$DATA"

```
Check that buffer is updating:
"Active Devices: X | Total: Y"

If seeing this but no $DATA:
The serial format might be different
Check: sendDataToLaptop() function in esp_sniffer.ino
```

## Advanced Fixes

### Resetting ESP32 Completely

```cpp
// Add to setup()
#include <EEPROM.h>
#include <nvs_flash.h>

// Full reset
nvs_flash_erase();
nvs_flash_init();
EEPROM.begin(512);
for (int i = 0; i < 512; i++) EEPROM.write(i, 0);
EEPROM.commit();
```

### Debugging Packet Loss

```cpp
// Add counters
unsigned long packetCount = 0;
unsigned long timeLastCheck = 0;

void loop() {
  // Calculate packets/sec
  if (millis() - timeLastCheck >= 1000) {
    Serial.printf("Packets/sec: %lu\n", packetCount);
    packetCount = 0;
    timeLastCheck = millis();
  }
}
```

### Low Memory Warnings

```cpp
// In loop()
if (millis() % 10000 == 0) {  // Every 10 seconds
  Serial.printf("Free DRAM: %d bytes\n", ESP.getFreeHeap());
}

// Typical free memory: 100-200 KB on ESP32-C6
// If < 50 KB, something is leaking
```

## Support Resources

If issue persists:

1. **Check Serial Output**
   - Open Arduino Serial Monitor (115200)
   - Note exact error messages
   - Capture 30 seconds of output

2. **Isolate the Problem**
   - Test ESP32 alone (no laptop)
   - Test laptop dashboard with mock data
   - Test serial connection independently

3. **Gather Information**
   ```bash
   # System info
   uname -a  # Linux/Mac
   
   # USB info
   lsusb
   ls -la /dev/tty*
   
   # Python version
   python3 --version
   python3 -c "import serial; print(serial.__version__)"
   ```

4. **Common Error Codes**
   | Code | Meaning | Fix |
   |------|---------|-----|
   | ESP_ERR_INVALID_STATE | WiFi not ready | Restart ESP32 |
   | ESP_ERR_NO_MEM | Out of memory | Reduce MAX_DEVICES |
   | TIMEOUT | No response | Check connections |
   | CRC_ERROR | Data corruption | Check USB cable |

---

**Still stuck?** Check system logs and serial output for specific error codes
