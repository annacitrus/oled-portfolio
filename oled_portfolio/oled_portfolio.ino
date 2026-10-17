#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include "terminal_font.h"
#include "small_terminal_font.h"
#include "starkhacks_photo.h"
#include "slide_3_photo.h"
#include "personal_photo.h"

// The original intro-only font silently omits the new StarkHacks letters.
static_assert(TERMINAL_GLYPH_COUNT >= 14,
              "Outdated terminal_font.h: copy the updated font header or run "
              "tools/generate-terminal-font.ps1 before uploading.");

// GPIO numbers, not physical header positions. See README.md for wiring.
#if defined(ARDUINO_ARCH_ESP32)
#if !defined(CONFIG_IDF_TARGET_ESP32S3)
#error "Select an ESP32-S3 board; this wiring is not for the original ESP32."
#endif
constexpr uint8_t TFT_CS = 10;
constexpr uint8_t TFT_DC = 9;
constexpr uint8_t TFT_RESET = 8;
constexpr uint8_t TFT_MOSI = 11;
constexpr uint8_t TFT_SCK = 12;
constexpr uint8_t BACK_BUTTON = 4;
constexpr uint8_t NEXT_BUTTON = 5;
#elif defined(ARDUINO_ARCH_AVR)
// Legacy Nano wiring, including the original 5 V -> 3.3 V signal dividers.
constexpr uint8_t TFT_CS = 9;
constexpr uint8_t TFT_DC = 8;
constexpr uint8_t TFT_RESET = 7;
constexpr uint8_t BACK_BUTTON = 4;
constexpr uint8_t NEXT_BUTTON = 2;
#else
#error "This project supports ESP32-S3 and the legacy AVR Nano."
#endif

constexpr uint32_t TFT_SPI_HZ = 8000000;  // Conservative breadboard SPI speed.

Adafruit_ILI9341 display(TFT_CS, TFT_DC, TFT_RESET);

constexpr uint16_t BACKGROUND = ILI9341_BLACK;
constexpr uint16_t FOREGROUND = 0xD69A;  // Terminal gray, approximately #D4D4D4.
constexpr unsigned long START_DELAY_MS = 600;
constexpr unsigned long TYPE_INTERVAL_MS = 140;
constexpr unsigned long CURSOR_BLINK_MS = 500;
constexpr unsigned long DEBOUNCE_MS = 30;

const char INTRO_TEXT[] PROGMEM = "Anna Citron";
const char STARK_TITLE[] PROGMEM = "StarkHacks";
const char STARK_SUBTITLE_1[] PROGMEM = "the world's largest";
const char STARK_SUBTITLE_2[] PROGMEM = "hardware hackathon";
const char STARK_STAT_1[] PROGMEM = "2M Reel views";
const char STARK_STAT_2[] PROGMEM = "3k applicants";
const char PERSONAL_USERNAME[] PROGMEM = "@annacitrus";
const char PERSONAL_LABEL[] PROGMEM = "personal account";
constexpr uint8_t SLIDE_COUNT = 4;

struct Button {
  uint8_t pin;
  bool lastReading;
  bool stableReading;
  unsigned long changedAt;
};

Button backButton = {BACK_BUTTON, HIGH, HIGH, 0};
Button nextButton = {NEXT_BUTTON, HIGH, HIGH, 0};

uint8_t currentSlide = 0;
uint8_t typedCharacters = 0;
uint8_t activeLine = 0;
unsigned long slideStartedAt = 0;
unsigned long lastCharacterAt = 0;
unsigned long lastCursorAt = 0;
bool cursorVisible = true;
int16_t textX = 0;
int16_t textY = 0;

bool wasPressed(Button &button, unsigned long now) {
  const bool reading = digitalRead(button.pin);
  if (reading != button.lastReading) {
    button.lastReading = reading;
    button.changedAt = now;
  }
  if (reading != button.stableReading &&
      now - button.changedAt >= DEBOUNCE_MS) {
    button.stableReading = reading;
    return reading == LOW;
  }
  return false;
}

bool isSmallLine() {
  return (currentSlide == 1 || currentSlide == 3) && activeLine > 0;
}

uint8_t cellWidth() { return isSmallLine() ? 8 : TERMINAL_CELL_WIDTH; }

PGM_P lineText() {
  if (currentSlide == 0) return INTRO_TEXT;
  if (currentSlide == 3) return activeLine == 0 ? PERSONAL_USERNAME : PERSONAL_LABEL;
  if (activeLine == 0) return STARK_TITLE;
  if (activeLine == 1) return STARK_SUBTITLE_1;
  if (activeLine == 2) return STARK_SUBTITLE_2;
  return activeLine == 3 ? STARK_STAT_1 : STARK_STAT_2;
}

void positionLine() {
  textX = (display.width() - (strlen_P(lineText()) + 1) * cellWidth()) / 2;
  if (currentSlide == 3) {
    textY = activeLine == 0 ? 136 : 168;
    return;
  }
  textY = currentSlide == 0 ? (display.height() - TERMINAL_CELL_HEIGHT) / 2
                           : activeLine == 0 ? 28 : activeLine == 1 ? 78
                           : activeLine == 2 ? 98 : activeLine == 3 ? 286 : 304;
}

