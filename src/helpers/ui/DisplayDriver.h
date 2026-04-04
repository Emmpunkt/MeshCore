#pragma once

#include <stdint.h>
#include <string.h>

class DisplayDriver {
  int _w, _h;
protected:
  DisplayDriver(int w, int h) { _w = w; _h = h; }
public:
  enum Color { DARK=0, LIGHT, RED, GREEN, BLUE, YELLOW, ORANGE }; // on b/w screen, colors will be !=0 synonym of light

  int width() const { return _w; }
  int height() const { return _h; }

  virtual bool isOn() = 0;
  virtual void turnOn() = 0;
  virtual void turnOff() = 0;
  virtual void clear() = 0;
  virtual void startFrame(Color bkg = DARK) = 0;
  virtual void setTextSize(int sz) = 0;
  virtual void setColor(Color c) = 0;
  virtual void setCursor(int x, int y) = 0;
  virtual void print(const char* str) = 0;
  virtual void printWordWrap(const char* str, int max_width) { print(str); }   // fallback to basic print() if no override
  virtual void fillRect(int x, int y, int w, int h) = 0;
  virtual void drawRect(int x, int y, int w, int h) = 0;
  virtual void drawXbm(int x, int y, const uint8_t* bits, int w, int h) = 0;
  virtual uint16_t getTextWidth(const char* str) = 0;
  virtual void drawTextCentered(int mid_x, int y, const char* str) {   // helper method (override to optimise)
    int w = getTextWidth(str);
    setCursor(mid_x - w/2, y);
    print(str);
  }
  virtual void drawTextRightAlign(int x_anch, int y, const char* str) {
    int w = getTextWidth(str);
    setCursor(x_anch - w, y);
    print(str);
  }
  virtual void drawTextLeftAlign(int x_anch, int y, const char* str) {
    setCursor(x_anch, y);
    print(str);
  }
  
  // convert UTF-8 characters to displayable block characters for compatibility
  virtual void translateUTF8ToBlocks(char* dest, const char* src, size_t dest_size) {
    size_t j = 0;
    for (size_t i = 0; src[i] != 0 && j < dest_size - 1; i++) {
      unsigned char c = (unsigned char)src[i];
      if (c >= 32 && c <= 126) {
        dest[j++] = c;  // ASCII printable
      } else if (j + 2 < dest_size && (c == 0xE4 || c == 0x84)) {
        dest[j++] = 'a'; dest[j++] = 'e'; // ae (a-umlaut)
      } else if (j + 2 < dest_size && (c == 0xC4 || c == 0x8E)) {
        dest[j++] = 'A'; dest[j++] = 'e'; // Ae (A-umlaut)
      } else if (j + 2 < dest_size && (c == 0xF6 || c == 0x94)) {
        dest[j++] = 'o'; dest[j++] = 'e'; // oe (o-umlaut)
      } else if (j + 2 < dest_size && (c == 0xD6 || c == 0x99)) {
        dest[j++] = 'O'; dest[j++] = 'e'; // Oe (O-umlaut)
      } else if (j + 2 < dest_size && (c == 0xFC || c == 0x81)) {
        dest[j++] = 'u'; dest[j++] = 'e'; // ue (u-umlaut)
      } else if (j + 2 < dest_size && (c == 0xDC || c == 0x9A)) {
        dest[j++] = 'U'; dest[j++] = 'e'; // Ue (U-umlaut)
      } else if (j + 2 < dest_size && (c == 0xDF || c == 0xE1)) {
        dest[j++] = 's'; dest[j++] = 's'; // ss (sharp s)
      } else if (c == 0xC3 && src[i + 1] != 0) {
        // Common German umlauts/transforms for displays without UTF-8 glyphs.
        unsigned char n = (unsigned char)src[i + 1];
        if (n == 0xA4 && j + 2 < dest_size) { dest[j++] = 'a'; dest[j++] = 'e'; i++; continue; } // ä
        if (n == 0x84 && j + 2 < dest_size) { dest[j++] = 'A'; dest[j++] = 'e'; i++; continue; } // Ä
        if (n == 0xB6 && j + 2 < dest_size) { dest[j++] = 'o'; dest[j++] = 'e'; i++; continue; } // ö
        if (n == 0x96 && j + 2 < dest_size) { dest[j++] = 'O'; dest[j++] = 'e'; i++; continue; } // Ö
        if (n == 0xBC && j + 2 < dest_size) { dest[j++] = 'u'; dest[j++] = 'e'; i++; continue; } // ü
        if (n == 0x9C && j + 2 < dest_size) { dest[j++] = 'U'; dest[j++] = 'e'; i++; continue; } // Ü
        if (n == 0x9F && j + 2 < dest_size) { dest[j++] = 's'; dest[j++] = 's'; i++; continue; } // ß

        dest[j++] = '?';
        i++; // consumed 2-byte sequence
      } else if (c >= 0x80) {
        dest[j++] = '?';
        while (src[i + 1] && ((unsigned char)src[i + 1] & 0xC0) == 0x80)
          i++;  // skip UTF-8 continuation bytes
      }
    }
    dest[j] = 0;
  }
  
  // draw text with ellipsis if it exceeds max_width
  virtual void drawTextEllipsized(int x, int y, int max_width, const char* str) {
    char temp_str[256];  // reasonable buffer size
    size_t len = strlen(str);
    if (len >= sizeof(temp_str)) len = sizeof(temp_str) - 1;
    memcpy(temp_str, str, len);
    temp_str[len] = 0;
    
    if (getTextWidth(temp_str) <= max_width) {
      setCursor(x, y);
      print(temp_str);
      return;
    }
    
    // for variable-width fonts (GxEPD), add space after ellipsis
    // for fixed-width fonts (OLED), keep tight spacing to save precious characters
    const char* ellipsis;
    // use a simple heuristic: if 'i' and 'l' have different widths, it's variable-width
    int i_width = getTextWidth("i");
    int l_width = getTextWidth("l");
    if (i_width != l_width) {
      ellipsis = "... ";  // variable-width fonts: add space
    } else {
      ellipsis = "...";   // fixed-width fonts: no space
    }
    
    int ellipsis_width = getTextWidth(ellipsis);
    int str_len = strlen(temp_str);
    
    while (str_len > 0 && getTextWidth(temp_str) > max_width - ellipsis_width) {
      temp_str[--str_len] = 0;
    }
    strcat(temp_str, ellipsis);
    
    setCursor(x, y);
    print(temp_str);
  }

  virtual void endFrame() = 0;
};
