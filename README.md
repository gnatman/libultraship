# libultraship (VR & DirectInput FFB Fork)

[![OpenXR](https://img.shields.io/badge/OpenXR-D3D11-blue.svg)](https://www.khronos.org/openxr/)
[![Target](https://img.shields.io/badge/Target-PSVR%202%20%7C%20SteamVR-purple.svg)]()
[![Force Feedback](https://img.shields.io/badge/FFB-DirectInput-orange.svg)]()
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

**libultraship (LUS)** is a modern C++ library providing high-performance reimplementations of libultra (N64 SDK) functions, asset management (`.otr`/`.o2r`), audio processing, and Fast3D rendering on contemporary hardware.

This fork extends libultraship with:
* **Native OpenXR Virtual Reality:** Full stereoscopic 6DoF rendering via Direct3D 11 (`XR_KHR_D3D11_enable`).
* **Decoupled Quad-Layer HUD:** Routes 2D UI elements to dedicated OpenXR Composition Quad Layers (`XR_TYPE_COMPOSITION_LAYER_QUAD`) via explicit `G_NOOP` markers (`0x4C555301` / `0x4C555302`), preventing distortion and ghosting in 3D space.
* **Base Tracking Space API:** Allows downstream games to anchor the camera origin to a moving vehicle or character in world space while composing 6DoF head tracking on top.
* **Display Refresh Rate Detection:** Automatic query of native HMD refresh rates (90 Hz, 120 Hz) via `XR_FB_display_refresh_rate`.
* **Zero-Overhead Spectator Mirror:** Presenting the left-eye view on the PC monitor without re-rendering or vsync blocking.
* **Mock Pose Harness:** Mouse and keyboard-driven head simulation (`Alt` + mouse, arrow keys) for rapid iteration without wearing a headset.
* **DirectInput Force Feedback:** Native FFB subsystem supporting Direct Drive wheelbases, hardware soft-lock, and synthesized physical forces.
* **100% Upstream Non-VR Compatibility:** Gated behind `-DENABLE_VR=ON`. When built with `ENABLE_VR=OFF` (the default), the library produces binaries identical in behavior to vanilla upstream libultraship.

---

## VR Architecture Overview

The VR subsystem is isolated in `src/vr/` as a peer module to `src/fast/` and `src/ship/`:

```
libultraship/
├── include/
│   ├── fast/
│   │   └── lus_gbi.h       # G_NOOP tag markers (G_NOOP_LUS_VR_HUD_BEGIN / END)
│   └── ship/
│       └── window/Window.h # Public VR APIs: SetVRBaseTrackingSpace, GetVRPose
├── src/
│   ├── vr/                 # VR Runtime module
│   │   ├── VRRuntime.h/.cpp# OpenXR instance, session, swapchains, tracking
│   │   ├── MockVRPose.h/.cpp# Desktop mouse/keyboard head tracking harness
│   │   └── VRToggle.h/.cpp # Runtime VR enablement queries
│   ├── fast/
│   │   ├── Fast3dWindow.cpp# Stereo swapchain acquisition, HUD pass, mirror
│   │   └── interpreter.cpp # G_NOOP marker dispatch, matrix injection
│   └── controller/         # DirectInput FFB device & manager
└── CMakeLists.txt          # -DENABLE_VR=ON/OFF toggle
```

### Public VR APIs
* `Context::GetInstance()->GetWindow()->SetVRBaseTrackingSpace(pos, rot)`: Called by downstream games to anchor tracking to the in-game camera.
* `Ship::VRToggle::IsVREnabled()`: Queries if VR mode is currently active.
* `Ship::VRRuntime::GetInstance()->GetRefreshRate()`: Returns detected HMD refresh rate (e.g. 90.0f, 120.0f).

---

## Contributing
LUS accepts any and all contributions. You can interact with the project via PRs, issues, email (kenixwhisperwind@gmail.com), or [Discord](https://discord.gg/shipofharkinian).
Please see [CONTRIBUTING.md](https://github.com/Kenix3/libultraship/blob/main/CONTRIBUTING.md) for more information.

## Versioning
We use semantic versioning. The API is defined as: every C linkage function, variable, struct, class, public class method, or enum included from `libultraship.h`.

---

## Building

### Prerequisites
Install system dependencies before building.

**Linux (Debian/Ubuntu):**
```bash
sudo apt-get install $(cat .github/workflows/apt-deps.txt)
```

**macOS:**
```bash
brew install $(cat .github/workflows/brew-deps.txt)
```

Then install the required Python packages (needed to generate asset keys):
```bash
pip install -r requirements.txt
```

### Windows (VR & FFB Enabled)

On Windows, LUS uses [vcpkg](https://vcpkg.io) to manage C++ dependencies.

#### Generating a Visual Studio solution (x64 with VR)
```powershell
cmake -S . -B "build/x64" -G "Visual Studio 17 2022" -A x64 -DUSE_AUTO_VCPKG=ON -DENABLE_VR=ON
```

#### Compiling
```powershell
cmake --build .\build\x64 --config Release
```

#### Non-VR Vanilla Build (Default)
```powershell
cmake -S . -B "build/x64" -G "Visual Studio 17 2022" -A x64 -DUSE_AUTO_VCPKG=ON -DENABLE_VR=OFF
cmake --build .\build\x64 --config Release
```

---

### Linux / macOS (Non-VR)
```bash
cmake -H. -Bbuild
cmake --build build
```

---

## Sponsors & Third-Party Libraries
Thank you to JetBrains for providing CLion licenses to open source contributors.

LUS makes use of the following third-party libraries:
* **OpenXR-SDK** (Apache 2.0) OpenXR loader and header definitions.
* **Fast3D** (MIT) Display list rendering.
* **ImGui** (MIT) Graphical user interface.
* **SDL2** (zlib) Windowing, event management, and input.
* **DirectInput** (Microsoft DirectX SDK) Force Feedback wheel communications.
* **StormLib** & **libzip** (MIT / BSD) Archive reading and extraction.
* **spdlog** & **{fmt}** (MIT) High-performance logging and text formatting.
* **nlohmann-json** (MIT) Configuration serialization and persistence.