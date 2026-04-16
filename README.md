# Zwitscher - Directory-Based Random MP3 Player 🎵
![Image of the Zwitscherbox](https://github.com/menckow/Zwitscher/blob/main/doc/Zwitscher.png)
A smart, directory-based MP3 player based on the **ESP32-S3 (YB-ESP32-S3-AMP)** controller.

### ✨ Key features at a glance
- **Directory-based playback** with an intro sound for each folder.
- **PIR sensor** starts random MP3 playback when motion is detected.
- **Hardware controls** for volume (potentiometer) and folder selection (button).
- **Friendship lamp function:** Lights up when *another* box detects motion.
- **Two separate MQTT integrations** for smart home systems (e.g. Home Assistant) and the friendship lamp function.
- **Configurable LED effects** (brightness, fade duration).
- **NVS memory** for volume and last folder to retain state after standby.
- **Integrated web portal** for easy configuration in the event of connection issues (including visual colour selection for the LED).
- **Central Device Management:** Fully compatible with the **Lamp Manager Dashboard**. Supports real-time status, version reporting, and Last Will (LWT) for offline detection.

## 🌟 Detailed features

- **Directory-based playback:** MP3s are organised into folders on the SD card. A button allows you to switch between directories.
- **Intro sound:** When switching folders, an `intro.mp3` file from the new directory is played automatically (if available).
- **Motion detector (PIR):** Detects movement and then plays a random MP3 file from the current directory. It also triggers the sending of an MQTT signal to the friendship lamp.
- **Hardware controls:**
  - **Potentiometer** for smooth and seamless volume control.
  - **Button** with debounce to switch directories.
- **Standby mode & NVS:**
  - After 5 minutes of inactivity, the ESP32 automatically enters **standby mode**. Audio playback stops and the LED ring is switched off, but Wi-Fi and MQTT remain active (important for the friendship lamp’s passive receiver function).
  - Wakes up internally when the PIR sensor is triggered or when a button is pressed.
  - The current directory and volume are stored in **NVS (Non-Volatile Storage)** to ensure data integrity.
- **Quiet Time (Do Not Disturb):**
  - Configurable via the Web UI, you can select your **Timezone** (e.g. Europe/Berlin) and define a sleep window (e.g. `22:00` to `08:00`). 
  - Using an internal background **NTP time sync**, the device will strictly ignore any incoming external colour MQTT signals during this window, ensuring it doesn't accidentally wake you up at night with bright LEDs. Local motion detection (PIR audio) continues to function.
- **Friendly lamp (RGB LED ring):**
  - **Presence:** If *another* box detects movement and sends an MQTT signal, your own LED ring lights up in the colour of the sender. Triggering *your own* PIR sensor **does not** cause your own lamp to light up, but only sends a signal to the others.
  - **Two Topics:** The box distinguishes between messages on the `ZWITSCHERBOX_TOPIC` (the ring lights up completely in the colour sent) and the `FRIENDLAMP_TOPIC` (every third LED on the ring lights up in the complementary colour).
  - The LEDs fade in and out with a gentle **fade effect**. The duration and brightness can be adjusted in `config.txt`.
  - **Boot Check Sequence:** When powering on, the LEDs intuitively visualise the health of the boot process by lighting up individual LEDs sequentially in green (or red for failure) to signal initialisation stages (1. Hardware, 2. Wi-Fi, 3. Internal MQTT, 4. External MQTT).
- **Smart Home / MQTT Integration:**
  - The MQTT functionality is split into two parts:
    - An integration for **status messages** sent to an internal broker (e.g. Home Assistant).
    - A separate integration for the **Friendship Lamp feature**, which can also use a public broker.
    - Both can be enabled independently of one another. The device remains completely offline when both are disabled.
  - Connects via WiFi and sends real-time status updates via MQTT (volume, playback status, errors, current IP) **only** to the internal broker.
- **Timeouts:** The maximum playback duration for a random session is limited to 5 minutes to prevent endless playback.
- **Web configuration portal:** If a valid Wi-Fi connection cannot be established, the device launches its own password-protected access point (`Zwitscherbox`, **Password: `12345678`**). All settings can be conveniently configured via a web interface using drop-down menus (including a Wi-Fi scanner), without having to manually edit the SD card. The colour of the LED Ring (your colour) can be set particularly intuitively using a visual HTML colour picker. You can optionally secure this UI with a permanent administrator password.
- **SD Card File Manager:** The web interface also includes a fully-fledged `/files` file manager endpoint! This enables you to directly navigate the SD card directory, create new folders, delete files, and upload new MP3s over Wi-Fi without having to physically remove the SD card from the ESP32. Large MP3 files are smoothly transferred using asynchronous, memory-safe data chunks along with a live progress bar.
## 🛠 Hardware requirements

- **Board:** YB-ESP32-S3-AMP (based on Arduino ESP32 core >= v3.1.1)
- **SD card module:** Integrated (SPI)
- **PIR sensor:** Connected to `Pin 18`
- **Push button:** Connected to `Pin 17` (internal pull-up configured)
- **Potentiometer:** Connected to `Pin 4`
- **RGB LED ring (NeoPixel):** Data line connected to `Pin 16`
- **I2S Audio:** Pins for I2S configuration (integrated into the audio library).

## 🗂 Folder structure on the SD card
The SD card should be formatted as follows:
```
/
├── config.txt                   # (Optional) Wi-Fi and MQTT configuration
├── BirdSounds/                  # Directory 1
│   ├── intro.mp3                # Intro for Directory 1 (‘You are now listening to birds’)
│   ├── blackbird.mp3
│   └── thrush.mp3
└── Waterfall/                   # Directory 2
    ├── intro.mp3
    ├── waterfall1.mp3
    └── waterfall2.mp3
```

## ⚙️ Configuration (`config.txt`)

To use MQTT and Wi-Fi, create a `config.txt` file in the root directory of the SD card. Here is an example with all the new options:

```ini
# --- 1. MQTT activation ---
# Set this value to 1 to enable status reporting to your internal
# MQTT broker (e.g. Home Assistant).
MQTT_INTEGRATION=1

# Set this value to 1 to enable the Friend Lamp function via MQTT
#. This can be the internal broker or a public broker.
FRIENDLAMP_MQTT_INTEGRATION=1

# Note: If both of the above values are set to 0, the device remains offline (no Wi-Fi).

# --- 2. Wi-Fi settings ---
# Required if any of the MQTT integrations are enabled.
WIFI_SSID=YourWi-FiName
WIFI_PASS=YourWi-FiPassword

# --- 3. Internal MQTT broker (for Home Assistant) ---
MQTT_SERVER=192.168.1.100
MQTT_PORT=1883
MQTT_USER=mqtt_user
MQTT_PASS=mqtt_password
MQTT_CLIENT_ID=ESP32_AudioPlayer
MQTT_BASE_TOPIC=audioplayer

# --- 5. Friendship Lamp (RGB LED ring) ---
# Turns the LED ring hardware on (1) or off (0).
FRIENDLAMP_ENABLE=1
# Fixed transmission colour for THIS box in hexadecimal format (visually selectable in the web portal)
FRIENDLAMP_COLOR=0000FF
# Topic for friendship colour signals
FRIENDLAMP_TOPIC=freundschaftslampe/farbe
# Topic for Zwitscherbox colour signals
ZWITSCHERBOX_TOPIC=zwitscherbox/farbe

# Control for LED effects
LED_FADE_EFFECT=1
LED_FADE_DURATION=1000
LED_BRIGHTNESS=100

# --- External broker for the friendship lamp (optional) ---
# If these fields are left blank, the internal broker is used automatically.
FRIENDLAMP_MQTT_SERVER=broker.hivemq.com
FRIENDLAMP_MQTT_PORT=1883
FRIENDLAMP_MQTT_USER=
FRIENDLAMP_MQTT_PASS=

# TLS encryption (only relevant for the external Friendlamp broker)
# Set to 1 to encrypt the connection to the Friendlamp broker (usually port 8883).
# The internal Home Assistant broker remains unaffected and unencrypted.
FRIENDLAMP_MQTT_TLS_ENABLED=0

# If TLS is enabled, the root certificate (BEGIN_CERT...END_CERT) must be included in config.txt.
```
If the file is omitted or both `_INTEGRATION` flags are set to `0`, the player runs completely offline.

## 🚀 Installation & Compilation

This project uses **PlatformIO**. 
1. Clone or download the repository.
2. Open the folder in VSCode / PlatformIO.
3. The dependencies (such as `ESP32-audioI2S` and `PubSubClient`) are automatically downloaded via `platformio.ini` during the first build.
4. Connect the board via USB and click **Upload and Monitor**.

## 📡 MQTT Topics

When the MQTT connection is active, the setup publishes to the following topics (based on `MQTT_BASE_TOPIC`, e.g. `audioplayer`) only to the internal broker:

* `audioplayer/status`: Status messages (‘Online’, ‘Playing Intro’, ‘Entering Standby’, ‘Woke up from Standby’, etc.)
* `audioplayer/volume`: Current volume.
* `audioplayer/directory`: The path to the newly selected directory on the SD card.
* `audioplayer/playing`: The path of the MP3 file currently being played or ‘STOPPED’.
* `audioplayer/ip_address`: The current IP address on the Wi-Fi network.
* `audioplayer/friendlamp`: Used to exchange colours for the friendship lamp between devices.
* `audioplayer/error`: Error messages (e.g. file system errors, missing MP3s).

### 📝 MQTT JSON Payload (Friendship Lamp & Twitter Box)

The hardware communicates with each other using a structured JSON format for colour transmission and effect control. For reasons of backwards compatibility, older string messages in the legacy format (`SenderID:HexColour`) are also decoded without error (fallback mechanism). 

A standard JSON packet from the sender (or your smart home system) looks like this:

```json
{
  ‘client_id’: ‘My_PIR_Sensor_ESP’,
  ‘color’: ‘FF0000’,
  ‘effect’: ‘fade’,
  ‘duration’: 30000
}
```

- **`client_id` (string):** The unique name (defined in MQTT_CLIENT_ID) of the device playing the content (used to ensure the box does not respond to its own messages and to prevent infinite loops).

- **`color` (string):** The HEX colour code (e.g. `00FF00` or `#00FF00` for green). If the word `RAINBOW` is passed as the colour, the hidden, circular light effect starts.
- **`effect` (string):** Controls the animation behaviour of the NeoPixel LEDs. Supported values are `‘fade’` (soft fade), `“blink”` (flashing once per second in time with the beat) and `‘rainbow’`.
- **`duration` (Integer):** Time in milliseconds for how long the LEDs should remain lit before switching off automatically (default: `30000` = 30 seconds). This allows Home Assistant to perfectly control the duration of the visual signals depending on the application scenario (e.g. short alarm vs. long background lighting).

### 🔄 OTA Update via MQTT

The hardware also supports Over-The-Air (OTA) firmware updates triggered over MQTT. Send a JSON payload to the `zwitscherbox/update/trigger` topic (this is a static topic, independent of your topic configuration):

```json
{
  "url": "http://your-server.com/firmware.bin",
  "version": "7.0.1"
}
```

If the `version` in the payload differs from the current `FW_VERSION` in the source code, the device will pause all active audio playback, illuminate the LED ring in solid blue, and begin downloading the firmware. Detailed progress and status updates will be published to the individual status topic `zwitscherbox/status/<ClientID>` and a general log topic `zwitscherbox/update/status`. Upon successful installation, the device will automatically reboot. Should the update fail, the LEDs will briefly pulse red and an error message will be published to the status topic.

## 📄 Licence

This project was developed exclusively as a private/open-source solution. 

## 🐞 Troubleshooting

### Build error: ‘not able to power down in light sleep’

When compiling the project, the following error message may appear, which typically occurs during the initialisation of the NeoPixel LED ring:

```
E (3637) rmt: rmt_new_tx_channel(269): unable to power down into light sleep
[  3664][E][esp32-hal-rmt.c:548] rmtInit(): GPIO 16 - RMT TX initialisation error.
```

- **Cause:** This error is caused by a bug in older versions of the ESP32 framework (Arduino Core) for PlatformIO. The RMT peripheral driver used by the NeoPixel library has a compatibility issue with the ESP32’s power-saving modes.

- **Solution:** To resolve this, the `platformio.ini` file has been modified. The `platform` setting has been set to a specific Git repository containing a newer version of the framework that fixes this error whilst also providing the necessary board definitions for the YB-ESP32-S3-AMP board.

The relevant line in `platformio.ini` is:
```ini
[env]
platform = https://github.com/pioarduino/platform-espressif32.git
```

This change ensures that the project is built using a compatible and bug-fixed version of the ESP32 framework.
