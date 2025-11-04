/*
 * ESP32-C6 WiFi Security Monitor
 * Monitors WiFi networks for various attacks including:
 * - Deauthentication attacks
 * - Beacon spam
 * - Unusual channel activity
 * 
 * Display: Shows real-time graphs and statistics on attached LCD
 * 
 * Hardware Requirements:
 * - ESP32-C6 board with 1.47" LCD (ST7789, 172x320)
 * 
 * Libraries needed:
 * - Adafruit_GFX
 * - Adafruit_ST7789 (for 1.47" LCD)
 * - esp_wifi (built-in)
 */

#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <nvs_flash.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h> // ST7789 for 1.47" LCD
#include <SPI.h>

// ===== Display Configuration for ESP32-C6 1.47" LCD =====
// Correct pinout from official demo code
#define LCD_WIDTH          172
#define LCD_HEIGHT         320
#define TFT_MISO           5    // MISO - GPIO5
#define TFT_MOSI           6    // MOSI - GPIO6
#define TFT_SCLK           7    // SCLK - GPIO7
#define TFT_CS             14   // CS - GPIO14
#define TFT_DC             15   // DC - GPIO15
#define TFT_RST            21   // RST - GPIO21
#define TFT_BL             22   // Backlight - GPIO22

// RGB LED Configuration (WS2812)
#define RGB_PIN            8    // RGB LED - GPIO8
#define NUM_LEDS           1

// Display object for ST7789 1.47" LCD
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// ===== WiFi Monitoring Configuration =====
#define MAX_CHANNELS 13
#define CHANNEL_HOP_INTERVAL 250  // ms
#define GRAPH_UPDATE_INTERVAL 1000 // ms
#define MAX_SSIDS 50
#define DEAUTH_THRESHOLD 5  // Deauth packets per second to trigger alert (reduced for better detection)

// ===== Data Structures =====
struct PacketStats {
  uint32_t beaconCount;
  uint32_t deauthCount;
  uint32_t disassocCount;
  uint32_t probeReqCount;
  uint32_t probeRespCount;
  uint32_t dataCount;
  uint32_t totalPackets;
};

struct ChannelStats {
  uint32_t packetCount;
  uint32_t beaconCount;
  uint16_t deauthRate;  // packets per second
};

struct SSIDInfo {
  char ssid[33];
  uint32_t beaconCount;
  int8_t rssi;
  uint8_t channel;
};

// ===== Global Variables =====
PacketStats stats = {0};
ChannelStats channelStats[MAX_CHANNELS] = {0};
SSIDInfo ssidList[MAX_SSIDS] = {0};
uint8_t ssidCount = 0;

uint8_t currentChannel = 1;
unsigned long lastChannelHop = 0;
unsigned long lastGraphUpdate = 0;
unsigned long lastStatsReset = 0;
unsigned long lastRGBUpdate = 0;

uint32_t deauthHistory[60] = {0};  // Last 60 seconds of deauth counts
uint8_t deauthHistoryIndex = 0;
uint32_t beaconHistory[60] = {0};  // Last 60 seconds of beacon counts
uint8_t beaconHistoryIndex = 0;

bool attackDetected = false;
String attackType = "";

// RGB LED variables
uint8_t rgbHue = 0;
uint8_t rgbBrightness = 50;

// ===== Color Definitions =====
#define COLOR_BACKGROUND ST77XX_BLACK
#define COLOR_TEXT       ST77XX_WHITE
#define COLOR_GRAPH      ST77XX_CYAN
#define COLOR_ALERT      ST77XX_RED
#define COLOR_WARNING    ST77XX_YELLOW
#define COLOR_NORMAL     ST77XX_GREEN

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

