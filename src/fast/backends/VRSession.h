#pragma once

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <d3d11.h>
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

    struct SwapchainBundle {
        XrSwapchain handle;
        uint32_t width;
        uint32_t height;
        std::vector<XrSwapchainImageD3D11KHR> images;
        std::vector<ID3D11RenderTargetView*> rtvs;
    };

    std::vector<SwapchainBundle> mSwapchains;
    std::vector<XrViewConfigurationView> mConfigViews;
    
    bool mSessionRunning = false;
    XrSessionState mSessionState = XR_SESSION_STATE_UNKNOWN;
    
    uint32_t mWidth = 0;
    uint32_t mHeight = 0;

    XrFrameState mFrameState = {XR_TYPE_FRAME_STATE};
};

} // namespace Fast
