/*
 * ESP32-S3 USB MIDI → WLED Controller
 * =====================================
 * Enumerates as "WLED MIDI 1" on the MPC One.
 * Plug the ESP32's OTG/USB port into the MPC One's USB-A host port.
 *
 * MIDI Channel 1 Note On → effect on Segment 0, random effect on Segment 1
 * MIDI Channel 2 Note On → palette on Segment 0, complementary palette on Segment 1
 * MIDI Clock (0xF8)      → beat-locked effect reset on both segments (throttled)
 * MIDI Start/Stop        → enables/disables beat sync
 *
 * Beat sync uses HTTP POST (same as note commands) but is throttled to a
 * minimum interval so it cannot flood the WLED instance.
 *
 * ─── Arduino IDE Board Settings ───────────────────────────────────────────
 *   Board:              ESP32S3 Dev Module
 *   USB Mode:           USB-OTG (TinyUSB)     ← critical
 *   USB CDC On Boot:    Disabled              ← critical: pure MIDI, no serial
 *   Upload Mode:        UART0 / Hardware CDC
 *
 * Flash via the UART port. Connect the MPC to the OTG/USB port.
 *
 * ─── Libraries (Library Manager) ──────────────────────────────────────────
 *   ArduinoJson       >= 6.x  (Benoit Blanchon)
 *   Adafruit NeoPixel         (Adafruit)
 */

#include "USB.h"
#include "USBMIDI.h"
#include "nvs_flash.h"

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>

// ─────────────────────────────────────────────
//  USBMIDI device name (shown on MPC One)
// ─────────────────────────────────────────────
USBMIDI MIDIusb("WLED");

// ─────────────────────────────────────────────
//  USER CONFIGURATION
// ─────────────────────────────────────────────
const char* WIFI_SSID     = "The Phantom Eye";
const char* WIFI_PASSWORD = "PhantomEye420";
const char* WLED_IP       = "192.168.8.127";
const int   WLED_PORT     = 80;

// Minimum ms between beat-reset HTTP calls.
// At 120 BPM a beat fires every 500ms — this
// prevents flooding WLED if tempo is very high.
const unsigned long BEAT_THROTTLE_MS = 200;

// ─────────────────────────────────────────────
//  NEOPIXEL — GPIO 38 on ESP32-S3 DevKitC-1
// ─────────────────────────────────────────────
#define NEO_PIN        38
#define NEO_BRIGHTNESS 204  // 80% of 255
Adafruit_NeoPixel led(1, NEO_PIN, NEO_GRB + NEO_KHZ800);

// ─────────────────────────────────────────────
//  FLASH STATE
// ─────────────────────────────────────────────
enum FlashState { FLASH_NONE, FLASH_MIDI_RX, FLASH_CH1_TX, FLASH_CH2_TX };
FlashState    flashState = FLASH_NONE;
unsigned long flashEnd   = 0;
const int     FLASH_MS   = 80;

// ─────────────────────────────────────────────
//  LED HELPERS
// ─────────────────────────────────────────────
void setLED(uint8_t r, uint8_t g, uint8_t b) {
  led.setPixelColor(0, led.Color(r, g, b));
  led.show();
}
void ledOff()           { setLED(0,   0,   0);   }
void ledGreen()         { setLED(0,   180, 0);   }
void ledRed()           { setLED(180, 0,   0);   }
void ledWhite()         { setLED(180, 180, 180); }
void ledCyan()          { setLED(0,   180, 180); }
void ledMagenta()       { setLED(180, 0,   180); }
void ledBlue(uint8_t b) { setLED(0,   0,   b);   }
void ledYellow()        { setLED(180, 140, 0);   }

void triggerFlash(FlashState s) {
  flashState = s;
  flashEnd   = millis() + FLASH_MS;
  switch (s) {
    case FLASH_MIDI_RX: ledWhite();   break;
    case FLASH_CH1_TX:  ledCyan();    break;
    case FLASH_CH2_TX:  ledMagenta(); break;
    default: break;
  }
}

void updateLED() {
  if (flashState != FLASH_NONE && millis() >= flashEnd) {
    flashState = FLASH_NONE;
    if (WiFi.status() == WL_CONNECTED) ledGreen();
    else                               ledRed();
  }
}

