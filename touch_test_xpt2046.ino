#include <SPI.h>
#include <XPT2046_Touchscreen.h>

#define T_CS 21
#define T_IRQ 22

XPT2046_Touchscreen ts(T_CS, T_IRQ);

bool touchActive = false;
unsigned long lastPrint = 0;

void setup() {
  Serial.begin(115200);
  ts.begin();
  ts.setRotation(0);

  Serial.println("XPT2046 touch test ready...");
  Serial.println("Touch the screen to see coordinates.");
}

void loop() {
  bool touched = ts.touched();

  if (touched) {
    TS_Point p = ts.getPoint();

    if (!touchActive) {
      touchActive = true;
      Serial.println("TOUCH START");
    }

    if (millis() - lastPrint > 120) {
      Serial.print("X: ");
      Serial.print(p.x);
      Serial.print(" | Y: ");
      Serial.print(p.y);
      Serial.print(" | Z: ");
      Serial.println(p.z);
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
