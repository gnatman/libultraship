#pragma once

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <d3d11.h>
#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_D3D11
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#else
#include <openxr/openxr.h>
#endif

#include <vector>
#include <memory>

namespace Fast {

class VRSession {
public:
    VRSession();
    ~VRSession();

    bool Init(ID3D11Device* device);
    void Shutdown();

    bool BeginFrame();
    void EndFrame(ID3D11DeviceContext* context, ID3D11RenderTargetView* leftEyeRtv, ID3D11RenderTargetView* rightEyeRtv);

    bool IsActive() const { return mSessionRunning; }
    
    uint32_t GetWidth() const { return mWidth; }
    uint32_t GetHeight() const { return mHeight; }

    void GetProjectionMatrix(int eye, float nearZ, float farZ, float mat[4][4]) const;
    void GetViewMatrix(int eye, float mat[4][4]) const;

    // Simplified head pose for now
    struct VRPose {
        float pos[3];
        float rot[3];
    };
    void GetHeadPose(float pos[3], float rot[3]) const;

    ID3D11RenderTargetView* BeginEye(int eye);
    void EndEye(int eye);
    ID3D11DepthStencilView* GetEyeDSV(int eye) const;

private:
    bool CreateInstance();
    bool CreateSession(ID3D11Device* device);
    bool CreateSwapchain();
    bool CreateReferenceSpaces();

    XrInstance mInstance = XR_NULL_HANDLE;
    XrSession mSession = XR_NULL_HANDLE;
    XrSpace mAppSpace = XR_NULL_HANDLE;
    XrSpace mViewSpace = XR_NULL_HANDLE;
    XrSystemId mSystemId = XR_NULL_SYSTEM_ID;
    XrDebugUtilsMessengerEXT mDebugMessenger = XR_NULL_HANDLE;

#ifdef _WIN32
    struct SwapchainBundle {
        XrSwapchain handle;
        uint32_t width;
        uint32_t height;
        std::vector<XrSwapchainImageD3D11KHR> images;
        std::vector<ID3D11RenderTargetView*> rtvs;
        ID3D11DepthStencilView* dsv;
    };
#else
    struct SwapchainBundle {
        XrSwapchain handle;
        uint32_t width;
        uint32_t height;
    };
#endif

    std::vector<SwapchainBundle> mSwapchains;
    std::vector<XrViewConfigurationView> mConfigViews;
    std::vector<XrView> mViews;
    
    bool mSessionRunning = false;
    XrSessionState mSessionState = XR_SESSION_STATE_UNKNOWN;
    
    uint32_t mWidth = 0;
    uint32_t mHeight = 0;

    XrFrameState mFrameState = {XR_TYPE_FRAME_STATE};
    uint32_t mCurrentImageIndices[2] = {0, 0};
    bool mFrameStarted = false;
};

} // namespace Fast
