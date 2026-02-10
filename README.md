<div align="center">

# MicMute-S
### Professional-Grade Microphone Control for Windows

<div align="center">

[![Release](https://img.shields.io/github/v/release/suvojeet-sengupta/MicMute?style=for-the-badge&color=0078D6&logo=github)](https://github.com/suvojeet-sengupta/MicMute/releases)
[![Build Status](https://img.shields.io/github/actions/workflow/status/suvojeet-sengupta/MicMute/build.yml?branch=main&style=for-the-badge&logo=github-actions&logoColor=white)](https://github.com/suvojeet-sengupta/MicMute/actions)
[![Platform](https://img.shields.io/badge/Platform-Windows%2010%2F11-0078D6?style=for-the-badge&logo=windows&logoColor=white)](https://microsoft.com)
[![Language](https://img.shields.io/badge/Language-C%2B%2B23-00599C?style=for-the-badge&logo=c%2B%2B&logoColor=white)](https://isocpp.org/)
[![License](https://img.shields.io/github/license/suvojeet-sengupta/MicMute?style=for-the-badge&color=blueviolet)](LICENSE)

**MicMute-S** is a high-performance, low-latency microphone management utility engineered for Windows 10 and 11. It provides a robust privacy layer with system-wide controls, active monitoring, and seamless integration for professional workflows.

[**Explore Releases**](https://github.com/suvojeet-sengupta/MicMute/releases) • [**Report Bug**](https://github.com/suvojeet-sengupta/MicMute/issues) • [**Request Feature**](https://github.com/suvojeet-sengupta/MicMute/issues)

</div>

---

## 🚀 Key Features

### 🛡️ Privacy & Security
- **Global Hardware Mute**: Instantly toggle all audio input devices at the system level using customizable global hotkeys.
- **Persistent Force-Mute**: An active enforcement layer that detects and reverses unauthorized unmute attempts by third-party applications (Zoom, Teams, etc.).
- **Visual Assurance**: High-contrast system tray indicators and unobtrusive OSD (On-Screen Display) provide immediate status confirmation.

### 🎨 Modern Experience
- **Native Windows UI**: Fully optimized for Windows 11 with Mica and Acrylic effects, following Fluent Design guidelines.
- **Real-Time Visualizer**: Integrated low-latency audio level metering for visual feedback of microphone activity.
- **Streamlined Workflow**: A compact, non-intrusive control panel designed for power users and multitaskers.

### 🔌 Advanced Integrations
- **Ozonetel Bridge**: Built-in support for Ozonetel-based CRM systems via a dedicated Chrome extension.
- **Local API Gateway**: A lightweight HTTP server allows for external automation and integration with Stream Decks or other hardware controllers.
- **Direct-to-Disk Recording**: A reliable WAV engine that streams audio directly to storage, ensuring zero data loss during sessions.

## 🛠️ Technical Stack

- **Core**: C++23 (Latest standard for maximum performance and safety)
- **Framework**: Win32 API / WASAPI (Windows Audio Session API)
- **UI Architecture**: GDI+ / Dwmapi (Native hardware-accelerated rendering)
- **Networking**: WinHTTP (Lightweight asynchronous HTTP server)
- **Build System**: MSVC (Microsoft Visual C++ Compiler)

## 📦 Installation

1.  **Download**: Navigate to the [Latest Release](https://github.com/suvojeet-sengupta/MicMute/releases/latest) and download `MicMute-S-Setup.exe`.
2.  **Execute**: Run the installer and follow the guided setup.
3.  **Launch**: Access MicMute-S from the system tray. Use the right-click menu for configuration.

> [!TIP]
> Add MicMute-S to your startup applications via the Settings panel to ensure your privacy is protected from the moment you log in.

## 🔨 Building from Source

### Prerequisites
- **Visual Studio 2022** with the "Desktop development with C++" workload.
- **Inno Setup 6** for generating the distribution installer.
- **Windows SDK** (latest version recommended).

### Compilation
1. Clone the repository:
   ```bash
   git clone https://github.com/suvojeet-sengupta/MicMute.git
   ```
2. Open the solution in Visual Studio or use the provided `build.bat` from a Developer Command Prompt:
   ```cmd
   build.bat
   ```
3. The binary will be available in `build\Release\MicMute-S.exe`.

---

<div align="center">
  Developed with ❤️ by <a href="https://github.com/suvojeet-sengupta">Suvojeet Sengupta</a>
</div>
