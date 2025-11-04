/*
 * ESP32-C6 Person Counter (Project Cell)
 * Uses WiFi promiscuous mode to detect unique devices (phones, tablets, etc.)
 * and estimate the number of people in the room.
 * 
 * Display: Shows device count and detection stats on 1.47" LCD
 * RGB LED: Visual feedback based on occupancy level
 * 
 * Hardware Requirements:
 * - ESP32-C6 board with 1.47" LCD (ST7789, 172x320)
 * - WS2812 RGB LED
 * 
 * Libraries needed:
 * - Adafruit_GFX
 * - Adafruit_ST7789
 * - esp_wifi (built-in)
 */

#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <nvs_flash.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>

// ===== Display Configuration for ESP32-C6 1.47" LCD =====
#define LCD_WIDTH          172
#define LCD_HEIGHT         320
#define TFT_MISO           5    // MISO - GPIO5
#define TFT_MOSI           6    // MOSI - GPIO6
#define TFT_SCLK           7    // SCLK - GPIO7
#define TFT_CS             14   // CS - GPIO14
#define TFT_DC             15   // DC - GPIO15
#define TFT_RST            21   // RST - GPIO21
#define TFT_BL             22   // Backlight - GPIO22

// RGB LED Configuration
#define RGB_PIN            8    // RGB LED - GPIO8

// Display object
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// ===== WiFi Monitoring Configuration =====
#define MAX_CHANNELS 13
#define CHANNEL_HOP_INTERVAL 250  // ms
#define DISPLAY_UPDATE_INTERVAL 1000 // ms
#define MAX_DEVICES 100
#define DEVICE_TIMEOUT 60000  // 60 seconds - remove device if not seen

// ===== Data Structures =====
struct DeviceInfo {
  uint8_t mac[6];
  unsigned long lastSeen;
  int8_t rssi;
  bool active;
};

// ===== Global Variables =====
DeviceInfo devices[MAX_DEVICES];
uint16_t deviceCount = 0;
uint16_t activeDeviceCount = 0;

uint8_t currentChannel = 1;
unsigned long lastChannelHop = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long lastDeviceCleanup = 0;
unsigned long lastRGBUpdate = 0;

uint32_t totalPackets = 0;
uint32_t probeRequests = 0;
uint32_t dataFrames = 0;

// Device count history for graph
uint16_t deviceHistory[60] = {0};
uint8_t historyIndex = 0;

// RGB LED variables
uint8_t rgbHue = 0;

// ===== Color Definitions =====
#define COLOR_BACKGROUND ST77XX_BLACK
#define COLOR_TEXT       ST77XX_WHITE
#define COLOR_PRIMARY    ST77XX_CYAN
#define COLOR_ACCENT     ST77XX_MAGENTA
#define COLOR_LOW        ST77XX_GREEN
#define COLOR_MEDIUM     ST77XX_YELLOW
#define COLOR_HIGH       ST77XX_RED

// ===== WiFi Packet Structures =====
typedef struct {
  unsigned frame_ctrl:16;
  unsigned duration_id:16;
  uint8_t addr1[6];
  uint8_t addr2[6];
  uint8_t addr3[6];
  unsigned sequence_ctrl:16;
  uint8_t addr4[6];
} wifi_ieee80211_mac_hdr_t;

typedef struct {
  wifi_ieee80211_mac_hdr_t hdr;
  uint8_t payload[0];
} wifi_ieee80211_packet_t;

// ===== Helper Functions =====
bool isBroadcast(uint8_t* mac) {
  return (mac[0] == 0xFF && mac[1] == 0xFF && mac[2] == 0xFF &&
          mac[3] == 0xFF && mac[4] == 0xFF && mac[5] == 0xFF);
}

bool isMulticast(uint8_t* mac) {
  return (mac[0] & 0x01);
}

