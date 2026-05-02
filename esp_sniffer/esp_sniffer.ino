/*
 * ESP32-C6 Integrated WiFi Sniffer & Analysis System
 * 
 * Features:
 * - Detects all WiFi devices (MAC addresses)
 * - Tracks signal strength (RSSI) for location estimation
 * - Detects WiFi attacks (deauth, beacon spam, probe flood)
 * - Displays summary on ESP LCD
 * - Sends detailed data to laptop for advanced analytics
 * 
 * Hardware:
 * - ESP32-C6 with 1.47" LCD (ST7789, 172x320)
 * - WS2812 RGB LED
 * 
 * Communication:
 * - Serial @ 115200 → Laptop analysis
 */

#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <nvs_flash.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <map>
#include <vector>

using namespace std;

// ===== Display Configuration =====
#define LCD_WIDTH          172
#define LCD_HEIGHT         320
#define TFT_MISO           5
#define TFT_MOSI           6
#define TFT_SCLK           7
#define TFT_CS             14
#define TFT_DC             15
#define TFT_RST            21
#define TFT_BL             22
#define RGB_PIN            8

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// ===== WiFi Monitoring Configuration =====
#define MAX_CHANNELS       13
#define CHANNEL_HOP_INTERVAL 300    // ms
#define DISPLAY_UPDATE_INTERVAL 1000 // ms
#define SERIAL_UPDATE_INTERVAL 500   // ms (send detailed data to laptop)
#define MAX_DEVICES        200
#define DEVICE_TIMEOUT     45000     // 45 seconds

// ===== Color Definitions =====
#define COLOR_BG           ST77XX_BLACK
#define COLOR_TEXT         ST77XX_WHITE
#define COLOR_PRIMARY      ST77XX_CYAN
#define COLOR_ALERT        ST77XX_RED
#define COLOR_WARNING      ST77XX_YELLOW
#define COLOR_GOOD         ST77XX_GREEN

// ===== Device Tracking Structure =====
struct DeviceRecord {
  uint8_t mac[6];
  int8_t maxRssi;          // Strongest signal seen
  int8_t lastRssi;         // Most recent signal
  unsigned long lastSeen;
  uint32_t packetCount;
  uint8_t lastChannel;
  bool isActive;
  uint32_t probeReqCount;  // Probe requests from this device
  uint8_t rssiHistory[20]; // RSSI history for location estimation
  uint8_t rssiIndex;
};

// ===== Global Data Structures =====
std::map<std::string, DeviceRecord> deviceMap;
std::vector<DeviceRecord> activeDevices;

// Global statistics
struct GlobalStats {
  uint32_t totalPackets;
  uint32_t beaconCount;
  uint32_t probeReqCount;
  uint32_t probeRespCount;
  uint32_t dataCount;
  uint32_t deauthCount;
  uint32_t disassocCount;
  uint32_t actionCount;
  uint8_t uniqueDeviceCount;
  uint8_t activeDeviceCount;
  bool attackDetected;
  String attackType;
} stats = {0};

// Timing
uint8_t currentChannel = 1;
unsigned long lastChannelHop = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long lastSerialUpdate = 0;
unsigned long lastStatsReset = 0;
unsigned long lastRGBUpdate = 0;

// History for graphing
uint16_t deviceHistory[60] = {0};
uint8_t historyIndex = 0;
uint16_t attackHistory[60] = {0};
uint8_t attackHistoryIndex = 0;

// ===== WiFi Frame Structures =====
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
std::string macToString(uint8_t* mac) {
  char macStr[18];
  sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", 
          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return std::string(macStr);
}

bool isBroadcast(uint8_t* mac) {
  return (mac[0] == 0xFF && mac[1] == 0xFF && mac[2] == 0xFF &&
          mac[3] == 0xFF && mac[4] == 0xFF && mac[5] == 0xFF);
}

bool isMulticast(uint8_t* mac) {
  return (mac[0] & 0x01);
}

