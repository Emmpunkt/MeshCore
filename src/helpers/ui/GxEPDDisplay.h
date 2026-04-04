#pragma once

#include <SPI.h>
#include <Wire.h>

#ifdef USE_ADAFRUIT_SSD1680
  #include <Adafruit_EPD.h>
#else
#define ENABLE_GxEPD2_GFX 0

#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>
#include <GxEPD2_4C.h>
#include <GxEPD2_7C.h>
#endif

#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSans18pt7b.h>
#include <CRC32.h>

#include "DisplayDriver.h"

#ifndef DISPLAY_ROTATION
  #define DISPLAY_ROTATION 3
#endif

#ifdef USE_ADAFRUIT_SSD1680
  // Adafruit_GFX reports landscape geometry for rotations 0 and 2.
  #if (DISPLAY_ROTATION == 0) || (DISPLAY_ROTATION == 2)
    #define EINK_UI_WIDTH 250
    #define EINK_UI_HEIGHT 122
  #else
    #define EINK_UI_WIDTH 122
    #define EINK_UI_HEIGHT 250
  #endif
#else
  #if (DISPLAY_ROTATION == 1) || (DISPLAY_ROTATION == 3)
    #define EINK_UI_WIDTH 250
    #define EINK_UI_HEIGHT 122
  #else
    #define EINK_UI_WIDTH 122
    #define EINK_UI_HEIGHT 250
  #endif
#endif

class GxEPDDisplay : public DisplayDriver {

#ifdef USE_ADAFRUIT_SSD1680
  Adafruit_SSD1680 display;
  const float scale_x  = EINK_SCALE_X;
  const float scale_y  = EINK_SCALE_Y;
  const float offset_x = EINK_X_OFFSET;
  const float offset_y = EINK_Y_OFFSET;
#elif defined(EINK_DISPLAY_MODEL)
  GxEPD2_BW<EINK_DISPLAY_MODEL, EINK_DISPLAY_MODEL::HEIGHT> display;
  const float scale_x  = EINK_SCALE_X; 
  const float scale_y  = EINK_SCALE_Y;
  const float offset_x = EINK_X_OFFSET;
  const float offset_y = EINK_Y_OFFSET;
#else
  GxEPD2_BW<GxEPD2_150_BN, 200> display;
  const float scale_x  = 1.5625f;
  const float scale_y  = 1.5625f;
  const float offset_x = 0;
  const float offset_y = 10;
#endif
  bool _init = false;
  bool _isOn = false;
  uint16_t _curr_color;
  CRC32 display_crc;
  int last_display_crc_value = 0;

public:
#ifdef USE_ADAFRUIT_SSD1680
  GxEPDDisplay() : DisplayDriver(EINK_UI_WIDTH, EINK_UI_HEIGHT), display(250, 122, PIN_SPI_MOSI, PIN_SPI_SCK, PIN_DISPLAY_DC, PIN_DISPLAY_RST, PIN_DISPLAY_CS, -1, -1, PIN_DISPLAY_BUSY) {}
#elif defined(EINK_DISPLAY_MODEL)
  GxEPDDisplay() : DisplayDriver(EINK_UI_WIDTH, EINK_UI_HEIGHT), display(EINK_DISPLAY_MODEL(PIN_DISPLAY_CS, PIN_DISPLAY_DC, PIN_DISPLAY_RST, PIN_DISPLAY_BUSY)) {}
#else
  GxEPDDisplay() : DisplayDriver(128, 128), display(GxEPD2_150_BN(DISP_CS, DISP_DC, DISP_RST, DISP_BUSY)) {}
#endif

  bool begin();

  bool isOn() override {return _isOn;};
  void turnOn() override;
  void turnOff() override;
  void clear() override;
  void startFrame(Color bkg = DARK) override;
  void setTextSize(int sz) override;
  void setColor(Color c) override;
  void setCursor(int x, int y) override;
  void print(const char* str) override;
  void fillRect(int x, int y, int w, int h) override;
  void drawRect(int x, int y, int w, int h) override;
  void drawXbm(int x, int y, const uint8_t* bits, int w, int h) override;
  uint16_t getTextWidth(const char* str) override;
  void endFrame() override;
};
