<h1 align="center">� TootegaVideoTool</h1>

<p align="center">
  <strong>Professional Screen Capture and Video Editing Application for Windows</strong>
</p>

<p align="center">
  <a href="#features">Features</a> •
  <a href="#screenshots">Screenshots</a> •
  <a href="#usage">Usage</a> •
  <a href="#building">Building</a> •
  <a href="#license">License</a>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Platform-Windows%2010%2F11-blue?style=flat-square" alt="Platform">
  <img src="https://img.shields.io/badge/Architecture-x64-green?style=flat-square" alt="Architecture">
  <img src="https://img.shields.io/badge/C%2B%2B-20-orange?style=flat-square" alt="C++ Standard">
  <img src="https://img.shields.io/badge/Framework-MFC-purple?style=flat-square" alt="Framework">
  <img src="https://img.shields.io/badge/License-Proprietary-red?style=flat-square" alt="License">
</p>

---

## Overview

**TootegaVideoTool** is a professional-grade Windows desktop application for screen capture and video editing. Built on the MFC framework with Windows Media Foundation for hardware-accelerated video processing, it provides a complete workflow from window capture to final video export.

The application uses a MDI (Multiple Document Interface) architecture, allowing users to work with multiple capture sessions and video editing projects simultaneously.

---

## Features

### 🎬 Screen Capture

| Feature | Description |
|---------|-------------|
| **Window Selection** | Enumerate and select any visible window for capture |
| **Live Preview** | Real-time preview of the selected window |
| **Region Selection** | Drag-to-select capture area within the window |
| **Configurable FPS** | 1-60 frames per second recording |
| **Grayscale Mode** | Optional grayscale conversion for reduced file size |
| **H.264 Encoding** | Hardware-accelerated MP4 output via Media Foundation |

### ✂️ Video Editing

| Feature | Description |
|---------|-------------|
| **Frame-Accurate Navigation** | Scrub through video frame-by-frame |
| **Visual Timeline** | Thumbnail strip for quick navigation |
| **Mark In/Out** | Set precise trim points |
| **Video Export** | Export trimmed segments with re-encoding |
| **Crop Support** | Define crop region during export |
| **Frame Extraction** | Save individual frames as images |
| **Audio Toggle** | Include or exclude audio track |

### 🖥️ User Interface

| Feature | Description |
|---------|-------------|
| **MDI Architecture** | Multiple simultaneous documents |
| **Modern Ribbon-style** | MFC Feature Pack controls |
| **Status Indicators** | Recording time, FPS, file size |
| **Keyboard Shortcuts** | Efficient workflow navigation |
| **Export Progress** | Real-time progress with ETA and cancel |

---

## Application Modules

### 📷 Screen Capture Module

Create a new capture session via **File > New Capture** (or toolbar button):

1. **Select Window** — Choose from the dropdown list of available windows
2. **Refresh** — Update the window list to find new windows
3. **Preview** — View the selected window in real-time
4. **Select Region** — Drag to define the capture area (optional)
5. **Configure** — Set FPS and grayscale options
6. **Record** — Click Start to begin recording, Stop to finish

**Output:**
- Format: MP4 (H.264 + AAC)
- Location: User-specified path
- Naming: Automatic timestamp-based naming available

### 🎞️ Video Editor Module

Open a video file via **File > Open Video**:

1. **Navigate** — Use slider, thumbnails, or frame input
2. **Mark Range** — Set In/Out points for the segment to keep
3. **Preview** — Play the marked range
4. **Crop** — Enable crop mode and select region (optional)
5. **Export** — Save the trimmed (and optionally cropped) video

**Supported Formats (Input):**
- MP4 (H.264, H.265)
- AVI (various codecs via Media Foundation)
- WMV, MOV (with appropriate codecs)

**Export Format:**
- MP4 (H.264 re-encoded)
- Maintains original resolution (unless cropped)

---

## Architecture

```
TootegaVideoTool/
├── Include/
│   ├── framework.h              # MFC/SDK includes, library references
│   ├── pch.h                    # Precompiled header
│   └── Resource.h               # Resource identifiers
│
├── Source/
│   └── pch.cpp                  # Precompiled header generation
│
├── Resource/
│   └── TootelaUITools.rc        # Resources (menus, dialogs, icons)
│
└── TootegaVideoTool.vcxproj     # Visual Studio project file
```

### Resource Identifiers

| Category | IDs | Description |
|----------|-----|-------------|
| **Menus** | `IDR_MAINFRAME`, `IDR_CAPTURETYPE`, `IDR_VIDEOEDITORTYPE` | Application and document menus |
| **Dialogs** | `IDD_ABOUTBOX`, `IDD_EXPORT_PROGRESS` | Modal dialogs |
| **Capture Controls** | `IDC_COMBO_WINDOWS` through `IDC_PREVIEW_PANEL` | Capture view controls |
| **Editor Controls** | `IDC_EDIT_FRAMES` through `IDC_CHECK_CROP` | Video editor controls |
| **Commands** | `ID_WINDOW_MANAGER`, `ID_FILE_OPEN_VIDEO` | Menu commands |

---

## Keyboard Shortcuts

### Global

| Shortcut | Action |
|----------|--------|
| `Ctrl+N` | New Capture Session |
| `Ctrl+O` | Open Video File |
| `Ctrl+S` | Save (context-dependent) |
| `F1` | Help / About |

### Capture View

| Shortcut | Action |
|----------|--------|
| `F5` | Refresh Window List |
| `F9` | Start Recording |
| `F10` | Stop Recording |

