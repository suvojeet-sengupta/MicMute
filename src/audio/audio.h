#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <wrl/client.h> // Modern ComPtr
#include <string>

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
std::wstring GetMicDeviceName();

// Force a re-scan of input + output devices. Called periodically so the
// speaker meter follows the active default render device after the user
// switches headphones / monitors / Bluetooth output.
void RefreshAudioDevices();