// ===== Promiscuous Mode Callback =====
void IRAM_ATTR promiscuous_rx_cb(void *buf, wifi_promiscuous_pkt_type_t type) {
  // Only process management frames
  if (type != WIFI_PKT_MGMT) return;

  wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;
  const uint8_t *frame = pkt->payload;
  const uint16_t frame_ctrl = (frame[1] << 8) | frame[0];
  
  // Extract frame type and subtype
  const uint8_t frameType = (frame_ctrl & 0x000C) >> 2;
  const uint8_t frameSubType = (frame_ctrl & 0x00F0) >> 4;

  stats.totalPackets++;
  if (currentChannel > 0 && currentChannel <= MAX_CHANNELS) {
    channelStats[currentChannel - 1].packetCount++;
  }

  // Management frames (type = 0)
  if (frameType == 0x00) {
    
    // Beacon Frame (Subtype: 0x08)
    if (frameSubType == 0x08) {
      stats.beaconCount++;
      if (currentChannel > 0 && currentChannel <= MAX_CHANNELS) {
        channelStats[currentChannel - 1].beaconCount++;
      }
      
      // Parse SSID from beacon frame
      // Management frame structure: MAC Header (24 bytes) + Frame Body
      // Beacon body: Timestamp(8) + Interval(2) + Capability(2) + IEs
      // SSID IE: Tag(1) + Length(1) + SSID
      const uint8_t *ie_start = frame + 36; // Skip MAC header + fixed beacon fields
      uint16_t ie_offset = 0;
      uint16_t frame_len = pkt->rx_ctrl.sig_len;
      
      // Look for SSID IE (tag 0)
      while ((36 + ie_offset + 2) < frame_len) {
        uint8_t ie_tag = ie_start[ie_offset];
        uint8_t ie_len = ie_start[ie_offset + 1];
        
        if (ie_tag == 0) { // SSID IE
          if (ie_len > 0 && ie_len <= 32 && (36 + ie_offset + 2 + ie_len) <= frame_len) {
            char ssid[33];
            memcpy(ssid, &ie_start[ie_offset + 2], ie_len);
            ssid[ie_len] = '\0';
            
            // Only process visible SSIDs (not hidden)
            bool is_printable = true;
            for (int i = 0; i < ie_len; i++) {
              if (ssid[i] < 32 || ssid[i] > 126) {
                is_printable = false;
                break;
              }
            }
            
            if (is_printable && ie_len > 0) {
              // Update SSID list
              bool found = false;
              for (int i = 0; i < ssidCount; i++) {
                if (strcmp(ssidList[i].ssid, ssid) == 0) {
                  ssidList[i].beaconCount++;
                  ssidList[i].rssi = pkt->rx_ctrl.rssi;
                  ssidList[i].channel = currentChannel;
                  found = true;
                  break;
                }
              }
              
              if (!found && ssidCount < MAX_SSIDS) {
                strcpy(ssidList[ssidCount].ssid, ssid);
                ssidList[ssidCount].beaconCount = 1;
                ssidList[ssidCount].rssi = pkt->rx_ctrl.rssi;
                ssidList[ssidCount].channel = currentChannel;
                ssidCount++;
              }
            }
          }
          break; // SSID found, stop searching
        }
        
        ie_offset += ie_len + 2;
        if (ie_offset > 200) break; // Safety check
      }
    }
    
    // Deauthentication Frame (Subtype: 0x0C)
    else if (frameSubType == 0x0C) {
      stats.deauthCount++;
      if (currentChannel > 0 && currentChannel <= MAX_CHANNELS) {
        channelStats[currentChannel - 1].deauthRate++;
      }
    }
    
    // Disassociation Frame (Subtype: 0x0A)
    else if (frameSubType == 0x0A) {
      stats.disassocCount++;
    }
    
    // Probe Request (Subtype: 0x04)
    else if (frameSubType == 0x04) {
      stats.probeReqCount++;
    }
    
    // Probe Response (Subtype: 0x05)
    else if (frameSubType == 0x05) {
      stats.probeRespCount++;
    }
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

// ===== Attack Detection =====
void detectAttacks() {
  attackDetected = false;
  
  // Deauth attack detection (more sensitive)
  if (stats.deauthCount >= DEAUTH_THRESHOLD) {
    attackDetected = true;
    attackType = "DEAUTH!";
  }
  
  // Beacon spam detection (lowered threshold for better detection)
  // Detecting more than 15 unique SSIDs or excessive beacons
  if (ssidCount > 15 || stats.beaconCount > 80) {
    uint8_t highBeaconCount = 0;
    for (int i = 0; i < ssidCount; i++) {
      if (ssidList[i].beaconCount > 5) {
        highBeaconCount++;
      }
    }
    if (highBeaconCount > 10 || ssidCount > 20) {
      attackDetected = true;
      attackType = "BEACON SPAM!";
    }
  }
  
  // Check for unusual disassociation activity
  if (stats.disassocCount > 5) {
    attackDetected = true;
    attackType = "DISASSOC!";
  }
}

// ===== RGB LED Functions =====
void setRGBColor(uint8_t r, uint8_t g, uint8_t b) {
  // Using NeoPixel-compatible timing for WS2812
  neopixelWrite(RGB_PIN, r, g, b);
}

void updateRGBFade() {
  // Rainbow fade effect
  rgbHue += 2;
  if (rgbHue >= 255) rgbHue = 0;
  
  // Convert HSV to RGB
  uint8_t r, g, b;
  if (attackDetected) {
    // Flash red on attack
    r = (millis() % 500 < 250) ? 255 : 0;
    g = 0;
    b = 0;
  } else {
    // Rainbow fade
    hsvToRgb(rgbHue, 255, rgbBrightness, &r, &g, &b);
  }
  
  setRGBColor(r, g, b);
}

void hsvToRgb(uint8_t h, uint8_t s, uint8_t v, uint8_t* r, uint8_t* g, uint8_t* b) {
  uint8_t region, remainder, p, q, t;
  
  if (s == 0) {
    *r = v;
    *g = v;
    *b = v;
    return;
  }
  
  region = h / 43;
  remainder = (h - (region * 43)) * 6;
  
  p = (v * (255 - s)) >> 8;
  q = (v * (255 - ((s * remainder) >> 8))) >> 8;
  t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;
  
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
void initDisplay() {
  // Initialize pins
  pinMode(TFT_CS, OUTPUT);
  pinMode(TFT_DC, OUTPUT);
  pinMode(TFT_RST, OUTPUT);
  pinMode(TFT_BL, OUTPUT);
  
  // Setup SPI with correct pins (SCLK, MISO, MOSI)
  SPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI);
  
  // Hardware reset
  digitalWrite(TFT_RST, HIGH);
  delay(50);
  digitalWrite(TFT_RST, LOW);
  delay(50);
  digitalWrite(TFT_RST, HIGH);
  delay(150);
  
  // Initialize ST7789 1.47" LCD
  tft.init(LCD_WIDTH, LCD_HEIGHT);
  
  // Enable backlight with PWM for smooth control (new LEDC API)
  ledcAttach(TFT_BL, 1000, 10);  // Pin, 1kHz, 10-bit resolution
  ledcWrite(TFT_BL, 1023);  // Full brightness
  
  tft.setRotation(0);  // Portrait mode
  tft.fillScreen(ST77XX_BLACK);
  delay(100);
  
  // Display "Project Cell" splash screen
  tft.setTextColor(ST77XX_CYAN);
  tft.setTextSize(3);
  tft.setCursor(15, 120);
  tft.println("Project");
  tft.setCursor(40, 150);
  tft.println("Cell");
  delay(2000);
  
  tft.fillScreen(ST77XX_BLACK);
  delay(200);
  
  // Display startup message
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(30, 100);
  tft.println("WiFi");
  tft.setCursor(15, 130);
  tft.println("Monitor");
  tft.setCursor(10, 170);
  tft.setTextSize(1);
  tft.println("Initializing...");
  delay(1000);
  tft.fillScreen(ST77XX_BLACK);
}

void drawGraph(uint32_t* data, uint8_t dataSize, uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint16_t color, String label) {
  // Draw border
  tft.drawRect(x, y, width, height, COLOR_TEXT);
  
  // Draw label
  tft.setCursor(x + 2, y - 8);
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT);
  tft.print(label);
  
  // Find max value for scaling
  uint32_t maxVal = 1;
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
static uint8_t prevChannel = 0;
static uint8_t prevSsidCount = 0;
static uint32_t prevDeauthCount = 0;
static uint32_t prevBeaconCount = 0;
static uint32_t prevTotalPackets = 0;
static bool prevAttackDetected = false;
static String prevAttackType = "";
static bool firstUpdate = true;

void updateDisplay() {
  // Draw static elements only once
  if (firstUpdate) {
    tft.fillScreen(COLOR_BACKGROUND);
    
    // Title - draw once
    tft.setTextSize(2);
    tft.setTextColor(COLOR_TEXT);
    tft.setCursor(15, 5);
    tft.println("WiFi Monitor");
    
    // Labels - draw once
    tft.setTextSize(1);
    tft.setCursor(5, 28);
    tft.print("CH:");
    tft.setCursor(90, 28);
    tft.print("APs:");
    
    // Statistics labels
    uint8_t yPos = 58;
    tft.setCursor(5, yPos);
    tft.print("Deauth:");
    tft.setCursor(5, yPos + 20);
    tft.print("Beacons:");
    tft.setCursor(5, yPos + 40);
    tft.print("Total:");
    
    firstUpdate = false;
  }
  
  // Update channel ONLY if changed
  if (currentChannel != prevChannel) {
    tft.fillRect(35, 26, 50, 16, COLOR_BACKGROUND);
    tft.setTextSize(2);
    tft.setTextColor(COLOR_TEXT);
    tft.setCursor(35, 28);
    tft.print(currentChannel);
    prevChannel = currentChannel;
  }
  
  // Update SSID count ONLY if changed
  if (ssidCount != prevSsidCount) {
    tft.fillRect(120, 26, 50, 16, COLOR_BACKGROUND);
    tft.setTextSize(2);
    tft.setTextColor(COLOR_TEXT);
    tft.setCursor(120, 28);
    tft.print(ssidCount);
    prevSsidCount = ssidCount;
  }
  
  // Update attack alert ONLY if changed
  if (attackDetected != prevAttackDetected || (attackDetected && attackType != prevAttackType)) {
    tft.fillRect(0, 50, 172, 24, COLOR_BACKGROUND);
    if (attackDetected) {
      tft.fillRect(0, 50, 172, 24, COLOR_ALERT);
      tft.setTextColor(COLOR_BACKGROUND);
      tft.setTextSize(2);
      tft.setCursor(10, 56);
      tft.print(attackType);
      tft.setTextColor(COLOR_TEXT);
      prevAttackType = attackType;
    }
    prevAttackDetected = attackDetected;
  }
  
  // Calculate statistics position (shifts down if attack detected)
  uint8_t yPos = attackDetected ? 82 : 58;
  static uint8_t prevYPos = 58;
  
  // If position changed, redraw all statistics labels
  if (yPos != prevYPos) {
    tft.fillRect(0, 58, 172, 120, COLOR_BACKGROUND);
    tft.setTextSize(1);
    tft.setTextColor(COLOR_TEXT);
    tft.setCursor(5, yPos);
    tft.print("Deauth:");
    tft.setCursor(5, yPos + 20);
    tft.print("Beacons:");
    tft.setCursor(5, yPos + 40);
    tft.print("Total:");
    prevYPos = yPos;
    // Force redraw of all values
    prevDeauthCount = 0xFFFFFFFF;
    prevBeaconCount = 0xFFFFFFFF;
    prevTotalPackets = 0xFFFFFFFF;
  }
  
  // Update statistics ONLY if changed
  tft.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
  
  if (stats.deauthCount != prevDeauthCount) {
    tft.fillRect(60, yPos - 3, 110, 16, COLOR_BACKGROUND);
    tft.setTextSize(2);
    tft.setCursor(60, yPos - 3);
    tft.print(stats.deauthCount);
    prevDeauthCount = stats.deauthCount;
  }
  
  if (stats.beaconCount != prevBeaconCount) {
    tft.fillRect(60, yPos + 17, 110, 16, COLOR_BACKGROUND);
    tft.setTextSize(2);
    tft.setCursor(60, yPos + 17);
    tft.print(stats.beaconCount);
    prevBeaconCount = stats.beaconCount;
  }
  
  if (stats.totalPackets != prevTotalPackets) {
    tft.fillRect(60, yPos + 37, 110, 16, COLOR_BACKGROUND);
    tft.setTextSize(2);
    tft.setCursor(60, yPos + 37);
    tft.print(stats.totalPackets);
    prevTotalPackets = stats.totalPackets;
  }
  
  // Redraw graphs (smooth animation)
  uint8_t graphY = yPos + 62;
  tft.fillRect(5, graphY - 10, 162, 115, COLOR_BACKGROUND);
  
  tft.setTextSize(1);
  drawGraph(deauthHistory, 30, 5, graphY, 162, 45, COLOR_ALERT, "Deauth/s");
  drawGraph(beaconHistory, 30, 5, graphY + 58, 162, 45, COLOR_GRAPH, "Beacon/s");
}

void updateGraphData() {
  // Update deauth history
  deauthHistory[deauthHistoryIndex] = stats.deauthCount;
  deauthHistoryIndex = (deauthHistoryIndex + 1) % 60;
  
  // Update beacon history
  beaconHistory[beaconHistoryIndex] = stats.beaconCount;
  beaconHistoryIndex = (beaconHistoryIndex + 1) % 60;
}

// ===== Setup =====
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n=================================");
  Serial.println("ESP32-C6 WiFi Security Monitor");
  Serial.println("=================================\n");
  
  // Initialize RGB LED
  pinMode(RGB_PIN, OUTPUT);
  setRGBColor(0, 0, 255);  // Blue on startup
  
  // Initialize display
  Serial.println("Initializing display...");
  initDisplay();
  Serial.println("Display initialized!");
  
  // Initialize WiFi in promiscuous mode - CRITICAL SEQUENCE
  Serial.println("\nInitializing WiFi subsystem...");
  
  // Method 1: Using WiFi library (recommended for Arduino)
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  
  // Enable promiscuous mode
  Serial.println("Enabling promiscuous mode...");
  esp_err_t err = esp_wifi_set_promiscuous(true);
  if (err != ESP_OK) {
    Serial.printf("ERROR: Failed to enable promiscuous mode: %d\n", err);
  } else {
    Serial.println("Promiscuous mode enabled successfully!");
  }
  
  // Set promiscuous callback
  err = esp_wifi_set_promiscuous_rx_cb(&promiscuous_rx_cb);
  if (err != ESP_OK) {
    Serial.printf("ERROR: Failed to set promiscuous callback: %d\n", err);
  } else {
    Serial.println("Promiscuous callback registered!");
  }
  
  // Set initial channel
  err = esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);
  if (err != ESP_OK) {
    Serial.printf("ERROR: Failed to set channel: %d\n", err);
  } else {
    Serial.printf("Channel set to %d\n", currentChannel);
  }
  
  // Set promiscuous filter to only management frames
  wifi_promiscuous_filter_t filter;
  filter.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT;
  err = esp_wifi_set_promiscuous_filter(&filter);
  if (err != ESP_OK) {
    Serial.printf("WARNING: Failed to set filter: %d\n", err);
  } else {
    Serial.println("Filter set to management frames only");
  }
  
  Serial.println("\n=================================");
  Serial.println("Monitoring WiFi traffic...");
  Serial.println("=================================\n");
  
  lastChannelHop = millis();
  lastGraphUpdate = millis();
  lastStatsReset = millis();
  lastRGBUpdate = millis();
  
  // Set initial RGB to green (ready)
  setRGBColor(0, 50, 0);
}

