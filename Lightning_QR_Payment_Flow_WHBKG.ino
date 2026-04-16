// 🚀 STACKSWORTH_MATRIX_MASTER USING OUR SATONAK API
// Built By BitcoinManor.com
// v2.0.69 - CRITICAL PRE-SHIP HARDENING 
// - 🛡️ OTA TO LOOP: Moved OTA execution OUT of AsyncWebServer callback into main loop (prevents lwIP issues)
// - 🔋 BOOT POWER REDUCTION: Keep brightness at 1 during boot, restore after server starts (brownout mitigation)
// - ⚡ ANIMATION SHORTENED: Reduced boot animation from 10 to 5 loops (lower current draw during startup)
// - 🔒 PENDING FLAG: OTA now uses pendingOTA flag checked in loop() instead of direct callback execution
// - 💡 SAFER UPDATES: Long-running OTA HTTP/streaming/restart now in safe loop context, not async callback
// Base: v2.0.68 (reset logging, complete network protection, WiFi guards, fail-fast error handling)
// Previous v2.0.65 fix (preserved):
// - ✅ Removed manual watchdog init - ESP32 Arduino core manages it automatically
// Previous v2.0.64 fix (preserved):
// - ✅ Removed WDT reset from showRebootMessages() - fixed settings save crash
// Previous v2.0.63 fixes (all preserved):
// - ⏱️ BLOCK FIX: Height checks every 2 min (prevents stale block display)
// - 💾 SPIFFS FIX: Auto-format on mount failure
// - 🧠 SMART BOOT: Fetches only enabled metrics
// - 🚨 Emergency MAX7219 shutdown executes FIRST
// - Device naming, mDNS, OTA, all portal features preserved
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include <WiFiManager.h> 
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include "esp_task_wdt.h"
#include "Font_Data.h" // Optional, if using custom fonts
#include "time.h"
#include <FS.h>
#include <SPIFFS.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <WiFi.h>
#include <esp_wifi.h>  // Needed for esp_read_mac
#include "esp_system.h"
#include <Update.h>       // OTA updates (safe, low-level library)
// #include <HTTPUpdate.h>   // ❌ REMOVED: Causes lwIP crashes with AsyncWebServer
#include <DNSServer.h>
DNSServer dnsServer;
#include <Preferences.h>
Preferences prefs;
#include <ESPmDNS.h>  // 🌐 For mDNS hostname resolution (Matrix.local)

// 🧠 Memory Management Constants
#define MIN_HEAP_REQUIRED 160000  // Minimum free heap (bytes) required before API calls

// retrieve and store the MAC
String getShortMAC() {
  uint8_t mac[6];
  esp_wifi_get_mac(WIFI_IF_STA, mac);  // Get STA interface MAC
  char shortID[7];
  sprintf(shortID, "%02X%02X%02X", mac[3], mac[4], mac[5]);  // last 3 bytes (6 hex chars)
  return String(shortID);
}

String macID;



bool wifiConnected = false;
bool buttonPressed = false;
volatile bool pendingReboot = false;
bool rebootPhaseShown = false;
unsigned long rebootAt = 0;
bool apMode = false;
bool pendingOTA = false;  // Flag to trigger OTA from loop() instead of callback
bool apMsgShown = false;
bool initialFetchDone = false;  // Track if we've done first data fetch
bool hasEverConnected = false;  // Track if WiFi has ever successfully connected
unsigned long wifiDisconnectedAt = 0;  // Track when WiFi first disconnected
const unsigned long WIFI_FALLBACK_TIMEOUT = 6UL * 60UL * 60UL * 1000UL;  // 6 hours before falling back to AP mode (was 1 hour)

String savedSSID;
String savedPassword;
String savedCity;
String savedCurrency;  // 🌍 New: User's preferred currency (USD, EUR, etc.)
String savedTheme;     // 🎨 New: User's preferred theme (scroll, fade)
String savedTopText;   // 📝 New: User's custom top row message (max 10 chars)
String savedBottomText;// 📝 New: User's custom bottom row message (max 10 chars)
String savedTempUnit;  // 🌡️ New: User's preferred temperature unit (C or F)
String savedDeviceName;// 🆔 New: User's device nickname (for multi-unit households)
uint8_t savedBrightness = 2; // 💡 New: User's brightness preference (1-15)
// 📊 Display options: 0=Block, 1=Miner, 3=Price, 8=Fee, 10=Time/City, 11=Day/Date, 14=Custom (7 core defaults)
bool displayEnabled[15] = {true, true, false, true, false, false, false, false, true, false, true, true, false, false, true}; // 7 core metrics enabled by default
int savedTimezone = -99;


// Fetch and Display Cycles
uint8_t fetchCycle = 0;   // 👈 for rotating which API we fetch
uint8_t displayCycle = 0; // 👈 for rotating which screen we show

// initializes the server so we can later attach our custom HTML page routes
AsyncWebServer server(80);

static WiFiClient httpClient;

// 🐕 WDT Helper: Yield to WiFi/RTOS tasks
// NOTE: No explicit watchdog reset - ESP32 Arduino core handles WDT automatically
static inline void feedWDT() {
  delay(1);  // yield to WiFi/RTOS tasks
}

// ⛏️💥 BITCOIN MINER BOOT ANIMATION: Pickaxe mining Bitcoin!
// Pickaxe swings forward to SHATTER Bitcoin symbols (no miner body - just the tool!)
// 4-frame animation: pickaxe swing cycle (up → forward → down → strike)
// 🎮 PACMAN BOOT ANIMATION: Bitcoin symbols being eaten by PacMan!
// 4-frame animation with combined Bitcoin + PacMan sprite (18 bytes wide)
const uint8_t pacman[4][18] = {
  { 0xfe, 0x73, 0xfb, 0x7f, 0xf3, 0x7b, 0xfe, 0x00, 0x00, 0x00, 0x3c, 0x7e, 0x7e, 0xff, 0xe7, 0xc3, 0x81, 0x00 },
  { 0xfe, 0x7b, 0xf3, 0x7f, 0xfb, 0x73, 0xfe, 0x00, 0x00, 0x00, 0x3c, 0x7e, 0xff, 0xff, 0xe7, 0xe7, 0x42, 0x00 },
  { 0xfe, 0x73, 0xfb, 0x7f, 0xf3, 0x7b, 0xfe, 0x00, 0x00, 0x00, 0x3c, 0x7e, 0xff, 0xff, 0xff, 0xe7, 0x66, 0x24 },
  { 0xfe, 0x7b, 0xf3, 0x7f, 0xf3, 0x7b, 0xfe, 0x00, 0x00, 0x00, 0x3c, 0x7e, 0xff, 0xff, 0xff, 0xff, 0x7e, 0x3c },
};
const uint8_t PACMAN_DATA_WIDTH = (sizeof(pacman[0])/sizeof(pacman[0][0]));

// Removed explosion pattern - not needed for PacMan animation
const uint8_t explosion[3][7] = {
  { 0x08, 0x14, 0x22, 0x41, 0x22, 0x14, 0x08 },  // Frame 0: small burst
  { 0x49, 0x2A, 0x14, 0x00, 0x14, 0x2A, 0x49 },  // Frame 1: expanding
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },  // Frame 2: cleared
};

// 🌍 API Endpoints & Configuration
const char* FIRMWARE_VERSION = "v2.0.69";
const char* UPDATE_URL = "https://satonak.bitcoinmanor.com/firmware/stacksworth.bin";

// API endpoints for fallback services  
const char *BTC_API = "https://api.coingecko.com/api/v3/simple/price?ids=bitcoin&vs_currencies=usd&include_24hr_change=true";
const char *BLOCK_API = "https://blockchain.info/q/getblockcount";
const char *FEES_API = "https://mempool.space/api/v1/fees/recommended";
const char *MEMPOOL_BLOCKS_API = "https://mempool.space/api/blocks";
const char *BLOCKSTREAM_TX_API_BASE = "https://blockstream.info/api/block/";

// ===== SatoNak API (authoritative) =====
#define USE_SATONAK_PRICE 1    // 1 = use SatoNak for price, 0 = keep old source

static const char* SATONAK_BASE   = "https://satonak.bitcoinmanor.com";
static const char* SATONAK_PRICE  = "/api/price";   // supports ?fiat=EUR etc.
static const char* SATONAK_HEIGHT = "/api/height";  // (for later)
static const char* SATONAK_MINER  = "/api/miner";   // (for later)

// OTA Update endpoints (fully functional as of v2.0.60)
const char* FIRMWARE_URL  = "https://satonak.bitcoinmanor.com/firmware/stacksworth.bin";
const char* VERSION_CHECK_URL = "https://satonak.bitcoinmanor.com/api/version";

// Staggered fetch intervals (ms)
const unsigned long INTERVAL_BLOCK_HEIGHT = 5UL * 60UL * 1000UL; // 5 min
const unsigned long INTERVAL_MINER       = 6UL * 60UL * 1000UL; // 6 min
const unsigned long INTERVAL_PRICE       = 7UL * 60UL * 1000UL; // 7 min
const unsigned long INTERVAL_CHANGE24H   = 8UL * 60UL * 1000UL; // 8 min
const unsigned long INTERVAL_FEE         = 9UL * 60UL * 1000UL; // 9 min
const unsigned long INTERVAL_HASHRATE    = 10UL * 60UL * 1000UL; // 10 min
const unsigned long INTERVAL_CIRC_SUPPLY = 11UL * 60UL * 1000UL; // 11 min
const unsigned long INTERVAL_ATH         = 12UL * 60UL * 1000UL; // 12 min
const unsigned long INTERVAL_DAYS_ATH    = 13UL * 60UL * 1000UL; // 13 min

// Last fetch timestamps
unsigned long lastBlockHeightFetch   = 0;
unsigned long lastMinerFetch        = 0;
unsigned long lastPriceFetch        = 0;
unsigned long lastFeeFetch          = 0;
unsigned long lastHashrateFetch     = 0;
unsigned long lastCircSupplyFetch   = 0;
unsigned long lastAthFetch          = 0;
unsigned long lastDaysAthFetch      = 0;
unsigned long lastChange24hFetch    = 0;

// default fiat (can be "USD", "EUR", etc.) - now loaded from preferences
static String getCurrentFiatCode() {
  return savedCurrency.length() > 0 ? savedCurrency : "USD";
}

// Get currency symbol for display
static String getCurrencySymbol() {
  String fiat = getCurrentFiatCode();
  if (fiat == "USD") return "$";
  if (fiat == "CAD") return "$";       // Clean $ since top row shows "CAD PRICE"
  if (fiat == "EUR") return "";        // Clean number since top row shows "EUR PRICE"  
  if (fiat == "GBP") return "";        // Clean number since top row shows "GBP PRICE"
  if (fiat == "JPY") return "";        // Clean number since top row shows "JPY PRICE" (avoid encoding issues)
  if (fiat == "AUD") return "$";       // Clean $ since top row shows "AUD PRICE"
  if (fiat == "CHF") return "";        // Clean number since top row shows "CHF PRICE"
  if (fiat == "CNY") return "";        // Clean number since top row shows "CNY PRICE"
  if (fiat == "SEK") return "";        // Clean number since top row shows "SEK PRICE"
  if (fiat == "NOK") return "";        // Clean number since top row shows "NOK PRICE"
  return ""; // fallback to no symbol
}

static inline String satonakUrl(const char* path, const char* fiat = nullptr) {
  String u = String(SATONAK_BASE) + String(path);
  if (fiat && fiat[0] != '\0') {
    u += "?fiat="; u += fiat;
  }
  return u;
}

// 🎨 Get animation effects based on user's theme preference
static void getThemeEffects(textEffect_t &effectIn, textEffect_t &effectOut) {
  if (savedTheme == "fade") {
    Serial.println("🎨 Using WIPE theme (power-efficient alternative to fade)");
    effectIn = PA_WIPE_CURSOR;
    effectOut = PA_WIPE_CURSOR;
  } else {
    // Default to scroll theme (safe fallback)
    Serial.println("🎨 Using SCROLL theme");
    effectIn = PA_SCROLL_LEFT;
    effectOut = PA_SCROLL_LEFT;
  }
}


// ---- Smash Buy phrases split into TOP / BOTTOM lines ----
const char* PHRASES[][2] = {
  { "SMASH",           "BUY!" },
  { "DON'T TRUST",     "VERIFY!" },
  { "STACK",           "SATS" },
  { "RUN A",           "NODE" },
  { "NOT YOUR",        "KEYS" },
  { "FIX THE",         "MONEY" },
  { "STAY",            "HUMBLE" },
  { "OPT",             "OUT" },
  { "LOW TIME",       "PREFERENCE" },  // fun variation
  { "INFINITE",        "GAME" },
  { "HARDER",          "MONEY" },
  { "BITCOIN",         "> FIAT" },
  { "LET'S",            "GO" }
};
#define NUM_PHRASES (sizeof(PHRASES) / sizeof(PHRASES[0]))

String mapWeatherCode(int code)
{
  if (code == 0)
    return "Sunny";
  else if (code == 1)
    return "Mostly Sunny";
  else if (code == 2)
    return "Partly Cloudy";
  else if (code == 3)
    return "Cloudy";
  else if (code >= 45 && code <= 48)
    return "Foggy";
  else if (code >= 51 && code <= 57)
    return "Drizzle";
  else if (code >= 61 && code <= 67)
    return "Rain";
  else if (code >= 71 && code <= 77)
    return "Snowy";
  else if (code >= 80 && code <= 82)
    return "Showers";
  else if (code >= 85 && code <= 86)
    return "Snow Showers";
  else if (code >= 95 && code <= 99)
    return "Thunderstorm";
  else
    return "Unknown";
}

// Time Config
const char *ntpServer = "pool.ntp.org";
// 🌍 Timezone strings that auto-handle DST (no more manual updates needed!)
const char* TIMEZONE_STRINGS[] = {
  "UTC0",                                    // UTC +0
  "GMT0BST,M3.5.0/1,M10.5.0",              // London +0/+1
  "CET-1CEST,M3.5.0,M10.5.0/3",            // Paris/Berlin +1/+2
  "EET-2EEST,M3.5.0/3,M10.5.0/4",          // Helsinki +2/+3
  "MSK-3",                                  // Moscow +3 (no DST)
  "JST-9",                                  // Tokyo +9 (no DST)
  "AEST-10AEDT,M10.1.0,M4.1.0/3",          // Sydney +10/+11
  "NZST-12NZDT,M9.5.0,M4.1.0/3",           // Auckland +12/+13
  "HST10",                                  // Hawaii -10 (no DST)
  "AKST9AKDT,M3.2.0,M11.1.0",              // Alaska -9/-8
  "PST8PDT,M3.2.0,M11.1.0",                // Pacific -8/-7
  "MST7MDT,M3.2.0,M11.1.0",                // Mountain -7/-6
  "CST6CDT,M3.2.0,M11.1.0",                // Central -6/-5
  "EST5EDT,M3.2.0,M11.1.0"                 // Eastern -5/-4
};

const char* TIMEZONE_NAMES[] = {
  "UTC (+0)", "London (+0/+1)", "Paris (+1/+2)", "Helsinki (+2/+3)",
  "Moscow (+3)", "Tokyo (+9)", "Sydney (+10/+11)", "Auckland (+12/+13)",
  "Hawaii (-10)", "Alaska (-9/-8)", "Pacific (-8/-7)", "Mountain (-7/-6)",
  "Central (-6/-5)", "Eastern (-5/-4)"
};

#define NUM_TIMEZONES (sizeof(TIMEZONE_STRINGS) / sizeof(TIMEZONE_STRINGS[0]))

