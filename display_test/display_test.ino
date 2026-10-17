#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

// Same wiring as the portfolio; buttons are not needed for this test.
Adafruit_ILI9341 display(9, 8, 7);  // CS, DC, RESET.

const uint16_t colors[] = {
  ILI9341_RED, ILI9341_GREEN, ILI9341_BLUE, ILI9341_BLACK, ILI9341_WHITE
};
const char *const colorNames[] = {"RED", "GREEN", "BLUE", "BLACK", "WHITE"};
uint8_t colorIndex = 0;

void setup() {
  Serial.begin(9600);
  Serial.println(F("ILI9341 test starting: CS=D9, DC=D8, RESET=D7"));
  delay(250);
  // Slow SPI to test resistor dividers and breadboard wiring more reliably.
  display.begin(1000000);
  display.setRotation(0);
  Serial.println(F("Initialization commands sent; cycling colors at 1 MHz SPI."));
  // This is not an acknowledgement from the display: MISO is not connected.
}

void loop() {
  Serial.println(colorNames[colorIndex]);
  display.fillScreen(colors[colorIndex]);
  delay(1500);
  colorIndex = (colorIndex + 1) % (sizeof(colors) / sizeof(colors[0]));
}
