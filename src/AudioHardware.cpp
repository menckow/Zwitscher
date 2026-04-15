#include "AudioHardware.h"
#include "GlobalConfig.h"
#include "MqttHandler.h"
#include "Audio.h"
#include <FS.h>
#include <SD.h>

void findMp3Directories(File dir) {
    // ... (Code unverändert) ...
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

void loadFilesFromCurrentDirectory() {
    // ... (Code unverändert, nutzt jetzt den potenziell geladenen currentDirectoryIndex) ...
    currentMp3Files.clear();
    // Zusätzliche Sicherheitsprüfung hier, falls Index trotz Validierung in Setup ungültig wäre
    if (currentDirectoryIndex < 0 || currentDirectoryIndex >= directoryList.size()) {
         Serial.printf("Warning: loadFiles called with invalid index %d. List size %d.\n", currentDirectoryIndex, directoryList.size());
         // Optional: Auf Index 0 zurücksetzen, falls Liste nicht leer ist
         if (!directoryList.empty()) currentDirectoryIndex = 0;
         else return; // Nichts zu laden
    }

    String currentDirPath = directoryList[currentDirectoryIndex];
    Serial.println("Loading files from: " + currentDirPath);
    File dir = SD.open(currentDirPath);
    if (!dir || !dir.isDirectory()) {
        Serial.println("Error opening directory: " + currentDirPath);
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
    Serial.printf("  Found %d playable MP3 files (excluding intro).\n", currentMp3Files.size());
}

void audio_eof_mp3(const char *info) {
    Serial.println("\n>>> audio_eof_mp3 called <<<");
    Serial.print("eof_mp3 info: "); Serial.println(info);
    playing = false;
    publishMqtt(mqtt_topic_playing, "STOPPED (EOF)"); // Status senden

    if (playingIntro) {
        playingIntro = false;
        Serial.println(">>> Intro playback finished. Ready for PIR. <<<");
        publishMqtt(mqtt_topic_status, "Intro Finished"); // Status senden
    } else if (inPlaybackSession) {
        inPlaybackSession = false;
        playbackStartTime = 0;
        Serial.println(">>> Random file finished. Ready for PIR. <<<");
        publishMqtt(mqtt_topic_status, "Random File Finished"); // Status senden
    } else {
         Serial.println(">>> EOF occurred unexpectedly. <<<");
         publishMqtt(mqtt_topic_debug, "Unexpected EOF"); // Debug senden
    }
     Serial.printf(">>> audio_eof_mp3 end state: playing=%d, playingIntro=%d, inPlaybackSession=%d <<<\n\n",
                  playing, playingIntro, inPlaybackSession);
}

void checkVolumePot() {
    unsigned long currentTime = millis();
    if (currentTime - lastPotReadTime >= potReadInterval) {
        lastPotReadTime = currentTime;
        int rawPotValue = analogRead(POT_PIN);
        if (smoothedPotValue < 0.0) { smoothedPotValue = (float)rawPotValue; }
        else { smoothedPotValue = potSmoothingFactor * (float)rawPotValue + (1.0 - potSmoothingFactor) * smoothedPotValue; }
        int currentVolume = map((int)smoothedPotValue, 0, 4095, 0, 21);

        if (currentVolume != lastVolume) {
            audio.setVolume(currentVolume);
            int oldVolume = lastVolume; // Merken für die Ausgabe
            lastVolume = currentVolume;
            Serial.print("Volume set to: "); Serial.print(currentVolume);
            Serial.print(" (Raw: "); Serial.print(rawPotValue); Serial.print(", Smoothed: "); Serial.print(smoothedPotValue, 1); Serial.println(")");
            publishMqtt(mqtt_topic_volume, String(currentVolume)); // Lautstärke senden
        }
    }
}

void checkButton() {
    int reading = digitalRead(BUTTON_PIN);
    if (reading != lastButtonState) { lastDebounceTime = millis(); }
    if ((millis() - lastDebounceTime) > debounceDelay) {
        if (reading != buttonState) {
            buttonState = reading;
            if (buttonState == LOW) {
                lastPirActivityTime = millis();
                if (isStandby) {
                    isStandby = false;
                    Serial.println("Woke up from Standby (Button)");
                    publishMqtt(mqtt_topic_status, "Woke up from Standby");
                }
                Serial.println("\n--- Button pressed! Changing directory ---");
                publishMqtt(mqtt_topic_status, "Button Pressed"); // Status senden
                if (playing || playingIntro || inPlaybackSession) {
                    audio.stopSong(); playing = false; playingIntro = false; inPlaybackSession = false;
                    Serial.println("Stopped current playback.");
                    publishMqtt(mqtt_topic_playing, "STOPPED (Button)"); // Status senden
                }
                if (!directoryList.empty()) {
                    currentDirectoryIndex = (currentDirectoryIndex + 1) % directoryList.size();
                    String newDirPath = directoryList[currentDirectoryIndex];
                    Serial.println("Selected directory: " + newDirPath);
                    publishMqtt(mqtt_topic_directory, newDirPath, true); // Verzeichnis senden (retained)
                    loadFilesFromCurrentDirectory();
                    String introPath = newDirPath + "/" + introFileName;
                    if (audio.connecttoFS(SD, introPath.c_str())) {
                        playing = true; playingIntro = true;
                        Serial.println("Playing intro: " + introPath);
                        publishMqtt(mqtt_topic_playing, introPath); // Intro senden
                        publishMqtt(mqtt_topic_status, "Playing Intro"); // Status senden
                    } else {
                        Serial.println("intro.mp3 not found/error in " + newDirPath);
                         publishMqtt(mqtt_topic_error, "intro.mp3 not found/error"); // Fehler senden
                        playingIntro = false;
                    }
                    preferences.begin("appState", false);
                    preferences.putInt("dirIndex", currentDirectoryIndex);
                    preferences.end();
                } else {
                    Serial.println("No directories found.");
                    publishMqtt(mqtt_topic_error, "Button pressed, but no directories"); // Fehler senden
                 }
                Serial.println("--- Button action complete ---");
            }
        }
    }
    lastButtonState = reading;
}