// Global Data Variables
int btcPrice = 0, blockHeight = 0, feeRate = 0, satsPerDollar = 0;
char btcText[16], blockText[16], feeText[16], satsText[16], satsText2[16];
char timeText[16], dateText[16], dayText[16];
char hashrateText[16];  // New global for hashrate display
char circSupplyText[16];  // New global for circulating supply top line  
char circPercentText[16]; // New global for circulating supply bottom line
float latitude = 0.0;
float longitude = 0.0;
String weatherCondition = "Unknown";
int temperature = 0;
float btcChange24h = 0.0;
char changeText[16];
String minerName = "Unknown";
String hashrate = "Unknown";  // New global for hashrate data
String circSupply = "Unknown"; // New global for circulating supply data
String athPrice = "Unknown";   // New global for ATH price data
String daysSinceAth = "Unknown"; // New global for days since ATH data
char athText[16];              // New global for ATH price display
char daysAthText[16];          // New global for days since ATH display

String formatWithCommas(int number)
{
  String numStr = String(number);
  String result = "";
  int len = numStr.length();
  for (int i = 0; i < len; i++)
  {
    if (i > 0 && (len - i) % 3 == 0)
      result += ",";
    result += numStr[i];
  }
  return result;
}

// Save successful values to cache for fallback use
void saveDisplayCache() {
  prefs.begin("cache", false);  // write mode
  prefs.putString("btcText", String(btcText));
  prefs.putString("blockText", String(blockText));
  prefs.putString("feeText", String(feeText));
  prefs.putString("satsText", String(satsText));
  prefs.putString("changeText", String(changeText));
  prefs.putString("athText", String(athText));
  prefs.putString("daysAthText", String(daysAthText));
  prefs.putString("hashrateText", String(hashrateText));
  prefs.putString("circSupplyText", String(circSupplyText));
  prefs.putString("minerName", minerName);
  prefs.end();
  Serial.println("💾 Display cache saved");
}

// LED Matrix Config
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_ZONES 2
#define ZONE_SIZE 8
#define MAX_DEVICES (MAX_ZONES * ZONE_SIZE)
#define SCROLL_SPEED 20
#define FETCH_INTERVAL 120000

#define ZONE_LOWER 0
#define ZONE_UPPER 1

#define CLK_PIN 18
#define DATA_PIN 23
#define CS_PIN 5
#define BUTTON_PIN 25   //Pin for Smash Buy Button

MD_Parola P = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);
// Brightness: 0 = dimmest, 15 = brightest
uint8_t BRIGHTNESS = 2;

// Function to adjust brightness
void setBrightness(uint8_t level) {
  if (level > 15) level = 15;  // Clamp to max
  BRIGHTNESS = level;
  
  // Set intensity for each zone individually (required for multi-zone displays)
  P.setIntensity(ZONE_UPPER, BRIGHTNESS);
  P.setIntensity(ZONE_LOWER, BRIGHTNESS);
  
  // Save brightness setting to preferences
  prefs.begin("stacksworth", false);
  prefs.putUChar("brightness", BRIGHTNESS);
  prefs.end();
  
  Serial.printf("💡 Brightness set to: %d/15 for all zones\n", BRIGHTNESS);
}

// Function to cycle brightness (for potential button control)
void cycleBrightness() {
  BRIGHTNESS = (BRIGHTNESS + 3) % 16;  // Step by 3 for noticeable changes
  if (BRIGHTNESS == 0) BRIGHTNESS = 3; // Don't go completely dark
  setBrightness(BRIGHTNESS);
}
unsigned long lastFetchTime = 0;
uint8_t cycle = 0;             // 🔥 Needed for animation control
unsigned long lastApiCall = 0; // 🔥 Needed for fetch timing
unsigned long lastMemoryCheck = 0;
unsigned long lastWeatherUpdate = 0;
unsigned long lastNTPUpdate = 0;

const unsigned long WEATHER_UPDATE_INTERVAL = 30UL * 60UL * 1000UL; // 30 minutes
const unsigned long NTP_UPDATE_INTERVAL = 10UL * 60UL * 1000UL;     // 10 minutes
const unsigned long MEMORY_CHECK_INTERVAL = 5UL * 60UL * 1000UL;    // 5 minutes

const uint32_t BTC_INTERVAL     = 300000;   // 5 min
const uint32_t FEE_INTERVAL     = 300000;   // 5 min
const uint32_t BLOCK_INTERVAL   = 120000;   // 2 min (blocks vary 1-20 min)
const uint32_t WEATHER_INTERVAL = 1800000;  // 30 min

const uint32_t FEE_OFFSET     =  90000;   // +1.5 min after BTC
const uint32_t BLOCK_OFFSET   = 180000;   // +3   min after BTC
const uint32_t WEATHER_OFFSET =  60000;   // +1   min after BTC

static uint32_t lastBTC = 0, lastFee = 0, lastBlock = 0, lastWeather = 0;
static uint32_t bootMs = 0;


// Pre Connection Message for home users
void showPreConnectionMessage()
{
  static uint8_t step = 0;
  static unsigned long lastUpdate = 0;

  if (millis() - lastUpdate < 2500)
    return; // Wait for 2.5 seconds between steps
  lastUpdate = millis();

  switch (step)
  {
  case 0:
    P.displayZoneText(ZONE_UPPER, "ENTER THE", PA_CENTER, 0, 2500, PA_FADE, PA_FADE);
    P.displayZoneText(ZONE_LOWER, "MATRIX", PA_CENTER, 0, 2500, PA_FADE, PA_FADE);
    break;
  case 1:
    P.displayZoneText(ZONE_UPPER, "Connect Your", PA_CENTER, 0, 2000, PA_FADE, PA_FADE);
    P.displayZoneText(ZONE_LOWER, "Device Inside", PA_CENTER, 0, 2000, PA_FADE, PA_FADE);
    break;
  case 2:
    P.displayZoneText(ZONE_UPPER, "WiFi Settings", PA_CENTER, 0, 2000, PA_FADE, PA_FADE);
    P.displayZoneText(ZONE_LOWER, "Labelled", PA_CENTER, 0, 2000, PA_FADE, PA_FADE);
    break;
  case 3:
    P.displayZoneText(ZONE_UPPER, "SW-MATRIX", PA_CENTER, 0, 2000, PA_FADE, PA_FADE);
    P.displayZoneText(ZONE_LOWER, "******", PA_CENTER, 0, 2000, PA_FADE, PA_FADE);
    break;
  case 4:
    P.displayZoneText(ZONE_UPPER, "OR TYPE", PA_CENTER, 0, 2000, PA_FADE, PA_FADE);
    P.displayZoneText(ZONE_LOWER, "192.168.4.1", PA_CENTER, 0, 2000, PA_FADE, PA_FADE);
    break;
  case 5:
    P.displayZoneText(ZONE_UPPER, "SETUP WiFi", PA_CENTER, 0, 2000, PA_FADE, PA_FADE);
    P.displayZoneText(ZONE_LOWER, "and hit SAVE", PA_CENTER, 0, 2000, PA_FADE, PA_FADE);
    break;
  default:
    step = 0; // Reset the sequence
    return;
  }

  step++;
}

 //Load Saved WiFi + City + Timezone on Boot
void loadSavedSettingsAndConnect() {
  prefs.begin("stacksworth", true);  

  savedSSID = prefs.getString("ssid", "");
  savedPassword = prefs.getString("password", "");
  savedCity = prefs.getString("city", "");
  savedCurrency = prefs.getString("currency", "USD");  // 🌍 Default to USD
  savedTheme = prefs.getString("theme", "scroll");     // 🎨 Default to scroll
  savedTopText = prefs.getString("toptext", "");       // 📝 Custom top row message
  savedBottomText = prefs.getString("bottomtext", ""); // 📝 Custom bottom row message
  savedTempUnit = prefs.getString("tempunit", "C");    // 🌡️ Default to Celsius
  savedDeviceName = prefs.getString("devicename", ""); // 🆔 User's device nickname
  savedBrightness = prefs.getUChar("brightness", 2);  // 💡 Load brightness preference (default 2)
  savedTimezone = prefs.getInt("timezone", -99);
  BRIGHTNESS = savedBrightness;                        // 💡 Apply saved brightness
  
  // 📊 Load display options for each case (default all enabled)
  for (int i = 0; i < 15; i++) {
    String key = "show" + String(i);
    displayEnabled[i] = prefs.getUChar(key.c_str(), 1) == 1;  // Default to 1 (enabled)
  }

  prefs.end();

  if (savedSSID != "" && savedPassword != "") {
    Serial.println("✅ Found Saved WiFi Credentials:");
    Serial.println("SSID: " + savedSSID);
    Serial.println("Password: (hidden)");
    Serial.println("City: " + savedCity);
    Serial.println("Currency: " + savedCurrency);        // 🌍 New
    Serial.println("Theme: " + savedTheme);              // 🎨 New
    Serial.println("Custom Top: " + savedTopText);       // 📝 New
    Serial.println("Custom Bottom: " + savedBottomText); // 📝 New
    Serial.println("Temperature Unit: " + savedTempUnit); // 🌡️ New
    Serial.println("Device Name: " + savedDeviceName);   // 🆔 New
    Serial.printf("Brightness: %d/15\n", BRIGHTNESS);    // 💡 New
    Serial.print("Timezone offset (hours): ");
    Serial.println(savedTimezone);

    WiFi.mode(WIFI_STA);
    WiFi.begin(savedSSID.c_str(), savedPassword.c_str());

    Serial.print("🔌 Connecting to WiFi...");
    unsigned long startAttemptTime = millis();

    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 30000) {
      Serial.print(".");
      // esp_task_wdt_reset(); // ❌ v2.0.66: Disabled - ESP32 Arduino core handles WDT automatically
      delay(500);
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\n✅ Connected to WiFi successfully!");
      Serial.print("🌍 IP Address: ");
      Serial.println(WiFi.localIP());
      wifiConnected = true; // 👉 set this!!
      hasEverConnected = true; // 👉 Remember we've connected successfully
      wifiDisconnectedAt = 0; // Reset disconnection timer
      WiFi.setAutoReconnect(true); // Enable auto-reconnect for normal operation
      
      // 🔄 v2.0.66: Removed boot-time auto update check
      // Updates are now manual-only via portal button for cleaner boot and reduced network dependency
      Serial.println("💡 OTA updates available at http://matrix.local (manual updates only)");
      
      // 🌍 Configure timezone using proper timezone strings (auto-handles DST!)
      if (savedTimezone != -99 && savedTimezone >= 0 && savedTimezone < NUM_TIMEZONES) {
        const char* tzString = TIMEZONE_STRINGS[savedTimezone];
        configTzTime(tzString, ntpServer);
        Serial.printf("🕒 Timezone configured: %s (%s)\n", TIMEZONE_NAMES[savedTimezone], tzString);
      } else {
        // Default to Mountain Time if no valid timezone saved
        configTzTime(TIMEZONE_STRINGS[11], ntpServer); // Mountain Time
        Serial.println("🕒 Using default Mountain Time timezone");
      }
    } else {
      Serial.println("\n❌ Failed to connect to WiFi, falling back to Access Point...");
      WiFi.persistent(false); // Don't save WiFi config to flash
      startAccessPoint();
    }
  } else {
    Serial.println("⚠️ No saved WiFi credentials found, starting Access Point...");
    WiFi.persistent(false); // Don't save WiFi config to flash
    startAccessPoint();
  }
}

// 🔄 OTA Update Functions

