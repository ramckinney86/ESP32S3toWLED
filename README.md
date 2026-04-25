Using the Arduino IDE, upload this code to an ESP32 S3 to turn it into a Class Compliant MIDI Device. This allows one to connect to the board via MIDIUSB

My exact use case is to connect the microcontroller to an Akai MPC One and use the MIDI Sequencer to control LEDs remotely without the use of a standalong computer with DMX, ARTNET or other lighting software. The device turns MIDI Notes directly into WLED Updates over a WLAN.

Libraries and Dependencies:
- "USB.h"
- "USBMIDI.h"
- "nvs_flash.h"

- <Arduino.h>
- <WiFi.h>
- <HTTPClient.h>
- <ArduinoJson.h>
- <Adafruit_NeoPixel.h>

Hardware Requirements:
- ESP32 S3 Microcontroller
- Wireless Netowork (I use a local router that is used exclusively for WLED and not my home network)
- ESP32 with WLED Installed (I use a QuinLED-Dig-Uno v3 which is an ESP32 based, addressable LED controller. It has 2 channels)
- LED Strips
- MIDI Controller or computer with MIDI Sequencing capabilities)



# ESP32-S3 MIDI → WLED Controller
## Complete Setup & Reference Guide

---

## Overview

This project turns an **Espressif ESP32-S3 DevKitC-1** into a class-compliant USB MIDI device that receives MIDI messages from a host (such as the **Akai MPC One**) and translates them into real-time lighting commands sent to a **WLED** instance over WiFi.

- **MIDI Channel 1** notes set LED effects on your WLED segments
- **MIDI Channel 2** notes set color palettes on your WLED segments
- **MIDI Clock** from the host syncs effect animations to the beat
- The ESP32 shows up on the MPC One as **"WLED MIDI 1"** — no drivers needed

---

## Hardware Requirements

| Item | Details |
|---|---|
| **ESP32-S3 DevKitC-1** | Espressif official board. Must be the S3 variant — the S3 has native USB-OTG hardware required for class-compliant MIDI. |
| **WLED-compatible LED controller** | Any ESP8266/ESP32 running WLED firmware, connected to your LED strip. |
| **Akai MPC One** (or any USB MIDI host) | Any device with a USB-A host port that can send MIDI Clock + Note On messages. |
| **USB-C cable (×2)** | One for flashing (UART port), one for MIDI (OTG port). Both must be **data cables** — charge-only cables will not work. |
| **WiFi router** | The ESP32 and the WLED device must be on the same local network. |

### ESP32-S3 DevKitC-1 — Two USB Ports

The DevKitC-1 has **two USB-C connectors**. They are not interchangeable:

```
┌─────────────────────────────┐
│  [UART]              [USB]  │
│    ↑                   ↑    │
│  Flash              MIDI /  │
│  only              OTG port │
└─────────────────────────────┘
```

- **UART port** — used exclusively for flashing firmware from Arduino IDE
- **USB / OTG port** — connects to the MPC One; this is what enumerates as a MIDI device

> **Important:** Never flash via the OTG port. Never connect the MPC to the UART port.

---

## Software Requirements