bool isValidMAC(uint8_t* mac) {
  if (isBroadcast(mac) || isMulticast(mac)) return false;
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

// ===== Device Management =====
void addOrUpdateDevice(uint8_t* mac, int8_t rssi, uint8_t channel, bool isProbeReq = false) {
  if (!isValidMAC(mac)) return;
  
  std::string macStr = macToString(mac);
  unsigned long now = millis();
  
  if (deviceMap.find(macStr) != deviceMap.end()) {
    // Update existing device
    DeviceRecord& dev = deviceMap[macStr];
    dev.lastRssi = rssi;
    if (rssi > dev.maxRssi) dev.maxRssi = rssi;
    dev.lastSeen = now;
    dev.isActive = true;
    dev.lastChannel = channel;
    dev.packetCount++;
    
    // Store RSSI for history
    dev.rssiHistory[dev.rssiIndex] = (rssi + 100); // Store as unsigned
    dev.rssiIndex = (dev.rssiIndex + 1) % 20;
    
    if (isProbeReq) dev.probeReqCount++;
  } else if (deviceMap.size() < MAX_DEVICES) {
    // Add new device
    DeviceRecord newDev;
    memcpy(newDev.mac, mac, 6);
    newDev.maxRssi = rssi;
    newDev.lastRssi = rssi;
    newDev.lastSeen = now;
    newDev.isActive = true;
    newDev.lastChannel = channel;
    newDev.packetCount = 1;
    newDev.probeReqCount = isProbeReq ? 1 : 0;
    newDev.rssiIndex = 0;
    memset(newDev.rssiHistory, 0, sizeof(newDev.rssiHistory));
    
    deviceMap[macStr] = newDev;
  }
}

void updateDeviceStatus() {
  unsigned long now = millis();
  stats.uniqueDeviceCount = deviceMap.size();
  stats.activeDeviceCount = 0;
  activeDevices.clear();
  
  for (auto& pair : deviceMap) {
    DeviceRecord& dev = pair.second;
    if (now - dev.lastSeen > DEVICE_TIMEOUT) {
      dev.isActive = false;
    }
    if (dev.isActive) {
      stats.activeDeviceCount++;
      activeDevices.push_back(dev);
    }
  }
}

// ===== Attack Detection =====
void detectAttacks() {
  stats.attackDetected = false;
  
  // Deauth attack
  if (stats.deauthCount > 3) {
    stats.attackDetected = true;
    stats.attackType = "DEAUTH ATTACK";
  }
  
  // Probe flood
  if (stats.probeReqCount > 50) {
    stats.attackDetected = true;
    stats.attackType = "PROBE FLOOD";
  }
  
  // Beacon spam
  if (stats.beaconCount > 100) {
    stats.attackDetected = true;
    stats.attackType = "BEACON SPAM";
  }
  
  // Disassociation attack
  if (stats.disassocCount > 5) {
    stats.attackDetected = true;
    stats.attackType = "DISASSOC ATTACK";
  }
}

// ===== Promiscuous Mode Callback =====
void IRAM_ATTR promiscuous_rx_cb(void *buf, wifi_promiscuous_pkt_type_t type) {
  wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;
  const uint8_t *frame = pkt->payload;
  const uint16_t frame_ctrl = (frame[1] << 8) | frame[0];
  
  const uint8_t frameType = (frame_ctrl & 0x000C) >> 2;
  const uint8_t frameSubType = (frame_ctrl & 0x00F0) >> 4;
  
  stats.totalPackets++;
  uint8_t* addr2 = (uint8_t*)&frame[10]; // Source address
  
  // Management frames (type = 0)
  if (frameType == 0x00) {
    // Beacon (0x08)
    if (frameSubType == 0x08) {
      stats.beaconCount++;
      addOrUpdateDevice(addr2, pkt->rx_ctrl.rssi, currentChannel);
    }
    // Probe Request (0x04)
    else if (frameSubType == 0x04) {
      stats.probeReqCount++;
      addOrUpdateDevice(addr2, pkt->rx_ctrl.rssi, currentChannel, true);
    }
    // Probe Response (0x05)
    else if (frameSubType == 0x05) {
      stats.probeRespCount++;
      addOrUpdateDevice(addr2, pkt->rx_ctrl.rssi, currentChannel);
    }
    // Deauth (0x0C)
    else if (frameSubType == 0x0C) {
      stats.deauthCount++;
    }
    // Disassoc (0x0A)
    else if (frameSubType == 0x0A) {
      stats.disassocCount++;
    }
  }
  // Data frames (type = 2)
  else if (frameType == 0x02) {
    stats.dataCount++;
    addOrUpdateDevice(addr2, pkt->rx_ctrl.rssi, currentChannel);
  }
  // Action frames (type = 3)
  else if (frameType == 0x03) {
    stats.actionCount++;
    addOrUpdateDevice(addr2, pkt->rx_ctrl.rssi, currentChannel);
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

// ===== RGB LED =====
void setRGBColor(uint8_t r, uint8_t g, uint8_t b) {
  neopixelWrite(RGB_PIN, r, g, b);
}

void updateRGB() {
  if (stats.attackDetected) {
    // Red flash on attack
    setRGBColor((millis() % 500 < 250) ? 50 : 0, 0, 0);
  } else if (stats.activeDeviceCount > 15) {
    // Red - high activity
    setRGBColor(30, 0, 0);
  } else if (stats.activeDeviceCount > 5) {
    // Yellow - medium activity
    setRGBColor(30, 30, 0);
  } else if (stats.activeDeviceCount > 0) {
    // Green - low activity
    setRGBColor(0, 30, 0);
  } else {
    // Blue - idle
    setRGBColor(0, 0, 20);
  }
}

// ===== Display Functions =====
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
  
  tft.init(LCD_WIDTH, LCD_HEIGHT);
  
  ledcAttach(TFT_BL, 1000, 10);
  ledcWrite(TFT_BL, 1023);
  
  tft.setRotation(0);
  tft.fillScreen(COLOR_BG);
  delay(100);
  
  // Splash screen
  tft.setTextColor(COLOR_PRIMARY);
  tft.setTextSize(3);
  tft.setCursor(10, 100);
  tft.println("WiFi");
  tft.setCursor(20, 140);
  tft.println("Sniffer");
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT);
  tft.setCursor(20, 180);
  tft.println("Integrated Analysis");
  delay(2000);
  
  tft.fillScreen(COLOR_BG);
}

void updateDisplay() {
  // Title
  static bool titleDrawn = false;
  if (!titleDrawn) {
    tft.setTextSize(2);
    tft.setTextColor(COLOR_PRIMARY);
    tft.setCursor(5, 5);
    tft.println("WiFi Sniffer");
    titleDrawn = true;
  }
  
  // Active devices count (large)
  static uint16_t prevActive = 0;
  if (stats.activeDeviceCount != prevActive) {
    tft.fillRect(10, 35, 150, 40, COLOR_BG);
    tft.setTextSize(4);
    uint16_t color = COLOR_GOOD;
    if (stats.activeDeviceCount > 15) color = COLOR_ALERT;
    else if (stats.activeDeviceCount > 5) color = COLOR_WARNING;
    
    tft.setTextColor(color);
    String devStr = String(stats.activeDeviceCount);
    tft.setCursor(50, 40);
    tft.println(devStr);
    
    tft.setTextSize(1);
    tft.setTextColor(COLOR_TEXT);
    tft.setCursor(70, 75);
    tft.print("devices");
    prevActive = stats.activeDeviceCount;
  }
  
  // Attack alert
  static bool prevAttack = false;
  if (stats.attackDetected != prevAttack || stats.attackDetected) {
    tft.fillRect(0, 85, 172, 30, COLOR_BG);
    if (stats.attackDetected) {
      tft.fillRect(0, 85, 172, 30, COLOR_ALERT);
      tft.setTextColor(COLOR_BG);
      tft.setTextSize(1);
      tft.setCursor(10, 92);
      tft.println("! ATTACK DETECTED !");
      tft.setCursor(20, 102);
      tft.println(stats.attackType);
    }
    prevAttack = stats.attackDetected;
  }
  
  // Statistics
  uint8_t yStart = stats.attackDetected ? 125 : 100;
  static uint8_t prevYStart = 100;
  
  if (yStart != prevYStart) {
    tft.fillRect(0, 100, 172, 100, COLOR_BG);
    prevYStart = yStart;
  }
  
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT);
  
  // Compact stats display with clearing
  // Line 1: Channel and Packets
  tft.fillRect(5, yStart, 162, 10, COLOR_BG);
  tft.setCursor(5, yStart);
  tft.printf("CH:%d | Pkt:%lu", currentChannel, stats.totalPackets);
  
  // Line 2: Probe Requests and Probe Responses
  tft.fillRect(5, yStart + 12, 162, 10, COLOR_BG);
  tft.setCursor(5, yStart + 12);
  tft.printf("PB:%u | PR:%u", stats.probeReqCount, stats.probeRespCount);
  
  // Line 3: Deauth and Disassoc
  tft.fillRect(5, yStart + 24, 162, 10, COLOR_BG);
  tft.setCursor(5, yStart + 24);
  tft.printf("DE:%u | DA:%u", stats.deauthCount, stats.disassocCount);
  
  // Line 4: Beacons and Actions
  tft.fillRect(5, yStart + 36, 162, 10, COLOR_BG);
  tft.setCursor(5, yStart + 36);
  tft.printf("Beacons:%u | Act:%u", stats.beaconCount, stats.actionCount);
  
  // Graph area
  tft.drawLine(0, yStart + 50, 172, yStart + 50, COLOR_TEXT);
  tft.setCursor(5, yStart + 55);
  tft.setTextSize(1);
  tft.setTextColor(COLOR_PRIMARY);
  tft.print("Recent Activity");
  
  // Simple bar chart for active devices over time
  for (int i = 0; i < 30; i++) {
    int barHeight = (deviceHistory[i] * 30) / 20;
    if (barHeight > 30) barHeight = 30;
    tft.drawLine(10 + i * 5, yStart + 85, 10 + i * 5, yStart + 85 - barHeight, COLOR_PRIMARY);
  }
}