// Display update progress on Matrix
void showUpdateProgress(int percentage) {
  static char progressText[20];
  snprintf(progressText, sizeof(progressText), "UPDATE %d%%", percentage);
  P.displayClear();
  P.displayZoneText(ZONE_UPPER, "UPDATING", PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
  P.displayZoneText(ZONE_LOWER, progressText, PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
  P.displayReset(ZONE_UPPER);
  P.displayReset(ZONE_LOWER);
  P.displayAnimate();
  Serial.printf("📥 Update progress: %d%%\n", percentage);
}

// ✅ OTA UPDATE - Using Update.h directly (no HTTPUpdate library conflicts)
bool performOTAUpdate(const char* firmwareURL) {
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("❌ OTA aborted: No WiFi connection.");
    return false;
  }
  
  Serial.println("🚀 Starting OTA update...");
  Serial.printf("📥 Downloading from: %s\n", firmwareURL);
  
  // Show update starting on display
  P.displayClear();
  P.displayZoneText(ZONE_UPPER, "OTA UPDATE", PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
  P.displayZoneText(ZONE_LOWER, "STARTING", PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
  P.displayReset(ZONE_UPPER);
  P.displayReset(ZONE_LOWER);
  for (int i = 0; i < 10; i++) { P.displayAnimate(); delay(10); }
  
  HTTPClient http;
  http.setTimeout(15000);
  
  if (!http.begin(firmwareURL)) {
    Serial.println("❌ Failed to connect to update server");
    P.displayZoneText(ZONE_LOWER, "CONN FAIL", PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
    P.displayReset(ZONE_LOWER);
    for (int i = 0; i < 20; i++) { P.displayAnimate(); delay(50); }
    return false;
  }
  
  int httpCode = http.GET();
  
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("❌ HTTP GET failed: %d\n", httpCode);
    http.end();
    P.displayZoneText(ZONE_LOWER, "DL FAILED", PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
    P.displayReset(ZONE_LOWER);
    for (int i = 0; i < 20; i++) { P.displayAnimate(); delay(50); }
    return false;
  }
  
  int contentLength = http.getSize();
  Serial.printf("📦 Firmware size: %d bytes\n", contentLength);
  
  if (contentLength <= 0) {
    Serial.println("❌ Invalid content length");
    http.end();
    return false;
  }
  
  // Begin OTA update
  if (!Update.begin(contentLength)) {
    Serial.printf("❌ Not enough space for OTA: %s\n", Update.errorString());
    http.end();
    P.displayZoneText(ZONE_LOWER, "NO SPACE", PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
    P.displayReset(ZONE_LOWER);
    for (int i = 0; i < 20; i++) { P.displayAnimate(); delay(50); }
    return false;
  }
  
  WiFiClient* stream = http.getStreamPtr();
  size_t written = 0;
  uint8_t buff[128];
  int lastPercent = -1;
  
  Serial.println("📥 Downloading firmware...");
  
  while (http.connected() && (written < contentLength)) {
    size_t available = stream->available();
    
    if (available) {
      int bytesToRead = ((available > sizeof(buff)) ? sizeof(buff) : available);
      int bytesRead = stream->readBytes(buff, bytesToRead);
      
      size_t bytesWritten = Update.write(buff, bytesRead);
      written += bytesWritten;
      
      // Update progress every 10%
      int percent = (written * 100) / contentLength;
      if (percent != lastPercent && percent % 10 == 0) {
        Serial.printf("📥 Progress: %d%%\n", percent);
        showUpdateProgress(percent);
        lastPercent = percent;
      }
      
      delay(10); // Yield during download
    }
    delay(1);
  }
  
  http.end();
  
  if (written != contentLength) {
    Serial.printf("❌ Written only %d/%d bytes\n", written, contentLength);
    Update.abort();
    P.displayZoneText(ZONE_LOWER, "INCOMPLETE", PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
    P.displayReset(ZONE_LOWER);
    for (int i = 0; i < 20; i++) { P.displayAnimate(); delay(50); }
    return false;
  }
  
  if (!Update.end()) {
    Serial.printf("❌ Update failed: %s\n", Update.errorString());
    P.displayZoneText(ZONE_LOWER, "FAILED", PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
    P.displayReset(ZONE_LOWER);
    for (int i = 0; i < 20; i++) { P.displayAnimate(); delay(50); }
    return false;
  }
  
  if (!Update.isFinished()) {
    Serial.println("❌ Update not finished");
    return false;
  }
  
  Serial.println("✅ OTA Update successful!");
  Serial.println("🔄 Rebooting in 3 seconds...");
  
  P.displayClear();
  P.displayZoneText(ZONE_UPPER, "UPDATE OK", PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
  P.displayZoneText(ZONE_LOWER, "REBOOTING", PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
  P.displayReset(ZONE_UPPER);
  P.displayReset(ZONE_LOWER);
  for (int i = 0; i < 50; i++) { P.displayAnimate(); delay(20); }
  
  delay(3000);
  ESP.restart();
  
  return true;
}

// Check if new version is available (safe - just checks version number)
String checkForUpdates() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠️ WiFi not connected, cannot check for updates");
    return "";
  }
  
  HTTPClient http;
  http.setTimeout(5000);
  
  Serial.printf("🔍 Checking for updates at: %s\n", VERSION_CHECK_URL);
  
  feedWDT(); // Feed watchdog before network call
  if (!http.begin(VERSION_CHECK_URL)) {
    Serial.println("❌ Failed to connect to update server");
    return "";
  }
  
  feedWDT(); // Feed watchdog before GET
  int httpCode = http.GET();
  feedWDT(); // Feed watchdog after GET
  
  if (httpCode == 200) {
    String latestVersion = http.getString();
    latestVersion.trim();
    
    Serial.printf("📋 Current version: %s\n", FIRMWARE_VERSION);
    Serial.printf("📋 Latest version: %s\n", latestVersion.c_str());
    
    http.end();
    return latestVersion;
  } else {
    Serial.printf("❌ HTTP error: %d\n", httpCode);
    http.end();
    return "";
  }
}

  
    // Access Point Code
    void startAccessPoint()
    {
      Serial.println("🚀 Starting Access Point...");
      WiFi.mode(WIFI_OFF); // Completely turn off WiFi first
      delay(100);
      WiFi.disconnect(true); // Disconnect and disable auto-reconnect
      WiFi.setAutoReconnect(false); // Explicitly disable auto-reconnect
      delay(100); // Give it a moment to fully disconnect
      WiFi.mode(WIFI_AP); // Now set to AP-only mode
      macID = getShortMAC();  // Store globally
      String ssid = "SW-MATRIX-" + getShortMAC();
      WiFi.softAP(ssid.c_str());

      apMode = true;
      apMsgShown = false;


      IPAddress myIP = WiFi.softAPIP();
      Serial.print("🌍 AP IP address: ");
      Serial.println(myIP);
      Serial.print("📶 AP SSID: ");
      Serial.println(ssid); // Helpful for debug

      // DNS Captive portal
      dnsServer.start(53, "*", myIP);
      Serial.println("🚀 DNS Server started for captive portal.");
    }

    // FETCH FUNCTIONS
    void fetchBitcoinData() {
  // Try SatoNak first, then fallback to CoinGecko
  if (fetchPriceFromSatoNak()) {
    Serial.println("✅ Bitcoin price fetched from SatoNak");
    return;
  }
  
  Serial.println("⚠️ SatoNak failed, trying CoinGecko fallback");
  
  // ✅ GUARD: Don't attempt CoinGecko unless WiFi is actually connected
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("🌐 WiFi not connected; skipping CoinGecko fallback");
    return;
  }

  if (ESP.getFreeHeap() < MIN_HEAP_REQUIRED) {
    Serial.println("❌ Not enough heap to safely fetch. Skipping BTC fetch.");
    return;
  }
  Serial.println("🔄 Fetching BTC Price from CoinGecko...");
  HTTPClient http;
  http.setTimeout(2000);
  http.setConnectTimeout(1500);
  
  feedWDT(); // Feed watchdog before network call
  http.begin(BTC_API);
  
  feedWDT(); // Feed watchdog after begin
  // Double-check WiFi right before blocking GET call
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠️ WiFi dropped before GET - aborting CoinGecko");
    http.end();
    return;
  }
  
  feedWDT(); // 🔥 CRITICAL: Feed watchdog BEFORE potentially slow GET call
  int httpCode = http.GET();
  feedWDT(); // 🔥 CRITICAL: Feed watchdog IMMEDIATELY after GET returns
  
  if (httpCode == 200) {
    feedWDT(); // Feed watchdog before JSON parsing
    DynamicJsonDocument doc(512);
    deserializeJson(doc, http.getString());
    feedWDT(); // Feed watchdog after JSON parsing
    btcPrice = doc["bitcoin"]["usd"];
    btcChange24h = doc["bitcoin"]["usd_24h_change"];
    satsPerDollar = 100000000 / btcPrice;

    String symbol = getCurrencySymbol();
    String currentFiat = getCurrentFiatCode();
    
    // Note: CoinGecko fallback only provides USD, so if user wants other currency,
    // they'll need to wait for SatoNak to come back online for FX conversion
    if (currentFiat == "USD") {
      sprintf(btcText, "$%s", formatWithCommas(btcPrice).c_str());
      sprintf(satsText, "$1=%d Sats", satsPerDollar);
    } else {
      sprintf(btcText, "$%s*", formatWithCommas(btcPrice).c_str()); // * indicates USD fallback
      sprintf(satsText, "$1=%d Sats*", satsPerDollar); // * shows it's USD fallback
    }
    sprintf(satsText2, "%d Sats", satsPerDollar);
    snprintf(changeText, sizeof(changeText), "%+.2f%%", btcChange24h);

    Serial.printf("✅ Updated BTC Price: $%d | Sats per $: %d\n", btcPrice, satsPerDollar);
    Serial.printf("✅ BTC Price: %s (%s)\n", btcText, satsText);
  } else {
    Serial.printf("❌ CoinGecko GET failed (%d)\n", httpCode);
  }
  http.end();
  Serial.printf("📈 Free heap after fetch: %d bytes\n", ESP.getFreeHeap());
}

// Returns true on success, false on any failure (so callers can fallback)
bool fetchPriceFromSatoNak() {
  if (ESP.getFreeHeap() < MIN_HEAP_REQUIRED) {
    Serial.println("❌ Low heap; skipping SatoNak price fetch");
    return false;
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("🌐 WiFi not connected; skipping SatoNak price fetch");
    return false;
  }

  String currentFiat = getCurrentFiatCode();
  String full = satonakUrl(SATONAK_PRICE, currentFiat.c_str()); // e.g. /api/price?fiat=EUR
  Serial.print("🌐 GET "); Serial.println(full);

  HTTPClient http;
  http.setTimeout(2000);      // Reduced to 2s to stay well under 12s WDT
  http.setConnectTimeout(1500);
  http.useHTTP10(true);
  http.setReuse(false);

  feedWDT();
  if (!http.begin(full)) {
    Serial.println("❌ http.begin failed (SatoNak)");
    http.end(); // ⚠️ CRITICAL: Always call end() even on begin() failure to free resources
    return false;
  }

  feedWDT();
  // Double-check WiFi right before blocking GET call
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠️ WiFi dropped before GET - aborting");
    http.end();
    return false;
  }
  int rc = http.GET();
  feedWDT();
  if (rc != 200) {
    Serial.printf("❌ SatoNak price GET failed (%d)\n", rc);
    http.end();
    return false;
  }

  String payload = http.getString();
  feedWDT();
  http.end();

  // Check if payload is plain text (just a number) vs JSON
  payload.trim();
  if (payload.length() > 0 && payload.length() < 16 && isdigit(payload.charAt(0))) {
    // Plain text response - just a price number like "103605.00"
    double px = payload.toFloat();
    if (px > 0) {
      btcPrice = (int)round(px);
      satsPerDollar = (int)(100000000.0 / px);
      
      String symbol = getCurrencySymbol();
      snprintf(btcText, sizeof(btcText), "%s%s", symbol.c_str(), formatWithCommas(btcPrice).c_str());
      
      // 🌍 Show sats per user's currency, not always USD!
      sprintf(satsText, "%s1=%d Sats", symbol.c_str(), satsPerDollar);
      sprintf(satsText2, "%d Sats", satsPerDollar);
      
      Serial.printf("✅ SatoNak Price (plain): %s | Sats/$: %d | Free heap: %d\n",
                    btcText, satsPerDollar, ESP.getFreeHeap());
      return true;
    }
  }

  // Try parsing as JSON
  DynamicJsonDocument doc(1536);
  DeserializationError e = deserializeJson(doc, payload);
  if (e) {
    Serial.printf("❌ SatoNak JSON parse error: %s\n", e.c_str());
    Serial.println("↪︎ Payload (trim): " + payload.substring(0, 220));
    return false;
  }

  // Respect your current fiat setting (e.g., "USD"/"EUR"/"CAD")
  String key = currentFiat; key.toLowerCase();

  double px = 0.0;
  if (doc.containsKey("price") && doc["price"].is<JsonObject>()) {
    if (doc["price"][key].is<double>()) px = (double)doc["price"][key];
    else if (doc["price"][key].is<long>()) px = (double)((long)doc["price"][key]);
  }
  if (px <= 0.0) {
    if (doc[key].is<double>()) px = (double)doc[key];
    else if (doc[key].is<long>()) px = (double)((long)doc[key]);
  }
  if (px <= 0.0) {
    Serial.println("❌ SatoNak: no valid price in payload");
    Serial.println("↪︎ Payload (trim): " + payload.substring(0, 220));
    return false;
  }

  double change = 0.0;
  if (doc["change_24h"].is<double>()) change = (double)doc["change_24h"];

  long sps = 0;
  if (doc["sats_per_usd"].is<long>()) sps = (long)doc["sats_per_usd"];
  if (sps == 0 && key == "usd") sps = (long)(100000000.0 / px);

  // Update your existing globals/buffers (exact names as in your sketch)
  btcPrice      = (int)round(px);
  btcChange24h  = (float)change;
  satsPerDollar = (int)sps;

  String symbol = getCurrencySymbol();
  
  if (currentFiat == "USD") {
    snprintf(btcText, sizeof(btcText), "$%s", formatWithCommas(btcPrice).c_str());
  } else {
    snprintf(btcText, sizeof(btcText), "%s%s", symbol.c_str(), formatWithCommas(btcPrice).c_str());
  }
  
  // 🌍 Show sats per user's selected currency!
  sprintf(satsText,   "%s1=%d Sats", symbol.c_str(), satsPerDollar);
  sprintf(satsText2,  "%d Sats", satsPerDollar);
  snprintf(changeText, sizeof(changeText), "%+.2f%%", btcChange24h);

  Serial.printf("✅ SatoNak Price: %s | 24h: %+.2f%% | Sats/$: %d | Free heap: %d\n",
                btcText, btcChange24h, satsPerDollar, ESP.getFreeHeap());
  return true;
}

// Fetch miner info from SatoNak API
bool fetchMinerFromSatoNak() {
  if (ESP.getFreeHeap() < MIN_HEAP_REQUIRED) {
    Serial.println("❌ Low heap; skipping SatoNak miner fetch");
    return false;
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("🌐 WiFi not connected; skipping SatoNak miner fetch");
    return false;
  }

  String full = String(SATONAK_BASE) + String(SATONAK_MINER);
  Serial.print("🌐 GET "); Serial.println(full);

  HTTPClient http;
  http.setTimeout(2000);
  http.setConnectTimeout(1500); 
  http.useHTTP10(true);
  http.setReuse(false);

  feedWDT();
  if (!http.begin(full)) {
    Serial.println("❌ http.begin failed (SatoNak miner)");
    http.end(); // ⚠️ CRITICAL: Always call end() even on begin() failure to free resources
    return false;
  }

  feedWDT();
  // Double-check WiFi right before blocking GET call
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(" WiFi dropped before GET - aborting");
    http.end();
    return false;
  }
  int rc = http.GET();
  feedWDT();
  if (rc != 200) {
    Serial.printf("❌ SatoNak miner GET failed (%d)\n", rc);
    http.end();
    return false;
  }

  String payload = http.getString();
  feedWDT();
  http.end();

  // For simple text response, just use the payload directly
  payload.trim();
  if (payload.length() > 0 && payload.length() < 32) {
    minerName = payload;
    Serial.printf("✅ SatoNak Miner: %s | Free heap: %d\n", minerName.c_str(), ESP.getFreeHeap());
    return true;
  } else {
    Serial.println("❌ SatoNak miner: invalid response");
    Serial.println("↪︎ Payload: " + payload.substring(0, 100));
    return false;
  }
}

// Fetch block height from SatoNak API
bool fetchHeightFromSatoNak() {
  if (ESP.getFreeHeap() < MIN_HEAP_REQUIRED) {
    Serial.println("❌ Low heap; skipping SatoNak height fetch");
    return false;
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("🌐 WiFi not connected; skipping SatoNak height fetch");
    return false;
  }

  String full = String(SATONAK_BASE) + String(SATONAK_HEIGHT);
  Serial.print("🌐 GET "); Serial.println(full);

  HTTPClient http;
  http.setTimeout(2000);
  http.setConnectTimeout(1500);
  http.useHTTP10(true);
  http.setReuse(false);

  feedWDT();
  if (!http.begin(full)) {
    Serial.println("❌ http.begin failed (SatoNak height)");
    http.end(); // ⚠️ CRITICAL: Always call end() even on begin() failure to free resources
    return false;
  }

  feedWDT();
  // Double-check WiFi right before blocking GET call
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(" WiFi dropped before GET - aborting");
    http.end();
    return false;
  }
  int rc = http.GET();
  feedWDT();
  if (rc != 200) {
    Serial.printf("❌ SatoNak height GET failed (%d)\n", rc);
    http.end();
    return false;
  }

  String payload = http.getString();
  feedWDT();
  http.end();

  // For simple text response, parse as integer
  payload.trim();
  int newHeight = payload.toInt();
  if (newHeight > 0 && newHeight > blockHeight - 100) { // sanity check
    blockHeight = newHeight;
    sprintf(blockText, "%d", blockHeight);
    Serial.printf("✅ SatoNak Height: %d | Free heap: %d\n", blockHeight, ESP.getFreeHeap());
    return true;
  } else {
    Serial.println("❌ SatoNak height: invalid response");
    Serial.println("↪︎ Payload: " + payload.substring(0, 50));
    return false;
  }
}


    void fetchBlockHeight()
    {
      // Try SatoNak only - no fallback to prevent WDT crashes
      // If it fails, continue with cached data
      if (fetchHeightFromSatoNak()) {
        Serial.println("✅ Block height fetched from SatoNak");
      } else {
        Serial.println("⚠️ SatoNak height fetch failed - continuing with cached data");
      }
    }

// Fetch fee rate from SatoNak API
bool fetchFeeFromSatoNak() {
  if (ESP.getFreeHeap() < MIN_HEAP_REQUIRED) {
    Serial.println("❌ Low heap; skipping SatoNak fee fetch");
    return false;
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("🌐 WiFi not connected; skipping SatoNak fee fetch");
    return false;
  }

  String full = String(SATONAK_BASE) + "/api/fee";
  Serial.print("🌐 GET "); Serial.println(full);

  HTTPClient http;
  http.setTimeout(2000);
  http.setConnectTimeout(1500); 
  http.useHTTP10(true);
  http.setReuse(false);

  if (!http.begin(full)) {
    Serial.println("❌ http.begin failed (SatoNak fee)");
    http.end(); // ⚠️ CRITICAL: Always call end() even on begin() failure to free resources
    return false;
  }

  feedWDT();
  // Double-check WiFi right before blocking GET call
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(" WiFi dropped before GET - aborting");
    http.end();
    return false;
  }
  int rc = http.GET();
  feedWDT();
  if (rc != 200) {
    Serial.printf("❌ SatoNak fee GET failed (%d)\n", rc);
    http.end();
    return false;
  }

  String payload = http.getString();
  feedWDT();
  http.end();

  // Check if payload is plain text (just a number) vs JSON
  payload.trim();
  if (payload.length() > 0 && payload.length() < 8 && isdigit(payload.charAt(0))) {
    // Plain text response - just a fee number like "15"
    int newFee = payload.toInt();
    if (newFee > 0 && newFee < 1000) { // sanity check
      feeRate = newFee;
      snprintf(feeText, sizeof(feeText), "%d sat/vB", feeRate);
      Serial.printf("✅ SatoNak Fee (plain): %d sat/vB | Free heap: %d\n", feeRate, ESP.getFreeHeap());
      return true;
    }
  }

  // Try parsing as JSON
  DynamicJsonDocument doc(512);
  DeserializationError e = deserializeJson(doc, payload);
  if (e) {
    Serial.printf("❌ SatoNak fee JSON parse error: %s\n", e.c_str());
    Serial.println("↪︎ Payload: " + payload.substring(0, 100));
    return false;
  }

  // Parse JSON response
  int newFee = 0;
  if (doc.containsKey("value")) {
    newFee = doc["value"];
  }
  
  if (newFee > 0 && newFee < 1000) { // sanity check
    feeRate = newFee;
    snprintf(feeText, sizeof(feeText), "%d sat/vB", feeRate);
    Serial.printf("✅ SatoNak Fee: %d sat/vB | Free heap: %d\n", feeRate, ESP.getFreeHeap());
    return true;
  }

  Serial.println("❌ SatoNak fee: invalid response");
  Serial.println("↪︎ Payload: " + payload.substring(0, 100));
  return false;
}

void fetchFeeRate() {
  // Try SatoNak first, then fallback to mempool.space
  if (fetchFeeFromSatoNak()) {
    Serial.println("✅ Fee rate fetched from SatoNak");
    return;
  }
  
  Serial.println("⚠️ SatoNak failed, trying mempool.space fallback");
  
  if (ESP.getFreeHeap() < MIN_HEAP_REQUIRED) {
    Serial.println("🛑 Low heap before Fee fetch; skipping");
    return;
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("🌐 WiFi not connected; skipping Fee fetch");
    return;
  }

  Serial.println("🔄 Fetching Fee Rate from mempool.space…");
  HTTPClient http;
  // short, explicit timeouts so we never stall long enough to trip WDT
  http.setTimeout(2000);         // Reduced from 3000ms
  http.setConnectTimeout(1500);  // Reduced from 2000ms 
  http.useHTTP10(true);          // simpler, avoids chunking issues
  http.setReuse(false);          // no keep-alive reuse

  feedWDT(); // Feed watchdog before network operation
  // FEES_API should be your existing endpoint string, unchanged
  if (!http.begin(httpClient, FEES_API)) {
    Serial.println("❌ http.begin failed; keeping last fee value");
    http.end(); // ⚠️ CRITICAL: Always call end() even on begin() failure to free resources
    return;
  }

  feedWDT(); // Feed watchdog after begin
  // Double-check WiFi right before blocking GET call
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠️ WiFi dropped before GET - aborting");
    http.end();
    return;
  }
  feedWDT(); // 🔥 CRITICAL: Feed watchdog BEFORE potentially slow GET call
  int rc = http.GET();
  feedWDT(); // 🔥 CRITICAL: Feed watchdog IMMEDIATELY after GET returns
  if (rc == 200) {
    feedWDT(); // Feed watchdog before JSON parsing
    String payload = http.getString();
    DynamicJsonDocument doc(512);
    DeserializationError e = deserializeJson(doc, payload);
    feedWDT(); // Feed watchdog after JSON parsing
    if (e) {
      Serial.printf("❌ Fee JSON parse error: %s; keeping last value\n", e.c_str());
    } else {
      // keep previous value if field missing
      int newFee = doc["fastestFee"] | feeRate;
      feeRate = newFee;
      // feeText should be your existing global char buffer
      snprintf(feeText, sizeof(feeText), "%d sat/vB", feeRate);
      Serial.printf("✅ Updated Fee Rate: %d sat/vB\n", feeRate);
    }
  } else {
    Serial.printf("❌ Fee GET failed (%d); keeping last value\n", rc);
  }
  http.end();
}

