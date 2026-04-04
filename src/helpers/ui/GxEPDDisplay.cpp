
#include "GxEPDDisplay.h"

#ifdef EXP_PIN_BACKLIGHT
  #include <PCA9557.h>
  extern PCA9557 expander;
#endif

#ifndef DISPLAY_ROTATION
  #define DISPLAY_ROTATION 3
#endif

#ifdef EINK_INVERT_COLORS
  #ifdef USE_ADAFRUIT_SSD1680
    #define EINK_BLACK EPD_WHITE
    #define EINK_WHITE EPD_BLACK
  #else
    #define EINK_BLACK GxEPD_WHITE
    #define EINK_WHITE GxEPD_BLACK
  #endif
#else
  #ifdef USE_ADAFRUIT_SSD1680
    #define EINK_BLACK EPD_BLACK
    #define EINK_WHITE EPD_WHITE
  #else
    #define EINK_BLACK GxEPD_BLACK
    #define EINK_WHITE GxEPD_WHITE
  #endif
#endif

#ifdef ESP32
  SPIClass SPI1 = SPIClass(FSPI);
#endif

#if defined(NRF52_PLATFORM)
  // SPIM3 is used by the LoRa radio. Use SPIM2 independently for the e-ink display
  // so the two peripherals never interfere, regardless of radio init order.
  // Hardcode the WisBlock IO-slot SPI pins (MISO=29, SCK=3, MOSI=30) to avoid
  // any interference from PIN_SPI_* macro redefinitions.
  static SPIClass DISPLAY_SPI(NRF_SPIM2, 29, 3, 30);
#endif

bool GxEPDDisplay::begin() {
#ifdef EINK_WHITE_SCREEN_TEST
  Serial.println("GxEPDDisplay::begin: start");
  Serial.print("  CS="); Serial.print(PIN_DISPLAY_CS);
  Serial.print(" DC="); Serial.print(PIN_DISPLAY_DC);
  Serial.print(" RST="); Serial.print(PIN_DISPLAY_RST);
  Serial.print(" BUSY="); Serial.print(PIN_DISPLAY_BUSY);
  Serial.print(" PWR="); Serial.println(PIN_DISPLAY_POWER);
#endif

  if (PIN_DISPLAY_POWER >= 0) {
    // Follow official RAK14000 behavior: weak pull-up on shared power pin.
    pinMode(PIN_DISPLAY_POWER, INPUT_PULLUP);
    digitalWrite(PIN_DISPLAY_POWER, HIGH);
    delay(300);
  }

#ifdef EINK_WHITE_SCREEN_TEST
  if (PIN_DISPLAY_BUSY >= 0) {
    pinMode(PIN_DISPLAY_BUSY, INPUT);
    Serial.print("  BUSY before init=");
    Serial.println(digitalRead(PIN_DISPLAY_BUSY));
  }
#endif

#ifdef USE_ADAFRUIT_SSD1680
  // Adafruit_SSD1680 is instantiated with software SPI pins in this build,
  // so touching global SPI here can break subsequent radio_init() on nRF52.
  display.begin();
  display.setRotation(DISPLAY_ROTATION);

  // Keep startup deterministic: one white full refresh after init.
  display.clearBuffer();
  display.fillScreen(EPD_WHITE);
  display.display(true);
  delay(180);

#ifdef EINK_FORCE_BOOT_PATTERN
  // Hardware bring-up check: force a visible black->white refresh sequence.
  display.clearBuffer();
  display.fillScreen(EPD_BLACK);
  display.display();
  delay(900);

  display.clearBuffer();
  display.fillScreen(EPD_WHITE);
  display.display();
  delay(900);
#endif

#ifdef EINK_WHITE_SCREEN_TEST
  if (PIN_DISPLAY_BUSY >= 0) {
    Serial.print("  BUSY after init=");
    Serial.println(digitalRead(PIN_DISPLAY_BUSY));
  }
#endif

  display.clearBuffer();
  display.fillScreen(EINK_WHITE);
  display.display(true);
#else
#if defined(NRF52_PLATFORM)
  // Use the dedicated DISPLAY_SPI instance (SPIM2) — keeps display and LoRa
  // radio (SPIM3 / SPI) on separate peripherals, so radio_init() cannot
  // interfere with the display SPI no matter when it is called.
  DISPLAY_SPI.begin();
  display.epd2.selectSPI(DISPLAY_SPI, SPISettings(1000000, MSBFIRST, SPI_MODE0));
#elif defined(ESP32)
  display.epd2.selectSPI(SPI1, SPISettings(4000000, MSBFIRST, SPI_MODE0));
  SPI1.begin(PIN_DISPLAY_SCLK, PIN_DISPLAY_MISO, PIN_DISPLAY_MOSI, PIN_DISPLAY_CS);
#else
  display.epd2.selectSPI(SPI, SPISettings(4000000, MSBFIRST, SPI_MODE0));
  SPI.begin();
#endif

  // Minimal init matching standard GxEPD2 examples.
  display.init(115200, true, 20, false);
  display.setRotation(DISPLAY_ROTATION);

#ifdef EINK_WHITE_SCREEN_TEST
  if (PIN_DISPLAY_BUSY >= 0) {
    Serial.print("  BUSY after init=");
    Serial.println(digitalRead(PIN_DISPLAY_BUSY));
  }
#endif

  // Use firstPage/nextPage to correctly initialize BOTH image buffers (new + old).
  // Calling display(false) only once leaves the old-image buffer uninitialized,
  // which causes the panel to flash the old garbage after the refresh cycle ends.
  display.setFullWindow();
#ifdef EINK_FORCE_BOOT_PATTERN
  display.firstPage();
  do {
    display.fillScreen(GxEPD_BLACK);
  } while (display.nextPage());
  delay(900);
#endif
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
  } while (display.nextPage());