// ===== Serial Output for Laptop =====
void sendDataToLaptop() {
  // Header
  Serial.print("$DATA|");
  
  // Basic stats
  Serial.printf("DEV:%d|ACT:%d|PKT:%lu|",
    stats.uniqueDeviceCount, stats.activeDeviceCount, stats.totalPackets);
  
  // Attack info
  Serial.printf("ATTACK:%d|TYPE:%s|",
    stats.attackDetected ? 1 : 0, stats.attackType.c_str());
  
  // Frame counts
  Serial.printf("BEA:%u|PRQ:%u|PRR:%u|DEA:%u|",
    stats.beaconCount, stats.probeReqCount, stats.probeRespCount, stats.deauthCount);
  
  // Send top devices by signal strength
  Serial.print("DEVICES:");
  int devCount = 0;
  for (auto& pair : deviceMap) {
    if (devCount >= 15) break; // Send top 15 devices
    if (pair.second.isActive) {
      Serial.printf("%s,%d,%lu,%d,%u;",
        pair.first.c_str(),              // MAC (std::string)
        pair.second.lastRssi,            // RSSI value
        pair.second.packetCount,         // Packet count
        pair.second.lastChannel,         // Channel
        pair.second.probeReqCount);      // Probe requests
      devCount++;
    }
  }
  
  Serial.println("|END");
}

