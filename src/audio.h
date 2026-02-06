#include <windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <wrl/client.h> // Modern ComPtr

using Microsoft::WRL::ComPtr;

// Function prototypes to avoid circular deps if any
void InitializeAudio();
void UninitializeAudio();
bool ToggleMuteAll();
void SetMuteAll(bool mute);
bool IsAnyMicMuted();
bool IsDefaultMicMuted();
float GetMicLevel();
float GetSpeakerLevel();