### 1. Arduino IDE
Download from [arduino.cc](https://www.arduino.cc/en/software). Version 2.x recommended.

### 2. ESP32 Board Package
In Arduino IDE, go to **File → Preferences** and add this URL to the "Additional boards manager URLs" field:

```
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```

Then go to **Tools → Board → Boards Manager**, search for `esp32` by Espressif, and install version **2.0.14 or later**.

> The `USBMIDI` and `USB` libraries used in this sketch are bundled with the ESP32 board package — no separate install needed.

### 3. Arduino Libraries (Library Manager)
Go to **Tools → Manage Libraries** and install both of the following:

| Library | Author | Minimum Version |
|---|---|---|
| **ArduinoJson** | Benoit Blanchon | 6.0.0 |
| **Adafruit NeoPixel** | Adafruit | any current |

### 4. WLED Firmware
The LED controller must be running **WLED 0.14 or later**. Download and install from [github.com/Aircoookie/WLED](https://github.com/Aircoookie/WLED).

WLED must be configured with at least **2 segments** defined before use. Set these up in the WLED web interface under **Config → LED Preferences → Segments**.

---

## Arduino IDE Board Settings

These settings must be correct before compiling or uploading. Go to **Tools** in the menu bar and set each option:

| Setting | Value |
|---|---|
| **Board** | `ESP32S3 Dev Module` |
| **USB Mode** | `USB-OTG (TinyUSB)` ← critical |
| **USB CDC On Boot** | `Disabled` ← critical |
| **Upload Mode** | `UART0 / Hardware CDC` |
| **CPU Frequency** | `240MHz` (default) |
| **Flash Size** | `4MB` (default) |
| **Partition Scheme** | `Default 4MB` |
| **Port** | Select the `usbserial` port (not `usbmodem`) |

> **Why CDC On Boot must be Disabled:** With CDC enabled, the ESP32 presents as a composite USB device (MIDI + serial). Some MIDI hosts including the MPC One will fail to enumerate a composite device correctly and won't see the MIDI interface. Disabling CDC means the device presents as pure MIDI only.

> **Why USB Mode must be TinyUSB:** The default "Hardware CDC and JTAG" mode does not support MIDI device class. TinyUSB is the USB stack that enables class-compliant MIDI enumeration.

---

## Sketch Configuration

Open `esp32s3_midi_wled.ino` and edit the **USER CONFIGURATION** section near the top:

```cpp
const char* WIFI_SSID     = "Your_Network_Name";
const char* WIFI_PASSWORD = "Your_Password";
const char* WLED_IP       = "192.168.x.xxx";  // Local IP of your WLED device
const int   WLED_PORT     = 80;               // Default WLED HTTP port
```

To find your WLED device's IP address, open the WLED web interface from a browser on the same network — the URL in the address bar is its IP.

Other tunable constants:

```cpp
const unsigned long BEAT_THROTTLE_MS = 200;  // Min ms between beat-reset HTTP calls
#define NEO_BRIGHTNESS 204                    // LED brightness 0-255 (204 = 80%)
```

---

## Flashing the Firmware

1. Connect the **UART port** (not OTG) to your Mac with a data USB-C cable
2. In Arduino IDE, select the correct port under **Tools → Port**
   - Look for a port named `/dev/cu.usbserial-XXXX`
   - Do **not** select `/dev/cu.usbmodem-XXXX` — that is the OTG port
3. Click **Upload** (→ arrow button)
4. If upload fails with "Failed to connect", hold the **BOOT** button on the board, tap **RESET**, then release **BOOT** — this forces bootloader mode
5. After a successful upload, unplug the UART cable

---

## Connecting to the MPC One

1. Connect the ESP32's **OTG/USB port** to one of the **MPC One's USB-A host ports** using a data cable
2. Power on or reboot the ESP32 (tap the RESET button)
3. On the MPC One: **Menu → Preferences → MIDI / Sync**
4. The device appears as **"WLED MIDI 1"** in the USB MIDI devices list
5. Enable it as a MIDI output and assign it to a MIDI track
6. Make sure **MIDI Clock** is enabled as a sync output:
   **Menu → Preferences → MIDI / Sync → Sync Send → MIDI Clock: On**

---

## How It Works

### MIDI Channel 1 — Effects

Sending a **Note On** on MIDI Channel 1 sets the LED effect:

- **Segment 0** receives the effect mapped from the note number (note 0 = effect 0, note 118 = effect 118, notes above 118 clamp to 118)
- **Segment 1** receives a random effect that differs from Segment 0
- **Note velocity** maps to effect speed (higher velocity = faster animation on Segment 0, slower on Segment 1)

### MIDI Channel 2 — Palettes

Sending a **Note On** on MIDI Channel 2 sets the color palette:

- **Segment 0** receives the palette mapped from the note number (0–127 mapped across 0–55 palette IDs)
- **Segment 1** receives a complementary palette chosen from a lookup table in the firmware

### MIDI Clock — Beat Sync

When the MPC One is playing and sending MIDI Clock:

- The ESP32 counts **24 clock pulses per beat**
- On each beat it sends a reset command to both segments, restarting the effect animations from frame 0
- Segment 1 gets a newly randomised effect on every beat
- Beat resets are throttled to a minimum of 200ms apart so rapid tempos don't flood WLED's HTTP server

### MIDI Start / Stop / Continue

- **Start (0xFA)** — enables beat sync, resets pulse counter
- **Continue (0xFB)** — re-enables beat sync without resetting the counter
- **Stop (0xFC)** — disables beat sync; effects continue playing but no longer reset on the beat

---

## LED Status Indicators

The onboard RGB NeoPixel (labeled **RGB@IO38**) shows the current state:

| LED | Meaning |
|---|---|
| 🔵 Pulsing blue | Connecting to WiFi on boot |
| 🟢 Solid green | WiFi connected, idle and ready |
| 🔴 Solid red | WiFi lost — auto-reconnects every 5 seconds |
| 🟡 Brief yellow | MPC One (USB host) connected and device enumerated |
| ⚪ Brief white (80ms) | MIDI Note On received |
| 🩵 Brief cyan (80ms) | Effect command sent to WLED (Channel 1) |
| 🟣 Brief magenta (80ms) | Palette command sent to WLED (Channel 2) |

---

## WLED Setup Requirements

In the WLED web interface, ensure the following before use:

1. **Two segments are defined** — go to the main WLED page, expand the segment panel, and create Segment 0 and Segment 1 covering your LED strip ranges
2. **JSON API is enabled** (it is by default) — the sketch communicates via HTTP POST to `/json/state`
3. **The WLED device has a static or reserved IP** — if the IP changes, update `WLED_IP` in the sketch and reflash. The easiest way is to reserve the IP in your router's DHCP settings

---

## Troubleshooting

**ESP32 not appearing on MPC One**
- Confirm you are plugged into the **OTG port**, not the UART port
- Confirm **USB CDC On Boot** is set to `Disabled` in Arduino IDE and the sketch has been reflashed with that setting
- Try a different USB cable — many USB-C cables are charge-only
- Tap the RESET button on the ESP32 after plugging into the MPC

**LED stuck on red after boot**
- The ESP32 cannot connect to WiFi — check `WIFI_SSID` and `WIFI_PASSWORD` in the sketch
- Ensure the ESP32 is within range of the router
- The board will retry every 5 seconds automatically

**WLED not responding to MIDI notes**
- Confirm `WLED_IP` is correct and the WLED device is powered on
- Open a browser and navigate to `http://192.168.x.xxx/json/state` — if you see JSON, the API is reachable
- Ensure both the ESP32 and WLED device are on the same WiFi network (same subnet)
- Check that 2 segments are configured in WLED

**Upload fails with "Failed to connect"**
- Hold **BOOT**, tap **RESET**, release **BOOT** — then immediately click Upload in Arduino IDE
- Confirm you are on the `usbserial` port, not the `usbmodem` port
- Try a different USB cable

**Beat sync not working**
- On the MPC One, go to **Menu → Preferences → MIDI / Sync → Sync Send** and enable MIDI Clock output on the port assigned to "WLED MIDI 1"
- Confirm the MPC is in playback (not stopped) — MIDI Clock only transmits while playing

---

## File Reference

| File | Description |
|---|---|
| `esp32s3_midi_wled.ino` | Main Arduino sketch |
| `MIDI_WLED_Reference.docx` | Full table of MIDI notes → effects and palettes |
| `LED_Status_Guide.docx` | LED color meanings reference |
| `ESP32_WLED_MIDI_Setup_Guide.md` | This document |

---

## Quick Reference Card

```
MIDI CH1 Note On  →  Segment 0: effect = note number
                      Segment 1: random effect
                      Velocity:  effect speed

MIDI CH2 Note On  →  Segment 0: palette mapped from note
                      Segment 1: complementary palette

MIDI Clock        →  Beat-locked effect restart (both segments)
MIDI Start        →  Enable beat sync
MIDI Stop         →  Disable beat sync

Flash via:  UART port  (usbserial)
MIDI via:   OTG port   (usbmodem)
```
