# VR Runtime Architecture (libultraship)

## Overview
Reusable VR infrastructure within libultraship to support stereoscopic rendering and head tracking via OpenXR.

## Core Components

### 1. VRSession
`src/vr/VRSession.{h,cpp}`
- Manages `XrInstance`, `XrSession`, and reference `XrSpace`.
- Handles lifecycle (init, loss, resume).
- Owns per-eye `XrSwapchain`.

### 2. VRSwapchain
`src/vr/VRSwapchain.{h,cpp}`
- Wraps XR swapchain images.
- Caches D3D11 RTV/DSV for direct rendering.

### 3. VRInput
`src/vr/VRInput.{h,cpp}`
- Manages `XrActionSet` and suggested bindings for VR controllers.
- Feeds inputs into libultraship's `ControlDeck` pipeline.

### 4. VRPose
`src/vr/VRPose.h`
- Definition of `VRPose` struct:
  ```cpp
  struct VRPose {
    XrPosef head;
    XrPosef eyes[2];
    XrFovf fov[2];
    XrTime displayTime;
  };
  ```

### 5. GFX Backend Extension
`src/fast/backends/gfx_direct3d11_vr.{h,cpp}`
- Thin extension of the D3D11 backend.
- Exposes `GetDevice()` and `BindExternalRenderTarget(ID3D11Texture2D*)`.

## Rendering Pipeline
1. `xrWaitFrame` / `xrLocateViews` to get `VRPose`.
2. `xrAcquireSwapchainImage` to get the D3D11 texture.
3. Bind the texture as the active render target in the Gfx backend.
4. Game renders its scene for the specific eye.
5. `xrReleaseSwapchainImage`.
6. Repeat for the other eye.
7. `xrEndFrame` submits the layers.