bool isValidMAC(uint8_t* mac) {
  // Check if not broadcast or multicast
  if (isBroadcast(mac) || isMulticast(mac)) return false;
  
  // Check if not null
  bool allZero = true;
  for (int i = 0; i < 6; i++) {
    if (mac[i] != 0) {
      allZero = false;
      break;
    }
  }
  return !allZero;
}

bool macEquals(uint8_t* mac1, uint8_t* mac2) {
  for (int i = 0; i < 6; i++) {
    if (mac1[i] != mac2[i]) return false;
  }
  return true;
}

int findDevice(uint8_t* mac) {
  for (int i = 0; i < deviceCount; i++) {
    if (macEquals(devices[i].mac, mac)) {
      return i;
    }
  }
  return -1;
}

void addOrUpdateDevice(uint8_t* mac, int8_t rssi) {
  if (!isValidMAC(mac)) return;
  
  int idx = findDevice(mac);
  unsigned long now = millis();
  
  if (idx >= 0) {
    // Update existing device
    devices[idx].lastSeen = now;
    devices[idx].rssi = rssi;
    devices[idx].active = true;
  } else if (deviceCount < MAX_DEVICES) {
    // Add new device
    memcpy(devices[deviceCount].mac, mac, 6);
    devices[deviceCount].lastSeen = now;
    devices[deviceCount].rssi = rssi;
    devices[deviceCount].active = true;
    deviceCount++;
  }
}

// ===== Promiscuous Mode Callback =====
void IRAM_ATTR promiscuous_rx_cb(void *buf, wifi_promiscuous_pkt_type_t type) {
  wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;
  const uint8_t *frame = pkt->payload;
  const uint16_t frame_ctrl = (frame[1] << 8) | frame[0];
  
  const uint8_t frameType = (frame_ctrl & 0x000C) >> 2;
  const uint8_t frameSubType = (frame_ctrl & 0x00F0) >> 4;
  
  totalPackets++;
  
  // Extract MAC addresses (source address is addr2 for most frames)
  uint8_t* addr2 = (uint8_t*)&frame[10]; // Source address
  
  // Management frames (type = 0)
  if (type == WIFI_PKT_MGMT && frameType == 0x00) {
    // Probe Request (Subtype: 0x04) - phones actively searching
    if (frameSubType == 0x04) {
      probeRequests++;
      addOrUpdateDevice(addr2, pkt->rx_ctrl.rssi);
    }
    // Beacon, probe response, etc.
    else if (frameSubType == 0x08 || frameSubType == 0x05) {
      addOrUpdateDevice(addr2, pkt->rx_ctrl.rssi);
    }
  }
  // Data frames (type = 2) - active communication
  else if (type == WIFI_PKT_DATA && frameType == 0x02) {
    dataFrames++;
    addOrUpdateDevice(addr2, pkt->rx_ctrl.rssi);
  }
}

// ===== Channel Hopping =====
void changeChannel() {
  currentChannel++;
  if (currentChannel > MAX_CHANNELS) {
    currentChannel = 1;
  }
  esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);
}

// ===== Device Management =====
void cleanupOldDevices() {
  unsigned long now = millis();
  activeDeviceCount = 0;
  
  for (int i = 0; i < deviceCount; i++) {
    if (now - devices[i].lastSeen > DEVICE_TIMEOUT) {
      devices[i].active = false;
    }
    if (devices[i].active) {
      activeDeviceCount++;
    }
  }
}

// ===== RGB LED Functions =====
void setRGBColor(uint8_t r, uint8_t g, uint8_t b) {
  neopixelWrite(RGB_PIN, r, g, b);
}

void updateRGB() {
  // Color based on occupancy level
  if (activeDeviceCount == 0) {
    // Blue pulse when empty
    uint8_t brightness = (sin(millis() / 500.0) * 64) + 64;
    setRGBColor(0, 0, brightness);
  } else if (activeDeviceCount <= 5) {
    // Green - low occupancy
    setRGBColor(0, 50, 0);
  } else if (activeDeviceCount <= 15) {
    // Yellow - medium occupancy
    setRGBColor(50, 50, 0);
  } else {
    // Red - high occupancy
    setRGBColor(50, 0, 0);
  }
}

