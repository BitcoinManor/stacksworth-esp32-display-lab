#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

uint16_t x, y;

// Calibrated for setRotation(2)
uint16_t calData[5] = { 243, 3512, 270, 3585, 2 };

struct Target {
  int x, y, w, h;
  const char* name;
  bool active;
};

Target targets[5] = {
  {20, 20, 40, 40, "TOP LEFT", false},
  {260, 20, 40, 40, "TOP RIGHT", false},
  {20, 420, 40, 40, "BOTTOM LEFT", false},
  {260, 420, 40, 40, "BOTTOM RIGHT", false},
  {140, 220, 40, 40, "CENTER", false}
};

void drawTargets() {
  tft.fillScreen(TFT_BLACK);

  for (int i = 0; i < 5; i++) {
    uint16_t color = targets[i].active ? TFT_GREEN : TFT_ORANGE;
    tft.fillRect(targets[i].x, targets[i].y, targets[i].w, targets[i].h, color);
  }

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(70, 180);
  tft.println("CALIBRATION");
}

bool inTarget(const Target &t, uint16_t tx, uint16_t ty) {
  return (tx >= t.x && tx <= t.x + t.w &&
          ty >= t.y && ty <= t.y + t.h);
}

void setup() {
  Serial.begin(115200);

  tft.init();
  tft.setRotation(2);
  tft.setTouch(calData);

  drawTargets();

  Serial.println("Touch calibration test ready...");
}

void loop() {
  if (tft.getTouch(&x, &y)) {
    Serial.print("TOUCH -> X: ");
    Serial.print(x);
    Serial.print(" | Y: ");
    Serial.println(y);

    for (int i = 0; i < 5; i++) {
      if (inTarget(targets[i], x, y)) {
        targets[i].active = true;
        drawTargets();

        Serial.print("HIT: ");
        Serial.println(targets[i].name);

        delay(250);

        targets[i].active = false;
        drawTargets();
        break;
      }
    }
  }

  delay(50);
}
