#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include "terminal_font.h"
#include "small_terminal_font.h"
#include "starkhacks_photo.h"

// The original intro-only font silently omits the new StarkHacks letters.
static_assert(TERMINAL_GLYPH_COUNT >= 14,
              "Outdated terminal_font.h: copy the updated font header or run "
              "tools/generate-terminal-font.ps1 before uploading.");

// Hardware SPI on the Uno: MOSI = D11, SCK = D13.
constexpr uint8_t TFT_CS = 9;
constexpr uint8_t TFT_DC = 8;
constexpr uint8_t TFT_RESET = 7;
constexpr uint8_t BACK_BUTTON = 4;
constexpr uint8_t NEXT_BUTTON = 2;

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
constexpr uint8_t SLIDE_COUNT = 2;

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

bool isSmallLine() { return currentSlide == 1 && activeLine > 0; }

uint8_t cellWidth() { return isSmallLine() ? 8 : TERMINAL_CELL_WIDTH; }

PGM_P lineText() {
  if (currentSlide == 0) return INTRO_TEXT;
  if (activeLine == 0) return STARK_TITLE;
  return activeLine == 1 ? STARK_SUBTITLE_1 : STARK_SUBTITLE_2;
}

void positionLine() {
  textX = (display.width() - (strlen_P(lineText()) + 1) * cellWidth()) / 2;
  textY = currentSlide == 0 ? (display.height() - TERMINAL_CELL_HEIGHT) / 2
                           : activeLine == 0 ? 28 : activeLine == 1 ? 78 : 98;
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

void drawStarkPhoto() {
  // One scanline in RAM; all photo data stays in flash (no framebuffer).
  uint16_t row[STARK_PHOTO_WIDTH];
  display.startWrite();
  display.setAddrWindow(24, 140, STARK_PHOTO_WIDTH, STARK_PHOTO_HEIGHT);
  for (uint16_t y = 0; y < STARK_PHOTO_HEIGHT; ++y) {
    for (uint16_t x = 0; x < STARK_PHOTO_WIDTH; x += 2) {
      const uint8_t pair = pgm_read_byte(
          &starkPhotoPixels[(y * STARK_PHOTO_WIDTH + x) / 2]);
      row[x] = pgm_read_word(&starkPhotoPalette[pair >> 4]);
      row[x + 1] = pgm_read_word(&starkPhotoPalette[pair & 0x0F]);
    }
    display.writePixels(row, STARK_PHOTO_WIDTH);
  }
  display.endWrite();
}

void beginSlide(uint8_t index, unsigned long now) {
  currentSlide = index;
  display.fillScreen(BACKGROUND);

  if (currentSlide == 1) drawStarkPhoto();
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
  if (currentSlide == 1 && activeLine < 2 &&
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
  display.begin();
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
