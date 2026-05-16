#pragma once

#define RADIOLIB_STATIC_ONLY 1
#include <RadioLib.h>
#include <helpers/radiolib/RadioLibWrappers.h>
#include <RAK4631Board.h>
#include <helpers/radiolib/CustomSX1262Wrapper.h>
#include <helpers/AutoDiscoverRTCClock.h>
#include <helpers/sensors/EnvironmentSensorManager.h>

#ifdef DISPLAY_CLASS
  #if defined(EINK_DISPLAY_MODEL)
    #include <helpers/ui/GxEPDDisplay.h>
  #else
    #include <helpers/ui/SSD1306Display.h>
  #endif
  extern DISPLAY_CLASS display;
  #include <helpers/ui/MomentaryButton.h>
  extern MomentaryButton user_btn;
  #if defined(PIN_USER_BTN_ANA) || defined(PIN_BTN_LEFT)
  extern MomentaryButton analog_btn;
  #endif
  #if defined(PIN_BTN_RIGHT)
  extern MomentaryButton right_btn;
  #endif
#endif

extern RAK4631Board board;
extern WRAPPER_CLASS radio_driver;
extern AutoDiscoverRTCClock rtc_clock;
extern EnvironmentSensorManager sensors;

bool radio_init();
uint32_t radio_get_rng_seed();
void radio_set_params(float freq, float bw, uint8_t sf, uint8_t cr);
void radio_set_tx_power(int8_t dbm);
mesh::LocalIdentity radio_new_identity();