// ─────────────────────────────────────────────
//  USB CONNECTION EVENTS
// ─────────────────────────────────────────────
void onUsbEvent(void* arg, esp_event_base_t base, int32_t id, void* data) {
  if (base != ARDUINO_USB_EVENTS) return;
  switch (id) {
    case ARDUINO_USB_STARTED_EVENT:
      ledYellow();
      delay(300);
      if (WiFi.status() == WL_CONNECTED) ledGreen();
      else                               ledRed();
      break;
    case ARDUINO_USB_STOPPED_EVENT: break;
    case ARDUINO_USB_SUSPEND_EVENT: break;
    case ARDUINO_USB_RESUME_EVENT:  break;
    default: break;
  }
}

// ─────────────────────────────────────────────
//  WLED LIMITS
// ─────────────────────────────────────────────
const int MAX_EFFECT_ID  = 118;
const int MAX_PALETTE_ID = 55;

const uint8_t COMPLEMENTARY_PALETTE[] = {
  //  0   1   2   3   4   5   6   7   8   9
      6,  7,  8,  9, 10, 11,  0,  1,  2,  3,
  // 10  11  12  13  14  15  16  17  18  19
     16, 17, 18, 19, 20, 10, 11, 12, 13, 14,
  // 20  21  22  23  24  25  26  27  28  29
     26, 27, 28, 29, 30, 20, 21, 22, 23, 24,
  // 30  31  32  33  34  35  36  37  38  39
     36, 37, 38, 39, 40, 30, 31, 32, 33, 34,
  // 40  41  42  43  44  45  46  47  48  49
     46, 47, 48, 49, 50, 40, 41, 42, 43, 44,
  // 50  51  52  53  54  55
      4,  5,  6,  7,  0,  1
};

// ─────────────────────────────────────────────
//  GLOBALS
// ─────────────────────────────────────────────
String wledUrl;

// ─────────────────────────────────────────────
//  TEMPO SYNC STATE
// ─────────────────────────────────────────────
uint8_t       clockPulseCount  = 0;
bool          beatFlag         = false;
bool          clockRunning     = false;
unsigned long lastBeatSentMs   = 0;   // throttle guard
int           currentFx0       = 0;
int           currentFx1       = 17;
int           currentSx        = 128;

// ─────────────────────────────────────────────
//  HELPERS
// ─────────────────────────────────────────────
int clamp(int v, int lo, int hi) {
  return (v < lo) ? lo : (v > hi) ? hi : v;
}

void sendToWLED(const String& payload) {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  http.begin(wledUrl);
  http.addHeader("Content-Type", "application/json");
  http.POST(payload);
  http.end();
}

// ─────────────────────────────────────────────
//  BEAT RESET
//  Throttled: skips if last send was too recent.
//  Segment 0: locked to note-assigned effect.
//  Segment 1: new random effect every beat.
//  "r":true tells WLED to restart the animation
//  from frame 0 even if fx ID hasn't changed.
// ─────────────────────────────────────────────
void sendBeatReset() {
  unsigned long now = millis();
  if (now - lastBeatSentMs < BEAT_THROTTLE_MS) return;
  lastBeatSentMs = now;

  // Pick a new random effect for segment 1, guaranteed different from segment 0
  int newFx1;
  do {
    newFx1 = random(0, MAX_EFFECT_ID + 1);
  } while (newFx1 == currentFx0);
  currentFx1 = newFx1;

  StaticJsonDocument<256> doc;
  JsonArray seg = doc.createNestedArray("seg");
  JsonObject s0 = seg.createNestedObject();
  s0["id"] = 0; s0["fx"] = currentFx0; s0["sx"] = currentSx; s0["r"] = true;
  JsonObject s1 = seg.createNestedObject();
  s1["id"] = 1; s1["fx"] = currentFx1; s1["sx"] = 255 - currentSx; s1["r"] = true;
  String payload;
  serializeJson(doc, payload);
  sendToWLED(payload);
}

// ─────────────────────────────────────────────
//  CHANNEL 1: Note → Effect
// ─────────────────────────────────────────────
void handleChannel1(uint8_t note, uint8_t velocity) {
  int fx0 = clamp((int)note, 0, MAX_EFFECT_ID);
  int fx1 = (fx0 + 17 + random(1, MAX_EFFECT_ID / 2)) % (MAX_EFFECT_ID + 1);
  if (fx1 == fx0) fx1 = (fx1 + 1) % (MAX_EFFECT_ID + 1);

  currentFx0 = fx0;
  currentFx1 = fx1;
  currentSx  = (int)map(velocity, 0, 127, 0, 255);

  StaticJsonDocument<256> doc;
  JsonArray seg = doc.createNestedArray("seg");
  JsonObject s0 = seg.createNestedObject();
  s0["id"] = 0; s0["fx"] = fx0; s0["sx"] = currentSx; s0["ix"] = 128; s0["r"] = true;
  JsonObject s1 = seg.createNestedObject();
  s1["id"] = 1; s1["fx"] = fx1; s1["sx"] = 255 - currentSx; s1["ix"] = 128; s1["r"] = true;

  String payload;
  serializeJson(doc, payload);
  triggerFlash(FLASH_CH1_TX);
  sendToWLED(payload);
}