// Fetch hashrate from SatoNak API
bool fetchHashrateFromSatoNak() {
  if (ESP.getFreeHeap() < MIN_HEAP_REQUIRED) {
    Serial.println("❌ Low heap; skipping SatoNak hashrate fetch");
    return false;
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("🌐 WiFi not connected; skipping SatoNak hashrate fetch");
    return false;
  }

  String full = String(SATONAK_BASE) + "/api/hashrate";
  Serial.print("🌐 GET "); Serial.println(full);

  HTTPClient http;
  http.setTimeout(2000);
  http.setConnectTimeout(1500); 
  http.useHTTP10(true);
  http.setReuse(false);

  if (!http.begin(full)) {
    Serial.println("❌ http.begin failed (SatoNak hashrate)");
    http.end(); // ⚠️ CRITICAL: Always call end() even on begin() failure to free resources
    return false;
  }

  feedWDT();
  // Double-check WiFi right before blocking GET call
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(" WiFi dropped before GET - aborting");
    http.end();
    return false;
  }
  int rc = http.GET();
  feedWDT();
  if (rc != 200) {
    Serial.printf("❌ SatoNak hashrate GET failed (%d)\n", rc);
    http.end();
    return false;
  }

  String payload = http.getString();
  feedWDT();
  http.end();

  // Parse and format the hashrate number
  payload.trim();
  if (payload.length() > 0 && payload.length() < 32 && payload != "na") {
    strncpy(hashrateText, payload.c_str(), sizeof(hashrateText));
    hashrateText[sizeof(hashrateText)-1] = '\0';
    hashrate = payload; // keep if you also want the raw string elsewhere
    Serial.printf("✅ SatoNak Hashrate -> Display: %s | Free heap: %d\n",
                  hashrateText, ESP.getFreeHeap());
    return true;
  }
  
  Serial.println("❌ SatoNak hashrate: invalid response");
  Serial.println("↪︎ Payload: " + payload.substring(0, 100));
  return false;
}

// Fetch circulating supply from SatoNak API
bool fetchCircSupplyFromSatoNak() {
  if (ESP.getFreeHeap() < MIN_HEAP_REQUIRED) {
    Serial.println("❌ Low heap; skipping SatoNak circulating supply fetch");
    return false;
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("🌐 WiFi not connected; skipping SatoNak circulating supply fetch");
    return false;
  }

  String full = String(SATONAK_BASE) + "/api/circsupply";
  Serial.print("🌐 GET "); Serial.println(full);

  HTTPClient http;
  http.setTimeout(2000);
  http.setConnectTimeout(1500); 
  http.useHTTP10(true);
  http.setReuse(false);

  if (!http.begin(full)) {
    Serial.println("❌ http.begin failed (SatoNak circulating supply)");
    http.end(); // ⚠️ CRITICAL: Always call end() even on begin() failure to free resources
    return false;
  }

  feedWDT();
  // Double-check WiFi right before blocking GET call
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(" WiFi dropped before GET - aborting");
    http.end();
    return false;
  }
  int rc = http.GET();
  feedWDT();
  if (rc != 200) {
    Serial.printf("❌ SatoNak circulating supply GET failed (%d)\n", rc);
    http.end();
    return false;
  }

  String payload = http.getString();
  feedWDT();
  http.end();

  // For simple text response, parse as number
  payload.trim();
  Serial.printf("🔍 Raw circulating supply payload: '%s'\n", payload.c_str());
  
  if (payload.length() > 0 && payload.length() < 16 && payload != "na") {
    // Remove commas temporarily for parsing
    String cleanPayload = payload;
    cleanPayload.replace(",", "");
    Serial.printf("🔍 After removing commas for parsing: '%s'\n", cleanPayload.c_str());
    
    long supply = cleanPayload.toInt();
    Serial.printf("🔍 Parsed supply as: %ld\n", supply);
    
    if (supply > 0 && supply <= 21000000) { // sanity check - supply should be reasonable
      circSupply = payload; // Store original with commas for display
      
      // Format for display: actual numbers with commas
      // Top: current supply with commas (e.g., "19,942,004")
      strncpy(circSupplyText, payload.c_str(), sizeof(circSupplyText));
      circSupplyText[sizeof(circSupplyText) - 1] = '\0';
      
      // Bottom: max supply (always "21,000,000")
      strncpy(circPercentText, "/21 Million", sizeof(circPercentText));
      circPercentText[sizeof(circPercentText) - 1] = '\0';
      
      Serial.printf("✅ SatoNak Circulating Supply: %s (%s, %s) | Free heap: %d\n", 
                    circSupply.c_str(), circSupplyText, circPercentText, ESP.getFreeHeap());
      return true;
    } else {
      Serial.printf("❌ SatoNak circulating supply: supply value %ld out of range (expected 0-21000000)\n", supply);
    }
  }
  
  Serial.println("❌ SatoNak circulating supply: invalid response");
  Serial.println("↪︎ Payload: " + payload.substring(0, 100));
  return false;
}

// Fetch ATH price from SatoNak API
bool fetchAthFromSatoNak() {
  if (ESP.getFreeHeap() < MIN_HEAP_REQUIRED) {
    Serial.println("❌ Low heap; skipping SatoNak ATH fetch");
    return false;
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("🌐 WiFi not connected; skipping SatoNak ATH fetch");
    return false;
  }

  String full = String(SATONAK_BASE) + "/api/ath";
  Serial.print("🌐 GET "); Serial.println(full);

  HTTPClient http;
  http.setTimeout(2000);
  http.setConnectTimeout(1500); 
  http.useHTTP10(true);
  http.setReuse(false);

  if (!http.begin(full)) {
    Serial.println("❌ http.begin failed (SatoNak ATH)");
    http.end(); // ⚠️ CRITICAL: Always call end() even on begin() failure to free resources
    return false;
  }

  feedWDT();
  // Double-check WiFi right before blocking GET call
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(" WiFi dropped before GET - aborting");
    http.end();
    return false;
  }
  int rc = http.GET();
  feedWDT();
  if (rc != 200) {
    Serial.printf("❌ SatoNak ATH GET failed (%d)\n", rc);
    http.end();
    return false;
  }

  String payload = http.getString();
  feedWDT();
  http.end();

  // Parse ATH price - API returns plain text like "73750.07"
  payload.trim();
  if (payload.length() > 0 && payload.length() < 16 && payload != "na") {
    float athPriceNum = payload.toFloat();
    if (athPriceNum > 0) {
      athPrice = payload; // Store raw value
      
  // Format for display - add $ and format nicely with commas (e.g. $126,080)
  // Use existing helper to insert thousand separators
  snprintf(athText, sizeof(athText), "$%s", formatWithCommas((int)round(athPriceNum)).c_str());
      
      Serial.printf("✅ SatoNak ATH: %s -> Display: %s | Free heap: %d\n", 
                    athPrice.c_str(), athText, ESP.getFreeHeap());
      return true;
    }
  }
  
  Serial.println("❌ SatoNak ATH: invalid response");
  Serial.println("↪︎ Payload: " + payload.substring(0, 100));
  return false;
}

// Fetch 24H change from SatoNak API
bool fetchChange24hFromSatoNak() {
  if (ESP.getFreeHeap() < MIN_HEAP_REQUIRED) {
    Serial.println("❌ Low heap; skipping SatoNak 24H change fetch");
    return false;
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("🌐 WiFi not connected; skipping SatoNak 24H change fetch");
    return false;
  }

  String full = String(SATONAK_BASE) + "/api/change24h";
  Serial.print("🌐 GET "); Serial.println(full);

  HTTPClient http;
  http.setTimeout(2000);
  http.setConnectTimeout(1500); 
  http.useHTTP10(true);
  http.setReuse(false);

  if (!http.begin(full)) {
    Serial.println("❌ http.begin failed (SatoNak 24H change)");
    http.end(); // ⚠️ CRITICAL: Always call end() even on begin() failure to free resources
    return false;
  }

  feedWDT();
  // Double-check WiFi right before blocking GET call
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(" WiFi dropped before GET - aborting");
    http.end();
    return false;
  }
  int rc = http.GET();
  feedWDT();
  if (rc != 200) {
    Serial.printf("❌ SatoNak 24H change GET failed (%d)\n", rc);
    http.end();
    return false;
  }

  String payload = http.getString();
  feedWDT();
  http.end();

  // Parse 24H change - API returns plain text like "+1.29%" or "-2.45%"
  payload.trim();
  if (payload.length() > 0 && payload.length() < 16 && payload != "na") {
    // Extract the numeric value for internal storage
    String numStr = payload;
    numStr.replace("+", "");
    numStr.replace("%", "");
    btcChange24h = numStr.toFloat();
    
    // Store the formatted display text
    strncpy(changeText, payload.c_str(), sizeof(changeText));
    changeText[sizeof(changeText) - 1] = '\0';
    
    Serial.printf("✅ SatoNak 24H Change: %s (%.2f%%) | Free heap: %d\n", 
                  changeText, btcChange24h, ESP.getFreeHeap());
    return true;
  }
  
  Serial.println("❌ SatoNak 24H change: invalid response");
  Serial.println("↪︎ Payload: " + payload.substring(0, 100));
  return false;
}

