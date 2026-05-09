#pragma once

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <unknwn.h>
#include <d3d11.h>
#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_D3D11
#endif

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <string>
#include <vector>
#include <memory>
#include "ship/window/VRPose.h"

struct ID3D11Texture2D;
struct ID3D11RenderTargetView;
struct ID3D11DepthStencilView;

namespace Ship {

class VRQuadLayer;

class VRRuntime {
public:
    static std::shared_ptr<VRRuntime> GetInstance();
    
    VRRuntime();
    ~VRRuntime();

    bool Init();
    void Shutdown();
    void Update();

    bool IsInitialized() const { return mInitialized; }
    XrInstance GetInstanceHandle() const { return mInstance; }
    XrSession GetSessionHandle() const { return mSession; }
    XrSessionState GetSessionState() const { return mSessionState; }
    void DrawPerformanceOverlay();

    struct Swapchain {
        XrSwapchain handle;
        int32_t width;
        int32_t height;
        std::vector<ID3D11Texture2D*> images;
        std::vector<ID3D11RenderTargetView*> rtvs;
        std::vector<ID3D11DepthStencilView*> dsvs;
        std::vector<ID3D11ShaderResourceView*> srvs;
    };

    const VRPose& GetPose() const { return mCurrentPose; }

    void GetProjectionMatrix(int eye, float* m, float nearZ, float farZ) const;
    void GetViewMatrix(int eye, float* m) const;

    uint32_t AcquireImage(int eye);
    void ReleaseImage(int eye);
    void* GetSwapchainRTV(int eye, uint32_t index) const { return mSwapchains[eye].rtvs[index]; }
    void* GetSwapchainDSV(int eye, uint32_t index) const { return mSwapchains[eye].dsvs[index]; }
    void* GetSwapchainImage(int eye, uint32_t index) const { return mSwapchains[eye].images[index]; }
    void* GetSwapchainSRV(int eye, uint32_t index) const { return mSwapchains[eye].srvs[index]; }
    void GetSwapchainDimensions(int eye, int32_t* w, int32_t* h) const { *w = mSwapchains[eye].width; *h = mSwapchains[eye].height; }

    int CreateQuadLayer(int32_t width, int32_t height);
    uint32_t AcquireQuadImage(int layerIndex);
    void ReleaseQuadImage(int layerIndex);
    void* GetQuadRTV(int layerIndex, uint32_t imageIndex) const;
    void* GetQuadDSV(int layerIndex, uint32_t imageIndex) const;
    void* GetQuadSRV(int layerIndex, uint32_t imageIndex) const;
    void GetQuadDimensions(int layerIndex, int32_t* w, int32_t* h) const;
    void SetQuadPose(int layerIndex, XrPosef pose);
    void SetQuadSize(int layerIndex, XrExtent2Df size);

    bool BeginFrame();
    void EndFrame();
    bool ShouldRender() const { return mFrameState.shouldRender; }

private:
    bool CreateInstance();
    bool CreateSession();
    bool CreateSwapchains();
    void HandleSessionState(XrSessionState state);
    void UpdatePose(XrTime predictedTime);

    bool mInitialized = false;
    XrInstance mInstance = XR_NULL_HANDLE;
    XrSystemId mSystemId = XR_NULL_SYSTEM_ID;
    XrSession mSession = XR_NULL_HANDLE;
    XrSessionState mSessionState = XR_SESSION_STATE_UNKNOWN;
    XrSpace mStageSpace = XR_NULL_HANDLE;

    XrFrameState mFrameState = { XR_TYPE_FRAME_STATE };
    VRPose mCurrentPose;
    std::vector<XrView> mViews;
    Swapchain mSwapchains[2];
    std::vector<std::shared_ptr<VRQuadLayer>> mQuadLayers;
    
    static std::shared_ptr<VRRuntime> mInstancePtr;
};

} // namespace Ship
