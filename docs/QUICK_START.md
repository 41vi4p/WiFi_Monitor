# Quick Start Guide

## 5-Minute Setup

### Hardware Check
- [ ] ESP32-C6 connected to laptop via USB
- [ ] 1.47" LCD connected to ESP32
- [ ] RGB LED connected to GPIO 8
- [ ] Power supply connected

### Step 1: Upload Firmware (5 min)

```bash
# 1. Open Arduino IDE
# 2. File → Open → esp_sniffer/esp_sniffer.ino
# 3. Tools → Board → ESP32-C6
# 4. Tools → Port → /dev/ttyUSB0 (adjust as needed)
# 5. Click Upload (⏵ button)
# 6. Wait for "Upload complete"
```

### Step 2: Start Dashboard (2 min)

```bash
# In terminal
pip install -r requirements.txt
python3 laptop_dashboard.py
```

### Step 3: Connect

1. In dashboard window, click "Connect"
2. Wait for green "Connected" status
3. Dashboard starts updating automatically

## What You'll See

### On ESP32 LCD
- Device count (big number)
- Attack alerts (if detected)
- Channel hopping
- Basic statistics

### On Laptop
- List of all detected devices
- Signal strength graph
- Attack timeline
- Real-time statistics
- Distance estimates

## Key Metrics

| Metric | Meaning |
|--------|---------|
| **RSSI** | Signal strength (-100 worst, -30 best) |
| **ACT** | Active devices right now |
| **PKT** | Total packets captured |
| **CH** | WiFi channel (1-13) |
| **DEAUTH** | Deauthentication attacks |

## Testing

### Test Device Detection
1. Run dashboard
2. Use phone/laptop to search WiFi networks
3. See new MAC address appear in dashboard

### Test Attack Detection
1. Get close with your device
2. Turn WiFi off and on rapidly
3. Should detect probe floods

### Test Location Estimation
1. Note distance of device
2. Check estimated distance in dashboard
3. Move away - distance should increase

## Common Issues

| Issue | Solution |
|-------|----------|
| "Serial port not found" | Check USB, try `/dev/ttyUSB0` or `/dev/ttyACM0` |
| "No data received" | Check ESP32 serial monitor in Arduino IDE |
| "LCD blank" | Verify GPIO connections (5,6,7,14,15,21,22) |
| "RGB LED not working" | Use GPIO 8, verify polarity |
| "Few devices detected" | Move closer to active WiFi networks |

## Port Discovery

```bash
# Find your ESP32's serial port

# Linux
ls /dev/tty*
# Look for /dev/ttyUSB0 or /dev/ttyACM0

# Mac
ls /dev/tty.usbserial*
# Look for /dev/tty.usbserial-*

# Windows
# Device Manager → COM Ports
# Look for Silicon Labs or CH340
```

## Monitor ESP32 Output

```bash
# Use Arduino IDE Serial Monitor
# Or use screen (Linux/Mac)
screen /dev/ttyUSB0 115200

# Exit: Ctrl+A then Ctrl+\
```

## Next Steps

- [ ] Read SYSTEM_GUIDE.md for full documentation
- [ ] Review data format in dashboard
- [ ] Explore detected networks and devices
- [ ] Monitor for suspicious activity

## Tips

1. **Clearer view**: Run in room with multiple WiFi networks
2. **Better detection**: Keep ESP32 away from metal objects
3. **More devices**: Position ESP32 centrally in your monitoring area
4. **Less noise**: Monitor during network activity (people using WiFi)

---

Ready to monitor? Start with Step 1! 🚀