void hsvToRgb(uint8_t h, uint8_t s, uint8_t v, uint8_t* r, uint8_t* g, uint8_t* b) {
  if (s == 0) {
    *r = v; *g = v; *b = v;
    return;
  }
  
  uint8_t region = h / 43;
  uint8_t remainder = (h - (region * 43)) * 6;
  uint8_t p = (v * (255 - s)) >> 8;
  uint8_t q = (v * (255 - ((s * remainder) >> 8))) >> 8;
  uint8_t t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;
  
  switch (region) {
    case 0:  *r = v; *g = t; *b = p; break;
    case 1:  *r = q; *g = v; *b = p; break;
    case 2:  *r = p; *g = v; *b = t; break;
    case 3:  *r = p; *g = q; *b = v; break;
    case 4:  *r = t; *g = p; *b = v; break;
    default: *r = v; *g = p; *b = q; break;
  }
}

// ===== Display Functions =====
void drawBootAnimation() {
  tft.fillScreen(COLOR_BACKGROUND);
  
  // Rainbow animation effect
  for (int frame = 0; frame < 50; frame++) {
    uint8_t hue = (frame * 5) % 255;
    uint8_t r, g, b;
    hsvToRgb(hue, 255, 255, &r, &g, &b);
    
    // Convert to 16-bit color (RGB565)
    uint16_t color565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    
    // Draw "Project Cell" with animated color
    tft.setTextSize(3);
    tft.setTextColor(color565);
    tft.setCursor(20, 100);
    tft.println("Project");
    tft.setCursor(45, 140);
    tft.println("Cell");
    
    // Update RGB LED to match
    setRGBColor(r >> 2, g >> 2, b >> 2);
    
    delay(40);
  }
  
  delay(500);
  
  // Fade out
  for (int brightness = 255; brightness >= 0; brightness -= 15) {
    tft.setTextSize(3);
    uint8_t r, g, b;
    hsvToRgb(180, 255, brightness, &r, &g, &b);
    uint16_t color565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    
    tft.fillScreen(COLOR_BACKGROUND);
    tft.setTextColor(color565);
    tft.setCursor(20, 100);
    tft.println("Project");
    tft.setCursor(45, 140);
    tft.println("Cell");
    
    setRGBColor(r >> 2, g >> 2, b >> 2);
    delay(20);
  }
  
  tft.fillScreen(COLOR_BACKGROUND);
  delay(200);
  
  // "Initializing..." message
  tft.setTextColor(COLOR_PRIMARY);
  tft.setTextSize(2);
  tft.setCursor(20, 130);
  tft.println("Person");
  tft.setCursor(25, 155);
  tft.println("Counter");
  tft.setTextSize(1);
  tft.setCursor(30, 190);
  tft.println("Initializing...");
  
  delay(1500);
  tft.fillScreen(COLOR_BACKGROUND);
}

void initDisplay() {
  pinMode(TFT_CS, OUTPUT);
  pinMode(TFT_DC, OUTPUT);
  pinMode(TFT_RST, OUTPUT);
  pinMode(TFT_BL, OUTPUT);
  
  SPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI);
  
  // Hardware reset
  digitalWrite(TFT_RST, HIGH);
  delay(50);
  digitalWrite(TFT_RST, LOW);
  delay(50);
  digitalWrite(TFT_RST, HIGH);
  delay(150);
  
  // Initialize ST7789
  tft.init(LCD_WIDTH, LCD_HEIGHT);
  
  // Enable backlight
  ledcAttach(TFT_BL, 1000, 10);
  ledcWrite(TFT_BL, 1023);
  
  tft.setRotation(0);  // Portrait mode
  tft.fillScreen(COLOR_BACKGROUND);
  delay(100);
  
  // Show boot animation
  drawBootAnimation();
}

