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
