# 🐦 Zwitscherbox & Friendship Lamp

A smart, motion-controlled MP3 player with an integrated light ring, Wi-Fi configuration portal, and smart home connectivity (MQTT).

## 🌟 Overview

This device combines an interactive MP3 player with a "Friendship Lamp". As soon as the motion detector (PIR) senses a person, the device plays a random audio file from the SD card.
Simultaneously, it can send a signal over the internet (MQTT) to a second box, causing its integrated LED ring to light up – so you instantly know when someone is thinking of you!

## ✨ Key Features

- **Motion Detection:** Automatically starts random audio playback upon detecting movement.
- **Directory System:** Supports multiple folders with different sounds. A button press changes the directory (confirmed by a short identification sound `intro.mp3`).
- **Friendship Lamp (LED Ring):**
  - **Standard Signal:** The entire ring illuminates (with a smooth fade if activated) and dims out again after 30 seconds.
  - **Friendship Signal:** The ring lights up, but every 3rd LED appears as a highlight in the *complementary color* for visual distinction.
- **Easy Setup:** No programming required – Wi-Fi, light color, and Smart Home server can be easily configured via a convenient web interface on your phone.
- **Smart-Home Ready:** Can be fully integrated into a Smart Home (e.g., Home Assistant) via MQTT (Status, Volume, Currently Playing Track, IP Address).
- **Auto Standby/Memory:** The device automatically remembers the current folder and volume. After 5 minutes of inactivity, the device independently enters power-saving mode.

---

## 🛠️ 1. Initial Setup (Wi-Fi Portal)

When the box is started for the first time (or cannot find its saved Wi-Fi), it automatically opens its own setup network.

1. Search for the Wi-Fi network **"Zwitscherbox"** with your smartphone or laptop and connect.
2. A login window (Captive Portal) should open automatically. Alternatively, open your browser and enter any address.
3. On the configuration page, you can configure all settings:
   - **Network Name & Password:** Enter your standard home Wi-Fi here.
   - **Select Color:** Determine which color your LED ring should light up in when the box receives a signal.
   - **MQTT / Smart Home:** (Optional) Enable the connection to your own or an external Smart Home server to connect the box with your friends.
4. Click on **"Save Configuration"**. The box will now restart and connect to the normal home Wi-Fi.

---

## 🎛️ 2. Operation on the Device

* **Volume Knob (Potentiometer):** Turn the knob to continuously adjust the volume.
* **Press Button:** Switches to the next audio folder on the SD card. A short identification melody is played during the switch, after which the system is immediately ready to react to motion again.
* **Motion Sensor:** If movement is detected, the device randomly selects an MP3 file from the current folder and plays it.

---

## 📁 3. SD Card Preparation

For the box to play music, the included SD card must be prepared as follows:

1. Create one or more subfolders on the SD card (e.g., `Birdsongs`, `ForestSounds`, `Music`).
2. Copy your desired sound files into each folder (they must end exactly with `.mp3`).
3. **IMPORTANT:** Add another MP3 file to every main folder and name it exactly `intro.mp3`. This will always be played briefly when someone switches to the folder using the button. This way, you immediately know which folder is currently activated.

---

## 💡 4. How Does the Illumination Work?

As soon as your box detects motion, it not only plays an audio file but also secretly broadcasts a signal in the background over the internet (MQTT) indicating which color you want to light up in.

* **Your box receives a generic "Zwitscherbox Signal":** The LED ring fills completely with the receiver color and smoothly transitions into a fade-out (dimming) after about 30 seconds. (Ideal for the main system).
* **Your box receives a signal on the "Friendship Channel":** A special pattern is visually displayed: The ring lights up, but every third LED shines as a highlight in the *complementary color*. This way, you can see at a glance on which of the two channels the box was currently addressed!

---

## 🔧 Technical Specifications (For the tech-savvy)
- **Components:** ESP32 (Model: YB-ESP32-S3-AMP), PIR Motion Sensor (Pin 18), Button (Pin 17), Analog Potentiometer (Pin 4), NeoPixel LED Ring (16 LEDs on Pin 16).
- **Storage:** NVS (Non-Volatile Storage) securely saves volume and folder selection across reboots.
- **Playback Safety:** Individual audio tracks have a built-in time limit of 5 minutes. If the room remains unused, the system silently enters standby mode.