void drawGraph(uint16_t* data, uint8_t dataSize, uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint16_t color, String label) {
  // Draw border
  tft.drawRect(x, y, width, height, COLOR_TEXT);
  
  // Draw label
  tft.setCursor(x + 2, y - 8);
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT);
  tft.print(label);
  
  // Find max value
  uint16_t maxVal = 1;
  for (int i = 0; i < dataSize; i++) {
    if (data[i] > maxVal) maxVal = data[i];
  }
  
  // Draw graph
  uint8_t step = width / dataSize;
  for (int i = 0; i < dataSize - 1; i++) {
    uint8_t y1 = y + height - (data[i] * (height - 2) / maxVal);
    uint8_t y2 = y + height - (data[i + 1] * (height - 2) / maxVal);
    uint8_t x1 = x + i * step;
    uint8_t x2 = x + (i + 1) * step;
    tft.drawLine(x1, y1, x2, y2, color);
  }
  
  // Draw max value
  tft.setCursor(x + width - 25, y + 2);
  tft.setTextSize(1);
  tft.print(maxVal);
}

// Static variables to track previous values
static uint16_t prevActiveCount = 0;
static uint16_t prevDeviceCount = 0;
static uint8_t prevChannel = 0;
static uint32_t prevProbeReqs = 0;
static uint32_t prevPackets = 0;
static bool firstUpdate = true;

void updateDisplay() {
  // Only draw static elements on first update
  if (firstUpdate) {
    tft.fillScreen(COLOR_BACKGROUND);
    
    // Title - draw once
    tft.setTextSize(2);
    tft.setTextColor(COLOR_PRIMARY);
    tft.setCursor(10, 8);
    tft.println("Project Cell");
    
    // Label - draw once
    tft.setTextSize(1);
    tft.setTextColor(COLOR_TEXT);
    tft.setCursor(25, 45);
    tft.println("People Detected:");
    
    // Statistics labels - draw once
    tft.setTextSize(1);
    tft.setTextColor(COLOR_TEXT);
    tft.setCursor(5, 135);
    tft.print("Total Devices:");
    tft.setCursor(5, 150);
    tft.print("Channel:");
    tft.setCursor(5, 165);
    tft.print("Probe Reqs:");
    tft.setCursor(5, 180);
    tft.print("Packets:");
    
    firstUpdate = false;
  }
  
  // Update main count ONLY if changed
  if (activeDeviceCount != prevActiveCount) {
    // Clear only the number area
    tft.fillRect(20, 70, 132, 50, COLOR_BACKGROUND);
    
    // Huge number display
    tft.setTextSize(5);
    uint16_t countColor = COLOR_LOW;
    if (activeDeviceCount > 15) countColor = COLOR_HIGH;
    else if (activeDeviceCount > 5) countColor = COLOR_MEDIUM;
    
    tft.setTextColor(countColor);
    String countStr = String(activeDeviceCount);
    int16_t x1, y1;
    uint16_t w, h;
    tft.getTextBounds(countStr, 0, 0, &x1, &y1, &w, &h);
    tft.setCursor((172 - w) / 2, 70);
    tft.print(countStr);
    
    prevActiveCount = activeDeviceCount;
  }
  
  // Update statistics ONLY if changed
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT, COLOR_BACKGROUND); // Use background as text background
  
  if (deviceCount != prevDeviceCount) {
    tft.fillRect(105, 135, 60, 8, COLOR_BACKGROUND);
    tft.setCursor(105, 135);
    tft.print(deviceCount);
    prevDeviceCount = deviceCount;
  }
  
  if (currentChannel != prevChannel) {
    tft.fillRect(65, 150, 60, 8, COLOR_BACKGROUND);
    tft.setCursor(65, 150);
    tft.print(currentChannel);
    prevChannel = currentChannel;
  }
  
  if (probeRequests != prevProbeReqs) {
    tft.fillRect(85, 165, 80, 8, COLOR_BACKGROUND);
    tft.setCursor(85, 165);
    tft.print(probeRequests);
    prevProbeReqs = probeRequests;
  }
  
  if (totalPackets != prevPackets) {
    tft.fillRect(65, 180, 100, 8, COLOR_BACKGROUND);
    tft.setCursor(65, 180);
    tft.print(totalPackets);
    prevPackets = totalPackets;
  }
  
  // Redraw graph every update (smooth animation)
  tft.fillRect(5, 205, 162, 50, COLOR_BACKGROUND);
  drawGraph(deviceHistory, 30, 5, 205, 162, 50, COLOR_PRIMARY, "Device Count");
}

