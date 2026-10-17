#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

// Same wiring as the portfolio; buttons are not needed for this test.
#if defined(ARDUINO_ARCH_ESP32)
#if !defined(CONFIG_IDF_TARGET_ESP32S3)
#error "Select an ESP32-S3 board; this wiring is not for the original ESP32."
#endif
constexpr uint8_t TFT_CS = 10;
constexpr uint8_t TFT_DC = 9;
constexpr uint8_t TFT_RESET = 8;
constexpr uint8_t TFT_MOSI = 11;
constexpr uint8_t TFT_SCK = 12;
#elif defined(ARDUINO_ARCH_AVR)
constexpr uint8_t TFT_CS = 9;
constexpr uint8_t TFT_DC = 8;
constexpr uint8_t TFT_RESET = 7;
#else
#error "This project supports ESP32-S3 and the legacy AVR Nano."
#endif
Adafruit_ILI9341 display(TFT_CS, TFT_DC, TFT_RESET);

const uint16_t colors[] = {
  ILI9341_RED, ILI9341_GREEN, ILI9341_BLUE, ILI9341_BLACK, ILI9341_WHITE
};
const char *const colorNames[] = {"RED", "GREEN", "BLUE", "BLACK", "WHITE"};
uint8_t colorIndex = 0;

void setup() {
  Serial.begin(9600);
  Serial.print(F("ILI9341 test starting: CS="));
  Serial.print(TFT_CS);
  Serial.print(F(", DC="));
  Serial.print(TFT_DC);
  Serial.print(F(", RESET="));
  Serial.println(TFT_RESET);
  delay(250);
  // Slow SPI to test jumper wiring more reliably.
#if defined(ARDUINO_ARCH_ESP32)
  SPI.begin(TFT_SCK, -1, TFT_MOSI, TFT_CS);
#endif
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
