#include "AudioEngine.h"
#include "AppConfig.h"
#include "MqttHandler.h"
#include <Preferences.h>

extern AppConfig config;
Preferences preferences;

// Note: these could be loaded from AppConfig instead of hardcoded in AudioEngine.
const unsigned long potReadInterval = 100;
const float potSmoothingFactor = 0.2;
const int POT_PIN = 4;
const int BUTTON_PIN = 17;
const int PIR_PIN = 18;
const unsigned long debounceDelay = 50;

AudioEngine::AudioEngine(Audio& audioRef) : audio(audioRef) {
    currentState = PlaybackState::INITIALIZING;
    playbackStartTime = 0;
    lastPirActivityTime = 0;
    currentDirectoryIndex = -1;
    introFileName = "intro.mp3";
    
    lastPotReadTime = 0;
    lastVolume = -1;
    smoothedPotValue = -1.0;
    
    lastButtonState = HIGH;
    buttonState = HIGH;
    lastDebounceTime = 0;
}

void AudioEngine::init() {
    preferences.begin("appState", true);
    currentDirectoryIndex = preferences.getInt("dirIndex", -1);
    preferences.end();
    
    if (directoryList.empty()) {
        Serial.println("Warning: AudioEngine init with empty directoryList");
    } else {
        if (currentDirectoryIndex < 0 || currentDirectoryIndex >= directoryList.size()) {
            currentDirectoryIndex = 0;
        }
        loadFilesFromCurrentDirectory();
    }
    
    currentState = PlaybackState::IDLE;
    lastPirActivityTime = millis();
}

void AudioEngine::update() {
    audio.loop();
    
    // Process hardware interactions (potentiometer, buttons, PIR logic)
    // Only process these if we are not in standby? Actually we can wake up from standby via button.
    checkVolumePot();
    checkButton();
    checkPirAndTimeout();
}

void AudioEngine::findMp3Directories(File dir) {
    while (true) {
        File entry = dir.openNextFile();
        if (!entry) break;
        if (entry.isDirectory()) {
            File subdir = SD.open(entry.path());
            bool hasMp3 = false;
            if(subdir) {
                while(true) {
                    File subEntry = subdir.openNextFile();
                    if(!subEntry) break;
                    String subName = String(subEntry.name());
                    subName.toLowerCase();
                    if (!subEntry.isDirectory() && subName.endsWith(".mp3")) {
                        hasMp3 = true;
                        subEntry.close();
                        break;
                    }
                    subEntry.close();
                }
                subdir.close();
            }
            if (hasMp3) {
                directoryList.push_back(String(entry.path()));
                Serial.println("Found MP3 directory: " + String(entry.path()));
            }
        }
        entry.close();
    }
}

void AudioEngine::loadFilesFromCurrentDirectory() {
    currentMp3Files.clear();
    if (currentDirectoryIndex < 0 || currentDirectoryIndex >= directoryList.size()) {
         if (!directoryList.empty()) currentDirectoryIndex = 0;
         else return;
    }

    String currentDirPath = directoryList[currentDirectoryIndex];
    Serial.println("Loading files from: " + currentDirPath);
    File dir = SD.open(currentDirPath);
    if (!dir || !dir.isDirectory()) {
        if(dir) dir.close();
        return;
    }
    while (true) {
        File entry = dir.openNextFile();
        if (!entry) break;
        if (!entry.isDirectory()) {
            String fileName = String(entry.name());
            String lowerCaseFileName = fileName;
            lowerCaseFileName.toLowerCase();
            if (lowerCaseFileName.endsWith(".mp3") && !lowerCaseFileName.endsWith(introFileName)) {
                 currentMp3Files.push_back(fileName);
            }
        }
        entry.close();
    }
    dir.close();
    Serial.printf("  Found %d playable MP3 files.\n", currentMp3Files.size());
}

void AudioEngine::playIntro() {
    if (directoryList.empty()) return;
    String currentDirPath = directoryList[currentDirectoryIndex];
    String introPath = currentDirPath + "/" + introFileName;
    
    if (audio.connecttoFS(SD, introPath.c_str())) {
        currentState = PlaybackState::PLAYING_INTRO;
        Serial.println("Playing intro: " + introPath);
        mqttHandler.publish(config.getTopicPlaying(), introPath);
        mqttHandler.publish(config.getTopicStatus(), "Playing Intro");
    } else {
        Serial.println("intro.mp3 not found: " + introPath);
        mqttHandler.publish(config.getTopicError(), "intro.mp3 not found");
        currentState = PlaybackState::IDLE;
    }
}

void AudioEngine::playRandomTrack() {
    if (currentMp3Files.empty()) {
        Serial.println("No random MP3 files found.");
        return;
    }
    int randomIndex = random(0, currentMp3Files.size());
    String randomFileName = currentMp3Files[randomIndex];
    String currentDirPath = directoryList[currentDirectoryIndex];
    String randomFilePath = currentDirPath + "/" + randomFileName;
    
    if (audio.connecttoFS(SD, randomFilePath.c_str())) {
        currentState = PlaybackState::PLAYING_RANDOM;
        playbackStartTime = millis();
        Serial.println("Playing random file: " + randomFilePath);
        mqttHandler.publish(config.getTopicPlaying(), randomFilePath);
        mqttHandler.publish(config.getTopicStatus(), "Playing Random File");
    } else {
        Serial.println("Error playing random file.");
        mqttHandler.publish(config.getTopicError(), "File play error");
        currentState = PlaybackState::IDLE;
    }
}

void AudioEngine::stopPlayback() {
    if (currentState == PlaybackState::PLAYING_INTRO || currentState == PlaybackState::PLAYING_RANDOM) {
        audio.stopSong();
        currentState = PlaybackState::IDLE;
        Serial.println("Stopped current playback.");
        mqttHandler.publish(config.getTopicPlaying(), "STOPPED (Manual)");
    }
}

