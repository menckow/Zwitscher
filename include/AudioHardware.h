#ifndef AUDIO_HARDWARE_H
#define AUDIO_HARDWARE_H

#include <Arduino.h>
#include <FS.h>
#include <vector>

void findMp3Directories(File dir);
void loadFilesFromCurrentDirectory();
void audio_eof_mp3(const char *info);
void checkVolumePot();
void checkButton();

#endif
