#include <SPI.h>
#include <XPT2046_Touchscreen.h>

#define T_CS 21
#define T_IRQ 22

XPT2046_Touchscreen ts(T_CS, T_IRQ);

// Raw calibration values from your corner taps
const int RAW_X_MIN = 500;
const int RAW_X_MAX = 3370;
const int RAW_Y_MIN = 490;
const int RAW_Y_MAX = 3610;

// Screen size in portrait
const int SCREEN_W = 320;
const int SCREEN_H = 480;

// Button area from your current UI
const int BTN_X = 25;
const int BTN_Y = 250;
const int BTN_W = 270;
const int BTN_H = 90;

bool touchActive = false;
unsigned long lastPrint = 0;

int clampInt(int v, int lo, int hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

int mapClamped(int value, int inMin, int inMax, int outMin, int outMax) {
  value = clampInt(value, inMin, inMax);
  return map(value, inMin, inMax, outMin, outMax);
}

void setup() {
  Serial.begin(115200);
  ts.begin();
  ts.setRotation(0);

  Serial.println("Touch mapping test ready...");
}

void loop() {
  if (ts.touched()) {
    TS_Point p = ts.getPoint();

    int sx = mapClamped(p.x, RAW_X_MIN, RAW_X_MAX, 0, SCREEN_W - 1);
    int sy = mapClamped(p.y, RAW_Y_MIN, RAW_Y_MAX, 0, SCREEN_H - 1);

    bool inButton =
      (sx >= BTN_X && sx <= (BTN_X + BTN_W) &&
       sy >= BTN_Y && sy <= (BTN_Y + BTN_H));

    if (!touchActive) {
      touchActive = true;
      Serial.println("TOUCH START");
    }

    if (millis() - lastPrint > 150) {
      Serial.print("RAW X: ");
      Serial.print(p.x);
      Serial.print(" | RAW Y: ");
      Serial.print(p.y);
      Serial.print(" | Z: ");
      Serial.print(p.z);

      Serial.print(" || SCREEN X: ");
      Serial.print(sx);
      Serial.print(" | SCREEN Y: ");
      Serial.print(sy);

      Serial.print(" || BUTTON: ");
      Serial.println(inButton ? "YES" : "NO");

      lastPrint = millis();
    }
  } else {
    if (touchActive) {
      touchActive = false;
      Serial.println("TOUCH RELEASED");
    }
  }

  delay(20);
}
