#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

#define TFT_BL 15

// Colors
#define SW_BG        0x0000  // Black
#define SW_ORANGE    0xFD20
#define SW_CYAN      0x07FF
#define SW_WHITE     0xFFFF
#define SW_DARKGREY  0x4208
#define SW_GREY      0x8410

void drawLogoPlate() {
  int x = 40;
  int y = 35;
  int w = 240;
  int h = 70;

  tft.drawRoundRect(x, y, w, h, 10, SW_ORANGE);
  tft.drawRoundRect(x + 2, y + 2, w - 4, h - 4, 10, SW_CYAN);

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(SW_WHITE, SW_BG);
  tft.setTextSize(2);
  tft.drawString("STACKSWORTH", x + w / 2, y + h / 2 - 6);

  tft.setTextSize(1);
  tft.setTextColor(SW_CYAN, SW_BG);
  tft.drawString("WHERE DATA COMES TO LIFE", x + w / 2, y + h / 2 + 18);
}

void drawStatusText() {
  tft.setTextDatum(MC_DATUM);

  tft.setTextColor(SW_WHITE, SW_BG);
  tft.setTextSize(2);
  tft.drawString("BITCOIN SWITCH", 160, 145);

  tft.setTextColor(SW_GREY, SW_BG);
  tft.setTextSize(1);
  tft.drawString("VENDING INTERFACE TEST", 160, 168);
}

void drawTapBox(bool highlight) {
  int x = 45;
  int y = 205;
  int w = 230;
  int h = 70;

  uint16_t border1 = highlight ? SW_ORANGE : SW_DARKGREY;
  uint16_t border2 = highlight ? SW_CYAN : SW_GREY;
  uint16_t textCol = highlight ? SW_WHITE : SW_GREY;

  tft.fillRoundRect(x, y, w, h, 12, SW_BG);
  tft.drawRoundRect(x, y, w, h, 12, border1);
  tft.drawRoundRect(x + 2, y + 2, w - 4, h - 4, 12, border2);

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(textCol, SW_BG);
  tft.setTextSize(2);
  tft.drawString("TAP HERE", x + w / 2, y + 24);

  tft.setTextSize(1);
  tft.drawString("TO START", x + w / 2, y + 50);
}

void drawFooter() {
  tft.drawFastHLine(25, 300, 270, SW_DARKGREY);

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(SW_ORANGE, SW_BG);
  tft.setTextSize(1);
  tft.drawString("STACKSWORTH DISPLAY LAB", 160, 314);
}

void drawScreen(bool highlight) {
  tft.fillScreen(SW_BG);

  drawLogoPlate();
  drawStatusText();
  drawTapBox(highlight);
  drawFooter();
}

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  tft.init();
  tft.setRotation(1);   // Change if needed
  tft.fillScreen(SW_BG);

  drawScreen(false);
}

void loop() {
  drawTapBox(false);
  delay(700);

  drawTapBox(true);
  delay(700);
}