void updateGraphData() {
  deviceHistory[historyIndex] = activeDeviceCount;
  historyIndex = (historyIndex + 1) % 60;
}

// ===== Setup =====
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n================================");
  Serial.println("Project Cell - Person Counter");
  Serial.println("================================\n");
  
  // Initialize RGB LED
  pinMode(RGB_PIN, OUTPUT);
  setRGBColor(0, 0, 255);  // Blue on startup
  
  // Initialize display
  Serial.println("Initializing display...");
  initDisplay();
  Serial.println("Display ready!");
  
  // Initialize WiFi in promiscuous mode
  Serial.println("\nInitializing WiFi subsystem...");
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  
  // Enable promiscuous mode
  Serial.println("Enabling promiscuous mode...");
  esp_err_t err = esp_wifi_set_promiscuous(true);
  if (err != ESP_OK) {
    Serial.printf("ERROR: Failed to enable promiscuous mode: %d\n", err);
  } else {
    Serial.println("Promiscuous mode enabled!");
  }
  
  // Set promiscuous callback
  err = esp_wifi_set_promiscuous_rx_cb(&promiscuous_rx_cb);
  if (err != ESP_OK) {
    Serial.printf("ERROR: Failed to set callback: %d\n", err);
  } else {
    Serial.println("Callback registered!");
  }
  
  // Set initial channel
  err = esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);
  if (err != ESP_OK) {
    Serial.printf("ERROR: Failed to set channel: %d\n", err);
  } else {
    Serial.printf("Channel set to %d\n", currentChannel);
  }
  
  // Set filter to all frame types
  wifi_promiscuous_filter_t filter;
  filter.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA;
  err = esp_wifi_set_promiscuous_filter(&filter);
  if (err != ESP_OK) {
    Serial.printf("WARNING: Failed to set filter: %d\n", err);
  } else {
    Serial.println("Filter set to MGMT + DATA frames");
  }
  
  Serial.println("\n================================");
  Serial.println("Monitoring for devices...");
  Serial.println("================================\n");
  
  lastChannelHop = millis();
  lastDisplayUpdate = millis();
  lastDeviceCleanup = millis();
  lastRGBUpdate = millis();
  
  setRGBColor(0, 50, 0);  // Green - ready
}

// ===== Main Loop =====
void loop() {
  unsigned long currentMillis = millis();
  
  // Update RGB LED
  if (currentMillis - lastRGBUpdate >= 100) {
    updateRGB();
    lastRGBUpdate = currentMillis;
  }
  
  // Channel hopping
  if (currentMillis - lastChannelHop >= CHANNEL_HOP_INTERVAL) {
    changeChannel();
    lastChannelHop = currentMillis;
  }
  
  // Cleanup old devices
  if (currentMillis - lastDeviceCleanup >= 5000) {
    cleanupOldDevices();
    lastDeviceCleanup = currentMillis;
  }
  
  // Update display
  if (currentMillis - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL) {
    updateGraphData();
    updateDisplay();
    
    // Print to serial
    Serial.printf("Active Devices: %d | Total: %d | CH:%d | Probes:%d\n",
                  activeDeviceCount, deviceCount, currentChannel, probeRequests);
    
    // Reset per-second counters
    probeRequests = 0;
    totalPackets = 0;
    dataFrames = 0;
    
    lastDisplayUpdate = currentMillis;
  }
  
  delay(1);
}