// ===== Main Loop =====
void loop() {
  unsigned long currentMillis = millis();
  
  // Update RGB LED fade (every 50ms)
  if (currentMillis - lastRGBUpdate >= 50) {
    updateRGBFade();
    lastRGBUpdate = currentMillis;
  }
  
  // Channel hopping
  if (currentMillis - lastChannelHop >= CHANNEL_HOP_INTERVAL) {
    changeChannel();
    lastChannelHop = currentMillis;
  }
  
  // Update display
  if (currentMillis - lastGraphUpdate >= GRAPH_UPDATE_INTERVAL) {
    detectAttacks();
    updateGraphData();
    updateDisplay();
    
    // Print stats to serial
    Serial.printf("CH:%d | Beacons:%d | Deauth:%d | SSIDs:%d | Total:%d\n",
                  currentChannel, stats.beaconCount, stats.deauthCount, 
                  ssidCount, stats.totalPackets);
    
    // Reset per-second counters
    stats.beaconCount = 0;
    stats.deauthCount = 0;
    stats.disassocCount = 0;
    stats.probeReqCount = 0;
    stats.probeRespCount = 0;
    
    lastGraphUpdate = currentMillis;
  }
  
  // Reset SSID list periodically (every 30 seconds)
  if (currentMillis - lastStatsReset >= 30000) {
    ssidCount = 0;
    memset(ssidList, 0, sizeof(ssidList));
    lastStatsReset = currentMillis;
  }
  
  delay(1);  // Small delay for stability
}
