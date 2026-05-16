
#include "GxEPDDisplay.h"

#ifdef EXP_PIN_BACKLIGHT
  #include <PCA9557.h>
  extern PCA9557 expander;
#endif

#ifndef DISPLAY_ROTATION
  #define DISPLAY_ROTATION 3
#endif

#ifdef ESP32
  SPIClass SPI1 = SPIClass(FSPI);
#elif defined(PIN_DISPLAY_SCLK)
  // nRF52 with display on IO-slot SPI pins: use SPIM2 so SPIM3 stays free for LoRa radio
  static SPIClass displaySPI(NRF_SPIM2, PIN_DISPLAY_MISO, PIN_DISPLAY_SCLK, PIN_DISPLAY_MOSI);
#endif

bool GxEPDDisplay::begin() {
  // Only register the SPI bus — do NOT call SPI.begin() or display.init() here.
  // Hardware init is deferred to _doHWInit(), called lazily from startFrame().
  // This avoids initializing SPIM3 before radio_init(), which would cause the
  // radio to hang (RadioLib needs to own the first SPI.begin() call sequence).
#ifdef ESP32
  display.epd2.selectSPI(SPI1, SPISettings(4000000, MSBFIRST, SPI_MODE0));
#elif defined(PIN_DISPLAY_SCLK)
  display.epd2.selectSPI(displaySPI, SPISettings(4000000, MSBFIRST, SPI_MODE0));
#else
  display.epd2.selectSPI(SPI, SPISettings(4000000, MSBFIRST, SPI_MODE0));
#endif
  _init = true;
  return true;
}

void GxEPDDisplay::_doHWInit() {
  if (_hw_init_done) return;
#ifdef PIN_DISPLAY_EN
  pinMode(PIN_DISPLAY_EN, OUTPUT);
  digitalWrite(PIN_DISPLAY_EN, HIGH);
  delay(200);
#endif
#ifdef ESP32
  SPI1.begin(PIN_DISPLAY_SCLK, PIN_DISPLAY_MISO, PIN_DISPLAY_MOSI, PIN_DISPLAY_CS);
#elif defined(PIN_DISPLAY_SCLK)
  displaySPI.begin();
#else
  SPI.begin();
#endif

#if defined(PIN_DISPLAY_BUSY) && defined(PIN_DISPLAY_CS) && defined(PIN_DISPLAY_DC)
  // Recovery: if the SSD1680 controller is stuck with BUSY HIGH (e.g. power
  // was cut during a refresh, or a previous GPIO conflict left it confused),
  // send SWRESET (0x12) before init. SSD1680 accepts SWRESET while BUSY is
  // HIGH; after reset BUSY goes LOW within ~200ms.
  if (PIN_DISPLAY_BUSY >= 0 && digitalRead(PIN_DISPLAY_BUSY) == HIGH) {
    auto& _spi =
      #ifdef ESP32
        SPI1
      #elif defined(PIN_DISPLAY_SCLK)
        displaySPI
      #else
        SPI
      #endif
      ;
    pinMode(PIN_DISPLAY_CS, OUTPUT);  digitalWrite(PIN_DISPLAY_CS, HIGH);
    pinMode(PIN_DISPLAY_DC, OUTPUT);  digitalWrite(PIN_DISPLAY_DC, LOW);
    _spi.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
    digitalWrite(PIN_DISPLAY_CS, LOW);
    _spi.transfer(0x12);  // SWRESET
    digitalWrite(PIN_DISPLAY_CS, HIGH);
    _spi.endTransaction();
    unsigned long t = millis();
    while (digitalRead(PIN_DISPLAY_BUSY) == HIGH && millis() - t < 5000) delay(10);
  }
#endif

  display.init(115200, true, 2, false);
  display.setRotation(DISPLAY_ROTATION);
  setTextSize(1);
  display.setPartialWindow(0, 0, display.width(), display.height());
  display.fillScreen(GxEPD_WHITE);
  display.display(true);
  #if DISP_BACKLIGHT
  digitalWrite(DISP_BACKLIGHT, LOW);
  pinMode(DISP_BACKLIGHT, OUTPUT);
  #endif
  _hw_init_done = true;
}

void GxEPDDisplay::turnOn() {
  if (!_init) begin();
  if (!_hw_init_done) _doHWInit();
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
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);
  display_crc.reset();
}

void GxEPDDisplay::startFrame(Color bkg) {
  if (!_hw_init_done) _doHWInit();
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(_curr_color = GxEPD_BLACK);
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
  // colours need to be inverted for epaper displays
  if (c == DARK) {
    display.setTextColor(_curr_color = GxEPD_WHITE);
  } else {
    display.setTextColor(_curr_color = GxEPD_BLACK);
  }
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
  if (!_hw_init_done) return;
  uint32_t crc = display_crc.finalize();
  if (crc != last_display_crc_value) {
    display.display(true);
    last_display_crc_value = crc;
  }
}