void AudioEngine::onAudioEof() {
    Serial.println("\n>>> audio_eof_mp3 called <<<");
    mqttHandler.publish(config.getTopicPlaying(), "STOPPED (EOF)");
    
    if (currentState == PlaybackState::PLAYING_INTRO) {
        currentState = PlaybackState::IDLE;
        Serial.println(">>> Intro playback finished. Ready for PIR. <<<");
        mqttHandler.publish(config.getTopicStatus(), "Intro Finished");
    } else if (currentState == PlaybackState::PLAYING_RANDOM) {
        currentState = PlaybackState::IDLE;
        playbackStartTime = 0;
        Serial.println(">>> Random file finished. Ready for PIR. <<<");
        mqttHandler.publish(config.getTopicStatus(), "Random File Finished");
    } else {
        currentState = PlaybackState::IDLE;
        Serial.println(">>> EOF occurred unexpectedly in non-playing state. <<<");
    }
}

void AudioEngine::checkVolumePot() {
    unsigned long currentTime = millis();
    if (currentTime - lastPotReadTime >= potReadInterval) {
        lastPotReadTime = currentTime;
        int rawPotValue = analogRead(POT_PIN);
        if (smoothedPotValue < 0.0) smoothedPotValue = (float)rawPotValue;
        else smoothedPotValue = potSmoothingFactor * (float)rawPotValue + (1.0 - potSmoothingFactor) * smoothedPotValue;
        
        int currentVolume = map((int)smoothedPotValue, 0, 4095, 0, 21);
        if (currentVolume != lastVolume) {
            audio.setVolume(currentVolume);
            lastVolume = currentVolume;
            Serial.printf("Volume set to: %d (Raw: %d)\n", currentVolume, rawPotValue);
            mqttHandler.publish(config.getTopicVolume(), String(currentVolume));
        }
    }
}

void AudioEngine::checkButton() {
    int reading = digitalRead(BUTTON_PIN);
    if (reading != lastButtonState) { lastDebounceTime = millis(); }
    if ((millis() - lastDebounceTime) > debounceDelay) {
        if (reading != buttonState) {
            buttonState = reading;
            if (buttonState == LOW) {
                lastPirActivityTime = millis();
                if (currentState == PlaybackState::STANDBY) {
                    currentState = PlaybackState::IDLE;
                    Serial.println("Woke up from Standby (Button)");
                    mqttHandler.publish(config.getTopicStatus(), "Woke up from Standby");
                }
                
                Serial.println("\n--- Button pressed! Changing directory ---");
                mqttHandler.publish(config.getTopicStatus(), "Button Pressed");
                
                stopPlayback();
                
                if (!directoryList.empty()) {
                    currentDirectoryIndex = (currentDirectoryIndex + 1) % directoryList.size();
                    String newDirPath = directoryList[currentDirectoryIndex];
                    mqttHandler.publish(config.getTopicDirectory(), newDirPath, true);
                    loadFilesFromCurrentDirectory();
                    
                    playIntro();
                    
                    preferences.begin("appState", false);
                    preferences.putInt("dirIndex", currentDirectoryIndex);
                    preferences.end();
                }
            }
        }
    }
    lastButtonState = reading;
}

#include "LedController.h"

const unsigned long maxPlaybackDuration = 5 * 60 * 1000UL;
const unsigned long deepSleepInactivityTimeout = 90 * 60 * 1000UL;

void AudioEngine::checkPirAndTimeout() {
    bool pirStateHigh = (digitalRead(PIR_PIN) == HIGH);

    if (pirStateHigh || currentState == PlaybackState::PLAYING_INTRO || currentState == PlaybackState::PLAYING_RANDOM) {
        lastPirActivityTime = millis();
    }

    if (currentState == PlaybackState::IDLE || currentState == PlaybackState::STANDBY) {
        if (pirStateHigh) {
            lastPirActivityTime = millis();
            if (currentState == PlaybackState::STANDBY) {
                currentState = PlaybackState::IDLE;
                Serial.println("Woke up from Standby (PIR)");
                mqttHandler.publish(config.getTopicStatus(), "Woke up from Standby");
            }
            Serial.println("\n+++ PIR TRIGGER +++");
            mqttHandler.publish(config.getTopicStatus(), "PIR Triggered");

            if (config.friendlamp_enabled) {
                // Send MQTT string
                mqttHandler.publish(config.zwitscherbox_topic, "{\"client_id\":\"" + config.mqtt_client_id + "\",\"color\":\"" + config.friendlamp_color + "\",\"effect\":\"fade\",\"duration\":30000}", false);
            }

            playRandomTrack();
        }
    }

    // Timeout
    if (currentState == PlaybackState::PLAYING_RANDOM) {
        unsigned long elapsedTime = millis() - playbackStartTime;
        if (elapsedTime >= maxPlaybackDuration) {
            Serial.println("\n!!! PIR playback timeout !!!");
            stopPlayback();
        }
    }

    // Standby Check
    if (currentState == PlaybackState::IDLE) {
        unsigned long inactivityDuration = millis() - lastPirActivityTime;
        if (inactivityDuration >= deepSleepInactivityTimeout) {
            preferences.begin("appState", false);
            preferences.putInt("volume", lastVolume);
            preferences.end();
            
            Serial.println("--- Entering Standby Mode ---");
            mqttHandler.publish(config.getTopicStatus(), "Entering Standby", true);
            
            if (config.friendlamp_enabled) {
                ledCtrl.startFadeOut();
            }
            currentState = PlaybackState::STANDBY;
        }
    }
}
