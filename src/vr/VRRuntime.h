#pragma once

#include <openxr/openxr.h>
#include <string>
#include <vector>
#include <memory>

struct ID3D11Texture2D;

namespace Ship {

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

    struct Swapchain {
        XrSwapchain handle;
        int32_t width;
        int32_t height;
        std::vector<ID3D11Texture2D*> images;
    };

private:
    bool CreateInstance();
    bool CreateSession();
    bool CreateSwapchains();
    void HandleSessionState(XrSessionState state);

    bool mInitialized = false;
    XrInstance mInstance = XR_NULL_HANDLE;
    XrSystemId mSystemId = XR_NULL_SYSTEM_ID;
    XrSession mSession = XR_NULL_HANDLE;
    XrSessionState mSessionState = XR_SESSION_STATE_UNKNOWN;

    Swapchain mSwapchains[2];
    
    static std::shared_ptr<VRRuntime> mInstancePtr;
};

} // namespace Ship