// Fetch days since ATH from SatoNak API
bool fetchDaysSinceAthFromSatoNak() {
  if (ESP.getFreeHeap() < MIN_HEAP_REQUIRED) {
    Serial.println("❌ Low heap; skipping SatoNak days since ATH fetch");
    return false;
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("🌐 WiFi not connected; skipping SatoNak days since ATH fetch");
    return false;
  }

  String full = String(SATONAK_BASE) + "/api/days_since_ath";
  Serial.print("🌐 GET "); Serial.println(full);

  HTTPClient http;
  http.setTimeout(2000);
  http.setConnectTimeout(1500); 
  http.useHTTP10(true);
  http.setReuse(false);

  if (!http.begin(full)) {
    Serial.println("❌ http.begin failed (SatoNak days since ATH)");
    http.end(); // ⚠️ CRITICAL: Always call end() even on begin() failure to free resources
    return false;
  }

  feedWDT();
  // Double-check WiFi right before blocking GET call
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(" WiFi dropped before GET - aborting");
    http.end();
    return false;
  }
  int rc = http.GET();
  feedWDT();
  if (rc != 200) {
    Serial.printf("❌ SatoNak days since ATH GET failed (%d)\n", rc);
    http.end();
    return false;
  }

  String payload = http.getString();
  feedWDT();
  http.end();

  // Parse days since ATH - API returns plain text like "45"
  payload.trim();
  if (payload.length() > 0 && payload.length() < 8 && payload != "na") {
    int days = payload.toInt();
    if (days >= 0) {
      daysSinceAth = payload; // Store raw value
      
      // Format for display - "## Days" format for top row
      snprintf(daysAthText, sizeof(daysAthText), "%d Days", days);
      
      Serial.printf("✅ SatoNak Days Since ATH: %s -> Display: %s | Free heap: %d\n", 
                    daysSinceAth.c_str(), daysAthText, ESP.getFreeHeap());
      return true;
    }
  }
  
  Serial.println("❌ SatoNak days since ATH: invalid response");
  Serial.println("↪︎ Payload: " + payload.substring(0, 100));
  return false;
}


    void fetchTime()
    {
      if (ESP.getFreeHeap() < MIN_HEAP_REQUIRED)
      {
        Serial.println("❌ Not enough heap to safely fetch. Skipping BTC fetch.");
        return;
      }
      struct tm timeinfo;
      if (!getLocalTime(&timeinfo))
      {
        Serial.println("❌ Failed to fetch local time! Keeping previous timeText...");
        return; // Don't overwrite global time values if fetch fails
      }

      Serial.println("⏰ Local time fetched successfully!");

      // Format to HH:MMam/pm, then strip leading zero
      char buf[16];
      strftime(buf, sizeof(buf), "%I:%M%p", &timeinfo);
      if (buf[0] == '0')
        memmove(buf, buf + 1, strlen(buf + 1) + 1); // Strip leading 0

      // ✅ Update globals only if time fetch succeeded
      strncpy(timeText, buf, sizeof(timeText));
      timeText[sizeof(timeText) - 1] = '\0';

      strftime(dateText, sizeof(dateText), "%b %d", &timeinfo);
      strftime(dayText, sizeof(dayText), "%A", &timeinfo);

      Serial.printf("✅ Updated Time: %s | Date: %s | Day: %s\n", timeText, dateText, dayText);
      Serial.printf("📈 Free heap after fetch: %d bytes\n", ESP.getFreeHeap());
    }

    void fetchLatLonFromCity()
    {
      if (WiFi.status() != WL_CONNECTED)
      {
        Serial.println("⚠️ No WiFi connection, skipping lat/lon fetch.");
        return;
      }
      
      if (ESP.getFreeHeap() < MIN_HEAP_REQUIRED)
      {
        Serial.println("❌ Not enough heap to safely fetch. Skipping BTC fetch.");
        return;
      }
      if (savedCity == "")
      {
        Serial.println("⚠️ No saved city found, skipping geolocation fetch.");
        return;
      }

      HTTPClient http;
      String url = "https://nominatim.openstreetmap.org/search?city=" + savedCity + "&format=json";
      http.setTimeout(2000);      // Prevent hanging
      http.setConnectTimeout(1500); // Connection timeout
      http.useHTTP10(true);       // Use HTTP/1.0 for better stability
      http.setReuse(false);       // Don't reuse connection
      http.begin(url);
      
      // esp_task_wdt_reset(); // ❌ v2.0.66: Disabled - ESP32 Arduino core handles WDT
      int httpResponseCode = http.GET();
      // esp_task_wdt_reset(); // ❌ v2.0.66: Disabled - ESP32 Arduino core handles WDT

      if (httpResponseCode == 200)
      {
        DynamicJsonDocument doc(2048);
        deserializeJson(doc, http.getString());

        if (!doc.isNull() && doc.size() > 0)
        {
          String latStr = doc[0]["lat"];
          String lonStr = doc[0]["lon"];

          Serial.println("🌎 Found City Location:");
          Serial.println("Latitude: " + latStr);
          Serial.println("Longitude: " + lonStr);

          latitude = latStr.toFloat();
          longitude = lonStr.toFloat();
        }
        else
        {
          Serial.println("❌ No matching city found!");
        }
      }
      else
      {
        Serial.print("❌ HTTP Request failed, code: ");
        Serial.println(httpResponseCode);
      }

      http.end();
      Serial.printf("📈 Free heap after fetch: %d bytes\n", ESP.getFreeHeap());
    }

    void fetchWeather()
    {
      if (WiFi.status() != WL_CONNECTED)
      {
        Serial.println("⚠️ No WiFi connection, skipping weather fetch.");
        return;
      }
      
      if (ESP.getFreeHeap() < MIN_HEAP_REQUIRED)
      {
        Serial.println("❌ Not enough heap to safely fetch. Skipping BTC fetch.");
        return;
      }
      if (savedCity == "")
      {
        Serial.println("❌ City not set, skipping weather fetch.");
        return;
      }
      
      // 🛡️ Safety guard: Don't fetch weather with invalid coordinates
      if (latitude == 0.0 && longitude == 0.0)
      {
        Serial.println("⚠️ No valid lat/lon, skipping weather fetch.");
        return;
      }

      String weatherURL = "https://api.open-meteo.com/v1/forecast?latitude=" + String(latitude, 6) +
                          "&longitude=" + String(longitude, 6) +
                          "&current=temperature_2m,weather_code&timezone=auto";

      HTTPClient http;
      http.setTimeout(2000);      // Prevent hanging
      http.setConnectTimeout(1500); // Connection timeout
      http.useHTTP10(true);       // Use HTTP/1.0 for better stability
      http.setReuse(false);       // Don't reuse connection
      http.begin(weatherURL);
      
      // esp_task_wdt_reset(); // ❌ v2.0.66: Disabled - ESP32 Arduino core handles WDT
      int httpCode = http.GET();
      // esp_task_wdt_reset(); // ❌ v2.0.66: Disabled - ESP32 Arduino core handles WDT

      if (httpCode == 200)
      {
        String payload = http.getString();
        if (payload.length() == 0)
        {
          Serial.println("❌ Empty weather payload received!");
          http.end();
          return;
        }

        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, payload);

        if (!error)
        {
          float temp = doc["current"]["temperature_2m"];
          int weatherCode = doc["current"]["weather_code"];
          String condition = mapWeatherCode(weatherCode);

          temperature = (int)temp;
          weatherCondition = condition;
          Serial.printf("✅ Updated Weather: %d°C | Condition: %s\n", temperature, weatherCondition.c_str());
          Serial.print("🌡️ Temperature: ");
          Serial.println(temperature);
          Serial.println("🌦️ Condition: " + weatherCondition);
        }
        else
        {
          Serial.println("❌ Failed to parse weather JSON");
        }
      }
      else
      {
        Serial.println("❌ Weather fetch failed, HTTP code: " + String(httpCode));
      }

      http.end(); // ✅ Always clean up!
      Serial.printf("📈 Free heap after fetch: %d bytes\n", ESP.getFreeHeap());
    }

    // Blocking helper to show reboot messages reliably
    void showRebootMessages()
    {
      Serial.println("💾 Showing SETTINGS SAVED message...");
      P.displayClear(); // Clear old text first
      P.displayZoneText(ZONE_UPPER, "SETTINGS", PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
      P.displayZoneText(ZONE_LOWER, "SAVED!", PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
      P.displayReset(ZONE_UPPER);
      P.displayReset(ZONE_LOWER);
      
      unsigned long start = millis();
      while (millis() - start < 2000)
      {
        // No WDT reset needed - this task isn't registered with watchdog
        P.displayAnimate();
        delay(10);
      }
      
      Serial.println("🔄 Showing REBOOTING message...");
      P.displayZoneText(ZONE_UPPER, "REBOOTING", PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
      P.displayZoneText(ZONE_LOWER, "NOW...", PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
      P.displayReset(ZONE_UPPER);
      P.displayReset(ZONE_LOWER);
      
      start = millis();
      while (millis() - start < 2000)
      {
        // No WDT reset needed - this task isn't registered with watchdog
        P.displayAnimate();
        delay(10);
      }
    }

    // Setup of device

    void setup()
    {
      // EMERGENCY HARDWARE SAFETY: Execute shutdown FIRST (before Serial, before delays)!
      
      // �🚨🚨 CRITICAL HARDWARE SAFETY 🚨🚨🚨
      // MAX7219 chips have UNDEFINED STATE at power-on!
      // They can randomly turn on ALL LEDs before our code runs.
      // This causes SEVERE OVERHEATING and BURNS OUT the ESP32/MAX7219!
      // We've lost multiple units to this issue.
      // SOLUTION: Force hardware shutdown via SPI IMMEDIATELY before anything else.
      
      // v2.0.66: Force pins to safe state BEFORE SPI initialization
      // This prevents random signals during the ESP32 boot delay (~200-300ms)
      pinMode(CS_PIN, OUTPUT);
      pinMode(CLK_PIN, OUTPUT);
      pinMode(DATA_PIN, OUTPUT);
      digitalWrite(CS_PIN, HIGH);    // CS high = chips ignore signals
      digitalWrite(CLK_PIN, LOW);    // Clock low = no transitions
      digitalWrite(DATA_PIN, LOW);   // Data low = no commands
      
      // Small delay to let pins stabilize before SPI takeover
      delayMicroseconds(100);
      
      // Initialize SPI immediately (pins already in safe state)
      SPI.begin();
      
      // Send SHUTDOWN command to ALL 16 MAX7219 chips in the daisy chain
      // Must send 16 times because chips are daisy-chained (data shifts through each chip)
      digitalWrite(CS_PIN, LOW);
      for (int i = 0; i < MAX_DEVICES; i++) {
        SPI.transfer(0x0C); // Shutdown register
        SPI.transfer(0x00); // Shutdown mode (LEDs OFF)
      }
      digitalWrite(CS_PIN, HIGH);
      
      // Send TEST MODE OFF to all chips (prevents all-LED display test state)
      digitalWrite(CS_PIN, LOW);
      for (int i = 0; i < MAX_DEVICES; i++) {
        SPI.transfer(0x0F); // Display test register
        SPI.transfer(0x00); // Normal operation (not test mode)
      }
      digitalWrite(CS_PIN, HIGH);
      
      // 🎉 LEDs are now SAFE - hardware shutdown complete in <1ms
      // Now we can safely initialize serial and other systems
      
      Serial.begin(115200);
      delay(100); // Allow serial to stabilize
      Serial.println("✅ EMERGENCY SHUTDOWN: All 16 MAX7219 chips forced OFF via hardware");
      Serial.println("🛡️ SAFETY: LED burnout prevented - chips in safe shutdown state");
      Serial.println("🚀 Starting normal initialization sequence...");
      
      // � DIAGNOSTIC: Log reset reason to identify unexpected reboots
      esp_reset_reason_t resetReason = esp_reset_reason();
      Serial.print("🔍 RESET REASON: ");
      switch (resetReason) {
        case ESP_RST_UNKNOWN:   Serial.println("UNKNOWN"); break;
        case ESP_RST_POWERON:   Serial.println("POWER ON (normal boot)"); break;
        case ESP_RST_EXT:       Serial.println("EXTERNAL PIN RESET"); break;
        case ESP_RST_SW:        Serial.println("SOFTWARE RESET"); break;
        case ESP_RST_PANIC:     Serial.println("⚠️ PANIC/EXCEPTION (crash detected)"); break;
        case ESP_RST_INT_WDT:   Serial.println("⚠️ INTERRUPT WATCHDOG TIMEOUT"); break;
        case ESP_RST_TASK_WDT:  Serial.println("⚠️ TASK WATCHDOG TIMEOUT"); break;
        case ESP_RST_WDT:       Serial.println("⚠️ OTHER WATCHDOG TIMEOUT"); break;
        case ESP_RST_DEEPSLEEP: Serial.println("DEEP SLEEP WAKEUP"); break;
        case ESP_RST_BROWNOUT:  Serial.println("⚠️ BROWNOUT (power supply voltage drop)"); break;
        case ESP_RST_SDIO:      Serial.println("SDIO RESET"); break;
        default:                Serial.printf("UNRECOGNIZED (%d)\n", resetReason); break;
      }
      
      // �🐕 NOTE: Watchdog managed by ESP32 Arduino core - no manual init or reset needed
      // The framework automatically handles watchdog for both setup() and loop()
      // TEST_BASIC_BOOT proves this works perfectly without any manual WDT calls
      
      // Now initialize P with proper library functions (chips already in safe state)
      P.begin(MAX_ZONES);
      delay(50); // Give MAX7219 chips time to initialize properly
      
      P.setZone(ZONE_LOWER, 0, ZONE_SIZE - 1);
      P.setZone(ZONE_UPPER, ZONE_SIZE, MAX_DEVICES - 1);
      P.setFont(nullptr);
      
      // Set to shutdown mode first, then configure safely
      // Put MAX7219 into shutdown immediately (LEDs OFF)
      P.displayShutdown(true);
      delay(10);

      // Wake briefly so register writes actually apply
      P.displayShutdown(false);
      delay(10);

      // Clear any random latched data
      P.displayClear();
      // Set safe, dim startup brightness
      P.setIntensity(ZONE_UPPER, 1);
      P.setIntensity(ZONE_LOWER, 1);
      // Clear again to ensure blank display with brightness applied
      P.displayClear();
      
      Serial.println("✅ LED Matrix safely initialized at brightness 1");
      
      // 🎮 LEGENDARY BOOT ANIMATION: PacMan chomping Bitcoin!
      // 🚀 NEW: Animation now loops WHILE WiFi connects - no blank screen!
      Serial.println("🎮 Starting PacMan boot animation...");
      
      // 📡 STEP 1: Load WiFi credentials and start connection BEFORE animation
      Serial.println("📡 Pre-loading WiFi credentials...");
      prefs.begin("stacksworth", true);  
      savedSSID = prefs.getString("ssid", "");
      savedPassword = prefs.getString("password", "");
      savedCity = prefs.getString("city", "");
      savedCurrency = prefs.getString("currency", "USD");
      savedTheme = prefs.getString("theme", "scroll");
      savedTopText = prefs.getString("toptext", "");
      savedBottomText = prefs.getString("bottomtext", "");
      savedTempUnit = prefs.getString("tempunit", "C");
      savedDeviceName = prefs.getString("devicename", "");
      savedBrightness = prefs.getUChar("brightness", 2);
      savedTimezone = prefs.getInt("timezone", -99);
      // 🔋 v2.0.68: Keep brightness at 1 during boot to reduce power draw
      // User's brightness will be restored after server startup
      // BRIGHTNESS = savedBrightness;  // Disabled - restored later for brownout mitigation
      
      // 📊 Load display enabled states (will use new defaults if not saved)
      for (int i = 0; i < 15; i++) {
        String key = "show" + String(i);
        // Only override default if user has explicitly saved a preference
        if (prefs.isKey(key.c_str())) {
          displayEnabled[i] = prefs.getUChar(key.c_str(), 1) == 1;
        }
      }
      prefs.end();
      
      // Start WiFi connection (non-blocking) if credentials exist
      bool wifiAttemptStarted = false;
      unsigned long wifiStartTime = millis();
      const unsigned long WIFI_TIMEOUT = 30000; // 30 second timeout
      
      if (savedSSID != "" && savedPassword != "") {
        Serial.println("✅ Found WiFi credentials, starting connection...");
        Serial.println("SSID: " + savedSSID);
        WiFi.mode(WIFI_STA);
        WiFi.begin(savedSSID.c_str(), savedPassword.c_str());
        wifiAttemptStarted = true;
        Serial.println("🔌 WiFi connecting in background while animation runs...");
      } else {
        Serial.println("⚠️ No WiFi credentials found - will start AP after animation");
      }
      
      // 🎮 STEP 2: Run PacMan animation - loops until WiFi connects OR timeout
      MD_MAX72XX* mx = P.getGraphicObject();
      if (mx) {
        mx->control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
        mx->clear();
        
        // Show "BOOTING" on top zone during animation
        P.displayZoneText(ZONE_UPPER, "BOOTING", PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
        P.displayReset(ZONE_UPPER);
        
        // Loop animation until WiFi connects (or timeout/max loops)
        uint8_t animLoop = 0;
        const uint8_t MAX_ANIMATION_LOOPS = 5; // 🔋 v2.0.68: Reduced from 10 to 5 to minimize boot power draw
        
        while (animLoop < MAX_ANIMATION_LOOPS) {
          // 🔍 Check if WiFi connected during animation
          if (wifiAttemptStarted && WiFi.status() == WL_CONNECTED) {
            Serial.println("✅ WiFi connected during boot animation!");
            wifiConnected = true;
            hasEverConnected = true;
            wifiDisconnectedAt = 0;
            WiFi.setAutoReconnect(true);
            break; // Exit animation loop
          }
          
          // ⏱️ Check WiFi connection timeout
          if (wifiAttemptStarted && (millis() - wifiStartTime > WIFI_TIMEOUT)) {
            Serial.println("⏱️ WiFi timeout - will start AP after animation");
            break; // Exit animation loop
          }
          
          // 🎮 If NOT attempting WiFi, just do 3 loops and exit
          if (!wifiAttemptStarted && animLoop >= 3) {
            break;
          }
          
          // Clear bottom zone for fresh animation
          mx->control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
          for (int col = 0; col < mx->getColumnCount(); col++) {
            mx->setColumn(col, 0);
          }
          
          // Lay out Bitcoin symbols across the entire bottom zone
          for (uint8_t i = 0; i < MAX_DEVICES; i++) {
            int baseCol = i * 8;
            mx->setPoint(0, baseCol + 4, true);
            mx->setPoint(0, baseCol + 2, true);
            mx->setPoint(1, baseCol + 5, true);
            mx->setPoint(1, baseCol + 4, true);
            mx->setPoint(1, baseCol + 3, true);
            mx->setPoint(1, baseCol + 2, true);
            mx->setPoint(2, baseCol + 5, true);
            mx->setPoint(2, baseCol + 1, true);
            mx->setPoint(3, baseCol + 5, true);
            mx->setPoint(3, baseCol + 4, true);
            mx->setPoint(3, baseCol + 3, true);
            mx->setPoint(3, baseCol + 2, true);
            mx->setPoint(4, baseCol + 5, true);
            mx->setPoint(4, baseCol + 1, true);
            mx->setPoint(5, baseCol + 5, true);
            mx->setPoint(5, baseCol + 4, true);
            mx->setPoint(5, baseCol + 3, true);
            mx->setPoint(5, baseCol + 2, true);
            mx->setPoint(6, baseCol + 4, true);
            mx->setPoint(6, baseCol + 2, true);
          }
          
          mx->control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
        
        // Animate PacMan eating across the display
        int16_t pacIdx = -PACMAN_DATA_WIDTH;
        uint8_t pacFrame = 0;
        int8_t pacDeltaFrame = 1;
        unsigned long lastFrame = millis();
        
        while (pacIdx < mx->getColumnCount() + PACMAN_DATA_WIDTH) {
          // 75ms per frame for smooth animation
          if (millis() - lastFrame >= 75) {
            lastFrame = millis();
            
            mx->control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
            
            // Clear old PacMan position
            for (uint8_t i = 0; i < PACMAN_DATA_WIDTH; i++) {
              int16_t col = pacIdx - PACMAN_DATA_WIDTH + i;
              if (col >= 0 && col < mx->getColumnCount()) {
                mx->setColumn(col, 0);
              }
            }
            
            // Draw PacMan at new position
            pacIdx++;
            for (uint8_t i = 0; i < PACMAN_DATA_WIDTH; i++) {
              int16_t col = pacIdx - PACMAN_DATA_WIDTH + i;
              if (col >= 0 && col < mx->getColumnCount()) {
                mx->setColumn(col, pacman[pacFrame][i]);
              }
            }
            
            // Advance animation frame (creates chomping effect)
            pacFrame += pacDeltaFrame;
            if (pacFrame == 0 || pacFrame == 3)
              pacDeltaFrame = -pacDeltaFrame;
            
            mx->control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
            
            // Keep top zone text animating
            P.displayAnimate();
          }
          
          delay(10);  // Small delay to not hog CPU
          delay(10); // Yield during animation
          }
        
        animLoop++; // Increment loop counter
        Serial.printf("🎮 Animation loop %d complete\n", animLoop);
        
        } // End animation while loop
        
        // Clear display after animation
        mx->clear();
        P.displayClear();
      }
      
      Serial.println("✅ 🎮 PacMan boot animation complete - LEGENDARY!");
      
      // 🌐 STEP 3: Complete WiFi setup and show connection status
      if (wifiConnected) {
        Serial.println("\n✅ WiFi connected successfully!");
        Serial.print("🌍 IP Address: ");
        Serial.println(WiFi.localIP());
        
        // 📺 IMMEDIATELY show WiFi Connected on display (before data fetch!)
        Serial.println("📢 Showing WiFi connected message on display...");
        P.displayZoneText(ZONE_UPPER, "WIFI", PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
        P.displayZoneText(ZONE_LOWER, "CONNECTED", PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
        P.displayReset(ZONE_UPPER);
        P.displayReset(ZONE_LOWER);
        // Pump display to show immediately
        for (int i = 0; i < 50; i++) {
          P.displayAnimate();
          delay(20);
        }
        delay(1000); // Hold "WIFI CONNECTED" for 1 second
        
        // �️ v2.0.66: Additional delay to let TCP/IP stack fully stabilize before starting server
        delay(500);
        
        // �🔄 Show Loading message for data fetch
        P.displayClear();
        P.displayZoneText(ZONE_UPPER, "LOADING", PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
        P.displayZoneText(ZONE_LOWER, "DATA...", PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
        P.displayReset(ZONE_UPPER);
        P.displayReset(ZONE_LOWER);
        for (int i = 0; i < 20; i++) {
          P.displayAnimate();
          delay(10);
        }
        
        // 🕒 Configure timezone
        if (savedTimezone != -99 && savedTimezone >= 0 && savedTimezone < NUM_TIMEZONES) {
          const char* tzString = TIMEZONE_STRINGS[savedTimezone];
          configTzTime(tzString, ntpServer);
          Serial.printf("🕒 Timezone configured: %s (%s)\n", TIMEZONE_NAMES[savedTimezone], tzString);
        } else {
          configTzTime(TIMEZONE_STRINGS[11], ntpServer); // Default: Mountain Time
          Serial.println("🕒 Using default Mountain Time timezone");
        }
        
        Serial.println("💡 OTA updates available at http://matrix.local");
        
      } else if (wifiAttemptStarted) {
        // WiFi credentials existed but connection failed
        Serial.println("\n❌ Failed to connect to WiFi, will start Access Point...");
        WiFi.persistent(false);
        startAccessPoint();
      } else {
        // No WiFi credentials saved
        Serial.println("\n⚠️ No saved WiFi credentials, will start Access Point...");
        WiFi.persistent(false);
        startAccessPoint();
      }
      
      // 🐕 Watchdog already initialized early before animation - skip duplicate init
      
      Serial.println("🚀 Starting STACKSWORTH Matrix Setup...");

      //Adding MAC Address to ID
      macID = getShortMAC();
      Serial.println("🆔 MAC Fragment: " + macID);

      prefs.begin("device", false);
      prefs.putString("shortMAC", macID);
      prefs.end();

    

      // Monitor available heap memory
      Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
      Serial.printf("Minimum free heap: %d bytes\n", ESP.getMinFreeHeap());
      Serial.printf("Free PSRAM: %d bytes\n", ESP.getFreePsram());

      // 🗂️ Mount SPIFFS with auto-format on failure
      Serial.println("🗂️ Mounting SPIFFS...");
      if (!SPIFFS.begin(true))
      {
        Serial.println("❌ SPIFFS mount failed - attempting manual format...");
        SPIFFS.format();
        delay(1000);
        if (!SPIFFS.begin(true)) {
          Serial.println("❌ Critical: SPIFFS still failed after format!");
          Serial.println("⚠️ Portal will run without HTML file (basic mode)");
          // Don't return - continue without SPIFFS for basic functionality
        } else {
          Serial.println("✅ SPIFFS formatted and mounted successfully!");
        }
      }
      else
      {
        Serial.println("✅ SPIFFS mounted successfully!");
      }

      if (!SPIFFS.exists("/STACKS_Wifi_Portal.html.gz"))
      {
        Serial.println("❌ HTML file NOT found");
      }
      else
      {
        Serial.println("✅ Custom HTML file found");
      }

      // 📡 WiFi and settings already loaded before animation - skip redundant call
      // (Settings were loaded inline and WiFi started before Pacman animation)
      Serial.println("📡 WiFi setup complete (handled during boot animation)");

      randomSeed(esp_random());

      // Show IP/Portal immediately (if WiFi not connected)
      if (!wifiConnected)
      {
        // Set a visible brightness for portal screen
        Serial.println("💡 Setting brightness for portal screen...");
        P.setIntensity(ZONE_UPPER, 3);  // Medium-low brightness for setup
        P.setIntensity(ZONE_LOWER, 3);
        
        // Show portal status and IP address immediately - NO welcome animation
        Serial.println("📡 Showing portal status and IP...");
        IPAddress apIP = WiFi.softAPIP();
        String ipDisplay = apIP.toString();
        P.displayZoneText(ZONE_UPPER, "OPEN PORTAL", PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
        P.displayZoneText(ZONE_LOWER, ipDisplay.c_str(), PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
        P.displayReset(ZONE_UPPER);
        P.displayReset(ZONE_LOWER);
        
        // Pump display once to lock it in
        for (int i = 0; i < 10; i++) {
          P.displayAnimate();
          delay(10);
        }
        
        apMsgShown = true;
        Serial.println("✅ Portal screen displayed - ready for setup");
      }

      // Now restore user's preferred brightness after welcome screens
      Serial.println("💡 Restoring user brightness settings...");
      P.setIntensity(ZONE_UPPER, BRIGHTNESS);
      P.setIntensity(ZONE_LOWER, BRIGHTNESS);
      
      Serial.printf("💡 Brightness restored to: %d/15 for all zones\n", BRIGHTNESS);

      // 🕒 Time Config - only set default if not already configured in loadSavedSettingsAndConnect()
      if (!wifiConnected) {
        Serial.println("🕒 Configuring default timezone (Mountain Time)...");
        configTzTime(TIMEZONE_STRINGS[11], ntpServer); // Default to Mountain Time
      }

      // Serve Custom HTML File
      server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
                {
  if (SPIFFS.exists("/STACKS_Wifi_Portal.html.gz")) {
    AsyncWebServerResponse *response = request->beginResponse(SPIFFS, "/STACKS_Wifi_Portal.html.gz", "text/html");
    response->addHeader("Content-Encoding", "gzip"); // Inform the browser that the file is GZIP-compressed
    request->send(response);
  } else {
    request->send(404, "text/plain", "Custom HTML file not found");
  } });

      // 📝 Handle Save Form Submission
      server.on("/save", HTTP_POST, [](AsyncWebServerRequest *request)
                {
  if (request->hasParam("ssid", true) && request->hasParam("password", true)) {
    // 🔐 Open prefs first so we can keep existing values if user leaves fields blank
    prefs.begin("stacksworth", false);

    String ssid = request->getParam("ssid", true)->value();
    String password = request->getParam("password", true)->value();
    String city = request->getParam("city", true)->value();
    String timezone = request->getParam("timezone", true)->value();
    String currency = request->getParam("currency", true)->value();
    String theme = request->getParam("theme", true)->value();
    String toptext = request->getParam("toptext", true)->value();
    String bottomtext = request->getParam("bottomtext", true)->value();
    String tempunit = request->getParam("tempunit", true)->value();
    String devicename = request->getParam("devicename", true)->value();
    String brightness = request->getParam("brightness", true)->value();

    // ✅ If user left SSID/PW blank, keep existing saved values
    // This prevents accidental Wi-Fi wipe when reopening Matrix.local to change other settings.
    ssid.trim();
    password.trim();

    if (ssid.length() == 0) {
      ssid = prefs.getString("ssid", "");
    }
    if (password.length() == 0) {
      password = prefs.getString("password", "");
    }

    // Validate and limit custom text to 11 characters
    if (toptext.length() > 11) toptext = toptext.substring(0, 11);
    if (bottomtext.length() > 11) bottomtext = bottomtext.substring(0, 11);
    // Validate and limit device name to 20 characters
    if (devicename.length() > 20) devicename = devicename.substring(0, 20);

    Serial.println("✅ Saving WiFi Settings:");
    Serial.println("SSID: " + ssid);
    Serial.println("Password: (hidden)");
    Serial.println("City: " + city);
    Serial.println("Timezone: " + timezone);
    Serial.println("Currency: " + currency);
    Serial.println("Theme: " + theme);
    Serial.println("Custom Top: " + toptext);
    Serial.println("Custom Bottom: " + bottomtext);
    Serial.println("Temperature Unit: " + tempunit);
    Serial.println("Device Name: " + devicename);
    Serial.println("Brightness: " + brightness);
    prefs.putString("ssid", ssid);
    prefs.putString("password", password);
    prefs.putString("city", city);
    prefs.putString("currency", currency);                          // 🌍 Store currency
    prefs.putString("theme", theme);                                // 🎨 Store theme
    prefs.putString("toptext", toptext);                            // 📝 Store custom top text
    prefs.putString("bottomtext", bottomtext);                      // 📝 Store custom bottom text
    prefs.putString("tempunit", tempunit);                          // 🌡️ Store temperature unit
    prefs.putString("devicename", devicename);                      // 🆔 Store device nickname
    
    // 💡 Clamp brightness before saving (safety check)
    uint8_t brightnessVal = brightness.toInt();
    if (brightnessVal < 1) brightnessVal = 1;
    if (brightnessVal > 15) brightnessVal = 15;
    prefs.putUChar("brightness", brightnessVal);                   // 💡 Store brightness (validated)
    
    // 📊 Store display options (show0-show14)
    for (int i = 0; i < 15; i++) {
      String key = "show" + String(i);
      String value = request->hasParam(key, true) ? request->getParam(key, true)->value() : "0";
      prefs.putUChar(key.c_str(), value.toInt());
      Serial.printf("Display %d: %s\n", i, value.c_str());
    }
    
    prefs.putInt("timezone", timezone.toInt());
    prefs.end();
    Serial.println("✅ Settings saved to NVS!");
    
    // ✅ IMMEDIATELY APPLY CHANGES (don't wait for reboot)
    savedBrightness = brightnessVal;  // Use validated value, not raw input
    savedTempUnit = tempunit;
    savedCity = city;
    savedCurrency = currency;
    savedTopText = toptext;
    savedBottomText = bottomtext;
    savedTheme = theme;
    
    // 🔄 Reload displayEnabled array from NVS
    prefs.begin("stacksworth", true);  // Open in read-only mode
    for (int i = 0; i < 15; i++) {
      String key = "show" + String(i);
      displayEnabled[i] = prefs.getUChar(key.c_str(), 1) == 1;
    }
    prefs.end();
    
    // 💡 Apply brightness immediately
    setBrightness(savedBrightness);


    // ✅ SEND HTTP 200 RESPONSE FIRST
    request->send(200, "text/plain", "Settings saved! Rebooting...");

    // Show reboot messages and restart
    showRebootMessages();
    ESP.restart();
  } else {
    Serial.println("❌ Missing parameters in form submission!");
    request->send(400, "text/plain", "Missing parameters");
  } });


      // Serve device info to the portal
      server.on("/deviceinfo", HTTP_GET, [](AsyncWebServerRequest *request) {
        String info = "{\"macid\":\"" + getShortMAC() + "\",";
        info += "\"devicename\":\"" + savedDeviceName + "\",";
        info += "\"version\":\"" + String(FIRMWARE_VERSION) + "\"}";
        request->send(200, "application/json", info);
      });
      
      // Legacy endpoint for backward compatibility
      server.on("/macid", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", getShortMAC());
      });
      
      // 🔍 Identify endpoint - blink display to show which device user is configuring
      server.on("/identify", HTTP_GET, [](AsyncWebServerRequest *request) {
        Serial.println("🔍 Identify button pressed - showing portal active message");
        
        // Send response immediately so browser doesn't hang
        request->send(200, "text/plain", "Device identify started!");
        
        // Show identification message on display
        P.displayClear();
        P.displayZoneText(ZONE_UPPER, "PORTAL", PA_CENTER, 0, 5000, PA_FADE, PA_FADE);
        P.displayZoneText(ZONE_LOWER, "ACTIVE", PA_CENTER, 0, 5000, PA_FADE, PA_FADE);
        P.synchZoneStart();
        
        // Wait for animation to complete
        // NOTE: No esp_task_wdt_reset() here - this runs in AsyncWebServer task context, not registered with WDT
        unsigned long startTime = millis();
        while (millis() - startTime < 5000) {
          P.displayAnimate();
          delay(10); // yield() happens in delay
        }
        
        Serial.println("✅ Identify animation complete");
      });

      // 🔄 OTA Update endpoints
      
      // Check for available updates (user-triggered only, no automatic checks)
      server.on("/checkupdate", HTTP_GET, [](AsyncWebServerRequest *request) {
        Serial.println("🔍 Checking for firmware updates...");
        
        String latestVersion = checkForUpdates();
        
        if (latestVersion.length() > 0) {
          // Compare versions
          if (latestVersion != String(FIRMWARE_VERSION)) {
            String response = "{\"updateAvailable\":true,\"currentVersion\":\"";
            response += FIRMWARE_VERSION;
            response += "\",\"latestVersion\":\"";
            response += latestVersion;
            response += "\"}";
            request->send(200, "application/json", response);
          } else {
            String response = "{\"updateAvailable\":false,\"currentVersion\":\"";
            response += FIRMWARE_VERSION;
            response += "\"}";
            request->send(200, "application/json", response);
          }
        } else {
          request->send(500, "text/plain", "Failed to check for updates");
        }
      });
      
      // Perform OTA update (manual only)
      // 🛡️ v2.0.68: OTA now triggered via flag, executed in loop() to avoid AsyncWebServer callback risks
      server.on("/doupdate", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (WiFi.status() != WL_CONNECTED)
        {
          request->send(503, "text/plain", "No WiFi connection. Connect to WiFi before updating.");
          return;
        }
        
        Serial.println("🚀 OTA update requested via web portal");
        
        // Send response immediately
        request->send(200, "text/plain", "Update starting. Matrix will reboot if successful.");
        
        // Set flag to trigger OTA in loop() instead of executing here
        // This keeps AsyncWebServer callback clean and avoids lwIP issues
        pendingOTA = true;
      });

      // Brightness control endpoint
      server.on("/brightness", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (request->hasParam("level")) {
          String levelStr = request->getParam("level")->value();
          uint8_t newBrightness = levelStr.toInt();
          if (newBrightness >= 1 && newBrightness <= 15) {
            setBrightness(newBrightness);
            request->send(200, "text/plain", "Brightness set to " + String(BRIGHTNESS));
          } else {
            request->send(400, "text/plain", "Invalid brightness level. Use 1-15");
          }
        } else {
          request->send(200, "text/plain", "Current brightness: " + String(BRIGHTNESS) + "/15");
        }
      });

      // 🆕 v2.0.57: Version info API endpoint
      server.on("/version", HTTP_GET, [](AsyncWebServerRequest *request) {
        String json = "{\"version\":\"" + String(FIRMWARE_VERSION) + 
                      "\",\"uptime\":" + String(millis() / 1000) + 
                      ",\"wifi_rssi\":" + String(WiFi.RSSI()) + 
                      ",\"free_heap\":" + String(ESP.getFreeHeap()) + "}";
        request->send(200, "application/json", json);
      });

      // Captive Portal Redirect
      server.onNotFound([](AsyncWebServerRequest *request)
                        { request->redirect("/");
                        });

      // Start Web Server
      Serial.println("🌐 Starting Async Web Server...");
      delay(2000); // 🕒 Let WiFi fully stabilize first
      server.begin();
      Serial.println("🌍 Async Web server started");
      delay(2000); // 🕒 Let server stabilize after starting
      
      // 🔋 v2.0.68: Restore user's saved brightness AFTER server fully started
      // Boot kept brightness at 1 to reduce power draw and minimize brownout risk
      if (savedBrightness > 0 && savedBrightness <= 15) {
        Serial.printf("✨ Restoring user brightness from %d to %d\n", BRIGHTNESS, savedBrightness);
        setBrightness(savedBrightness);
      }

      // 🌐 Setup mDNS with retry logic - ONLY when WiFi is connected in STA mode
      // ALL units use "Matrix" for simplicity
      bool mdnsStarted = false;
      if (wifiConnected && WiFi.status() == WL_CONNECTED) {
        for (int retry = 0; retry < 3 && !mdnsStarted; retry++) {
          if (MDNS.begin("Matrix")) {
            Serial.println("✅ mDNS responder started - Access at http://Matrix.local");
            Serial.println("💡 TIP: Use the Identify button in portal to confirm which unit you're configuring");
            MDNS.addService("http", "tcp", 80);
            mdnsStarted = true;
          } else {
            Serial.print("⚠️ mDNS setup attempt ");
            Serial.print(retry + 1);
            Serial.println(" failed, retrying...");
            delay(500);
          }
        }
        if (!mdnsStarted) {
          Serial.println("❌ mDNS failed after 3 attempts - use IP address instead");
        }
      } else {
        Serial.println("⚠️ Skipping mDNS - WiFi not connected (will be available in AP mode via IP)");
      }

      bootMs = millis();

      // Initialize with last known values or sensible first-boot defaults
      Serial.println("🔧 Loading cached values or setting first-boot defaults...");
      
      // Try to load last known good values from preferences
      prefs.begin("cache", true);  // read-only mode
      String lastBtcText = prefs.getString("btcText", "Connecting");
      String lastBlockText = prefs.getString("blockText", "Syncing");
      String lastFeeText = prefs.getString("feeText", "Checking");
      String lastSatsText = prefs.getString("satsText", "Updating");
      String lastChangeText = prefs.getString("changeText", "Fetching");
      String lastAthText = prefs.getString("athText", "Refreshing");
      String lastDaysAthText = prefs.getString("daysAthText", "Updating");
      String lastHashrateText = prefs.getString("hashrateText", "Checking");
      String lastCircSupplyText = prefs.getString("circSupplyText", "Counting");
      String lastMinerName = prefs.getString("minerName", "Discovering");
      prefs.end();
      
      // Apply cached or first-boot values
      strncpy(btcText, lastBtcText.c_str(), sizeof(btcText));
      strncpy(blockText, lastBlockText.c_str(), sizeof(blockText));
      strncpy(feeText, lastFeeText.c_str(), sizeof(feeText));
      strncpy(satsText, lastSatsText.c_str(), sizeof(satsText));
      strncpy(satsText2, "Updating", sizeof(satsText2));
      strncpy(timeText, "Syncing", sizeof(timeText));
      strncpy(dateText, "...", sizeof(dateText));
      strncpy(dayText, "Starting", sizeof(dayText));
      strncpy(hashrateText, lastHashrateText.c_str(), sizeof(hashrateText));
      strncpy(circSupplyText, lastCircSupplyText.c_str(), sizeof(circSupplyText));
      strncpy(circPercentText, "/21 Million", sizeof(circPercentText));
      strncpy(changeText, lastChangeText.c_str(), sizeof(changeText));
      strncpy(athText, lastAthText.c_str(), sizeof(athText));
      strncpy(daysAthText, lastDaysAthText.c_str(), sizeof(daysAthText));
      minerName = lastMinerName;
      
    
      // ⚠️ CRITICAL: Cannot do HTTP calls in setup() - causes lwIP threading assertion
      // Initial fetch will happen in first loop() iteration instead
      Serial.println("✅ Setup complete - initial data fetch will happen in loop()");

      // 👇  Manually trigger first animation cycle!
      cycle = 0;                                              // Start at first data set
      lastApiCall = millis() - FETCH_INTERVAL;                // Force immediate fetch
      lastWeatherUpdate = millis() - WEATHER_UPDATE_INTERVAL; // Force weather update soon
      lastNTPUpdate = millis() - NTP_UPDATE_INTERVAL;         // Force NTP update soon

      pinMode(BUTTON_PIN, INPUT_PULLUP);  //added this for the Smash Buy Button!!!

      bootMs = millis();
    }

    

    
    void loop()
    {
      // NOTE: Watchdog automatically managed by ESP32 Arduino core - no manual reset needed
      dnsServer.processNextRequest(); // Handle captive portal DNS magic
      
      // 🛡️ v2.0.68: Handle OTA execution in main loop (not in AsyncWebServer callback)
      // This avoids lwIP/async issues with long-running HTTP downloads inside callback context
      if (pendingOTA) {
        pendingOTA = false;  // Clear flag immediately
        Serial.println("⚡ Executing OTA update from main loop (safe context)...");
        delay(500);  // Brief delay to ensure HTTP response was sent
        performOTAUpdate(UPDATE_URL);  // This will reboot if successful
        // If we reach here, OTA failed - flag already cleared, loop continues normally
      }

      // 🚀 INITIAL FETCH - Run once on first loop iteration
      // 🎯 SMART: Only fetches data for metrics that are actually ENABLED by user
      // ⚡ FAST BOOT: Skips disabled metrics, optimizes boot time for user's config
      if (wifiConnected && !initialFetchDone && !apMode) {
        // 🛡️ v2.0.66: Add small delay to let TCP/IP stack fully stabilize after boot
        // Prevents lwIP assertion "sys_untimeout" race condition
        delay(500);
        
        Serial.println("🌍 Smart boot: Fetching only ENABLED metrics...");
        
        unsigned long now = millis();
        int fetchCount = 0;
        
        // Check which metrics are enabled and fetch only those
        if (displayEnabled[0]) { // Block Height
          Serial.printf("🔍 [%d] Fetching Block Height...\n", ++fetchCount); 
          delay(10); // Yield before network call
          fetchHeightFromSatoNak(); 
          lastBlockHeightFetch = now;
          lastBlock = now; // ⚡ Set periodic scheduler timer so it knows Block was fetched
          Serial.printf("✅ [%d] Block Height complete\n", fetchCount);
          delay(200);
        }
        
        if (displayEnabled[1]) { // Miner
          Serial.printf("🔍 [%d] Fetching Miner...\n", ++fetchCount); 
          delay(10); // Yield before network call
          fetchMinerFromSatoNak();
          Serial.printf("✅ [%d] Miner complete\n", fetchCount);
          delay(200);
        }
        
        if (displayEnabled[3]) { // Price (also needed for Sats/Currency calc)
          Serial.printf("🔍 [%d] Fetching Price...\n", ++fetchCount); 
          delay(10); // Yield before network call
          fetchPriceFromSatoNak(); 
          lastPriceFetch = now;
          lastBTC = now; // ⚡ Set periodic scheduler timer so it knows Price was fetched
          Serial.printf("✅ [%d] Price complete\n", fetchCount);
          delay(200);
        }
        
        if (displayEnabled[8]) { // Fee Rate
          Serial.printf("🔍 [%d] Fetching Fee Rate...\n", ++fetchCount); 
          delay(10); // Yield before network call
          fetchFeeRate(); 
          lastFee = now;
          Serial.printf("✅ [%d] Fee Rate complete\n", fetchCount);
          delay(200);
        }
        
        // Time is ALWAYS fetched (needed for Time/City, Day/Date, Moscow Time displays)
        if (displayEnabled[10] || displayEnabled[11] || displayEnabled[13]) {
          Serial.printf("🔍 [%d] Fetching Time...\n", ++fetchCount); 
          delay(10); // Yield before network call
          fetchTime();
          Serial.printf("✅ [%d] Time complete\n", fetchCount);
          delay(100);
        }
        
        if (displayEnabled[12]) { // Weather
          Serial.printf("🔍 [%d] Fetching Weather...\n", ++fetchCount); 
          delay(10); // Yield before network call
          // 🌍 Ensure lat/lon exist before fetching weather
          if (latitude == 0.0 && longitude == 0.0 && savedCity.length() > 0) {
            Serial.println("📍 Fetching lat/lon before weather...");
            fetchLatLonFromCity();
            delay(200);
          }
          fetchWeather(); 
          lastWeather = now;
          Serial.printf("✅ [%d] Weather complete\n", fetchCount);
          delay(200);
        }

        lastFetchTime = millis();
        Serial.printf("✅ Smart boot complete! Fetched %d enabled metrics. Others load in background.\n", fetchCount);
        saveDisplayCache();
        
        // Clear "LOADING DATA" message - displays will start immediately
        P.displayClear();
        
        initialFetchDone = true;
      }

      // If in AP portal mode AND never successfully connected before, show PORTAL OPEN
      // Once device has EVER connected successfully, NEVER go back to portal display
      if (apMode && !hasEverConnected && WiFi.status() != WL_CONNECTED)
      {
        feedWDT(); // Feed watchdog before processing
        
        // Keep the portal display active (re-set if needed)
        if (!apMsgShown)
        {
          IPAddress apIP = WiFi.softAPIP();
          String ipDisplay = apIP.toString();
          P.displayZoneText(ZONE_UPPER, "OPEN PORTAL", PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
          P.displayZoneText(ZONE_LOWER, ipDisplay.c_str(), PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
          P.displayReset(ZONE_UPPER);
          P.displayReset(ZONE_LOWER);
          apMsgShown = true;
        }
        
        P.displayAnimate();
        feedWDT(); // Feed watchdog after display
        delay(50); // Shorter delay to keep WDT happy
        return;
      }
      else if (apMode && wifiConnected)
      {
        // Exit portal mode - clean up DNS server
        Serial.println("✅ Exiting AP mode, stopping DNS server...");
        dnsServer.stop();
        apMode = false;
        apMsgShown = false;
      }

      // 🌐 WiFi RESILIENCE MONITOR (check every 10 seconds)
      // Strategy: Keep displaying cached data, only fall back to AP mode after extended failure
      static unsigned long lastWiFiCheck = 0;
      static bool reconnecting = false;
      unsigned long now = millis();
      
      if (!apMode && savedSSID.length() > 0 && now - lastWiFiCheck >= 10000) {
        lastWiFiCheck = now;
        
        if (WiFi.status() != WL_CONNECTED) {
          // WiFi is disconnected
          if (!reconnecting) {
            // Just noticed disconnection - mark timestamp
            Serial.println("⚠️ WiFi disconnected! Continuing with cached data, attempting reconnect...");
            wifiDisconnectedAt = now;  // Always set to NOW on first detection of new disconnection
            reconnecting = true;
            wifiConnected = false;
            Serial.printf("🕒 Disconnection timer started at: %lu ms\n", now);
          }
          
          // Check if we've been disconnected too long (6 hours)
          if (hasEverConnected && wifiDisconnectedAt > 0 && (now - wifiDisconnectedAt >= WIFI_FALLBACK_TIMEOUT)) {
            Serial.printf("🚨 WiFi disconnected for 6+ hours (started: %lu, now: %lu, diff: %lu)\n", wifiDisconnectedAt, now, now - wifiDisconnectedAt);
            Serial.println("🚨 Falling back to AP mode for reconfiguration.");
            startAccessPoint();
            return; // Exit to AP mode
          }
          
          // Try reconnecting (non-blocking) with proper timestamp tracking
          static unsigned long lastReconnectAttempt = 0;
          if (now - lastReconnectAttempt >= 10000) {  // Attempt every 10 seconds
            Serial.println("🔄 Retrying WiFi connection...");
            WiFi.begin(savedSSID.c_str(), savedPassword.c_str());
            lastReconnectAttempt = now;
          }
          
        } else if (reconnecting) {
          // WiFi reconnected!
          Serial.println("✅ WiFi reconnected successfully!");
          Serial.printf("🕒 Was disconnected for: %lu ms (%.1f minutes)\n", now - wifiDisconnectedAt, (now - wifiDisconnectedAt) / 60000.0);
          reconnecting = false;
          wifiConnected = true;
          wifiDisconnectedAt = 0; // Reset disconnection timer
        } else if (wifiConnected && wifiDisconnectedAt > 0) {
          // Sanity check: if we're connected but timer is still set, reset it
          Serial.println("🛠️ Sanity reset: WiFi connected but disconnection timer was still active");
          wifiDisconnectedAt = 0;
        }
      }

// 🛠️ Smash Buy Button Polling (Debounced)
static bool lastButtonState = HIGH;
bool currentButtonState = digitalRead(BUTTON_PIN);

if (lastButtonState == HIGH && currentButtonState == LOW) {
  // Falling edge: button was released, now pressed
  Serial.println("🚨 SMASH BUY Button Pressed!");
  buttonPressed = true;
}

lastButtonState = currentButtonState;

// USED FOR RANDOM PHRASES
if (buttonPressed) {
  buttonPressed = false;  // consume the event so it fires once

int idx = random(NUM_PHRASES);
const char* topLine    = PHRASES[idx][0];
const char* bottomLine = PHRASES[idx][1];

// Optional: small press lockout to avoid double-fires on long press/bounce
static unsigned long pressLockUntil = 0;
if (millis() < pressLockUntil) return;
pressLockUntil = millis() + 600; // 0.6s cooldown

// Show the message (nice and clean fade). Use your zone IDs as you already do.
// If you prefer, you can show the same phrase on both lines for impact.
P.displayClear();
P.displayZoneText(1, topLine,    PA_CENTER, 0, 2500, PA_FADE, PA_FADE);
P.displayZoneText(0, bottomLine, PA_CENTER, 0, 2500, PA_FADE, PA_FADE);

// Let the animation finish while keeping WDT happy (ESP32)
while (!P.displayAnimate()) {
  delay(10); // Yield to system
}

P.displayClear();
P.synchZoneStart();
delay(10); // Yield after animation complete
Serial.print("🎯 Smash Buy: ");
Serial.print(topLine);
Serial.print(" / ");
Serial.println(bottomLine);

}




  unsigned long currentMillis = millis();
      

      // ✅ Monitor heap health every 60 seconds
      static unsigned long lastMemoryCheck = 0;
      static unsigned long lastHeapLog = 0;
      if (currentMillis - lastMemoryCheck >= 60000)
      {
        Serial.printf("🧠 Free heap: %d | Min ever: %d\n", ESP.getFreeHeap(), ESP.getMinFreeHeap());
        lastMemoryCheck = currentMillis;
      }

      // 🚨 v2.0.66: Changed from auto-reboot to warning-only (prevents surprise boot loops)
      // For shipped units, cached data & degraded operation >>> unexpected restarts
      if (ESP.getFreeHeap() < 140000)
      {
        Serial.printf("⚠️ LOW HEAP WARNING: %d bytes (threshold: 140000)\n", ESP.getFreeHeap());
        // No restart - let unit continue with cached data
      }

      // ⏰ Fetch Time every 1 minute
      static unsigned long lastTimeFetch = 0;
      if (currentMillis - lastTimeFetch >= 60000)
      {
        fetchTime();
        lastTimeFetch = currentMillis;
      }

      // ── HTTP scheduler: serialize network calls to avoid overlap
if (WiFi.status() == WL_CONNECTED) {
  uint32_t now = millis();

  // 1) BTC every BTC_INTERVAL
  if (now - lastBTC >= BTC_INTERVAL) {
    delay(10); // Yield before network operations
    fetchBitcoinData();
    lastBTC = now;
    saveDisplayCache(); // Save after successful price update
    delay(10); // Yield after network operations
  }
  // 2) Fee every FEE_INTERVAL (offset removed - smart boot handles initial stagger)
  else if (now - lastFee >= FEE_INTERVAL) {
    delay(10); // Yield before network operations
    fetchFeeRate();
    lastFee = now;
    delay(10); // Yield after network operations
  }
  // 3) Block height every BLOCK_INTERVAL (offset removed - smart boot handles initial stagger)
  else if (now - lastBlock >= BLOCK_INTERVAL) {
    Serial.println("🔄 Starting periodic data refresh cycle...");
    Serial.println("🔍 [REFRESH-1/7] Fetching Block Height..."); delay(10); // Yield before network operations
    fetchBlockHeight();
    Serial.println("✅ [REFRESH-1/7] Block Height complete");
    Serial.println("🔍 [REFRESH-2/7] Fetching Miner..."); delay(10); // Yield after each API call
    fetchMinerFromSatoNak();
    Serial.println("✅ [REFRESH-2/7] Miner complete");
    Serial.println("🔍 [REFRESH-3/7] Fetching Hashrate..."); delay(10);
    fetchHashrateFromSatoNak();
    Serial.println("✅ [REFRESH-3/7] Hashrate complete");
    Serial.println("🔍 [REFRESH-4/7] Fetching Circulating Supply..."); delay(10);
    fetchCircSupplyFromSatoNak();
    Serial.println("✅ [REFRESH-4/7] Circulating Supply complete");
    Serial.println("🔍 [REFRESH-5/7] Fetching ATH..."); delay(10);
    fetchAthFromSatoNak();
    Serial.println("✅ [REFRESH-5/7] ATH complete");
    Serial.println("🔍 [REFRESH-6/7] Fetching Days Since ATH..."); delay(10);
    fetchDaysSinceAthFromSatoNak();
    Serial.println("✅ [REFRESH-6/7] Days Since ATH complete");
    Serial.println("🔍 [REFRESH-7/7] Fetching 24H Change..."); delay(10);
    fetchChange24hFromSatoNak();
    Serial.println("✅ [REFRESH-7/7] 24H Change complete");
    Serial.println("🎉 Periodic data refresh cycle completed successfully!");
    lastBlock = now;
    saveDisplayCache(); // Save after successful data updates
    delay(10); // Yield after network operations
  }
  // 4) Weather every WEATHER_INTERVAL (offset removed - smart boot handles initial stagger)
  else if (now - lastWeather >= WEATHER_INTERVAL) {
    delay(10); // Yield before network operations
    fetchWeather();
    lastWeather = now;
    delay(10); // Yield after network operations
  }
}



      // 🖥️ Rotate screens
  if (P.displayAnimate()) {
    Serial.print("🖥️ Displaying screen: ");
    Serial.println(displayCycle);

    switch (displayCycle) {
      case 0: {
        textEffect_t effectIn, effectOut;
        getThemeEffects(effectIn, effectOut);
        Serial.println("🖥️ Displaying BLOCK screen...");
        Serial.printf("🔤 Displaying text: %s (Top), %s (Bottom)\n", "BLOCK", blockText); 
        P.displayZoneText(ZONE_UPPER, "BLOCK",   PA_CENTER, SCROLL_SPEED, 10000, effectIn, effectOut);
        P.displayZoneText(ZONE_LOWER, blockText, PA_CENTER, SCROLL_SPEED, 10000, effectIn, effectOut);
        P.synchZoneStart(); // Force synchronization
        break;
      }

      case 1: {
        textEffect_t effectIn, effectOut;
        getThemeEffects(effectIn, effectOut);
        Serial.println("🖥️ Displaying MINER screen...");
        Serial.printf("🔤 Displaying text: %s (Top), %s (Bottom)\n", "MINER", minerName.c_str());
        P.displayZoneText(ZONE_UPPER, "MINED BY", PA_CENTER, SCROLL_SPEED, 10000, effectIn, effectOut);
        P.displayZoneText(ZONE_LOWER, minerName.c_str(), PA_CENTER, SCROLL_SPEED, 10000, effectIn, effectOut);
        P.synchZoneStart(); // Force synchronization
        break;
      }

      case 2: {
        textEffect_t effectIn, effectOut;
        getThemeEffects(effectIn, effectOut);
        Serial.println("🖥️ Displaying CIRCULATING SUPPLY screen...");
        Serial.printf("🔤 Displaying text: %s (Top), %s (Bottom)\n", circSupplyText, circPercentText);
        P.displayZoneText(ZONE_UPPER, circSupplyText, PA_CENTER, SCROLL_SPEED, 10000, effectIn, effectOut);
        P.displayZoneText(ZONE_LOWER, circPercentText, PA_CENTER, SCROLL_SPEED, 10000, effectIn, effectOut);
        P.synchZoneStart(); // Force synchronization
        break;
      }

      case 3: {
        textEffect_t effectIn, effectOut;
        getThemeEffects(effectIn, effectOut);
        String currentFiat = getCurrentFiatCode();
        static char priceLabel[48];  // Increased buffer size for safety
        snprintf(priceLabel, sizeof(priceLabel), "%s PRICE", currentFiat.c_str());
        Serial.println("🖥️ Displaying " + currentFiat + " PRICE screen...");
        Serial.printf("🔤 Displaying text: %s (Top), %s (Bottom)\n", priceLabel, btcText);
        Serial.printf("🔍 DEBUG: btcText='%s', length=%d\n", btcText, strlen(btcText));
        P.displayZoneText(ZONE_UPPER, priceLabel, PA_CENTER, SCROLL_SPEED, 10000, effectIn, effectOut);
        P.displayZoneText(ZONE_LOWER, btcText, PA_CENTER, SCROLL_SPEED, 10000, effectIn, effectOut);
        P.synchZoneStart();
        break;
      } 

          
      case 4: {
        textEffect_t effectIn, effectOut;
        getThemeEffects(effectIn, effectOut);
        Serial.println("🖥️ Displaying 24H CHANGE screen...");
        Serial.printf("🔤 Displaying text: %s (Top), %s (Bottom)\n", "24H CHANGE", changeText);
        P.displayZoneText(ZONE_UPPER, "24H CHANGE", PA_CENTER, SCROLL_SPEED, 10000, effectIn, effectOut);
        P.displayZoneText(ZONE_LOWER, changeText, PA_CENTER, SCROLL_SPEED, 10000, effectIn, effectOut);
        P.synchZoneStart();
        break;
      }

      case 5: {
        textEffect_t effectIn, effectOut;
        getThemeEffects(effectIn, effectOut);
        Serial.println("🖥️ Displaying ATH PRICE screen...");
        Serial.printf("🔤 Displaying text: %s (Top), %s (Bottom)\n", "ATH", athText);
        P.displayZoneText(ZONE_UPPER, "ATH", PA_CENTER, SCROLL_SPEED, 10000, effectIn, effectOut);
        P.displayZoneText(ZONE_LOWER, athText, PA_CENTER, SCROLL_SPEED, 10000, effectIn, effectOut);
        P.synchZoneStart(); // Force synchronization
        break;
      }

      case 6: {
        textEffect_t effectIn, effectOut;
        getThemeEffects(effectIn, effectOut);
        Serial.println("🖥️ Displaying DAYS SINCE ATH screen...");
        Serial.printf("🔤 Displaying text: %s (Top), %s (Bottom)\n", daysAthText, "Since ATH");
        P.displayZoneText(ZONE_UPPER, daysAthText, PA_CENTER, SCROLL_SPEED, 10000, effectIn, effectOut);
        P.displayZoneText(ZONE_LOWER, "Since ATH", PA_CENTER, SCROLL_SPEED, 10000, effectIn, effectOut);
        P.synchZoneStart(); // Force synchronization
        break;
      }

      case 7: {
        textEffect_t effectIn, effectOut;
        getThemeEffects(effectIn, effectOut);
        String currentFiat = getCurrentFiatCode();
        static char satsLabel[48];  // Increased buffer size for safety
        snprintf(satsLabel, sizeof(satsLabel), "SATS/%s", currentFiat.c_str());
        Serial.println("🖥️ Displaying SATS per " + currentFiat + " screen...");
        Serial.printf("🔤 Displaying text: %s (Top), %s (Bottom)\n", satsLabel, satsText);
        P.displayZoneText(ZONE_UPPER, satsLabel, PA_CENTER, SCROLL_SPEED, 10000, effectIn, effectOut);
        P.displayZoneText(ZONE_LOWER, satsText, PA_CENTER, SCROLL_SPEED, 10000, effectIn, effectOut);
        P.synchZoneStart(); // Force synchronization  
        break;
      }
        
      case 8: {
        textEffect_t effectIn, effectOut;
        getThemeEffects(effectIn, effectOut);
        Serial.println("🖥️ Displaying FEE RATE screen...");
        Serial.printf("🔤 Displaying text: %s (Top), %s (Bottom)\n", "FEE RATE", feeText);
        P.displayZoneText(ZONE_UPPER, "FEE RATE", PA_CENTER, SCROLL_SPEED, 10000, effectIn, effectOut);
        P.displayZoneText(ZONE_LOWER, feeText, PA_CENTER, SCROLL_SPEED, 10000, effectIn, effectOut);
        P.synchZoneStart(); // Force synchronization
        break;
      }

      case 9: {
        textEffect_t effectIn, effectOut;
        getThemeEffects(effectIn, effectOut);
        Serial.println("🖥️ Displaying HASHRATE screen...");
        Serial.printf("🔤 Displaying text: %s (Top), %s (Bottom)\n", "HASHRATE", hashrateText);
        P.displayZoneText(ZONE_UPPER, "HASHRATE", PA_CENTER, SCROLL_SPEED, 10000, effectIn, effectOut);
        P.displayZoneText(ZONE_LOWER, hashrateText, PA_CENTER, SCROLL_SPEED, 10000, effectIn, effectOut);
        P.synchZoneStart(); // Force synchronization
        break;
      }

        
      case 10: {
        textEffect_t effectIn, effectOut;
        getThemeEffects(effectIn, effectOut);
        Serial.println("🖥️ Displaying TIME and City screen...");
        Serial.printf("🔤 Displaying text: %s (Top), %s (Bottom)\n", "TIME", timeText);
        // Safety: Show "LOCAL TIME" if city is empty
        const char* cityDisplay = (savedCity.length() > 0) ? savedCity.c_str() : "LOCAL TIME";
        P.displayZoneText(ZONE_UPPER, cityDisplay, PA_CENTER, SCROLL_SPEED, 10000, effectIn, effectOut);
        P.displayZoneText(ZONE_LOWER, timeText, PA_CENTER, SCROLL_SPEED, 10000, effectIn, effectOut);
        P.synchZoneStart(); // Force synchronization
        break;
      }

        
      case 11: {
        textEffect_t effectIn, effectOut;
        getThemeEffects(effectIn, effectOut);
        Serial.println("🖥️ Displaying DAY/DATE screen...");
        Serial.printf("🔤 Displaying text: %s (Top), %s (Bottom)\n", dayText, dateText);
        P.displayZoneText(ZONE_UPPER, dayText, PA_CENTER, SCROLL_SPEED, 10000, effectIn, effectOut);
        P.displayZoneText(ZONE_LOWER, dateText, PA_CENTER, SCROLL_SPEED, 10000, effectIn, effectOut);
        P.synchZoneStart(); // Force synchronization
        break;
      }

        
      case 12: {
        textEffect_t effectIn, effectOut;
        getThemeEffects(effectIn, effectOut);
        Serial.println("🖥️ Displaying WEATHER screen...");
        static char tempDisplay[16];
        
        // 🌡️ Convert temperature based on user's preference
        int displayTemp = temperature;
        char tempUnit = 'C';
        if (savedTempUnit == "F") {
          displayTemp = (int)((temperature * 9.0 / 5.0) + 32); // Convert C to F
          tempUnit = 'F';
        }
        
        snprintf(tempDisplay, sizeof(tempDisplay), (displayTemp >= 0) ? "+%d%c" : "%d%c", displayTemp, tempUnit);
        String cond = weatherCondition;
        cond.replace("_", " ");
        cond.toLowerCase();
        // Safety: Only capitalize if string has content
        if (cond.length() > 0) {
          cond[0] = toupper(cond[0]);
        }
        static char condDisplay[32];
        strncpy(condDisplay, cond.c_str(), sizeof(condDisplay));
        condDisplay[sizeof(condDisplay) - 1] = '\0';

        Serial.printf("🔤 Displaying text: %s (Top), %s (Bottom)\n", tempDisplay, condDisplay);
        P.displayZoneText(ZONE_UPPER, tempDisplay, PA_CENTER, SCROLL_SPEED, 10000, effectIn, effectOut);
        P.displayZoneText(ZONE_LOWER, condDisplay, PA_CENTER, SCROLL_SPEED, 10000, effectIn, effectOut);
        P.synchZoneStart(); // Force synchronization
        break;
      }

      

      case 13: {
        textEffect_t effectIn, effectOut;
        getThemeEffects(effectIn, effectOut);
        Serial.println("🖥️ Displaying MOSCOW TIME screen...");
        Serial.printf("🔤 Displaying text: %s (Top), %s (Bottom)\n", "MOSCOW TIME", satsText2);
        P.displayZoneText(ZONE_UPPER, "MOSCOW TIME", PA_CENTER, SCROLL_SPEED, 10000, effectIn, effectOut);
        P.displayZoneText(ZONE_LOWER, satsText2, PA_CENTER, SCROLL_SPEED, 10000, effectIn, effectOut);
        P.synchZoneStart(); // Force synchronization  
        break;
      }

      case 14: {// Custom user message or default Satoshi tribute
        textEffect_t effectIn, effectOut;
        getThemeEffects(effectIn, effectOut);
        
        // Use custom text if provided, otherwise show Satoshi tribute
        const char* topLine = (savedTopText.length() > 0) ? savedTopText.c_str() : "Satoshi";
        const char* bottomLine = (savedBottomText.length() > 0) ? savedBottomText.c_str() : "Nakamoto";
        
        Serial.printf("🔤 Displaying custom message: %s (Top), %s (Bottom)\n", topLine, bottomLine);
        P.displayZoneText(ZONE_UPPER, topLine, PA_CENTER, SCROLL_SPEED, 10000, effectIn, effectOut); 
        P.displayZoneText(ZONE_LOWER, bottomLine, PA_CENTER, SCROLL_SPEED, 10000, effectIn, effectOut); 
        P.synchZoneStart();
        break;
      }
    }

      Serial.println("✅ Screen update complete.");
      
      // 📊 Advance to next enabled display cycle with safety checks
      uint8_t attempts = 0;
      bool foundEnabled = false;

      do {
        displayCycle = (displayCycle + 1) % 15;
        attempts++;
        
        if (displayEnabled[displayCycle]) {
          foundEnabled = true;
          break;
        }
        
        // Safety: If we've tried all 15 cases and found nothing, force-enable case 0
        if (attempts >= 15) {
          Serial.println("⚠️ All displays disabled! Force-enabling Case 0 (Block Height) as failsafe.");
          displayEnabled[0] = true;  // Force-enable Block Height display
          displayCycle = 0;
          foundEnabled = true;
          break;
        }
      } while (!foundEnabled);
      
    }
  }
