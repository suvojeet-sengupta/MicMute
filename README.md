<div align="center">

# MicMute-S
### The Ultimate Microphone Control Utility for Windows

[![Release](https://img.shields.io/github/v/release/suvojeet-sengupta/MicMute?style=for-the-badge&color=0078D6&logo=github)](https://github.com/suvojeet-sengupta/MicMute/releases)
[![Build Status](https://img.shields.io/github/actions/workflow/status/suvojeet-sengupta/MicMute/build.yml?branch=main&style=for-the-badge&logo=github-actions&logoColor=white)](https://github.com/suvojeet-sengupta/MicMute/actions)
[![Platform](https://img.shields.io/badge/Platform-Windows%2010%2F11-0078D6?style=for-the-badge&logo=windows&logoColor=white)](https://microsoft.com)
[![License](https://img.shields.io/github/license/suvojeet-sengupta/MicMute?style=for-the-badge&color=blueviolet)](LICENSE)
[![Downloads](https://img.shields.io/github/downloads/suvojeet-sengupta/MicMute/total?style=for-the-badge&color=success&logo=github)](https://github.com/suvojeet-sengupta/MicMute/releases)

<br/>

**MicMute-S** provides system-wide microphone control with a modern, high-performance interface. Designed for gamers, streamers, and professionals who need absolute audio privacy.

[Download Latest Version](https://github.com/suvojeet-sengupta/MicMute/releases/latest) • [Report Bug](https://github.com/suvojeet-sengupta/MicMute/issues) • [Request Feature](https://github.com/suvojeet-sengupta/MicMute/issues)

</div>

---

## Key Features

<table>
  <tr>
    <td width="50%">
      <h3>Global Mute Control</h3>
      <p>Instantly mute or unmute all connected microphones with a single click or hotkey. Works system-wide across all applications.</p>
    </td>
    <td width="50%">
      <h3>Force Mute Protection</h3>
      <p>Lock your microphone in a muted state. MicMute-S actively monitors and re-mutes your device if any other application attempts to unmute it.</p>
    </td>
  </tr>
  <tr>
    <td width="50%">
      <h3>Modern UI with Mica</h3>
      <p>Features a sleek, Windows 11-native design with Mica / Acrylic transparency effects and a dark mode interface.</p>
    </td>
    <td width="50%">
      <h3>Integrated Audio Meter</h3>
      <p>Real-time visual feedback of your microphone input levels, ensuring you know exactly when you're transmitting.</p>
    </td>
  </tr>
  <tr>
    <td width="50%">
      <h3>Built-in Recorder</h3>
      <p>Simple and efficient audio recorder included, allowing you to test your settings or record quick notes.</p>
    </td>
    <td width="50%">
      <h3>Smart Notifications</h3>
      <p>Receive unobtrusive Toast notifications and Tray icon changes (Green/Red) to instantly know your status.</p>
    </td>
  </tr>
</table>

## Technology Stack

![C++](https://img.shields.io/badge/C++17-00599C?style=for-the-badge&logo=c%2B%2B&logoColor=white)
![Win32 API](https://img.shields.io/badge/Win32_API-0078D6?style=for-the-badge&logo=windows&logoColor=white)
![Inno Setup](https://img.shields.io/badge/Inno_Setup-orange?style=for-the-badge&logo=windows-terminal&logoColor=white)
![GitHub Actions](https://img.shields.io/badge/GitHub_Actions-2088FF?style=for-the-badge&logo=github-actions&logoColor=white)

## Installation

1.  **Download**: Get the latest `MicMute-S-Setup.exe` from the [Releases Page](https://github.com/suvojeet-sengupta/MicMute/releases).
2.  **Install**: Run the installer and follow the on-screen instructions.
3.  **Launch**: MicMute-S will start automatically and minimize to the system tray.

> **Note**: Windows SmartScreen may appear since the binary is not signed. You can safely click "Run Anyway".

## Usage

*   **Toggle Mute**: Click the tray icon or use the configurable global hotkey.
*   **Settings**: Right-click the tray icon to access:
    *   **General**: Startup behavior, Overlay settings.
    *   **Hotkeys**: Customize your mute toggle shortcuts.
    *   **Audio**: Select specific devices or test input.
    *   **Appearance**: Custom themes and UI preferences.

## Building from Source

To build MicMute-S locally, you need the following prerequisites:

*   **Visual Studio 2019/2022** with C++ Desktop Development workload.
*   **Inno Setup 6** (for building the installer).

### Steps (Local)

1.  Clone the repository:
    ```bash
    git clone https://github.com/suvojeet-sengupta/MicMute.git
    ```
2.  Open the project solution in Visual Studio.
3.  Select **Release** configuration and **x64** platform.
4.  Build the solution (`Ctrl+Shift+B`).
5.  (Optional) Run `installer.iss` with Inno Setup Compiler to generate the setup file.

### Steps (Automated)

This repository aims to use GitHub Actions for CI/CD. Pushing to the `main` branch triggers the build workflow found in `.github/workflows/build.yml`.

## Contributing

Contributions are welcome! Please feel free to verify existing issues or create a new one.

1.  Fork the Project
2.  Create your Feature Branch (`git checkout -b feature/AmazingFeature`)
3.  Commit your Changes (`git commit -m 'Add some AmazingFeature'`)
4.  Push to the Branch (`git push origin feature/AmazingFeature`)
5.  Open a Pull Request

## License

Distributed under the MIT License. See `LICENSE` for more information.

---

<div align="center">
  <p>Developed with ❤️ by <a href="https://github.com/suvojeet-sengupta">Suvojeet Sengupta</a></p>
</div>
