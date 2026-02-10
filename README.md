<div align="center">

# MicMute-S
### Professional Microphone Control Utility for Windows

[![Release](https://img.shields.io/github/v/release/suvojeet-sengupta/MicMute?style=flat-square&color=0078D6&logo=github)](https://github.com/suvojeet-sengupta/MicMute/releases)
[![Build Status](https://img.shields.io/github/actions/workflow/status/suvojeet-sengupta/MicMute/build.yml?branch=main&style=flat-square&logo=github-actions&logoColor=white)](https://github.com/suvojeet-sengupta/MicMute/actions)
[![Platform](https://img.shields.io/badge/Platform-Windows%2010%2F11-0078D6?style=flat-square&logo=windows&logoColor=white)](https://microsoft.com)
[![License](https://img.shields.io/github/license/suvojeet-sengupta/MicMute?style=flat-square&color=blueviolet)](LICENSE)
[![Downloads](https://img.shields.io/github/downloads/suvojeet-sengupta/MicMute/total?style=flat-square&color=success&logo=github)](https://github.com/suvojeet-sengupta/MicMute/releases)

MicMute-S is a high-performance system-wide microphone control utility designed for Windows 10 and 11. Built with modern UI standards and a focus on privacy, it provides seamless audio management for professionals, streamers, and privacy-conscious users.

[Download Latest Release](https://github.com/suvojeet-sengupta/MicMute/releases/latest) | [Documentation](#usage) | [Build Instructions](#building-from-source)

</div>

---

## Core Capabilities

### Professional Audio Privacy
*   **System-Wide Control**: Toggle all connected microphones with a single global hotkey or tray interaction.
*   **Force Mute Protection**: Active monitoring layer that prevents other applications from unmuting your hardware.
*   **Visual Confidence**: Real-time tray indicators (Red/Green) and unobtrusive toast notifications ensure you always know your status.

### Modern Interface
*   **Windows Native Design**: Leverages Mica and Acrylic transparency effects for a premium look and feel.
*   **Dynamic Audio Metering**: Integrated real-time visualizer for input levels.
*   **Compact UI**: Refactored horizontal control panel for streamlined workspace impact.

### Enterprise Integration
*   **Ozonetel Bridge**: Dedicated Chrome extension integration for automatic call recording and status synchronization.
*   **Local HTTP API**: Lightweight server for external control and status polling.
*   **Streaming WAV Engine**: High-reliability recording system that writes directly to disk to prevent data loss.

## Technology Stack

| Component | Technology | Badge |
| :--- | :--- | :--- |
| **Language** | C++17 | ![C++](https://img.shields.io/badge/C++17-00599C?style=flat-square&logo=c%2B%2B&logoColor=white) |
| **API** | Win32 / WASAPI | ![Win32](https://img.shields.io/badge/Win32_API-0078D6?style=flat-square&logo=windows&logoColor=white) |
| **Installer** | Inno Setup | ![Inno](https://img.shields.io/badge/Inno_Setup-orange?style=flat-square&logo=windows-terminal&logoColor=white) |
| **Automation** | GitHub Actions | ![Actions](https://img.shields.io/badge/GitHub_Actions-2088FF?style=flat-square&logo=github-actions&logoColor=white) |

## Installation

1.  **Download**: Obtain the latest `MicMute-S-Setup.exe` from the [Releases](https://github.com/suvojeet-sengupta/MicMute/releases).
2.  **Install**: Follow the standard Inno Setup installation wizard.
3.  **Deploy**: The application runs from the system tray and starts automatically with Windows (optional).

> [!NOTE]
> As the binary is currently self-signed, Windows SmartScreen may present a warning. Select "More Info" -> "Run Anyway" to proceed.

## Building from Source

### Prerequisites
*   **Visual Studio 2022** (Desktop Development with C++)
*   **Inno Setup 6** (for installer generation)
*   **Python 3.x** (for icon generation scripts)

### Local Development Flow
1.  Clone the repository:
    ```bash
    git clone https://github.com/suvojeet-sengupta/MicMute.git
    ```
2.  Open `MicMute.sln` in Visual Studio.
3.  Build configuration: **Release** | **x64**.
4.  Execute build (`Ctrl+Shift+B`).
5.  Generate installer using `installer.iss`.

## Extension Ecosystem
MicMute-S includes a specialized Chrome extension located in `/extension` for integration with Ozonetel-based CRM systems. It communicates via a local HTTP bridge at `localhost:9876` for seamless state management.

---

<div align="center">
  Developed by <a href="https://github.com/suvojeet-sengupta">Suvojeet Sengupta</a>
</div>