### Video Editor

| Shortcut | Action |
|----------|--------|
| `Space` | Play / Pause |
| `I` | Set Mark In |
| `O` | Set Mark Out |
| `Home` | Go to Start |
| `End` | Go to End |
| `Left Arrow` | Previous Frame |
| `Right Arrow` | Next Frame |
| `Ctrl+E` | Export Selection |
| `Ctrl+Shift+S` | Save Current Frame |

---

## Building

### Prerequisites

| Component | Minimum Version | Recommended |
|-----------|-----------------|-------------|
| **Operating System** | Windows 10 (1903) | Windows 11 23H2 |
| **Visual Studio** | 2022 (17.0) | 2022 (17.9+) |
| **Windows SDK** | 10.0.19041.0 | 10.0.22621.0 |
| **C++ Toolset** | v143 | v143 |
| **MFC** | Static Library | Static Library |

### Dependencies

This application requires the following Tootega libraries:

| Library | Purpose |
|---------|---------|
| **TootegaWinLib** | Core utilities, logging, error handling |
| **TootegaWinMFCLib** | MFC components, video capture/editing |

### Build Commands

```powershell
# Build all dependencies and application (Release)
MSBuild ..\TootegaWinTools.slnx /p:Configuration=Release /p:Platform=x64

# Build application only (after dependencies are built)
MSBuild TootegaVideoTool.vcxproj /p:Configuration=Release /p:Platform=x64

# Build Debug
MSBuild TootegaVideoTool.vcxproj /p:Configuration=Debug /p:Platform=x64
```

### Build Order

The solution builds projects in dependency order:

1. `TootegaWinLib` — Core library
2. `TootegaWinMFCLib` — MFC components
3. `TootegaVideoTool` — Application executable

### Output

| Configuration | Output Path | Executable |
|---------------|-------------|------------|
| Debug | `x64\Debug\` | `TootelaUITools.exe` |
| Release | `x64\Release\` | `TootelaUITools.exe` |

---

## Runtime Requirements

| Requirement | Notes |
|-------------|-------|
| **Operating System** | Windows 10 version 1903 or later |
| **Architecture** | x64 only |
| **VC++ Runtime** | Not required (static CRT linking) |
| **Hardware** | GPU with DirectX 11 support recommended |
| **Disk Space** | ~50 MB for application + space for recordings |

### Windows Components Required

- **Media Foundation** — Built into Windows 10/11
- **DirectX 11** — GPU acceleration for video encoding
- **Desktop Window Manager** — For window capture

---

## Configuration

### Video Encoding Settings

The application uses optimized encoding settings:

| Setting | Value |
|---------|-------|
| **Video Codec** | H.264 (MPEG-4 AVC) |
| **Audio Codec** | AAC-LC |
| **Container** | MP4 |
| **Video Bitrate** | Auto (based on resolution × FPS) |
| **Audio Bitrate** | 128 kbps (when audio enabled) |
| **Profile** | Main |
| **Level** | Auto |

### Capture Defaults

| Setting | Default Value |
|---------|---------------|
| **FPS** | 30 |
| **Grayscale** | Off |
| **Capture Area** | Full window |

---

## Troubleshooting

### Common Issues

| Issue | Solution |
|-------|----------|
| **Window not listed** | Click Refresh; ensure window is visible and not minimized |
| **Recording fails to start** | Check disk space; ensure output path is writable |
| **Low FPS during recording** | Reduce capture resolution; disable other GPU-intensive apps |
| **Export fails** | Check disk space; ensure source file is not corrupted |
| **Black preview** | Some windows (e.g., DirectX games) may require admin rights |

### Hardware Acceleration

If hardware encoding is unavailable, the application falls back to software encoding. For best performance:

- Update GPU drivers
- Ensure DirectX 11 is functional
- Close other GPU-intensive applications during recording

---

## Technical Details

### Media Foundation Pipeline

```
┌─────────────┐    ┌──────────────┐    ┌─────────────┐
│   Window    │───▶│ GDI Capture  │───▶│ Color Conv. │
│   (HWND)    │    │  (BitBlt)    │    │ (RGB→NV12)  │
└─────────────┘    └──────────────┘    └─────────────┘
                                              │
                                              ▼
┌─────────────┐    ┌──────────────┐    ┌─────────────┐
│  MP4 File   │◀───│ Sink Writer  │◀───│ H.264 Enc.  │
│             │    │ (Container)  │    │ (MFT)       │
└─────────────┘    └──────────────┘    └─────────────┘
```

### Threading Model

| Thread | Purpose |
|--------|---------|
| **UI Thread** | MFC message loop, user interaction |
| **Capture Thread** | Window capture, frame queuing |
| **Encode Thread** | Media Foundation encoding (internal) |

---

## License

**Proprietary** — © 1999-2026 Tootega Pesquisa e Inovação. All rights reserved.

This application is part of the Tootega Windows Tools suite and is not available for public distribution.

---

## See Also

- [TootegaWinLib](../TootegaWinLib/README.md) — Core utility library
- [TootegaWinMFCLib](../TootegaWinMFCLib/README.md) — MFC components library
- [Main README](../README.md) — Project overview

---

<p align="center">
  Made with ❤️ by <a href="https://tootega.com.br">Tootega</a>
</p>

<p align="center">
  <sub>Copyright © 1999-2026 Tootega Pesquisa e Inovação. All rights reserved.</sub>
</p>