// ─────────────────────────────────────────────
//  CHANNEL 2: Note → Palette
// ─────────────────────────────────────────────
void handleChannel2(uint8_t note, uint8_t velocity) {
  int pal0 = clamp((int)map(note, 0, 127, 0, MAX_PALETTE_ID), 0, MAX_PALETTE_ID);
  int pal1 = clamp((int)COMPLEMENTARY_PALETTE[pal0], 0, MAX_PALETTE_ID);

  StaticJsonDocument<256> doc;
  JsonArray seg = doc.createNestedArray("seg");
  JsonObject s0 = seg.createNestedObject();
  s0["id"] = 0; s0["pal"] = pal0;
  JsonObject s1 = seg.createNestedObject();
  s1["id"] = 1; s1["pal"] = pal1;

  String payload;
  serializeJson(doc, payload);
  triggerFlash(FLASH_CH2_TX);
  sendToWLED(payload);
}

// ─────────────────────────────────────────────
//  MIDI RECEIVE
// ─────────────────────────────────────────────
void processMIDI() {
  midiEventPacket_t pkt;
  while (MIDIusb.readPacket(&pkt)) {
    uint8_t cin     = pkt.header & 0x0F;
    uint8_t channel = (pkt.byte1 & 0x0F) + 1;
    uint8_t note    = pkt.byte2;
    uint8_t vel     = pkt.byte3;

    // System Real-Time (CIN 0x0F, single-byte, no channel)
    if (cin == 0x0F) {
      switch (pkt.byte1) {
        case 0xF8:  // MIDI Clock — 24 pulses per beat
          if (clockRunning) {
            clockPulseCount++;
            if (clockPulseCount >= 24) {
              clockPulseCount = 0;
              beatFlag = true;   // consumed in loop()
            }
          }
          break;
        case 0xFA:  // Start
          clockRunning    = true;
          clockPulseCount = 0;
          break;
        case 0xFB:  // Continue
          clockRunning = true;
          break;
        case 0xFC:  // Stop
          clockRunning = false;
          break;
      }
    }

    // Note On
    if (cin == 0x09 && vel > 0) {
      triggerFlash(FLASH_MIDI_RX);
      if      (channel == 1) handleChannel1(note, vel);
      else if (channel == 2) handleChannel2(note, vel);
    }
  }
}

// ─────────────────────────────────────────────
//  SETUP
// ─────────────────────────────────────────────
void setup() {
  led.begin();
  led.setBrightness(NEO_BRIGHTNESS);
  ledOff();

  randomSeed(analogRead(0));

  nvs_flash_erase();
  nvs_flash_init();

  USB.manufacturerName("Espressif");
  USB.productName("WLED");
  USB.serialNumber("000001");
  USB.VID(0x303A);
  USB.PID(0x4001);
  USB.onEvent(onUsbEvent);
  MIDIusb.begin();
  USB.begin();

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int     attempts = 0;
  uint8_t pulse    = 0;
  bool    up       = true;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    ledBlue(pulse);
    if (up) { pulse += 15; if (pulse >= 180) up = false; }
    else    { pulse -= 15; if (pulse == 0)   up = true;  }
    delay(50);
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) ledGreen();
  else                               ledRed();

  wledUrl = String("http://") + WLED_IP + ":" + WLED_PORT + "/json/state";
}

// ─────────────────────────────────────────────
//  LOOP
// ─────────────────────────────────────────────
void loop() {
  processMIDI();
  updateLED();

  if (beatFlag) {
    beatFlag = false;
    sendBeatReset();
  }

  static unsigned long lastCheck = 0;
  if (millis() - lastCheck > 5000) {
    lastCheck = millis();
    if (WiFi.status() != WL_CONNECTED) {
      if (flashState == FLASH_NONE) ledRed();
      WiFi.reconnect();
    }
  }
}
