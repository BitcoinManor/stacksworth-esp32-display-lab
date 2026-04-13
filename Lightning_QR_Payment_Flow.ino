#ifdef TFT
#include <TFT_eSPI.h>
#include <qrcoded.h>

TFT_eSPI tft = TFT_eSPI(TFT_WIDTH, TFT_HEIGHT);

// ---------- Colors ----------
#define SW_BG        0x0000
#define SW_WHITE     0xFFFF
#define SW_ORANGE    0xFD20
#define SW_CYAN      0x07FF
#define SW_GREEN     0x07E0
#define SW_RED       0xF800
#define SW_GREY      0x8410
#define SW_DARKGREY  0x4208
#define SW_PURPLE    0x781F

// ---------- Touch calibration ----------
uint16_t calData[5] = { 243, 3512, 270, 3585, 2 };

// ---------- Screen state ----------
enum TFTScreenState {
  TFT_SCREEN_IDLE,
  TFT_SCREEN_PREPARING,
  TFT_SCREEN_PAYMENT,
  TFT_SCREEN_PAID,
  TFT_SCREEN_VENDING
};

TFTScreenState currentTFTScreen = TFT_SCREEN_IDLE;

// ---------- Touch / button state ----------
uint16_t touchX = 0, touchY = 0;
bool buttonHighlighted = false;
unsigned long lastTouchMs = 0;
const unsigned long BUTTON_HIGHLIGHT_MS = 180;

// ---------- QR timer ----------
unsigned long qrStartTime = 0;
unsigned long lastCountdownDrawMs = 0;

// Button area on home screen
const int BTN_X = 25;
const int BTN_Y = 250;
const int BTN_W = 270;
const int BTN_H = 90;

// Cancel button (QR screen)
const int CANCEL_X = 90;
const int CANCEL_Y = 420;
const int CANCEL_W = 140;
const int CANCEL_H = 40;

// ---------- Header ----------
void drawHeader() {
  tft.drawRoundRect(20, 20, 280, 85, 12, SW_ORANGE);
  tft.drawRoundRect(22, 22, 276, 81, 12, SW_CYAN);

  tft.setTextColor(SW_WHITE, SW_BG);
  tft.setTextSize(3);
  tft.setCursor(38, 36);
  tft.println("STACKSWORTH");

  tft.setTextColor(SW_ORANGE, SW_BG);
  tft.setTextSize(2);
  tft.setCursor(72, 68);
  tft.println("BITCOIN SWITCH");

  tft.setTextColor(SW_WHITE, SW_BG);
  tft.setTextSize(1);
  tft.setCursor(90, 90);
  tft.println("powered by LNbits");
}

// ---------- Heartbeat ----------
void drawHeartbeatDot(bool ping) {
  uint16_t color = ping ? SW_CYAN : SW_ORANGE;
  tft.fillCircle(160, 180, 10, color);

  tft.setTextSize(1);
  tft.setTextColor(SW_GREY, SW_BG);
  tft.setCursor(118, 200);
  tft.println("WS HEARTBEAT");
}

// ---------- Footer ----------
void drawStatusFooter(bool wifi, bool ws) {
  tft.drawFastHLine(20, 410, 280, SW_DARKGREY);

  tft.setTextSize(2);
  tft.setTextColor(SW_WHITE, SW_BG);

  tft.setCursor(35, 430);
  tft.print("WiFi");
  tft.fillCircle(115, 438, 8, wifi ? SW_GREEN : SW_RED);

  tft.setCursor(170, 430);
  tft.print("WS");
  tft.fillCircle(215, 438, 8, ws ? SW_GREEN : SW_RED);
}

// ---------- Button ----------
void drawButtonBox(const char* line1, const char* line2, bool highlight) {
  uint16_t border1 = highlight ? SW_GREEN : SW_ORANGE;
  uint16_t border2 = highlight ? SW_WHITE : SW_CYAN;
  uint16_t textCol = SW_WHITE;

  tft.fillRoundRect(BTN_X, BTN_Y, BTN_W, BTN_H, 14, SW_BG);
  tft.drawRoundRect(BTN_X, BTN_Y, BTN_W, BTN_H, 14, border1);
  tft.drawRoundRect(BTN_X + 2, BTN_Y + 2, BTN_W - 4, BTN_H - 4, 14, border2);

  tft.setTextColor(textCol, SW_BG);

  tft.setTextSize(3);
  tft.setCursor(65, BTN_Y + 18);
  tft.println(line1);

  tft.setTextSize(2);
  tft.setCursor(100, BTN_Y + 55);
  tft.println(line2);
}

