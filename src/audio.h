#ifndef AUDIO_H
#define AUDIO_H

#include <windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <audioclient.h>
#include <vector>

void InitializeAudio();
void UninitializeAudio();
bool ToggleMuteAll();
void SetMuteAll(bool mute);
bool IsAnyMicMuted();
bool IsDefaultMicMuted();
float GetMicLevel();  // Returns 0.0 to 1.0 audio peak level
float GetSpeakerLevel(); // Returns 0.0 to 1.0 speaker peak level

#endif
