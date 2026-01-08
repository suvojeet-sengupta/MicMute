#ifndef AUDIO_H
#define AUDIO_H

#include <windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <vector>

void InitializeAudio();
void UninitializeAudio();
bool ToggleMuteAll(); // Returns new state (true=Muted, false=Unmuted)
void SetMuteAll(bool mute);
bool IsAnyMicMuted(); // Returns true if at least the default mic is muted
bool IsDefaultMicMuted();

#endif
