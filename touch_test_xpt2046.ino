#include <SPI.h>
#include <XPT2046_Touchscreen.h>

#define T_CS 21
#define T_IRQ 22

XPT2046_Touchscreen ts(T_CS, T_IRQ);

void setup() {
  Serial.begin(115200);
  ts.begin();
  ts.setRotation(0);  // match your screen rotation

  Serial.println("Touch test ready...");
}

void loop() {
  if (ts.touched()) {
    TS_Point p = ts.getPoint();

    Serial.print("X: ");
    Serial.print(p.x);
    Serial.print(" | Y: ");
    Serial.print(p.y);
    Serial.print(" | Z: ");
    Serial.println(p.z);

    delay(200);
  }
}