#ifdef EINK_FORCE_BOOT_PATTERN
  delay(900);
#endif
#endif

#ifdef EINK_WHITE_SCREEN_TEST
  if (PIN_DISPLAY_BUSY >= 0) {
    Serial.print("  BUSY after full refresh=");
    Serial.println(digitalRead(PIN_DISPLAY_BUSY));
  }
  Serial.println("GxEPDDisplay::begin: EINK_WHITE_SCREEN_TEST - white screen drawn");
#endif

  _init = true;
  return true;
}

void GxEPDDisplay::turnOn() {
  if (!_init) begin();
#if defined(DISP_BACKLIGHT) && !defined(BACKLIGHT_BTN)
  digitalWrite(DISP_BACKLIGHT, HIGH);
#elif defined(EXP_PIN_BACKLIGHT) && !defined(BACKLIGHT_BTN)
  expander.digitalWrite(EXP_PIN_BACKLIGHT, HIGH);
#endif
  _isOn = true;
}

void GxEPDDisplay::turnOff() {
#if defined(DISP_BACKLIGHT) && !defined(BACKLIGHT_BTN)
  digitalWrite(DISP_BACKLIGHT, LOW);
#elif defined(EXP_PIN_BACKLIGHT) && !defined(BACKLIGHT_BTN)
  expander.digitalWrite(EXP_PIN_BACKLIGHT, LOW);
#endif
  _isOn = false;
}

void GxEPDDisplay::clear() {
  display.fillScreen(EINK_WHITE);
  display.setTextColor(EINK_BLACK);
  display_crc.reset();
}

void GxEPDDisplay::startFrame(Color bkg) {
  if (PIN_DISPLAY_POWER >= 0) {
    pinMode(PIN_DISPLAY_POWER, INPUT_PULLUP);
    digitalWrite(PIN_DISPLAY_POWER, HIGH);
  }

  if (bkg == DARK) {
    display.fillScreen(EINK_BLACK);
    display.setTextColor(_curr_color = EINK_WHITE);
  } else {
    display.fillScreen(EINK_WHITE);
    display.setTextColor(_curr_color = EINK_BLACK);
  }
  display.setTextWrap(false);  // never wrap on e-ink — clipping is handled by drawTextEllipsized
  display_crc.reset();
}

