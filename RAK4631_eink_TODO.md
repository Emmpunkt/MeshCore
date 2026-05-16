# RAK4631 Companion Radio BLE E-Ink — Änderungen & TODO

## Kontext für neue Claude-Sessions

Dieses Dokument beschreibt alle Änderungen gegenüber dem Original-MeshCore-Repository
(https://github.com/meshcore-dev/MeshCore) für den Build `RAK_4631_companion_radio_ble_eink`.

**Hardware:** RAK4631 WisBlock (nRF52840) + GxEPD2_213_BN E-Ink Display (2.13" 250×122px)
+ RAK14000 (3 Folientasten) + BLE Companion Radio + RAK12500 GPS (Slot A, optional).

**Wichtig:** `variants/xiao_s3_wio/*` sind eigene Änderungen des Users — niemals anfassen!

---

## Alle Änderungen gegenüber Original-MeshCore

### `src/helpers/ui/GxEPDDisplay.h`
**Was:** `bool _hw_init_done = false;` und `void _doHWInit();` hinzugefügt.

### `src/helpers/ui/GxEPDDisplay.cpp`
**Was:**
- `static SPIClass displaySPI(NRF_SPIM2, PIN_DISPLAY_MISO, PIN_DISPLAY_SCLK, PIN_DISPLAY_MOSI)`
  für nRF52 (SPIM2 = getrennt von Radio-SPI SPIM3)
- `begin()` ist lazy: registriert nur SPI-Bus, kein Hardware-Init
- `_doHWInit()` neu: erledigt `displaySPI.begin()` + `display.init()` beim ersten Aufruf
- `turnOn()` und `startFrame()` rufen `_doHWInit()` bei Bedarf auf
- `_doHWInit()`: **SWRESET-Recovery** — falls BUSY beim Start HIGH ist (Controller
  eingefroren nach unterbrochener Aktualisierung oder GPIO-Konflikt), wird `0x12`
  (SWRESET) direkt per SPI gesendet und bis zu 5s auf BUSY LOW gewartet.

**Warum lazy init:** Bei nRF52840 teilen SPIM2 (Display, GPIO 3/29/30) und SPIM3
(Radio) die gleichen GPIO-Pins. Lazy-Init verhindert dass RadioLib's SPI-Init durch
`display.begin()` überschrieben wird.

**Warum SWRESET-Recovery:** Nach MCU-Reset bleibt der Display-Controller powered
(kein Hardware-Reset ohne RST-Pin). Zustand von vorheriger Session bleibt erhalten →
BUSY stuck HIGH. Der SSD1680 akzeptiert SWRESET auch während BUSY HIGH; danach geht
BUSY innerhalb ~200ms auf LOW.

**ACHTUNG:** `fillRect/drawRect/drawXbm` verwenden KEIN y-Offset (absichtlich!).
`setCursor` NUTZT Offset. Diese Asymmetrie ist korrekt: Rects starten bei y=0,
Text-Cursor braucht Offset für Buchstaben-Ascender. Offset auf Rects = BROKEN!

### `variants/rak4631/target.cpp`
**Was:** SX1262 Power-Cycle in `radio_init()`:
`SPI.end()` → NSS LOW → Power OFF 200ms → Power ON → warte auf BUSY LOW.

**Warum:** Display-SPI korrumpiert beim Init den SX1262-Zustand. Power-Cycle stellt
den Radio danach wieder her.

### `variants/rak4631/RAK4631Board.cpp`
**Was:** `begin()` initialisiert `PIN_BTN_LEFT` und `PIN_BTN_RIGHT` als
`INPUT_PULLUP`, NSS-Pin auf OUTPUT+HIGH gesetzt bevor SX1262 eingeschaltet wird.
**Warum:** 3-Tasten-Layout (links/mitte/rechts). Floating-NSS beim SX1262-Boot verhindern.

### `variants/rak4631/target.h`
**Was:**
- `#include <helpers/ui/GxEPDDisplay.h>` wenn `EINK_DISPLAY_MODEL` definiert.
- `extern MomentaryButton analog_btn;` Guard auf `PIN_USER_BTN_ANA || PIN_BTN_LEFT`
- `extern MomentaryButton right_btn;` unter `PIN_BTN_RIGHT`

### `variants/rak4631/platformio.ini`
**Was:** Neues Environment `[env:RAK_4631_companion_radio_ble_eink]` mit:

| Flag | Wert | Warum |
|------|------|-------|
| `DISPLAY_CLASS` | `GxEPDDisplay` | E-Ink Display-Treiber |
| `EINK_DISPLAY_MODEL` | `GxEPD2_213_BN` | 2.13" Display-Modell |
| `EINK_SCALE_X` | `1.953125f` | 128→250px horizontal |
| `EINK_SCALE_Y` | `1.8f` | 64→122px vertikal |
| `EINK_X_OFFSET` | `0` | kein horizontaler Offset |
| `EINK_Y_OFFSET` | `10` | vertikaler Offset für Text-Ascender |
| `DISPLAY_ROTATION` | `3` | Landscape korrekte Ausrichtung (270° CW) |
| `PIN_DISPLAY_CS` | `26` | SPI Chip-Select |
| `PIN_DISPLAY_DC` | `17` | Data/Command (= WB_IO1) |
| `PIN_DISPLAY_RST` | `-1` | kein Reset-Pin |
| `PIN_DISPLAY_BUSY` | `4` | BUSY-Signal (= WB_IO4) |
| `PIN_DISPLAY_SCLK` | `3` | SPI Clock (SPIM2) |
| `PIN_DISPLAY_MOSI` | `30` | SPI MOSI (SPIM2) |
| `PIN_DISPLAY_MISO` | `29` | SPI MISO (SPIM2) |
| `PIN_USER_BTN` | `9` | Mittlerer Button (WB_IO5 = NFC1-Pin) |
| `PIN_BTN_LEFT` | `10` | Linker Button (WB_IO6 = NFC2-Pin) |
| `PIN_BTN_RIGHT` | `21` | Rechter Button (WB_IO3) |
| `BLE_PIN_CODE` | `111111` | BLE Pairing-PIN |
| `FRAME_QUEUE_SIZE` | `64` | Große Queue wg. E-Ink blockiert Thread |
| `AUTO_OFF_MILLIS` | `0` | Display bleibt immer an |
| `CLI_RESCUE_WINDOW_MS` | `0` | CLI-Rescue deaktiviert (GPIO 9 = NFC-Pin) |
| `OFFLINE_QUEUE_SIZE` | `256` | Offline-Nachrichten-Puffer |
| `-UENV_INCLUDE_RAK12035` | — | Kein RAK12035 Bodensensor in diesem Build |

### `src/helpers/sensors/EnvironmentSensorManager.h`
**Was:** `uint32_t _gps_boot_deadline = 0;` — Timer für non-blocking GPS-Boot-Wartezeit.

### `src/helpers/sensors/EnvironmentSensorManager.cpp`
**Was:**
- `rakGPSInit()`: `#if`-Guards für WB_IO4 und WB_IO5 — diese Pins werden niemals als
  GPS-Power-Probe verwendet, da sie mit DISPLAY_BUSY (pin4) bzw. PIN_USER_BTN (pin9)
  kollidieren würden.

  ```cpp
  #if !defined(PIN_DISPLAY_BUSY) || (PIN_DISPLAY_BUSY != 4)
    else if(gpsIsAwake(WB_IO4)){ ... }
  #endif
  #if (!defined(PIN_USER_BTN) || (PIN_USER_BTN != 9)) && ...
    else if(gpsIsAwake(WB_IO5)){ ... }
  #endif
  ```

- `gpsIsAwake()`: Deaktiviert nach erfolgreichem I2C-Init den TIMEPULSE-Ausgang:
  ```cpp
  UBX_CFG_TP5_data_t tpCfg;
  ublox_GNSS.getTimePulseParameters(&tpCfg);
  tpCfg.flags.bits.active = 0;  // TP-Pin = Hi-Z
  ublox_GNSS.setTimePulseParameters(&tpCfg);
  ublox_GNSS.saveConfiguration();  // persistent im GPS-Flash
  ```
  Außerdem: nach Slot-Suche `pinMode(ioPin, INPUT_PULLUP)` statt OUTPUT lassen.

- `start_gps()`: Non-blocking Boot-Delay.
  ```cpp
  pinMode(gpsResetPin, OUTPUT);
  digitalWrite(gpsResetPin, HIGH);
  pinMode(gpsResetPin, INPUT_PULLUP);
  gps_active = false;
  _gps_boot_deadline = millis() + 1500;
  ```
  `Wire_nRF52.cpp` hat keine I2C-Timeouts — GPS braucht ~300–1000ms bis I2C
  stabil ist. Zu früher Zugriff → CPU hängt in `endTransmission()` für immer.

- `loop()`: GPS-Polling nur 1× pro Sekunde (innerhalb `next_gps_update`-Timer).
  Außerdem: `_gps_boot_deadline`-Check aktiviert `gps_active` nach 1,5s.

**Warum GPS TIMEPULSE deaktivieren:** WisBlock Slot A: WB_IO1 = pin17 = PIN_DISPLAY_DC.
Der RAK12500 verbindet seinen TIMEPULSE-Ausgang mit WB_IO1. Solange GPS aktiv ist,
fährt der TP-Pin pin17 HIGH → SPI-Commands an das Display werden als Daten
interpretiert → Display-Controller blockiert mit BUSY stuck HIGH (10s Timeout).
Gleiches Prinzip galt in Slot D: WB_IO1 = pin9 = PIN_USER_BTN → TP brach Buttons.

### `src/helpers/nrf52/SerialBLEInterface.h`
**Was:** Default `FRAME_QUEUE_SIZE` von 12 auf 32 erhöht.
**Warum:** E-Ink `display.display(true)` blockiert den Thread 300ms–1,5s.
Größere Queue puffert BLE-Daten währenddessen (zusätzlich überschrieben durch FRAME_QUEUE_SIZE=64
in platformio.ini).

### `examples/companion_radio/ui-new/UITask.cpp`
**Was:**
- Page-Indicator-Dots vergrößert: inaktiv 1×1 → 3×2, aktiv 3×3 → 5×3
- `begin()`: `cancelClick()` für alle 3 Buttons (kein `begin()`, kein `check()`)
- `handleLongPress()`: via `CLI_RESCUE_WINDOW_MS` Makro konfigurierbar
- `loop()`: **Long-Press für `user_btn` entfernt** — Folientasten haben schlechten
  Druckpunkt für 1200ms-Hold; mit 3 dedizierten Tasten (prev/enter/next) unnötig
- `loop()`: Rechter Button (right_btn) auf CLICK→KEY_NEXT, linker (analog_btn) auf
  CLICK→KEY_PREV; mittlerer (user_btn) auf CLICK→KEY_ENTER
- `toggleGPS()`: GPS-Aus-Branch: `user_btn.check()` + `cancelClick()` entfernt
  (War Relikt aus GPS-in-Slot-D-Zeit; mit GPS in Slot A, pin34, überflüssig)

**Warum CLI_RESCUE_WINDOW_MS=0:** GPIO 9 (WB_IO5 = NFC1-Pin des nRF52840) ist als
NFC-Antennen-Pin kapazitiv belastet. Während BLE-Aktivität geht der Pin für >1000ms
auf LOW (RF-Kopplung) → wird als Long-Press erkannt → CLI Rescue → blockiert BLE.

---

## Sind die Änderungen ins Original mergebar?

**Ja, alle Änderungen können upstream eingereicht werden.**

| Datei | Mergebar? | Anmerkung |
|-------|-----------|-----------|
| GxEPDDisplay.h/cpp | ✅ | Echter Bug-Fix für nRF52+E-Ink+Radio SPI-Konflikt + SWRESET-Recovery |
| target.cpp | ✅ | Nötig für nRF52-Boards mit E-Ink-Display |
| RAK4631Board.cpp | ✅ | Defensiver Fix + 3-Tasten-Support |
| target.h | ✅ | Sauberere Include-Logik |
| platformio.ini (neues env) | ✅ | Neues Board-Target |
| SerialBLEInterface.h | ✅ | Besserer Default-Wert |
| UITask.cpp (Dots) | ✅ | Rückwärtskompatibel, nur cosmetic |
| UITask.cpp (CLI_RESCUE) | ✅ | Nützlich für Boards mit NFC-Pins |
| EnvironmentSensorManager (GPS Guards + TP-Fix) | ✅ | Echter Bug-Fix |

---

## TODO-Liste

### Offen (Feldtest ausstehend)

- [ ] **GPS Feldtest** — GPS einschalten, auf Fix warten, Position prüfen.
  **Hinweis erster Boot:** `gpsIsAwake()` deaktiviert TP und speichert ins GPS-Flash
  → ca. 3–5s längere Boot-Zeit beim allerersten Mal. Folgende Boots normal.

- [ ] **GPS Ein/Ausschalten** — Toggle mehrmals testen. Display und Buttons müssen
  nach jedem Toggle stabil bleiben. Kein Busy Timeout, keine Artefakte.

- [ ] **Display-Stabilität mit GPS** — Insbesondere: "GPS: Enabled"-Alert
  muss korrekt angezeigt werden ohne BUSY-Timeout.

### Erledigt

- [x] Display richtig herum (DISPLAY_ROTATION=3)
- [x] BLE-Verbindung funktioniert
- [x] Display zeigt Inhalte (GxEPD2 + lazy SPI-Init)
- [x] Radio (LoRa SX1262) funktioniert nach Power-Cycle
- [x] CLI-Rescue durch NFC-Pin-Glitch behoben (CLI_RESCUE_WINDOW_MS=0)
- [x] BLE-Queue-Overflow behoben (FRAME_QUEUE_SIZE=64)
- [x] Kontakte wiederherstellbar
- [x] Page-Indicator-Dots sichtbar (vergrößert)
- [x] Alle 3 Buttons funktionieren (links=prev, mitte=enter, rechts=next)
- [x] Long-Press entfernt (Folientasten, 3 dedizierte Tasten brauchen keinen Long-Press)
- [x] Batteriesymbol symmetrisch
- [x] GPS RAK12500 in Slot A (WB_IO2=34)
- [x] GPS TIMEPULSE deaktiviert (WB_IO1=pin17=DC Konflikt behoben)
- [x] Display SWRESET-Recovery (stuck BUSY nach unterbrochener Aktualisierung)
- [x] GPS WB_IO4/WB_IO5 Probe-Guards (#if-Guards verhindern DC/Button-Kollision)
- [x] GPS start_gps() non-blocking (1,5s Boot-Delay, kein Wire-Hang)
- [x] GPS Polling auf 1 Hz rate-limitiert

---

## Bekannte Eigenheiten (kein Handlungsbedarf)

- **GPIO 9 (PIN_USER_BTN) ist der NFC1-Pin** des nRF52840. Er ist kapazitiv belastet
  und reagiert auf BLE-RF. Deshalb: CLI_RESCUE_WINDOW_MS=0.

- **GPIO 10 (PIN_BTN_LEFT) ist der NFC2-Pin** — gleiche kapazitive Belastung.
  `CONFIG_NFCT_PINS_AS_GPIOS` (im Build gesetzt) konvertiert beide NFC-Pins zu
  normalen GPIOs (einmalige UICR-Schreiboperation).

- **WisBlock Slot A: TP/DC-Konflikt** — RAK12500 TIMEPULSE-Ausgang liegt auf
  WB_IO1 = pin17 = PIN_DISPLAY_DC. Per UBX-CFG-TP5 deaktiviert und im GPS-Flash
  gespeichert. Gilt sinngemäß für alle WisBlock-Slots wo WB_IO1 mit einem kritischen
  MCU-Pin kollidiert (Slot D: WB_IO1=pin9=Button war der ursprüngliche GPS-in-Slot-D-Bug).

- **SWRESET-Recovery im Boot** — Falls BUSY beim Start HIGH ist, sendet `_doHWInit()`
  `0x12` (SWRESET) und wartet bis zu 5s. Kein Handlungsbedarf, läuft automatisch.

- **GPS I2C Wire-Timeout** — `Wire_nRF52.cpp` hat KEINE I2C-Timeouts (infinite
  spin-loops in `endTransmission()`). Deshalb 1,5s Boot-Delay vor erstem GPS-I2C-Zugriff
  und 1 Hz Polling statt 100+ Hz. Niemals `ublox_GNSS`-Funktionen direkt nach GPS
  power-on aufrufen!

- **Offset-Asymmetrie in GxEPDDisplay.cpp:** `setCursor` nutzt `(x+offset)*scale`,
  `fillRect/drawRect/drawXbm` nutzen `x*scale` (kein Offset). Das ist ABSICHTLICH korrekt.
  Nicht "vereinheitlichen" — das bricht das Layout!

- **Display blockiert Thread:** `display.display(true)` in `endFrame()` wartet auf E-Ink
  (bis zu 1,5s für partial update auf GxEPD2_213_BN). BLE-Daten werden in dieser Zeit
  in der Frame-Queue gepuffert.