//----------QRCode----------
void drawQRCode(const char *data) {
  QRCode qrcode;
  uint8_t qrcodeData[qrcode_getBufferSize(12)];
  qrcode_initText(&qrcode, qrcodeData, 12, 0, data);

  int scale = 3;
  int qrSize = qrcode.size * scale;
  int offsetX = (tft.width() - qrSize) / 2;
  int offsetY = 160;

  // 🔥 ADD THIS HERE (quiet zone)
  tft.fillRect(offsetX - 12, offsetY - 12, qrSize + 24, qrSize + 24, TFT_WHITE);
  
  for (uint8_t y = 0; y < qrcode.size; y++) {
    for (uint8_t x = 0; x < qrcode.size; x++) {
      uint16_t color = qrcode_getModule(&qrcode, x, y) ? TFT_BLACK : TFT_WHITE;
      tft.fillRect(offsetX + x * scale, offsetY + y * scale, scale, scale, color);
    }
  }
}

// Countdown for Payment

void drawQrCountdown() {
  unsigned long elapsed = (millis() - qrStartTime) / 1000;
  int remaining = 120 - (int)elapsed;
  if (remaining < 0) remaining = 0;

  // Clear countdown area
  tft.fillRect(70, 390, 180, 20, SW_BG);

  tft.setTextColor(SW_WHITE, SW_BG);
  tft.setTextSize(2);
  tft.setCursor(78, 390);
  tft.print("EXPIRES IN: ");
  tft.print(remaining);
  tft.print("s");
}


// ---------- Screens ----------
void showIdleScreen(bool wifi, bool ws, bool ping) {
  currentTFTScreen = TFT_SCREEN_IDLE;

  tft.fillScreen(SW_BG);
  drawHeader();

  tft.setTextColor(SW_WHITE, SW_BG);
  tft.setTextSize(3);
  tft.setCursor(110, 135);
  tft.println("READY");

  drawHeartbeatDot(ping);
  drawButtonBox("TAP HERE", "TO START", buttonHighlighted);
  drawStatusFooter(wifi, ws);
}

void showPreparingScreen() {
  currentTFTScreen = TFT_SCREEN_PREPARING;

  tft.fillScreen(SW_BG);
  drawHeader();

  tft.setTextColor(SW_ORANGE, SW_BG);
  tft.setTextSize(3);
  tft.setCursor(48, 140);
  tft.println("PREPARING");

  tft.setTextColor(SW_WHITE, SW_BG);
  tft.setTextSize(2);
  tft.setCursor(58, 215);
  tft.println("Checking WiFi...");
  tft.setCursor(40, 250);
  tft.println("Preparing invoice...");
}

void showPaymentScreenPlaceholder() {
  currentTFTScreen = TFT_SCREEN_PAYMENT;

  tft.fillScreen(SW_BG);
  drawHeader();

  tft.setTextColor(SW_ORANGE, SW_BG);
  tft.setTextSize(2);
  tft.setCursor(15, 120);
  tft.println("SCAN TO PAY via LIGHTNING");

  
  drawQRCode("lightning:LNURL1DP68GURN8GHJ7ARPWQ68XCT5WVHXY6T5VDHKJMNDV9HX7U3WVDHK6TMZD96XXMMFDEEHW6T5VD5Z7CTSDYHHVVF0D3H82UNVG96RG4ENG499GJNSW9E85KRD8PF9ZJN4F3TN7URFDC7NXV3XV9KK7ATWWS7NZDFS9CCZVER4WFSHG6T0DC7NZDFSXQN8VCTJD9SKYMR984RXZMRNV5NXXMMDD4JKUAPAGESKCUM9YEJXJUMPVFKX2ARFD4JN6VQNFFHER");

    //drawQRCode("bitcoinmanor.com");
    
  tft.setTextColor(SW_WHITE, SW_BG);
  tft.setTextSize(2);
  tft.setCursor(110, 370);
  tft.println("150 SATS");

  drawQrCountdown();
  lastCountdownDrawMs = 0;

  // CANCEL button
  tft.drawRoundRect(90, 420, 140, 40, 10, SW_ORANGE);
  tft.drawRoundRect(92, 422, 136, 36, 10, SW_CYAN);

  tft.setTextColor(SW_WHITE, SW_BG);
  tft.setTextSize(2);
  tft.setCursor(115, 430);
  tft.println("CANCEL");

  //QR Start Timer
  qrStartTime = millis();
  Serial.println("QR SCREEN OPENED -> Timer started");
  
}