void drawCursor(bool visible) {
  const int16_t x = textX + typedCharacters * cellWidth();
  display.fillRect(x, textY + (isSmallLine() ? 3 : 5), cellWidth() - 2,
                   isSmallLine() ? 12 : 25,
                   visible ? FOREGROUND : BACKGROUND);
}

void drawTerminalCharacter(char character, int16_t x, int16_t y) {
  if (isSmallLine()) {
    for (uint8_t i = 0; i < SMALL_TERMINAL_GLYPH_COUNT; ++i) {
      if (pgm_read_byte(&smallTerminalCharacters[i]) == character) {
        display.drawBitmap(x, y, smallTerminalBitmaps[i], 8, 16, FOREGROUND);
        return;
      }
    }
    return;
  }
  for (uint8_t i = 0; i < TERMINAL_GLYPH_COUNT; ++i) {
    if (pgm_read_byte(&terminalCharacters[i]) == character) {
      display.drawBitmap(x, y, terminalBitmaps[i], TERMINAL_CELL_WIDTH,
                         TERMINAL_CELL_HEIGHT, FOREGROUND);
      return;
    }
  }
}

void drawGrayscalePhoto(const uint16_t *pixels,
                        uint16_t width, uint16_t height, uint16_t left,
                        uint16_t top, uint16_t outputWidth, uint16_t outputHeight) {
  // One scanline in RAM; all photo data stays in flash (no framebuffer).
  uint16_t row[224];
  display.startWrite();
  display.setAddrWindow(left, top, outputWidth, outputHeight);
  for (uint16_t y = 0; y < outputHeight; ++y) {
    const uint16_t sourceY = (uint32_t)y * height / outputHeight;
    for (uint16_t x = 0; x < outputWidth; ++x) {
      const uint16_t sourceX = (uint32_t)x * width / outputWidth;
      const uint32_t index = (uint32_t)sourceY * width + sourceX;
      row[x] = pgm_read_word(&pixels[index]);
    }
    display.writePixels(row, outputWidth);
  }
  display.endWrite();
}

void beginSlide(uint8_t index, unsigned long now) {
  currentSlide = index;
  display.fillScreen(BACKGROUND);

  if (currentSlide == 1) {
    drawGrayscalePhoto(starkPhotoPixels,
                       STARK_PHOTO_WIDTH, STARK_PHOTO_HEIGHT, 24, 140, 192, 144);
  } else if (currentSlide == 2) {
    drawGrayscalePhoto(collagePixels,
                       COLLAGE_WIDTH, COLLAGE_HEIGHT, 8, 10, 224, 300);
    return;  // The third slide displays only the supplied collage.
  } else if (currentSlide == 3) {
    drawGrayscalePhoto(personalPixels,
                       PERSONAL_WIDTH, PERSONAL_HEIGHT, 8, 8, 224, 304);
  }
  now = millis();  // Start the typing delay after the photo has finished drawing.
  activeLine = 0;
  positionLine();
  typedCharacters = 0;
  slideStartedAt = now;
  lastCharacterAt = now;
  lastCursorAt = now;
  cursorVisible = true;
  drawCursor(cursorVisible);
}

void updateTyping(unsigned long now) {
  if (currentSlide == 2) return;
  if (((currentSlide == 1 && activeLine < 4) ||
       (currentSlide == 3 && activeLine < 1)) &&
      typedCharacters == strlen_P(lineText()) &&
      now - lastCharacterAt >= TYPE_INTERVAL_MS) {
    drawCursor(false);
    ++activeLine;
    typedCharacters = 0;
    positionLine();
    cursorVisible = true;
    lastCursorAt = now;
    drawCursor(true);
  }
  if (typedCharacters < strlen_P(lineText()) &&
      now - slideStartedAt >= START_DELAY_MS &&
      now - lastCharacterAt >= TYPE_INTERVAL_MS) {
    drawCursor(false);
    const char character = pgm_read_byte(lineText() + typedCharacters);
    drawTerminalCharacter(character,
                          textX + typedCharacters * cellWidth(), textY);
    ++typedCharacters;
    lastCharacterAt = now;
    lastCursorAt = now;
    cursorVisible = true;
    drawCursor(cursorVisible);
  }

  if (now - lastCursorAt >= CURSOR_BLINK_MS) {
    lastCursorAt = now;
    cursorVisible = !cursorVisible;
    drawCursor(cursorVisible);
  }
}

void setup() {
  pinMode(BACK_BUTTON, INPUT_PULLUP);
  pinMode(NEXT_BUTTON, INPUT_PULLUP);
#if defined(ARDUINO_ARCH_ESP32)
  // Write-only display: MISO is not connected. Set routing before display.begin().
  SPI.begin(TFT_SCK, -1, TFT_MOSI, TFT_CS);
#endif
  display.begin(TFT_SPI_HZ);
  display.setRotation(0);  // Portrait: 240 wide, 320 high.
  beginSlide(0, millis());
}

void loop() {
  const unsigned long now = millis();
  // Read both every frame, including during the typing animation.
  const bool backPressed = wasPressed(backButton, now);
  const bool nextPressed = wasPressed(nextButton, now);

  if (backPressed) {
    beginSlide((currentSlide + SLIDE_COUNT - 1) % SLIDE_COUNT, now);
  } else if (nextPressed) {
    beginSlide((currentSlide + 1) % SLIDE_COUNT, now);
  }

  updateTyping(millis());
}
