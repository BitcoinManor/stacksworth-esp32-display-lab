#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

#define TFT_BL 15

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Starting TFT color test");

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  tft.init();
  tft.setRotation(1);

  Serial.println("TFT init done");
}

void loop() {
  Serial.println("RED");
  tft.fillScreen(TFT_RED);
  delay(1500);

  Serial.println("GREEN");
  tft.fillScreen(TFT_GREEN);
  delay(1500);

  Serial.println("BLUE");
  tft.fillScreen(TFT_BLUE);
  delay(1500);

  Serial.println("WHITE");
  tft.fillScreen(TFT_WHITE);
  delay(1500);

  Serial.println("BLACK");
  tft.fillScreen(TFT_BLACK);
  delay(1500);
}
