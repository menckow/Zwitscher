# 🐦 Zwitscher - User Documentation

Welcome to the guide for your new **Zwitscherbox**!
The Zwitscherbox is an intelligent, directory-based MP3 player. It plays relaxing sounds at the push of a button or through motion detection, and can simultaneously communicate over the internet with other boxes or Friendship Lamps.

---

## 🛠️ 1. Initial Setup

When you turn on the box for the first time, it searches for a known Wi-Fi network. If it doesn't find one, it creates its own setup Wi-Fi.

1. Open the Wi-Fi settings on your smartphone or computer.
2. Search for the network named **`Zwitscherbox`** and connect to it.
3. Enter the default password: **`12345678`**
4. Usually, a configuration window (Captive Portal) will open automatically. If not, open your web browser and go to the address **`http://192.168.4.1`**.
5. In the web interface, you can conveniently adjust all settings via the menus:
   * Select your **Home Wi-Fi** from the list and enter your password.
   * You can easily set the **Color of your Zwitscherbox** using a visual color picker.
   * If required, you can also provide MQTT server settings (for Smart Home).
6. Click Save. The box will restart and connect to your home network.

*Tip:* You don't need to remove the SD card for these settings; the web interface will adapt the `config.txt` for you!

---

## 💡 2. Everyday Operation

### 🎛️ Adjusting the Volume
Turn the **rotary knob (potentiometer)** to continuously adjust the volume of the sounds. The box remembers this volume for next time.

### 🎵 Changing Folders (Sounds)
Press the **button** on the box. The Zwitscherbox then switches to the next directory on the SD card (e.g., from "Birds" to "Waterfall").
If there is a file named `intro.mp3` in the new folder, it will be played briefly as confirmation (e.g., a voice saying "Waterfall").

### 🚶 Motion Sensor (PIR)
The box is equipped with an infrared motion sensor:
* As soon as it detects movement, it **randomly** selects an MP3 file from the currently selected folder and plays it.
* At the same time (if Wi-Fi is configured), it sends an invisible signal to other connected Friendship Lamps or Zwitscherboxes in your network.

### 💤 Standby Mode
If no movement has been detected for 5 minutes and no sound is playing, the box automatically enters a power-saving **Standby Mode**. The LEDs turn off, but it remains connected to the Wi-Fi in the background. As soon as you press the button or movement is detected, it wakes up immediately.

---

## 🎨 3. The Friendship Lamp Feature

Your Zwitscherbox has an integrated illuminated LED ring. This lights up when **other** Zwitscherboxes or Friendship Lamps in your network are activated!

* **How it works:** If a friend's motion sensor triggers, the LED ring on *your* box will light up in their predefined color.
* **Good to know:** If *your* own motion sensor triggers, your own LED ring will not light up. It only forwards the signal to the others.
* The ring uses smooth **fade effects**. Brightness and duration can be configured in the settings.

---

## 📁 4. Adding MP3s (File Manager)

You don't need to unscrew the box or take out the SD card to add new sounds!

1. Make sure you are in the same Wi-Fi network as the Zwitscherbox.
2. Open the file manager in your browser via the IP address of your Zwitscherbox by appending `/files` (e.g., `http://192.168.1.50/files`).
3. In this file manager, you can access the SD card directly:
   * Create **new folders** (categories).
   * Delete **old MP3s**.
   * Upload **new sounds** wirelessly via Wi-Fi. A progress bar will show you how long the upload will take.

*Structure tip:* Create a separate folder for each sound category. Optionally, you can place a file named `intro.mp3` in each folder, which will be played when you switch to this folder using the button.

---

## 📡 5. Smart Home Integration (For Professionals)

The Zwitscherbox can optionally be integrated into Smart Home systems (like Home Assistant):
* It has **two separate MQTT interfaces**: One for status messages to your internal Smart Home, and a separate one for the global Friendship Lamp connection.
* It transmits real-time values such as volume, playback status, the current IP address, and potential errors.
* It is fully compatible with the central **Lamp Manager Dashboard** for real-time monitoring of your device fleet.