void GxEPDDisplay::setTextSize(int sz) {
  display_crc.update<int>(sz);
  switch(sz) {
    case 1:  // Small
      display.setFont(&FreeSans9pt7b);
      break;
    case 2:  // Medium Bold
      display.setFont(&FreeSansBold12pt7b);
      break;
    case 3:  // Large
      display.setFont(&FreeSans18pt7b);
      break;
    default:
      display.setFont(&FreeSans9pt7b);
      break;
  }
}

void GxEPDDisplay::setColor(Color c) {
  display_crc.update<Color> (c);
  _curr_color = (c != DARK) ? EINK_WHITE : EINK_BLACK;
  display.setTextColor(_curr_color);
}

void GxEPDDisplay::setCursor(int x, int y) {
  display_crc.update<int>(x);
  display_crc.update<int>(y);
  display.setCursor((x+offset_x)*scale_x, (y+offset_y)*scale_y);
}

void GxEPDDisplay::print(const char* str) {
  display_crc.update<char>(str, strlen(str));
  display.print(str);
}

void GxEPDDisplay::fillRect(int x, int y, int w, int h) {
  display_crc.update<int>(x);
  display_crc.update<int>(y);
  display_crc.update<int>(w);
  display_crc.update<int>(h);
  display.fillRect(x*scale_x, y*scale_y, w*scale_x, h*scale_y, _curr_color);
}

void GxEPDDisplay::drawRect(int x, int y, int w, int h) {
  display_crc.update<int>(x);
  display_crc.update<int>(y);
  display_crc.update<int>(w);
  display_crc.update<int>(h);
  display.drawRect(x*scale_x, y*scale_y, w*scale_x, h*scale_y, _curr_color);
}

void GxEPDDisplay::drawXbm(int x, int y, const uint8_t* bits, int w, int h) {
  display_crc.update<int>(x);
  display_crc.update<int>(y);
  display_crc.update<int>(w);
  display_crc.update<int>(h);
  display_crc.update<uint8_t>(bits, w * h / 8);
  // Calculate the base position in display coordinates
  uint16_t startX = x * scale_x;
  uint16_t startY = y * scale_y;
  
  // Width in bytes for bitmap processing
  uint16_t widthInBytes = (w + 7) / 8;
  
  // Process the bitmap row by row
  for (uint16_t by = 0; by < h; by++) {
    // Calculate the target y-coordinates for this logical row
    int y1 = startY + (int)(by * scale_y);
    int y2 = startY + (int)((by + 1) * scale_y);
    int block_h = y2 - y1;
    
    // Scan across the row bit by bit
    for (uint16_t bx = 0; bx < w; bx++) {
      // Calculate the target x-coordinates for this logical column
      int x1 = startX + (int)(bx * scale_x);
      int x2 = startX + (int)((bx + 1) * scale_x);
      int block_w = x2 - x1;
      
      // Get the current bit
      uint16_t byteOffset = (by * widthInBytes) + (bx / 8);
      uint8_t bitMask = 0x80 >> (bx & 7);
      bool bitSet = pgm_read_byte(bits + byteOffset) & bitMask;
      
      // If the bit is set, draw a block of pixels
      if (bitSet) {
        // Draw the block as a filled rectangle
        display.fillRect(x1, y1, block_w, block_h, _curr_color);
      }
    }
  }
}

uint16_t GxEPDDisplay::getTextWidth(const char* str) {
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(str, 0, 0, &x1, &y1, &w, &h);
  return ceil((w + 1) / scale_x);
}

void GxEPDDisplay::endFrame() {
  uint32_t crc = display_crc.finalize();
#ifdef USE_ADAFRUIT_SSD1680
  if (crc != last_display_crc_value) {
    display.display(true);
    last_display_crc_value = crc;
  }
#else
  if (crc != last_display_crc_value) {
    display.display(true);
    last_display_crc_value = crc;
  }
#endif
}
