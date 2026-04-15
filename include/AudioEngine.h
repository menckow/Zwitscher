#pragma once

#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <vector>
#include "Audio.h"

enum class PlaybackState {
    INITIALIZING,
    IDLE,
    PLAYING_INTRO,
    PLAYING_RANDOM,
    STANDBY
};

class AudioEngine {
private:
    Audio& audio;
    
    // Playback State
    PlaybackState currentState;
    unsigned long playbackStartTime;
    unsigned long lastPirActivityTime;

    // Filesystem Tracking
    std::vector<String> directoryList;
    std::vector<String> currentMp3Files;
    int currentDirectoryIndex;
    String introFileName;

    // Hardware parameters
    unsigned long lastPotReadTime;
    int lastVolume;
    float smoothedPotValue;
    
    int lastButtonState;
    int buttonState;
    unsigned long lastDebounceTime;
    
    void loadFilesFromCurrentDirectory();

public:
    AudioEngine(Audio& audioRef);
    ~AudioEngine() = default;

    void init();
    void update(); // The main loop tick
    
    void findMp3Directories(File dir);
    void playIntro();
    void playRandomTrack();
    void stopPlayback();

    void onAudioEof();
    
    void checkVolumePot();
    void checkButton();
    void checkPirAndTimeout();

    PlaybackState getState() const { return currentState; }
    void setState(PlaybackState state) { currentState = state; }

    unsigned long getLastPirActivityTime() const { return lastPirActivityTime; }
    void updatePirActivity() { lastPirActivityTime = millis(); }
};
