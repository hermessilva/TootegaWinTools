<h1 align="center">🎨 TootegaWinMFCLib</h1>

<p align="center">
  <strong>MFC-Based UI Framework Library for Windows Desktop Applications</strong>
</p>

<p align="center">
  <a href="#features">Features</a> •
  <a href="#architecture">Architecture</a> •
  <a href="#modules">Modules</a> •
  <a href="#usage">Usage</a> •
  <a href="#building">Building</a> •
  <a href="#license">License</a>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Platform-Windows%2010%2F11-blue?style=flat-square" alt="Platform">
  <img src="https://img.shields.io/badge/Architecture-x64-green?style=flat-square" alt="Architecture">
  <img src="https://img.shields.io/badge/C%2B%2B-17%2F20-orange?style=flat-square" alt="C++ Standard">
  <img src="https://img.shields.io/badge/Framework-MFC-purple?style=flat-square" alt="Framework">
  <img src="https://img.shields.io/badge/License-Proprietary-red?style=flat-square" alt="License">
</p>

---

## Overview

**TootegaWinMFCLib** is a static library that extends [TootegaWinLib](../TootegaWinLib/README.md) with MFC-specific components for building rich Windows desktop applications. It provides a complete framework for screen capture, video recording, video editing, and multimedia processing using the MFC Document/View architecture and Windows Media Foundation APIs.

This library is **Windows-exclusive** and **MFC-dependent**, designed for applications requiring native Windows UI controls, MDI (Multiple Document Interface) support, and hardware-accelerated multimedia capabilities.

---

## Features

### 🎯 Core Design Principles

| Principle | Description |
|-----------|-------------|
| **MFC Document/View** | Full support for MDI architecture with custom document and view classes |
| **Media Foundation** | Hardware-accelerated video capture and encoding via Windows MF APIs |
| **RAII Resources** | Automatic cleanup for GDI objects, COM interfaces, and Media Foundation resources |
| **Static Linking** | Designed for static linking with MFC to produce self-contained binaries |
| **Modern C++** | Uses C++17/20 features while maintaining MFC compatibility |

### 🔧 What It Provides

- **Application Framework** — MDI application skeleton with menu bar, status bar, and window management
- **Screen Capture** — Real-time window capture with preview and region selection
- **Video Recording** — H.264/MP4 recording via Media Foundation Sink Writer
- **Video Editing** — Frame-accurate video editing with mark-in/out, export, and cropping
- **Window Enumeration** — List and filter capturable windows with icons and metadata
- **Thumbnail Strip** — Visual timeline control for video navigation
- **Preview Panel** — Live preview with resizable selection rectangle

---

## Architecture

```
TootegaWinMFCLib/
├── Include/
│   ├── TootegaWinMFCLib.h       # Master include (includes all)
│   │
│   ├── framework.h              # MFC/SDK precompiled headers
│   ├── pch.h                    # Precompiled header
│   ├── Resource.h               # Resource identifiers
│   │
│   │── Application Framework ───
│   ├── XApplication.h           # CWinAppEx-derived application class
│   ├── XMainFrame.h             # CMDIFrameWndEx-derived main frame
│   ├── XChildFrame.h            # CMDIChildWndEx-derived child frame
│   │
│   │── Screen Capture Module ───
│   ├── XCaptureDocument.h       # Document for capture sessions
│   ├── XCaptureView.h           # View with capture controls and preview
│   ├── XWindowEnumerator.h      # Window enumeration utility
│   ├── XPreviewPanel.h          # Live preview with selection tracker
│   ├── XVideoRecorder.h         # Media Foundation video encoder
│   │
│   │── Video Editor Module ─────
│   ├── XVideoEditorDocument.h   # Document for video files
│   ├── XVideoEditorView.h       # View with editing controls
│   ├── XVideoEditorFrame.h      # Child frame for video editor
│   └── XThumbnailStrip.h        # Visual timeline thumbnail strip
│
└── Source/
    ├── pch.cpp                  # Precompiled header generation
    ├── XApplication.cpp
    ├── XMainFrame.cpp
    ├── XChildFrame.cpp
    ├── XCaptureDocument.cpp
    ├── XCaptureView.cpp
    ├── XWindowEnumerator.cpp
    ├── XPreviewPanel.cpp
    ├── XVideoRecorder.cpp
    ├── XVideoEditorDocument.cpp
    ├── XVideoEditorView.cpp
    ├── XVideoEditorFrame.cpp
    └── XThumbnailStrip.cpp
```

---

## Modules

### 🖥️ XApplication — Application Framework

The `XApplication` class provides the MFC application skeleton:

```cpp
#include <TootegaWinMFCLib.h>

class XApplication : public CWinAppEx
{
public:
    virtual BOOL InitInstance() override;
    virtual int ExitInstance() override;

private:
    void InitializeMF();   // Initialize Media Foundation
    void ShutdownMF();     // Cleanup Media Foundation

    CMultiDocTemplate* _CaptureTemplate;       // Screen capture documents
    CMultiDocTemplate* _VideoEditorTemplate;   // Video editor documents
};
```

**Features:**
- Multi-document template registration for capture and video editing
- Media Foundation initialization/shutdown
- Command handlers for File > New Capture and File > Open Video

---

### 📷 XCaptureView — Screen Capture Interface

The `XCaptureView` provides a complete screen capture UI:

```cpp
#include <TootegaWinMFCLib.h>

// Create a new capture session
auto view = GetActiveView();  // XCaptureView*
view->OnBtnStartClicked();    // Start recording
view->OnBtnStopClicked();     // Stop recording
```

**UI Components:**
| Control | Description |
|---------|-------------|
| `CComboBox _ComboWindows` | Window selection dropdown |
| `CButton _BtnRefresh` | Refresh window list |
| `CEdit _EditFPS` | Frame rate input (1-60 FPS) |
| `CButton _CheckGrayscale` | Grayscale conversion toggle |
| `XPreviewPanel _PreviewPanel` | Live preview with selection |
| `CStatic _LabelRecordTime` | Recording duration display |
| `CStatic _LabelAvgFPS` | Real-time FPS monitor |

---

### 🎬 XVideoRecorder — Media Foundation Encoder

Hardware-accelerated video recording using Media Foundation:

```cpp
#include <TootegaWinMFCLib.h>

XVideoRecorder recorder;

// Start recording
bool success = recorder.Start(
    L"C:\\Videos\\capture.mp4",  // Output file path
    hwndSource,                   // Source window handle
    captureRect,                  // Capture region
    30,                           // FPS (1-60)
    false                         // Grayscale conversion
);

// Check status
if (recorder.IsRecording())
    recorder.Stop();

// Error handling
if (!success)
    CString error = recorder.GetLastError();
```

**Encoding Settings:**
| Parameter | Value |
|-----------|-------|
| Codec | H.264 (MPEG-4 AVC) |
| Container | MP4 |
| Bitrate | Auto-calculated based on resolution |
| Color Format | NV12 (hardware) or RGB32 (software) |

---

### 📹 XVideoEditorDocument — Video File Handler

Document class for loading and manipulating video files:

```cpp
#include <TootegaWinMFCLib.h>

// Video information structure
struct XVideoInfo
{
    UINT64  Duration;       // Duration in 100-nanosecond units
    UINT32  Width;          // Frame width
    UINT32  Height;         // Frame height
    double  FrameRate;      // Frames per second
    UINT64  TotalFrames;    // Total frame count
    UINT32  Bitrate;        // Video bitrate
    GUID    VideoFormat;    // Video codec GUID
    GUID    AudioFormat;    // Audio codec GUID
    bool    HasAudio;       // Audio track present
};

// Document usage
auto doc = GetDocument();  // XVideoEditorDocument*

// Get video metadata
const XVideoInfo& info = doc->GetVideoInfo();

// Frame navigation
LONGLONG position = doc->FrameToPosition(frameNumber);
UINT64 frame = doc->PositionToFrame(position);

// Frame extraction
CBitmap bitmap;
HRESULT hr = doc->GetFrameBitmap(position, bitmap);

// Mark in/out points
doc->SetMarkIn(startPosition);
doc->SetMarkOut(endPosition);

// Export trimmed video
hr = doc->ExportRange(
    L"output.mp4",
    doc->GetMarkIn(),
    doc->GetMarkOut(),
    [](const XExportProgress& progress) {
        UpdateProgress(progress.CurrentFrame, progress.TotalFrames);
        return !cancelRequested;  // Return false to cancel
    }
);

// Export with cropping
hr = doc->ExportRangeWithCrop(
    L"output.mp4",
    markIn, markOut,
    cropRect,
    progressCallback
);
```

---

### 🎞️ XThumbnailStrip — Visual Timeline

Custom control for video timeline visualization:

```cpp
#include <TootegaWinMFCLib.h>

XThumbnailStrip strip;

// Configuration
strip.SetFrameCount(10);           // Number of thumbnails
strip.SetTotalFrames(1000);        // Total video frames
strip.SetCurrentFrame(500);        // Playhead position

// Mark points (visual indicators)
strip.SetMarkIn(100);
strip.SetMarkOut(900);

// Thumbnail management
strip.SetThumbnailBitmap(0, hBitmap);
strip.RegenerateThumbnails();

// Hit testing
int index = strip.HitTest(point);
if (index >= 0)
    NavigateToThumbnail(index);

// Message handling
// WM_THUMBNAILCLICKED sent when user clicks a thumbnail
```

---

### 🔍 XPreviewPanel — Live Preview Control

