# Zwitscher - Directory-Based Random MP3 Player 🎵
![Image of the Zwitscherbox](https://github.com/menckow/Zwitscher/blob/main/doc/Zwitscher.png)
A smart, directory-based MP3 player based on the **ESP32-S3 (YB-ESP32-S3-AMP)** controller.

### ✨ Key features at a glance
- **Directory-based playback** with an intro sound for each folder.
- **PIR sensor** starts random MP3 playback when motion is detected — and, in v2, also fans out a family-circle signal so other boxes in the same circle light up.
- **Hardware controls** for volume (potentiometer) and folder selection (button).
- **Friendship lamp function (v2 family schema):** Lights up when *another* device in the same family circle sends a signal — whether that's a lamp button-press or another box's motion sensor. Lamp-originated signals are rendered with every 3rd LED in the complementary color so you can visually distinguish the source.
- **Family Circles:** Configure one or more `Familienkreise` (comma-separated) per box. The box belongs to all of them simultaneously and fans signals out to each.
- **Two separate MQTT integrations** for smart home systems (e.g. Home Assistant) and the friendship lamp function.
- **Configurable LED effects** (brightness, fade duration).
- **NVS memory** for volume and last folder to retain state after standby.
- **Integrated web portal** for easy configuration in the event of connection issues (including visual colour selection for the LED and family-circle assignment).
- **Web Upload OTA:** Drop a `.bin` directly via the configuration page — audio stops, LED turns blue, firmware is written in chunks and the box reboots automatically.
- **Central Device Management:** Fully compatible with the **Lamp Manager Dashboard**. Devices are grouped by family. Supports real-time status, version reporting, and Last Will (LWT) for offline detection.

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
- **Friendly lamp (RGB LED ring) — v2 family schema:**
  - **Presence:** If another device in the same family circle sends a signal — either a friend's lamp button-press or another box's PIR — your own LED ring lights up in the sender's color. Triggering *your own* PIR sensor does not cause your own lamp to light up, only fans the signal out to the family.
  - **Source-aware rendering:** Every signal carries a `sender_type` field. When the source is a **lamp**, every 3rd LED on the ring lights up in the complementary color — a clear visual hint that it came from a friend pressing a button. When the source is another **box** (PIR), the full ring lights up solid in the sender's color.
  - **Lamps ignore box signals** by design — a box's motion sensor doesn't wake a friend's lamp, only other boxes in the same family circle.
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
# v2 family schema: comma-separated list of family circle IDs the box belongs to.
# The box will subscribe to `fl/family/<id>/signal` for each entry and fan its
# PIR-triggered signals out to all of them. Whitespace is trimmed and entries
# are lowercased automatically.
FAMILY_IDS=schmidt,lieblings

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

## 🔘 Folder Taster & IP Address Visualization

The folder-change push button (connected to `Pin 17` or mapped Key on Audio Kit) has dual functions:
* **Short Press (< 3 seconds)**: Stops the current playback, changes to the next directory on the SD card, and plays the directory's intro sound.
* **Long Press (>= 3 seconds)**: Triggers the visual readout of the **last octet of the local IP address** on the RGB LED ring.
  * **Visual Pattern (Blinking Mode)**:
    * The last number of the IP address (e.g. `174` in `192.168.178.174`) is read out digit by digit.
    * For each digit $D$ (0 to 9), the entire LED ring flashes white $D + 1$ times (500 ms ON / 500 ms OFF).
    * To separate hundreds, tens, and ones, the entire LED ring lights up **Red for 1 second** (followed by a 500 ms pause).
  * **Note**: Releasing the button after the IP display starts will **not** trigger a folder change.

## 🚀 Installation & Compilation

This project uses **PlatformIO**. 
1. Clone or download the repository.
2. Open the folder in VSCode / PlatformIO.
3. The dependencies (such as `ESP32-audioI2S` and `PubSubClient`) are automatically downloaded via `platformio.ini` during the first build.
4. Connect the board via USB and click **Upload and Monitor**.

## 📡 MQTT Topics (v2 Family Schema)

When the MQTT connection is active, the box uses two topic families:

**Home Assistant / internal status** (based on `MQTT_BASE_TOPIC`, e.g. `audioplayer`):
* `audioplayer/status`: Status messages (‘Online’, ‘Playing Intro’, ‘Entering Standby’, ‘Woke up from Standby’, etc.)
* `audioplayer/volume`: Current volume.
* `audioplayer/directory`: The path to the newly selected directory on the SD card.
* `audioplayer/playing`: The path of the MP3 file currently being played or ‘STOPPED’.
* `audioplayer/ip_address`: The current IP address on the Wi-Fi network.
* `audioplayer/error`: Error messages (e.g. file system errors, missing MP3s).

**v2 family schema** (shared between lamps and boxes; consumed by the Dashboard):

| Purpose | Topic | Retained |
|---|---|---|
| Family signal (publisher + subscriber) | `fl/family/<familyId>/signal` | no |
| Device status (JSON) | `fl/device/<deviceId>/status` | **yes** |
| OTA per single device | `fl/device/<deviceId>/update/trigger` | no |
| OTA per family (boxes only) | `fl/family/<familyId>/update/trigger/box` | no |
| Global emergency OTA (boxes only) | `fl/_global/update/trigger/box` | no |
| OTA progress backchannel | `fl/device/<deviceId>/update/status` | no |

### 📝 Device Status Payload (retained JSON)

```json
{
  "type": "box",
  "fw": "V7.0.0",
  "state": "online",
  "color": "#0000FF",
  "families": ["schmidt", "lieblings"]
}
```

### 📝 Signal Payload (v2 family signal)

```json
{
  "client_id": "Kueche-Schmidt",
  "sender_type": "box",
  "color": "#0000FF",
  "effect": "fade",
  "duration": 30000,
  "ts": 1716729600
}
```

Fields explained:
- **`client_id`** (string): Unique device name (defined in `MQTT_CLIENT_ID`). Used for the **self-filter** so the box never reacts to its own messages.
- **`sender_type`** (string): Either `"lamp"` (button-press from a Friendship Lamp) or `"box"` (PIR-trigger from another box). Receivers use this to choose the LED rendering mode — lamps as source → complementary-color mode, boxes as source → solid color. Lamps additionally ignore any signal with `sender_type:"box"` so a box's motion sensor never wakes a friend's lamp.
- **`color`** (string): HEX color code (e.g. `#00FF00`). The literal `RAINBOW` triggers the rainbow effect.
- **`effect`** (string): `"fade"`, `"blink"`, or `"rainbow"`.
- **`duration`** (integer, ms): How long the LEDs stay lit before switching off (default `30000`).
- **`ts`** (Unix timestamp, optional): NTP-validated; signals older than 60 s are dropped to prevent retained messages from re-triggering after reconnect.

### 🔄 OTA Updates

Three update channels are supported, all using the same JSON payload format:

```json
{
  "url": "http://your-server.com/firmware.bin",
  "version": "7.0.1",
  "md5": "b3e3e3b3e3e3b3e3e3b3e3e3b3e3e3b3",
  "target_type": "box"
}
```

| Channel | Topic | Use case |
|---|---|---|
| Per-device | `fl/device/<deviceId>/update/trigger` | Update one specific box from the Dashboard |
| Per-family | `fl/family/<familyId>/update/trigger/box` | Update all boxes in a single family circle |
| Global | `fl/_global/update/trigger/box` | Emergency push across all boxes |

The `target_type` field is an optional defense-in-depth check — devices verify it matches their own type (`"box"`) before applying the update.

**Key Security & UI Features:**
- **Strict MD5 Validation:** The firmware verifies the provided `md5` hash internally before activating the new code. If the 32-character hash is missing or mismatched, the box aborts the update.
- **Dynamic LED Progress Ring:** During download, the RGB ring is repainted blue with progressive coverage.
- **Live MQTT Status Broadcasting:** The box streams download percentage updates (`Updating (30%)`, etc.) back to `fl/device/<deviceId>/update/status` and updates its main status topic as JSON.
- **Audio is automatically stopped** when an OTA starts, to keep the I²S DMA and Flash writes from colliding.
- If the installation fails the NeoPixel ring flashes red and the previous firmware continues to run.

### 💾 Web Upload OTA

In addition to the MQTT-triggered channels, the configuration page exposes a **Firmware-Update** section. Pick a `.bin`, optionally paste an MD5 hash, and submit. The box validates, writes via chunked `Update.write()`, and reboots automatically. Useful for first-time provisioning or when MQTT is temporarily unavailable.

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
