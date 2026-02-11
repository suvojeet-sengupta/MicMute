#pragma once
#include <windows.h>
#include <string>
#include <functional>

namespace AutoUpdater {
    // Check type
    enum class UpdateStatus {
        None,
        Checking,
        UpToDate,
        UpdateAvailable,
        Error
    };

    // Callback types
    using StatusCallback = std::function<void(UpdateStatus status, const std::string& version, const std::string& notes)>;
    using ProgressCallback = std::function<void(int percent)>;

    // Initialize updater
    void Init(HWND hParentWnd);

    // Check for update asynchronously
    // silent: if true, only notify on update available. if false, notify all states (for manual check)
    void CheckForUpdateAsync(bool silent);

    // Perform update
    // If installer: downloads and runs installer (silent)
    // If portable: downloads zip/exe and replaces via batch script
    void PerformUpdate(const std::string& downloadUrl, bool isInstaller, ProgressCallback onProgress);

    // Get latest status
    UpdateStatus GetCurrentStatus();
}