void showPaidScreen() {
  currentTFTScreen = TFT_SCREEN_PAID;

  tft.fillScreen(SW_PURPLE);

  tft.setTextColor(SW_WHITE, SW_PURPLE);
  tft.setTextSize(3);
  tft.setCursor(15, 140);
  tft.println("BITCOIN RECEIVED");

  tft.setCursor(35, 185);
  tft.println("via LIGHTNING");
}

void showVendScreen() {
  currentTFTScreen = TFT_SCREEN_VENDING;

  tft.fillScreen(SW_PURPLE);

  tft.setTextColor(SW_WHITE, SW_PURPLE);
  tft.setTextSize(3);
  tft.setCursor(55, 150);
  tft.println("VENDING...");

  tft.setTextSize(2);
  tft.setCursor(85, 220);
  tft.println("CHOOSE YOUR ITEM");
}

// ---------- Touch handling ----------
bool inHomeButton(uint16_t tx, uint16_t ty) {
  return (tx >= BTN_X && tx <= BTN_X + BTN_W &&
          ty >= BTN_Y && ty <= BTN_Y + BTN_H);
}

bool inCancelButton(uint16_t tx, uint16_t ty) {
  return (tx >= CANCEL_X && tx <= CANCEL_X + CANCEL_W &&
          ty >= CANCEL_Y && ty <= CANCEL_Y + CANCEL_H);
}

void loopTFT(bool wifi, bool ws, bool ping) {
  // QR timeout always runs, even with no touch
  if (currentTFTScreen == TFT_SCREEN_PAYMENT) {
    if (millis() - lastCountdownDrawMs >= 1000) {
      drawQrCountdown();
      lastCountdownDrawMs = millis();
    }

    if (millis() - qrStartTime > 120000) {
      Serial.println("QR TIMEOUT -> Returning to HOME");
      buttonHighlighted = false;
      showIdleScreen(wifi, ws, ping);
      return;
    }
  }

  if (!tft.getTouch(&touchX, &touchY)) {
    if (buttonHighlighted && millis() - lastTouchMs > BUTTON_HIGHLIGHT_MS) {
      buttonHighlighted = false;

      if (currentTFTScreen == TFT_SCREEN_IDLE) {
        showPreparingScreen();
        delay(2100);
        showPaymentScreenPlaceholder();
      }
    }
    return;
  }

  Serial.print("TOUCH -> X: ");
  Serial.print(touchX);
  Serial.print(" | Y: ");
  Serial.println(touchY);

  if (currentTFTScreen == TFT_SCREEN_IDLE && inHomeButton(touchX, touchY)) {
    lastTouchMs = millis();
    if (!buttonHighlighted) {
      buttonHighlighted = true;
      showIdleScreen(wifi, ws, ping);
      Serial.println("HOME BUTTON PRESSED -> Going to Payment");
    }
  }

  if (currentTFTScreen == TFT_SCREEN_PAYMENT && inCancelButton(touchX, touchY)) {
    Serial.println("CANCEL PRESSED -> Returning to HOME");
    buttonHighlighted = false;
    showIdleScreen(wifi, ws, ping);
    delay(300);
    return;
  }
}

// ---------- Hooks used by main app ----------
void setupTFT() {
  tft.init();

  Serial.println("TFT: " + String(TFT_WIDTH) + "x" + String(TFT_HEIGHT));
  Serial.println("TFT pin CS: " + String(TFT_CS));

  tft.setRotation(2);
  tft.setTouch(calData);
  tft.invertDisplay(false);
  tft.fillScreen(SW_BG);
}

void printTFT(String message, int x, int y) {
  tft.setTextSize(2);
  tft.setTextColor(SW_WHITE, SW_BG);
  tft.setCursor(x, y);
  tft.println(message);
}

void printHome(bool wifi, bool ws, bool ping) {
  // Only redraw the home screen if we are actually on the home screen
  if (currentTFTScreen == TFT_SCREEN_IDLE) {
    showIdleScreen(wifi, ws, ping);
  }
}

void clearTFT() {
  tft.fillScreen(SW_BG);
}

void flashTFT() {
  showPaidScreen();
  delay(2100);

  showVendScreen();
  delay(2500);

  buttonHighlighted = false;
  showIdleScreen(true, true, false);
}

#else
void setupTFT() {}
void printTFT(String message, int x, int y) {}
void printHome(bool wifi, bool ws, bool ping) {}
void clearTFT() {}
void flashTFT() {}
void loopTFT(bool wifi, bool ws, bool ping) {}
#endif