Real-time window preview with selection rectangle:

```cpp
#include <TootegaWinMFCLib.h>

XPreviewPanel preview;
preview.Create(WS_CHILD | WS_VISIBLE, rect, parentWnd, IDC_PREVIEW);

// Set capture source
preview.SetSourceWindow(hwndTarget);
preview.RefreshCapture();

// Selection rectangle
preview.SetTrackerVisible(true);
preview.SetSelectionRect(initialRect);
CRect selection = preview.GetScaledSelectionToSource();
```

**Features:**
- Automatic scaling to fit panel size
- `CRectTracker` integration for resizable selection
- Double-buffered painting for smooth updates

---

### 🔎 XWindowEnumerator — Window Discovery

Enumerate windows suitable for capture:

```cpp
#include <TootegaWinMFCLib.h>

XWindowEnumerator enumerator;
enumerator.Refresh();

for (const XWindowInfo& info : enumerator.GetWindows())
{
    // info.Handle     - HWND
    // info.Title      - Window title
    // info.ClassName  - Window class name
    // info.ProcessID  - Owner process ID
    // info.Icon       - Window icon (HICON)
}

// Static utilities
bool valid = XWindowEnumerator::IsValidCaptureWindow(hwnd);
CString title = XWindowEnumerator::GetWindowTitle(hwnd);
HICON icon = XWindowEnumerator::GetWindowIcon(hwnd);
```

---

## Usage

### Basic Integration

Include the master header in your application:

```cpp
// In your pch.h or framework.h
#include "TootegaWinLib.h"     // Core library (required dependency)
#include "TootegaWinMFCLib.h"  // MFC components
```

### Project Configuration

Add to your `.vcxproj`:

```xml
<ItemDefinitionGroup>
  <ClCompile>
    <AdditionalIncludeDirectories>
      $(ProjectDir)..\TootegaWinMFCLib\Include;
      $(ProjectDir)..\TootegaWinLib\Include;
      %(AdditionalIncludeDirectories)
    </AdditionalIncludeDirectories>
  </ClCompile>
  <Link>
    <AdditionalDependencies>
      TootegaWinMFCLib.lib;
      TootegaWinLib.lib;
      mfplat.lib;mfreadwrite.lib;mfuuid.lib;
      d3d11.lib;dxgi.lib;dwmapi.lib;
      %(AdditionalDependencies)
    </AdditionalDependencies>
  </Link>
</ItemDefinitionGroup>
```

---

## Building

### Requirements

| Component | Minimum Version | Recommended |
|-----------|-----------------|-------------|
| **Visual Studio** | 2022 (17.0) | 2022 (17.9+) |
| **Windows SDK** | 10.0.19041.0 | 10.0.22621.0 |
| **C++ Toolset** | v143 | v143 |
| **MFC** | Static Library | Static Library |

### Build Commands

```powershell
# Build Release
MSBuild TootegaWinMFCLib.vcxproj /p:Configuration=Release /p:Platform=x64

# Build Debug
MSBuild TootegaWinMFCLib.vcxproj /p:Configuration=Debug /p:Platform=x64
```

### Output

| Configuration | Output Path |
|---------------|-------------|
| Debug | `x64\Debug\TootegaWinMFCLib.lib` |
| Release | `x64\Release\TootegaWinMFCLib.lib` |

---

## Dependencies

### Required Libraries

| Library | Purpose |
|---------|---------|
| **TootegaWinLib** | Core utilities, error handling, logging |
| **MFC** | Application framework (static) |
| **ATL** | COM support for Media Foundation |

### Windows SDK Components

| Component | Purpose |
|-----------|---------|
| **Media Foundation** | Video capture and encoding |
| **Direct3D 11** | Hardware-accelerated video processing |
| **DXGI** | Desktop duplication (fallback capture) |
| **DWM** | Desktop Window Manager integration |

### Link Dependencies

```
mfplat.lib      # Media Foundation Platform
mfreadwrite.lib # Source Reader / Sink Writer
mfuuid.lib      # Media Foundation GUIDs
d3d11.lib       # Direct3D 11
dxgi.lib        # DXGI
dwmapi.lib      # DWM API
```

---

## License

**Proprietary** — © 1999-2026 Tootega Pesquisa e Inovação. All rights reserved.

This library is part of the Tootega Windows Tools suite and is not available for public distribution.

---

## See Also

- [TootegaWinLib](../TootegaWinLib/README.md) — Foundation library (required dependency)
- [TootegaVideoTool](../TootegaVideoTool/README.md) — Application using this library

---

<p align="center">
  Made with ❤️ by <a href="https://tootega.com.br">Tootega</a>
</p>

<p align="center">
  <sub>Copyright © 1999-2026 Tootega Pesquisa e Inovação. All rights reserved.</sub>
</p>
