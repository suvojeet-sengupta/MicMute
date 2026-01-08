# MicMute-S

**MicMute-S** is a professional, high-performance Windows application designed to give you total control over your microphone.

Developed by **Suvojeet Sengupta**.

![MicMute-S Badge](https://img.shields.io/badge/MicMute--S-v1.0-blue?style=for-the-badge&logo=windows)

## 🌟 Key Features

*   **Global Mute Control**: Instantly mute/unmute *all* connected microphones with a single click.
*   **System Tray Integration**: Quietly runs in the background. Left-click the tray icon to toggle mute.
*   **Force Mute Mode**: Lock your microphone in a muted state. MicMute-S will automatically re-mute it if any other app tries to unmute.
*   **Startup Friendly**: Option to launch automatically when Windows starts.
*   **Visual Status**: Clear tray icons (Green = Live, Red = Muted) and status text.

## 🚀 Installation

1.  Download the latest `MicMute-S-Setup.exe` from the [Releases](https://github.com/your-repo/releases) page.
2.  Run the installer.
3.  Follow the simple setup wizard.
4.  MicMute-S will launch and appear in your system tray.

## 🛠️ Building from Source

This project uses **GitHub Actions** for automated building.

### Prerequisites (Local Build)
*   Visual Studio 2019 or later with C++ workload.
*   [Inno Setup](https://jrsoftware.org/isinfo.php) (for installer).

### Automated Build
1.  Push changes to `main`.
2.  GitHub Actions will automatically:
    *   Compile the C++ code using MSVC.
    *   Generate the `.exe`.
    *   Build the installer using Inno Setup.
    *   Upload artifacts.

## 📄 License
All rights reserved by **Suvojeet Sengupta**.
