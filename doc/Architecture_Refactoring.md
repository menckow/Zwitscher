# Architecture Refactoring & Clean Code Guide

This document explains the comprehensive architectural refactoring implemented in the Zwitscherbox (Friendship Lamp) project. The goal of this refactoring was to convert a monolithic, hard-to-maintain `main.cpp` into a highly cohesive, modular, and maintainable C++ codebase.

## 1. The Monolithic Problem
Initially, all device logic was bundled inside `main.cpp`. This single file handled:
- WiFi and Networking (Access Point, Web Server, Captive Portal)
- MQTT dual-broker messaging (Home Assistant + Friendship Lamp external)
- Over-the-Air (OTA) firmware updates
- Hardware interaction (PIR sensor, button debouncing, potentiometer smoothing)
- SD Card file exploration and INI configuration parsing
- Adafruit NeoPixel animations via FreeRTOS mechanics
- Audio (`ESP32-audioI2S`) playback state

As features grew — notably the Friendship Lamp MQTT integration, LED Ring implementation, and full Web Configuration GUI — the single-file approach became difficult to navigate, prone to variable shadowing, and led to race conditions in LED updates.

## 2. The Modular Solution
The logic was aggressively decoupled into strict domains of responsibility. We created dedicated `.h` (Headers) and `.cpp` (Implementations) files in the `include/` and `src/` directories.

### `include/GlobalConfig.h`
A core file introduced to prevent circular dependencies. It acts as the central registry for all `extern` variables and shared configuration states (like `wifi_ssid`, `friendlamp_enabled`, `led_count`, `apMode`, and shared global pointers like the `AsyncWebServer`).

### `src/Config.cpp`
Responsible purely for SD card configuration mapping.
- Reads `config.txt` line by line.
- Cleanses string inputs and sets the global state variables.

### `src/WebManager.cpp`
Encapsulates all HTTP and asynchronous web interactions.
- Bootstraps the `ESPAsyncWebServer` for configuration.
- Manages the visual File Manager GUI.
- Hosts the Captive Portal `DNSServer` when the ESP32 acts as an Access Point (e.g. initial setup when no WiFi is reachable).

### `src/MqttHandler.cpp`
A heavily isolated networking module dedicated to real-time communication.
- Handles standard WiFi connection routines (`setup_wifi`).
- Implements parallel connection loops for the internal broker (Home Assistant) and the external broker (Friendshiplamp Public Broker) using separate `PubSubClient` instances.
- Securely executes Over-The-Air (OTA) `HTTPUpdate` firmwares over TLS.
- Houses the complex logic governing when and how MQTT clients reconnect upon failures.

### `src/LedController.cpp`
A hardware abstraction layer for the Adafruit NeoPixel strip.
- Introduces `neoPixelMutex` via FreeRTOS core concepts. This ensures `loop()` based LED fades and external MQTT command calls do not simultaneously write to the NeoPixel `strip` buffer.
- Computes non-blocking `rainbow` and `fade` algorithms.
- Exposes visual boot indicators (`setBootStatusLeds` and `setApModeLed`).

### `src/AudioHardware.cpp`
Encapsulates localized physical interactions without dirtying the network logic.
- Smooths analogue noise using moving-average algorithms to read the onboard Volume Potentiometer (`checkVolumePot`).
- Debounces the tactile physical switch (`checkButton`).
- Maps MP3 directories and injects files into memory vectors (`findMp3Directories`).

### `src/main.cpp`
Reduced to a pure **Orchestrator**. 
It simply calls `setup()` to sequentially initialize the modules (`Config` -> `Web` -> `MQTT` -> `LEDs` -> `Audio`), and it ticks the main `loop()`, acting as the traffic controller managing power states (PIR sensor wake-up, Deep-Sleep/Standby timeout).

## 3. Improvements Achieved
- **Decoupled Business Logic**: Testing network interactions without touching LED logic is now feasible.
- **Race Condition Immunity**: Enforced FreeRTOS mutexes prevent audio buffer collisions with intensive Neopixel math functions.
- **Boot Sequence Transparency**: Clean architectural scopes allowed us to neatly inject early-boot Status LEDs directly into `main.cpp` (SD Init), `WebManager` (AP Mode triggering), and `MqttHandler` (MQTT Check logic).
- **Soft-Reboot Safety**: By clearly separating `setup()` workflows from module functions, lingering NeoPixel Buffer states resulting from OTA soft-reboots were isolated and reliably neutralized upon instantiation.
