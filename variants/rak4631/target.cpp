#include <Arduino.h>
#include "target.h"
#include <helpers/ArduinoHelpers.h>

RAK4631Board board;

#ifndef PIN_USER_BTN
  #define PIN_USER_BTN (-1)
#endif

#ifdef DISPLAY_CLASS
  DISPLAY_CLASS display;
  MomentaryButton user_btn(PIN_USER_BTN, 1000, true, true);

  #if defined(PIN_USER_BTN_ANA)
  MomentaryButton analog_btn(PIN_USER_BTN_ANA, 1000, 20);
  #elif defined(PIN_BTN_LEFT)
  MomentaryButton analog_btn(PIN_BTN_LEFT, 1000, true, true, false);
  #endif
  #if defined(PIN_BTN_RIGHT)
  MomentaryButton right_btn(PIN_BTN_RIGHT, 1000, true, true, false);
  #endif
#endif

RADIO_CLASS radio = new Module(P_LORA_NSS, P_LORA_DIO_1, P_LORA_RESET, P_LORA_BUSY, SPI);

WRAPPER_CLASS radio_driver(radio, board);

VolatileRTCClock fallback_clock;
AutoDiscoverRTCClock rtc_clock(fallback_clock);

#if ENV_INCLUDE_GPS
  #include <helpers/sensors/MicroNMEALocationProvider.h>
  MicroNMEALocationProvider nmea = MicroNMEALocationProvider(Serial1, &rtc_clock);
  EnvironmentSensorManager sensors = EnvironmentSensorManager(nmea);
#else
  EnvironmentSensorManager sensors;
#endif

bool radio_init() {
  rtc_clock.begin(Wire);

  // The display SPI corrupts SX1262 state, so we power-cycle it here.
  // With PIN_DISPLAY_SCLK defined, the display uses displaySPI (SPIM2) not SPI (SPIM3).
  // SPI.end() only uninitialises SPIM3 — SPIM2/display is completely unaffected.
  SPI.end();                          // release MOSI/SCK from SPIM3 (prevents ESD back-power)
  digitalWrite(P_LORA_NSS, LOW);     // NSS LOW — removes last ESD back-power path

  digitalWrite(SX126X_POWER_EN, LOW);
  delay(200);
  digitalWrite(SX126X_POWER_EN, HIGH);
  digitalWrite(P_LORA_NSS, HIGH);    // NSS HIGH as SX1262 boots

  // Wait up to 2 s for BUSY LOW (SX1262 boot + calibration done).
  unsigned long t = millis();
  while (digitalRead(P_LORA_BUSY) && millis() - t < 2000) {}

  // SPI.begin() called by RadioLib Module::init() (initialized=false after SPI.end()).
  // This initialises SPIM3 for radio; SPIM2 (display) remains intact.
  return radio.std_init(&SPI);
}

uint32_t radio_get_rng_seed() {
  return radio.random(0x7FFFFFFF);
}

void radio_set_params(float freq, float bw, uint8_t sf, uint8_t cr) {
  radio.setFrequency(freq);
  radio.setSpreadingFactor(sf);
  radio.setBandwidth(bw);
  radio.setCodingRate(cr);
}

void radio_set_tx_power(int8_t dbm) {
  radio.setOutputPower(dbm);
}

mesh::LocalIdentity radio_new_identity() {
  RadioNoiseListener rng(radio);
  return mesh::LocalIdentity(&rng);  // create new random identity
}