// ===== Setup =====
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n=================================");
  Serial.println("ESP32-C6 Integrated WiFi Sniffer");
  Serial.println("=================================\n");
  
  pinMode(RGB_PIN, OUTPUT);
  setRGBColor(0, 0, 255);
  
  Serial.println("Initializing display...");
  initDisplay();
  Serial.println("Display ready!");
  
  // WiFi initialization
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
  
  // Set callback
  err = esp_wifi_set_promiscuous_rx_cb(&promiscuous_rx_cb);
  if (err != ESP_OK) {
    Serial.printf("ERROR: Failed to set callback: %d\n", err);
  }
  
  // Set initial channel
  err = esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);
  if (err != ESP_OK) {
    Serial.printf("ERROR: Failed to set channel: %d\n", err);
  }
  
  // Set filter for management and data frames
  wifi_promiscuous_filter_t filter;
  filter.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA;
  esp_wifi_set_promiscuous_filter(&filter);
  
  Serial.println("\n================================");
  Serial.println("Monitoring for WiFi activity...");
  Serial.println("================================\n");
  
  lastChannelHop = millis();
  lastDisplayUpdate = millis();
  lastSerialUpdate = millis();
  lastRGBUpdate = millis();
  
  setRGBColor(0, 50, 0);
}

// ===== Main Loop =====
void loop() {
  unsigned long currentMillis = millis();
  
  // RGB update
  if (currentMillis - lastRGBUpdate >= 100) {
    updateRGB();
    lastRGBUpdate = currentMillis;
  }
  
  // Channel hopping
  if (currentMillis - lastChannelHop >= CHANNEL_HOP_INTERVAL) {
    changeChannel();
    lastChannelHop = currentMillis;
  }
  
  // Update device status and detect attacks
  if (currentMillis - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL) {
    updateDeviceStatus();
    detectAttacks();
    updateGraphData();
    updateDisplay();
    
    // Debug serial output
    Serial.printf("Active:%d | Total:%d | CH:%d | Deauth:%d | Probe:%d\n",
      stats.activeDeviceCount, stats.uniqueDeviceCount, currentChannel,
      stats.deauthCount, stats.probeReqCount);
    
    // Reset counters
    stats.beaconCount = 0;
    stats.probeReqCount = 0;
    stats.probeRespCount = 0;
    stats.deauthCount = 0;
    stats.disassocCount = 0;
    stats.dataCount = 0;
    stats.actionCount = 0;
    stats.totalPackets = 0;
    
    lastDisplayUpdate = currentMillis;
  }
  
  // Send detailed data to laptop
  if (currentMillis - lastSerialUpdate >= SERIAL_UPDATE_INTERVAL) {
    sendDataToLaptop();
    lastSerialUpdate = currentMillis;
  }
  
  delay(1);
}

void updateGraphData() {
  deviceHistory[historyIndex] = stats.activeDeviceCount;
  historyIndex = (historyIndex + 1) % 60;
  
  attackHistory[attackHistoryIndex] = stats.attackDetected ? 1 : 0;
  attackHistoryIndex = (attackHistoryIndex + 1) % 60;
}
